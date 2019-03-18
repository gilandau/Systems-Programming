#include "masterComms.h"

void quickHeartbeat() {
  // sends signal to perform a quick heartbeat
  pthread_kill(heartbeatThread, SIGUSR1);
}

string userUpServers(string username) {
  // perform a quick heartbeat (we need to use semaphores so that we wait until
  // the heartbeat is over
  if (sem_trywait(&quickHeartbeatSem) < 0) {
    // someone already is doing a heartbeat, wait until they're done
    sem_wait(&quickHeartbeatSem);
    sem_post(&quickHeartbeatSem); // post immediately (we don't need it)
  } else {
    // no active heartbeat currently, send signal to the heartbeat thread to
    // perform one
    pthread_kill(heartbeatThread, SIGUSR1);
    sem_wait(&quickHeartbeatSem);
    sem_post(&quickHeartbeatSem);
  }

  // now we have the most up to date status of the servers, we simply find the
  // best one that (in order of the vectors, as the first is the primary load
  // for the user)
  string addrToSend("");
  // guaranteed to know that the user exists (calls to this function must check
  // this beforehand)
  vector<string> userServers = userToKVMap[username];
  sem_wait(&heartSem);
  for (auto it = userServers.begin(); it != userServers.end(); ++it) {
    if (serverStatus[*it]) {
      // up server
      addrToSend.append(*it);
      break;
    }
  }

  if (addrToSend == "") 
    addrToSend.append("SERVFAIL");
  sem_post(&heartSem);
  return addrToSend;
}

void performQuickHeartbeat() {
  if (sem_trywait(&quickHeartbeatSem) < 0) {
    sem_wait(&quickHeartbeatSem);
    sem_post(&quickHeartbeatSem);
  } else {
    pthread_kill(heartbeatThread, SIGUSR1);
    sem_wait(&quickHeartbeatSem);
    sem_post(&quickHeartbeatSem);
  }
}

bool serverLoadSorter(const pair<string,int>& a, const pair<string,int>& b) {
  if (a.second != b.second)
    return a.second < b.second;
  return a.first < b.first;
}

/*
void addToFile(string username, vector<string>& servers) {
  sem_wait(&userFile);
  ofstream out;
  out.open("systemUsers", ofstream::app);
  // append newline first 
  // out << endl; guess this isn't needed (done by the ostream)
  out << username << ":";
  // for each server in the vector, add to the out stream
  int numServs = servers.size();
  for (int i = 0; i < numServs; i++) {
    out << servers[i];
    if (i != numServs-1)
      out << ",";
  }
  out << endl;
  out.close();
  sem_post(&userFile);
}
*/

void addToFile(string username, vector<string>& servers) {
  sem_wait(&userFile);
  ofstream out;
  out.open("systemUsers", ofstream::app);
  out << username << ":" << servers[0] << endl;
  out.close();
  sem_post(&userFile);
}

bool inServerVector(string elem, vector<string>& v) {
  for (auto it = v.begin(); it != v.end(); ++it) 
    if (*it == elem)
      return true;
  return false;
}

vector<string> newClientServers(string username) {
  // while choosing a new server, we completely surround everything in a mutex
  // so that no other user could possibly mess up our choices and misread the
  // loads
  sem_wait(&loadSem);

  // first, we perform a quick heartbeat to get an up to date status on all
  // servers
  if (sem_trywait(&quickHeartbeatSem) < 0) {
    sem_wait(&quickHeartbeatSem);
    sem_post(&quickHeartbeatSem);
  } else {
    pthread_kill(heartbeatThread, SIGUSR1);
    sem_wait(&quickHeartbeatSem);
    sem_post(&quickHeartbeatSem);
  }

  vector<string> clientServers;

  // now, we select the primary server (based on kvLoad)
  // make vector of pairs to easily sort the servers
  vector<pair<string,int>> primLoad;
  for (auto it = kvLoad.begin(); it != kvLoad.end(); ++it) {
    pair<string, int> servPair(it->first, it->second);
    primLoad.push_back(servPair);
  }
  sort(primLoad.begin(), primLoad.end(), serverLoadSorter);

  // find the least load (prim) up server
  sem_wait(&heartSem);
  string primServ("");
  for (auto it = primLoad.begin(); it != primLoad.end(); ++it) {
    if (serverStatus[it->first]) {
      primServ.append(it->first);
      break;
    }
  }
  sem_post(&heartSem);

  if (primServ == "") {
    cerr << "No KV addresses up. Cannot assign address. " << endl;
    // return empty vector to signal to client that there are no servers
    vector<string> empty;
    sem_post(&loadSem);
    return empty;
  }

  // put the first KV as the primary server for the user
  clientServers.push_back(primServ);
  kvLoad[primServ]++;

  // now place the replications for primServ into clientServers
  vector<string> reps = kvReps[primServ];
  for (auto it = reps.begin(); it != reps.end(); ++it)
    clientServers.push_back(*it);
  
  // add this client's information to the userToKVMap
  userToKVMap[username] = clientServers;

  // update the file for the system
  addToFile(username, clientServers);

  sem_post(&loadSem);

  // return the addresses chosen
  return clientServers;
}

/*
void sendPrimaryToKV(string username, vector<string>& servers) {
  // need to connect to the first server and provide all other servers
  string primServ(servers[0]);
  string addr(primServ.substr(0, primServ.find(":")));
  string port(primServ.substr(primServ.find(":")+1));
  if (debug) 
    cerr << "Connecting to: " << primServ << endl;

  int portNum;
  try {
    portNum = stoi(port);
  } catch (invalid_argument e) {
    cerr << "bad port: " << port << endl;
    raise(SIGINT);
  }
  struct sockaddr_in saddr_in;
  saddr_in.sin_family = AF_INET;
  inet_aton(addr.c_str(), &(saddr_in.sin_addr));
  saddr_in.sin_port = htons(portNum);

  // create the packet
  Packet p;
  p.set_command("CLREP");
  // add the servers
  int numServs = servers.size();
  for (int i = 1; i < numServs; i++)
    p.add_arg(servers[i]);
  
  char *response = packetToChar(p);
  string rStr(response);

  int kv_sock;
  if ( (kv_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("kv_sock");
    raise(SIGINT);
  }

  // TODO check if KV addr is down

  if (connect(kv_sock, (struct sockaddr *) &saddr_in, sizeof(saddr_in)) 
      < 0) {
    perror("connect");
    raise(SIGINT);
  }

  do_send(&kv_sock, response, rStr.size());
  free(response);

  // wait for response from KV addr
  char buf[BUF_SIZE];
  int n = 0;
  n = do_recv(&kv_sock, buf, -1);
  if (n < 0) {
    perror("read");
    raise(SIGINT);
  }

  Packet r;
  if (!r.ParseFromString(buf)) {
    cerr << "Could not parse received packet" << endl;
    raise(SIGINT);
  }

  string command = r.command();
  transform(command.begin(), command.end(), command.begin(), ::tolower);
  if (command != "ok") {
    cerr << "Error, unexpected packet from KV store" << endl;
    raise(SIGINT);
  }

  // if command is ok, close socket
  close(kv_sock);
}
*/

string getStatusStr() {
  // creates the status string needed for the admin/frontend to get the status
  // of each server
  string statusStr("{");
  for (auto c = serverConfig.begin(); c != serverConfig.end(); ++c) {
    vector<string> servers = c->second;
    for (auto s = servers.begin(); s != servers.end(); ++s) {
      if (c->first == 'M') {
        // special case to not mess up serverStatus
        statusStr.insert(statusStr.end(), c->first);
        string addr((*s).substr(0, (*s).find(",")));
        statusStr.append(addr);
        statusStr.append("=true,");
        continue;
      }
      // transform s into just the main address
      string addr((*s).substr(0, (*s).find(",")));
      statusStr.insert(statusStr.end(), c->first);
      statusStr.append(addr);
      statusStr.append("=");
      statusStr.append( (serverStatus[addr] ? "true" : "false") );
      statusStr.append(",");
    }
  }
  statusStr.pop_back();
  statusStr.append("}");

  return statusStr;
}

void * client_handler(void * args) {
  int client_sock = *((int *) args);

  // read from the socket
  char buf[BUF_SIZE];
  int n = 0;
  n = do_recv(&client_sock, buf, -1);
  if (n == -1) {
    cerr << "Error reading" << endl;
    raise(SIGINT);
  }
  buf[n] = 0;

  cout << buf << endl;

  Packet packet_rcvd;
  if (!packet_rcvd.ParseFromString(buf)) {
    cerr << "Failed to parse received message." << endl;
    raise(SIGINT);
  }
  memset(buf, 0, sizeof(buf));

  // parse the command
  string command = packet_rcvd.command();
  transform(command.begin(), command.end(), command.begin(), ::tolower);
  if (command == "wheris") {
    string username;
    if (packet_rcvd.arg_size() != 0)
      username.append(packet_rcvd.arg(0));
    Packet packet_to_send;
    auto it = userToKVMap.find(username);
    if (it == userToKVMap.end()) {
      cout << username << " not present " << endl;
      packet_to_send.set_command("ABSENT");
    } else {
      string addrToSend = userUpServers(username);
      if (addrToSend == "SERVFAIL") {
        cout << username << " KV down: " << addrToSend << endl;
        packet_to_send.set_command("SERVFAIL");
      } else {
        cout << username << " is up: " << addrToSend << endl;
        packet_to_send.set_command("KVADDR");
        packet_to_send.add_arg(addrToSend);
      }
    }

    char *response = packetToChar(packet_to_send);
    string rStr(response);
    do_send(&client_sock, response, rStr.size());
    close(client_sock);
    free(response);
    pthread_exit(NULL);
  } else if (command == "newcl") {
    // new client registering
    string username = packet_rcvd.arg(0);
    if (userToKVMap.find(username) != userToKVMap.end()) {
      // user already exists, check for servers up and send it back
      // (or send SERVFAIL if all servers are down)
      Packet packet_to_send;
      // find up addresses for the user
      string addrToSend = userUpServers(username);
      if (addrToSend == "SERVFAIL") {
        packet_to_send.set_command("SERVFAIL");
      } else {
        packet_to_send.set_command("DUPCL");
        packet_to_send.add_arg(addrToSend);
      }

      char *response = packetToChar(packet_to_send);
      string rStr(response);
      do_send(&client_sock, response, rStr.size());
      close(client_sock);
      free(response);
      pthread_exit(NULL);
    } else {
      // new client, find the top three servers that are currently up
      // newClientServers() will also take care of adding the load for each
      // server and adding the client
      vector<string> newClAddr = newClientServers(username);
      if (debug) {
        cerr << "Addresses for new client: " << username << "\t" << endl;
        for (auto it = newClAddr.begin(); it != newClAddr.end(); ++it)
          cerr << *it << " ";
        cerr << endl;
        // print out the current load for each server and each clients' server
        cerr << "Load:" << endl;
        for (auto it = kvLoad.begin(); it != kvLoad.end(); ++it)
          cerr << "\tKV: " << it->first << " PrimLoad: " << it->second << endl;
        for (auto it = userToKVMap.begin(); it != userToKVMap.end(); ++it) {
          vector<string> servers = it->second;
          cerr << "User: " << it->first << " ";
          for (auto s = servers.begin(); s != servers.end(); ++s)
            cerr << *s << " ";
          cerr << endl;
        }
      }
      // prepare packet to send back
      Packet packet_to_send;
      
      // check to make sure we have addresses to send
      if (newClAddr.size() == 0) {
        packet_to_send.set_command("SERVFAIL");
      } else {
        packet_to_send.set_command("CLADDED");
        // just add the primary server
        packet_to_send.add_arg(newClAddr[0]);
      }

      char *response = packetToChar(packet_to_send);
      string rStr(response);
      do_send(&client_sock, response, rStr.size());
      close(client_sock);
      free(response);

      // now, connect to the primary KV addr and send the replication servers to
      // primary (only if we have an address to send)
      /* Not doing this anymore! :)
       * if (newClAddr.size() != 0)
        sendPrimaryToKV(username, newClAddr);
        */

      pthread_exit(NULL);
    }
  } else if (command == "adminup") {
    // command to get the status of all servers at this moment (we will use a
    // quick heartbeat to determine this)
    performQuickHeartbeat();
    // get the status sem
    sem_wait(&heartSem);
    // blank string to construct our encoded status string
    string statusStr = getStatusStr();
    sem_post(&heartSem);

    if (debug)
      cerr << "Status Str: " << statusStr << endl;

    // create packet to send back
    Packet to_send;
    to_send.set_command("SERVSTAT");
    to_send.set_data(statusStr);
    char *response = packetToChar(to_send);
    string rStr(response);
    do_send(&client_sock, response, rStr.size());
    close(client_sock);
    free(response);

    pthread_exit(NULL);
  } 
  return NULL;
}

void * master_handler(void * args) {
  sem_init(&loadSem, 0, 1);
  // extract master address from serverConfig
  string masterAddr = serverConfig['M'][0];
  int colon = masterAddr.find(":");
  int comma = masterAddr.find(",");
  string ip(masterAddr.substr(0, colon));
  string port(masterAddr.substr(colon+1, comma-colon-1));
  int portNum;
  try {
    portNum = stoi(port);
  } catch (invalid_argument e) {
    cerr << "bad port: " << port << endl;
    raise(SIGINT);
  }

  struct sockaddr_in saddr_in;
  socklen_t saddr_len = sizeof(struct sockaddr_in);

  saddr_in.sin_family = AF_INET;
  inet_aton(ip.c_str(), &(saddr_in.sin_addr));
  saddr_in.sin_port = htons(portNum);

  // open socket
  if ( (master_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("master_sock");
    raise(SIGINT);
  }

  // set sock options to reuse (on quit)
  int enable = 1;
  if (setsockopt(master_sock, SOL_SOCKET, SO_REUSEADDR, &enable, 
        sizeof(int)) < 0) {
    perror("sockopt");
    raise(SIGINT);
  }

  // bind socket
  if ( bind(master_sock, (struct sockaddr *) &saddr_in, saddr_len) < 0) {
    perror("bind");
    raise(SIGINT);
  }

  // listen on socket for up to 100 connections
  if (listen(master_sock, 100) < 0) {
    perror("listen");
    raise(SIGINT);
  }

  saddr_len = sizeof(struct sockaddr_in);
  if (debug)
    cerr << "server sock listening" << endl;
  
  struct sockaddr_in client_saddr_in;
  int client_sock;

  while (true) {
    if ( (client_sock = accept(master_sock, (struct sockaddr *)
            &client_saddr_in, &saddr_len)) < 0) {
      perror("accept");
      raise(SIGINT);
    }

    if (debug)
      cerr << "[" << client_sock << "] New connection " << endl;

    pthread_t thread;
    pthread_create(&thread, NULL, client_handler, &client_sock);
    pthread_detach(thread); // detach to get more clients

    saddr_len = sizeof(struct sockaddr_in); // ensure length is the same
  }

  return NULL;
}
