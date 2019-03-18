#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <signal.h>
#include <sys/signal.h>

#include "DATA_PACKET.pb.h"
#include "utility.h"

using namespace std;

#define BUF_SIZE 4096

struct heartbeatArgs {
  string heartAddr;
  bool* killed;
};

bool debug;
bool killed = false;
int servNum;
map<char, vector<string>> serverConfig;
pthread_t heartThread;
int adminSock;
sem_t numConnSem;
int numConnections = 0;

string getInputArgs(int argc, char *argv[]) {
  int opt;
  debug = false; // debug not on by default

  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
      case 'v':
        debug = true;
        break;
      default:
        cerr << "Error invalid argument for " << argv[0] << endl;
        cerr << "Usage: " << argv[0] << " [-v] </path/to/serverConfig> servNum"
          << endl;
        exit(1);
    }
  }

  string configFilename;
  if (optind == argc-2) {
    configFilename.append(argv[optind++]);
    servNum = atoi(argv[optind]);
  } else {
    cerr << "Error invalid argument for " << argv[0] << endl;
    cerr << "Usage: " << argv[0] << " [-v] </path/to/serverConfig> servNum"
      << endl;
    exit(1);
  }

  return configFilename;
}

void ctrlc_handler(int signum) {
  close(adminSock);
  exit(0);
}

char* packetToChar(Packet pkt) {
  string out;
  if (!pkt.SerializeToString(&out)) {
    cerr << "Could not serialize packet" << endl;
    raise(SIGINT);
  }
  string pre = generate_prefix(out.size());
  out = pre + out;
  char *msg = (char *) calloc(out.size(), sizeof(char));
  out.copy(msg, out.size());
  return msg;
}

Packet getServerStatus() {
  // connect to master and get the server status
  Packet to_send, blank;
  to_send.set_command("ADMINUP");
  char *response = packetToChar(to_send);
  string rStr(response);

  struct sockaddr_in masterAddr;
  masterAddr.sin_family = AF_INET;
  string masterStr(serverConfig['M'][0]);
  int colon = masterStr.find(":");
  int comma = masterStr.find(",");
  string ip(masterStr.substr(0, colon));
  string port(masterStr.substr(colon+1, comma-colon-1));
  int portNum;
  try {
    portNum = stoi(port);
  } catch (invalid_argument e) {
    cerr << "Bad port for master" << endl;
    return blank;
  }
  inet_aton(ip.c_str(), &(masterAddr.sin_addr));
  masterAddr.sin_port = htons(portNum);

  int masterSock;
  if ( (masterSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return blank;
  }

  if (connect(masterSock, (struct sockaddr *) &masterAddr, sizeof(masterAddr))
      < 0) {
    perror("connect");
    close(masterSock);
    return blank;
  }

  cout << "SENDING: " << response << endl;
  do_send(&masterSock, response, rStr.size());
  free(response);

  Packet r;
  char status[BUF_SIZE];
  int n = do_recv(&masterSock, status, -1); // indefinite b/c master always up
  status[n] = 0;
  cout << "STATUS: " << status << endl;
  if (!r.ParseFromString(status)) {
    cerr << "couldn't parse" << endl;
    return blank;
  }

  string command = r.command();
  transform(command.begin(), command.end(), command.begin(), ::tolower);
  if (command != "servstat") {
    cerr << "invalid response from master" << endl;
    return blank;
  }

  // close connection with master and return status string in data
  close(masterSock);
  return r;
}

int killRestartServer(string addr, string cmd) {
  // send kill command to addr
  string ip(addr.substr(0, addr.find(":")));
  string port(addr.substr(addr.find(":")+1));
  int portNum;
  try {
    portNum = stoi(port);
  } catch (invalid_argument e) {
    cerr << "bad port: " << port << endl;
    return -1;
  }
  struct sockaddr_in servAddr;
  servAddr.sin_family = AF_INET;
  inet_aton(ip.c_str(), &(servAddr.sin_addr));
  servAddr.sin_port = htons(portNum);
  Packet p;
  p.set_command(cmd);
  char *response = packetToChar(p);
  string rStr(response);

  int s;
  if ( (s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return -1;
  }
  if (connect(s, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
    perror("connect to serve");
    return -1;
  }

  // grab welcome message (give 3 seconds)
  char junk[BUF_SIZE];
  int junkN = do_recv(&s, junk, 3);
  if (junkN < 0 && debug)
    cerr << "timed out, just proceed" << endl;

  do_send(&s, response, rStr.size());
  free(response);

  // wait for OK response from server
  char buf[BUF_SIZE];
  int n = do_recv(&s, buf, 3);
  if (n < 0) {
    // if we don't receive an ok, we will simply exit and assume the server has
    // entered a kill/restart state 
    //perror("read");
    //raise(SIGINT);
    close(s);
    return 1;
  }
  buf[n] = 0;

  Packet r;
  if (!r.ParseFromString(buf)) {
    cerr << "Could not parse received packet" << endl;
    return -1;
  }

  string command = r.command();
  transform(command.begin(), command.end(), command.begin(), ::tolower);
  if (command != "ok") {
    cerr << "Error, unknown response from server " << addr << endl;
    return -1;
  }

  // if command ok, close socket
  close(s);
  return 1;
}

int replyWithStatus(int sock) {
  // get a server status and reply through sock
  //string statusStr = getServerStatus();
  
  //Packet to_send;
  //to_send.set_command("SERVSTAT");
  //to_send.set_data(statusStr);
  Packet to_send = getServerStatus();
  if (to_send.command() != "SERVSTAT") {
    return -1;
  }
  char *response = packetToChar(to_send);
  string rStr(response);
  cout << "SENDING TO FES: " << response << endl;
  if( do_send(&sock, response, rStr.size()) < 0) {
    cerr << "error sending..." << endl;
    return -1;
  }
  free(response);
  return 0;
}

void * client_handler(void * args) {
  int sock = *( (int *) args);

  // read from socket to see if we're needed
  char buf[BUF_SIZE];
  int n = 0;
  n = do_recv(&sock, buf, -1);
  if (n < 0) {
    perror("do_recv");
    raise(SIGINT);
  }
  buf[n] = 0;

  Packet r;
  if (!r.ParseFromString(buf)) {
    cerr << "couldn't parse" << endl;
    raise(SIGINT);
  }

  string command = r.command();
  transform(command.begin(), command.end(), command.begin(), ::tolower);

  // first check to make sure we aren't in killed state
  if (killed) {
    cout << "in killed state :)" << endl;
    // in kill state, only respond if command is restart
    if (command == "restart") {
     killed = false;
     Packet to_send;
     to_send.set_command("OK");
     char *response = packetToChar(to_send);
     string rStr(response);
     do_send(&sock, response, rStr.size());
     close(sock);
     free(response);
     sem_wait(&numConnSem);
     numConnections--;
     sem_post(&numConnSem);
     pthread_exit(NULL);
    } else {
      // just close the socket and return
      close(sock);
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      pthread_exit(NULL);
    }
  }

  if (command == "killserv") {
    string addrToKill = r.arg(0);
    // send the kill message (and send back error if it fails in any way)
    if (killRestartServer(addrToKill, "KILL") < 0) {
      send_error_pack(sock, "-ERR", "Could not kill server");
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      close(sock);
      pthread_exit(NULL);
    }
    // now get the current server status from master and send back
    // send back error if this fails in any way
    if (replyWithStatus(sock) < 0) {
      send_error_pack(sock, "-ERR", "Could not get status");
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      close(sock);
      pthread_exit(NULL);
    }
    sem_wait(&numConnSem);
    numConnections--;
    sem_post(&numConnSem);
    close(sock);
    pthread_exit(NULL);
  } else if (command == "restartserv") {
    string addrToStart = r.arg(0);
    // send the restart message (and send back error if it fails in any way)
    if (killRestartServer(addrToStart, "RESTART") < 0) {
      send_error_pack(sock, "-ERR", "Could not restart server");
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      close(sock);
      pthread_exit(NULL);
    }
    // reply with status of servers from master and send back
    // send back error if this fails in any way
    if (replyWithStatus(sock) < 0) {
      send_error_pack(sock, "-ERR", "Could not get status");
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      close(sock);
      pthread_exit(NULL);
    }
    sem_wait(&numConnSem);
    numConnections--;
    sem_post(&numConnSem);
    close(sock);
    pthread_exit(NULL);
  } else if (command == "kvdata")
  {
    // Get all the rows and columns for a given KVS
    // Get the ip of KVS xx.xx.xx.xx:mmmm
    string ip = r.arg(0);
    pair<string, int> addr;
    addr = get_ip_port(ip);

    // Connect to KVS to retrieve value
    int out_fd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in kvstore_addr;
    bzero(&kvstore_addr, sizeof(kvstore_addr));
    kvstore_addr.sin_family = AF_INET;
    kvstore_addr.sin_port = htons(addr.second);
    inet_pton(AF_INET, (addr.first).c_str(), &(kvstore_addr.sin_addr));
    int stat = connect(out_fd, (struct sockaddr*)&kvstore_addr, sizeof(kvstore_addr));

    if(stat == -1)
    {
      cerr << "Could not connect to KVS: " + ip + "\r\n";
      send_error_pack(sock, "-ERR", "Could not connect to KVS");
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      close(sock);
      pthread_exit(NULL);
    }
    Packet pack;
    pack.set_command("GETALL");
    send_packet(pack, out_fd);

    pack.Clear();
    char buff[4096];

    // Get response from KVS
    int idx = do_recv(&out_fd, buff, 5);
    if(idx < 0)
    {
      cerr << "Error reading input. Closing connection.\n";
      send_error_pack(sock, "-ERR", "Error reading input.");
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      close(sock);
      pthread_exit(NULL);
    }
    buff[idx] = 0;
    if(!pack.ParseFromString(buff))
    {
      cerr << "Failed to parse received message!\n";
      send_error_pack(sock, "-ERR", "Error Parsing packet");
    }

    // Forward packet to fes
    if(pack.status_code() == "200")
      send_packet(pack, sock);
    else
      send_error_pack(sock, "-ERR", "KVS could not find the requested data");

    sem_wait(&numConnSem);
    numConnections--;
    sem_post(&numConnSem);
  } 
  else if(command == "view")
  {
    string row = r.arg(0);
    string col = r.arg(1);
    string ip = r.data();

    // Get address of KVS
    pair<string, int> addr;
    addr = get_ip_port(ip);

    // Connect to KVS to retrieve value
    int out_fd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in kvstore_addr;
    bzero(&kvstore_addr, sizeof(kvstore_addr));
    kvstore_addr.sin_family = AF_INET;
    kvstore_addr.sin_port = htons(addr.second);
    inet_pton(AF_INET, (addr.first).c_str(), &(kvstore_addr.sin_addr));
    int stat = connect(out_fd, (struct sockaddr*)&kvstore_addr, sizeof(kvstore_addr));

    if(stat == -1)
    {
      cerr << "Could not connect to KVS: " + ip + "\r\n";
      send_error_pack(sock, "-ERR", "Could not connect to KVS");
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      close(sock);
      pthread_exit(NULL);
    }

    Packet pack;
    pack.set_row(row);
    pack.set_column(col);
    pack.set_command("GET");

    send_packet(pack, out_fd);

    pack.Clear();
    char buff[4096];

    // Get response from KVS
    int idx = do_recv(&out_fd, buff, 5);
    if(idx < 0)
    {
      cerr << "Error reading input. Closing connection.\n";
      send_error_pack(sock, "-ERR", "Error reading input.");
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      close(sock);
      pthread_exit(NULL);
    }
    buff[idx] = 0;
    if(!pack.ParseFromString(buff))
    {
      cerr << "Failed to parse received message!\n";
      send_error_pack(sock, "-ERR", "Error Parsing packet");
    }

    // Forward packet to fes
    if(pack.status_code() == "200")
      send_packet(pack, sock);
    else
      send_error_pack(sock, "-ERR", "KVS could not find the requested data");

    sem_wait(&numConnSem);
    numConnections--;
    sem_post(&numConnSem);
  }
  else if (command == "adminup") {
    // simply get the server status from master
    if (replyWithStatus(sock) < 0) {
      send_error_pack(sock, "-ERR", "Could not get status");
      sem_wait(&numConnSem);
      numConnections--;
      sem_post(&numConnSem);
      close(sock);
      pthread_exit(NULL);
    }
    sem_wait(&numConnSem);
    numConnections--;
    sem_post(&numConnSem);
    close(sock);
    pthread_exit(NULL);
  } else if (command == "kill") {
    // we are getting killed, so we need to set the kill state, reply, and
    // return
    if (debug)
      cerr << "getting killed" << endl;
    killed = true;
    Packet to_send;
    to_send.set_command("OK");
    char *response = packetToChar(to_send);
    string rStr(response);
    do_send(&sock, response, rStr.size());
    close(sock);
    sem_wait(&numConnSem);
    numConnections--;
    sem_post(&numConnSem);
    free(response);
    pthread_exit(NULL);
  } else if (command == "curload") {
    // sending back the current load
    Packet to_send;
    to_send.set_command("LOAD");
    sem_wait(&numConnSem);
    // waiting on current connections, but we'll subtract off this current one
    to_send.add_arg(to_string(numConnections-1));
    sem_post(&numConnSem);
    char *response = packetToChar(to_send);
    if (debug)
      cerr << "sending back: " << response << endl;
    string rStr(response);
    do_send(&sock, response, rStr.size());
    close(sock);
    sem_wait(&numConnSem);
    numConnections--;
    sem_post(&numConnSem);
    free(response);
    pthread_exit(NULL);
  }
  close(sock);
  return NULL;
}

int main(int argc, char* argv[]) {
  signal(SIGINT, ctrlc_handler);
  sem_init(&numConnSem, 0, 1);
  string configFilename = getInputArgs(argc, argv);

  serverConfig = parse_config(configFilename);
  // extract our address from serverConfig
  string fullAddr = serverConfig['C'][servNum-1];
  int colon = fullAddr.find(":");
  int comma = fullAddr.find(",");
  string ip(fullAddr.substr(0, colon));
  string mainPort(fullAddr.substr(colon+1, comma-colon-1));
  string heartPort(fullAddr.substr(comma+1));
  string heartAddr(ip);
  heartAddr.append(":");
  heartAddr.append(heartPort);
  int portNum;
  try {
    portNum = stoi(mainPort);
  } catch (invalid_argument e) {
    cerr << "Invalid port number" << endl;
    exit(1);
  }

  if (debug) {
    cerr << "Main Address: " << ip << ":" << mainPort << " Heart Address: " <<
      heartAddr << endl;
  }

  struct heartbeatArgs args;
  args.heartAddr = heartAddr;
  args.killed = &killed;
  pthread_create(&heartThread, NULL, heartbeat_client, &args);
  pthread_detach(heartThread);

  struct sockaddr_in adminAddr;
  socklen_t adminLen = sizeof(struct sockaddr_in);

  adminAddr.sin_family = AF_INET;
  inet_aton(ip.c_str(), &(adminAddr.sin_addr));
  adminAddr.sin_port = htons(portNum);

  if ( (adminSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("adminSock");
    exit(1);
  }

  int enable = 1;
  if (setsockopt(adminSock, SOL_SOCKET, SO_REUSEADDR, &enable,
        sizeof(int)) < 0) { 
    perror("setsockopt");
    raise(SIGINT);
  }

  if ( bind(adminSock, (struct sockaddr *) &adminAddr, adminLen) < 0) {
    perror("bind");
    raise(SIGINT);
  }

  if (listen(adminSock, 100) < 0) {
    perror("listen");
    raise(SIGINT);
  }

  adminLen = sizeof(struct sockaddr_in);

  struct sockaddr_in cAddr;
  int c_sock;

  while (true) {
    if ( (c_sock = accept(adminSock, (struct sockaddr *)
            &cAddr, &adminLen)) < 0) {
      perror("accept");
      raise(SIGINT);
    }
    sem_wait(&numConnSem);
    numConnections++;
    sem_post(&numConnSem);

    pthread_t cThread;
    pthread_create(&cThread, NULL, client_handler, &c_sock);
    pthread_detach(cThread);

    adminLen = sizeof(struct sockaddr_in);
  }

  return 0;
}
