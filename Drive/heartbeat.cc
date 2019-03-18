#include "heartbeat.h"

void sendToServerAddress(string server, char *heartbeatMsg) {
  string ip(server.substr(0, server.find(":")));
  string port(server.substr(server.find(":")+1));
  int portNum;
  try {
    portNum = stoi(port);
  } catch (invalid_argument e) {
    cerr << "bad port: " << port << endl;
    raise(SIGINT);
  }

  struct sockaddr_in servAddr;
  servAddr.sin_family = AF_INET;
  inet_aton(ip.c_str(), &(servAddr.sin_addr));
  servAddr.sin_port = htons(portNum);

  if ( sendto(heartbeat_sock, heartbeatMsg, strlen(heartbeatMsg),
        0, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
    perror("sendto");
    raise(SIGINT);
  }
}

bool timeCompare(struct timeval t0, struct timeval t1) {
  if (t0.tv_sec < t1.tv_sec) {
    return true;
  } else if (t0.tv_sec == t1.tv_sec) {
    // result based on usec
    return (t0.tv_usec < t1.tv_usec);
  } else {
    return false;
  }
}

struct timeval getDiff(struct timeval t0, struct timeval t1) {
  // returns t1-t0
  struct timeval tv;
  time_t secDiff = t1.tv_sec - t0.tv_sec;
  suseconds_t usecDiff = t1.tv_usec - t0.tv_usec;
  if (usecDiff < 0) {
    secDiff = secDiff-1;
    usecDiff = 1000000+usecDiff;
  }
  tv.tv_sec = secDiff;
  tv.tv_usec = usecDiff;
  return tv;
}

int timeoutReceiveLoop(int timeoutSec, int timeoutMilli) {
  struct timeval now, stop;
  gettimeofday(&now, NULL);
  stop.tv_sec = now.tv_sec + timeoutSec;
  stop.tv_usec = now.tv_usec + (timeoutMilli*1000);

  while (timeCompare(now, stop)) {
    struct timeval tv = getDiff(now, stop);
    if (setsockopt(heartbeat_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))
        < 0) {
      perror("setsockopt");
      raise(SIGINT);
    }

    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    char rBuf[BUF_SIZE];
    int rlen = recvfrom(heartbeat_sock, rBuf, BUF_SIZE-1, 0,
        (struct sockaddr *) &src, &srclen);
    if (rlen < 0) {
      if (errno == EAGAIN) {
        break;
      } else if (errno == EINTR) {
        // standard heartbeat was destroyed by the quick heartbeat, that's ok, we simply return because quick heartbeat registered another
        return EINTR;
      } else {
        // actual error (not timeout)
        cerr << "Error receiving packet " << rBuf << endl;
        raise(SIGINT);
      }
    }
    rBuf[rlen-2] = 0;

    Packet packet_rcvd;
    if (!packet_rcvd.ParseFromString(rBuf)) {
      cerr << "Unable to parse received message" << endl;
      raise(SIGINT);
    }

    string command = packet_rcvd.command();
    transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (command == "up") {
      string lookup(inet_ntoa(src.sin_addr));
      lookup.append(":");
      lookup.append(to_string(ntohs(src.sin_port)));
      // convert from heart port to prim port
      string server(heartToPrim[lookup]);
      sem_wait(&heartSem);
      serverStatus[server] = true;
      sem_post(&heartSem);
    }
    gettimeofday(&now, NULL);
  }
  return 0;
}

void heartbeat(int signum) {
  int timeoutSec, timeoutMilli;
  if (signum == SIGALRM) {
    // standard heartbeat
    if (debug)
      cerr << endl << "standard heartbeat" << endl;
    timeoutSec = standardTimeoutSec;
    timeoutMilli = standardTimeoutMilli;
  } else if (signum == SIGUSR1) {
    if (debug)
      cerr << endl << "quick heartbeat" << endl;
    timeoutSec = quickTimeoutSec;
    timeoutMilli = quickTimeoutMilli;
  } else {
    cerr << "Unknown signal to heartbeat" << endl;
    raise(SIGINT);
  }

  // don't do anything if the timeout is 0
  if (timeoutSec == 0 && timeoutMilli == 0) {
    if (signum == SIGUSR1)
      sem_post(&quickHeartbeatSem);
    alarm(heartbeatPeriod);
    return;
  }

  // prepare heartbeat message to send to each server
  Packet heartbeatPkt;
  heartbeatPkt.set_command("HEART");
  string out;
  if (!heartbeatPkt.SerializeToString(&out)) {
    cerr << "Could not serialize packet" << endl;
    raise(SIGINT);
  }
  out = out + "\r\n";
  char heartbeatMsg[out.size()];
  out.copy(heartbeatMsg, out.size());

  // set all servers as down and send message to the server
  if (debug)
    cerr << "BEGIN HEARTBEAT" << endl;
  sem_wait(&heartSem);
  for (auto it = serverStatus.begin(); it != serverStatus.end(); ++it) {
    serverStatus[it->first] = false;
    // send to the heartbeat port of it->first
    string server(primToHeart[it->first]);
    sendToServerAddress(server, heartbeatMsg);
  }
  sem_post(&heartSem);
  
  // wait in the timeout loop to receive responses from servers
  if (timeoutReceiveLoop(timeoutSec, timeoutMilli) == EINTR) {
    if (debug)
      cerr << "quick heartbeat finished and canceled standard, return" << endl;
    return;
  }

  if (debug) {
    sem_wait(&heartSem);
    cerr << "Status (after first heartbeat: " << endl;
    for (auto it = serverStatus.begin(); it != serverStatus.end(); ++it) {
      cerr << "Server: " << it->first << " Status: " <<
        (it->second ? "UP" : "DOWN") << endl;
    }
    cerr << "checking for another heartbeat..." << endl;
    sem_post(&heartSem);
  }

  if (signum == SIGUSR1) {
    // since this is a quick heartbeat, we are not going to check again
    alarm(heartbeatPeriod);
    if (debug)
      cerr << "Registering another heartbeat" << endl;
    sem_post(&quickHeartbeatSem);
    return;
  }

  // send another message if servers are still down
  bool wait = false;
  sem_wait(&heartSem);
  for (auto it = serverStatus.begin(); it != serverStatus.end(); ++it) {
    if (!it->second) {
      wait = true;
      string server(primToHeart[it->first]);
      sendToServerAddress(server, heartbeatMsg);
    }
  }
  sem_post(&heartSem);

  if (wait)
    cerr << "giving down servers another chance" << endl;

  // only timeout if we need to
  if (wait) {
    if (timeoutReceiveLoop(timeoutSec, timeoutMilli) == EINTR) {
      if (debug)
        cerr << "quick heartbeat canceled standard" << endl;
      return;
    }
  }

  // print status if we had to wait
  if (debug && wait) {
    cerr << "Status after second try:" << endl;
    for (auto it = serverStatus.begin(); it != serverStatus.end(); ++it) {
      cerr << "Server: " << it->first << " Status: " <<
        (it->second ? "UP" : "DOWN") << endl;
    }
  }
  if (debug)
    cerr << "Registering another heartbeat" << endl;

  alarm(heartbeatPeriod);
}

void * heartbeat_master(void * args) {
  // create socket for heartbeat messages
  string masterAddr = serverConfig['M'][0];
  string ip(masterAddr.substr(0, masterAddr.find(":")));
  struct sockaddr_in saddr_in;
  socklen_t saddr_len = sizeof(struct sockaddr_in);
  saddr_in.sin_family = AF_INET;
  inet_aton(ip.c_str(), &(saddr_in.sin_addr));
  saddr_in.sin_port = htons(6347);

  if ( (heartbeat_sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("heartbeat_sock");
    raise(SIGINT);
  }

  if (bind(heartbeat_sock, (struct sockaddr *) &saddr_in, saddr_len) < 0) {
    perror("bind");
    raise(SIGINT);
  }

  // register handler for SIGALRM and SIGUSR1
  signal(SIGALRM, heartbeat);
  signal(SIGUSR1, heartbeat);

  // do the first heartbeat, which registers subsequent heartbeats
  raise(SIGALRM);

  // pause forever (waiting for more signals)
  while (true)
    pause();

  return NULL;
}
