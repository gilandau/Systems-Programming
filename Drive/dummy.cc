#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>

#include "DATA_PACKET.pb.h"
#include "utility.h"

#define BUF_SIZE 4096

using namespace std;

bool debug;
map<string, int> frontEndLoadBalancer;
sem_t loadSem;
pthread_t heartThread;
int servNum;
int server_sock;
bool killed = false;
map<char, vector<string>> serverConfig;

struct heartbeatArgs {
  string heartAddr;
  bool* killed;
};

//TODO might need to redirect to another dummy (we'll talk on Friday)

std::string getInputArgs(int argc, char *argv[], bool *debug) {
  // only options are -v (debug printing to cerr) and the required filename
  // for the config file and the server number (there really will only be 1)
  int opt;
  *debug = false; // debug not on by default

  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
      case 'v':
        *debug = true;
        break;
      default:
        std::cerr << "Error invalid argument for " << argv[0] << std::endl;
        std::cerr << "Usage: " << argv[0] << " [-v] </path/to/serverConfig>"
          << std::endl;
        exit(1);
    }
  }

  // get required arguments
  std::string configFilename;
  if (optind == argc-2) {
    configFilename.append(argv[optind++]);
    servNum = atoi(argv[optind]);
  } else {
    std::cerr << "Error invalid argument for " << argv[0] << std::endl;
    std::cerr << "Usage: " << argv[0] << " [-v] </path/to/serverConfig>"
      << std::endl;
    exit(1);
  }

  return configFilename;
}

// parses the up data field from master
map<string, bool> parseDataField(string data) {
  data = data.substr(1, data.length()-2);
  map<string, bool> status;
  size_t lastComma = 0;
  size_t nextComma = data.find_first_of(",", lastComma);

  while (nextComma != string::npos) {
    string entry = data.substr(lastComma, nextComma-lastComma);
    string addrType(entry.substr(0, entry.find("=")));
    string addr(addrType.substr(0));
    string statusS(entry.substr(entry.find("=")+1));
    status[addr] = (statusS == "true");
    lastComma = nextComma + 1;
    nextComma = data.find_first_of(",", lastComma);
  }
  string entry = data.substr(lastComma);
  string addrType(entry.substr(0, entry.find("=")));
  string addr(addrType.substr(0));
  string statusS(entry.substr(entry.find("=")+1));
  status[addr] = (statusS == "true");

  return status;
}

// converts packet to character buffer
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

// finds and returns the server that is least loaded and up
std::string findLeastLoadServer(std::map<std::string, int> frontEndLoadBalancer) {
  // iterate over each server and select the one with minimum load (first get a
  // status for each of the front-end servers)
  // get master address: serverConfig['M'][0]
  Packet to_send; 
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
    raise(SIGINT);
  }
  inet_aton(ip.c_str(), &(masterAddr.sin_addr));
  masterAddr.sin_port = htons(portNum);

  // open master socket and connect
  int masterSock;
  if ( (masterSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    raise(SIGINT);
  }

  if (connect(masterSock, (struct sockaddr *) &masterAddr, sizeof(masterAddr))
      < 0) {
    perror("connect");
    close(masterSock);
    raise(SIGINT);
  }

  do_send(&masterSock, response, rStr.size());
  free(response);

  // receive reply from master
  Packet r;
  char status[BUF_SIZE];
  int n = do_recv(&masterSock, status, -1);
  status[n] = 0;
  if (!r.ParseFromString(status)) {
    cerr << "couldn't parse" << endl;
    raise(SIGINT);
  }

  // check to make sure we have the right response
  string command = r.command();
  transform(command.begin(), command.end(), command.begin(), ::tolower);
  if (command != "servstat") {
    cerr << "invalid response from master" << endl;
    raise(SIGINT);
  }

  close(masterSock);
  // parse the r.data() field
  map<string, bool> currServerStatus = parseDataField(r.data());

  // pick the least loaded server that is currently up according to master
  int minLoad = 100000;
  std::string minAddr;
  sem_wait(&loadSem);
  for (auto it = frontEndLoadBalancer.begin(); it != frontEndLoadBalancer.end();
      ++it) {
    if (it->second < minLoad && currServerStatus[it->first]) {
      minLoad = it->second;
      minAddr = it->first;
    }
  }
  sem_post(&loadSem);

  // ensure we have at least one up and running server
  if (minLoad != 100000) {
    return minAddr;
  } else {
    std::cerr << "ERROR: Too many clients or all servers down... exiting" 
      << std::endl;
    exit(1);
  }
}

// handles a message from the admin server to kill or restart dummy
void handleAdminMessage(int sock, char* lineBuf) {
  cout << "handling admin message" << endl;

  lineBuf[strlen(lineBuf)] = 0;
  lineBuf = &lineBuf[10];

  Packet r;
  if (!r.ParseFromString(lineBuf)) {
    cerr << "couldn't parse" << endl;
    raise(SIGINT);
  }

  string command = r.command();
  transform(command.begin(), command.end(), command.begin(), ::tolower);

  // need to check to see if this is a restart or kill command (and make sure we
  // are in the correct state)
  if (killed && command == "restart") {
    // we are getting rebooted, set state to up and send OK response 
    killed = false;
    Packet to_send;
    to_send.set_command("OK");
    char *response = packetToChar(to_send);
    string rStr(response);
    do_send(&sock, response, rStr.size());
    close(sock);
    free(response);
    pthread_exit(NULL);
  } else if (!killed && command == "kill") {
    cout << "getting killed" << endl;
    // getting killed, set state to down and send OK 
    killed = true;
    Packet to_send;
    to_send.set_command("OK");
    char *response = packetToChar(to_send);
    string rStr(response);
    do_send(&sock, response, rStr.size());
    close(sock);
    free(response);
    pthread_exit(NULL);
  } else {
    // invalid command in general, just close socket and return
    close(sock);
    pthread_exit(NULL);
  }
}

// handles new client connected to the dummy 
void * handleNewClient(void * arg) {
  int client_sock = *((int *) arg);
  // read from client_sock up to the first <CRLF> (that's all we need to
  // worry about as the dummy server)
  char rcvBuf[BUF_SIZE];
  char lineBuf[BUF_SIZE];
  int n, msgInd = 0;

  while ( (n = read(client_sock, rcvBuf, BUF_SIZE-1)) > 0) {
    // just read n bytes from the client, process each byte individually
    // and when we have a line, we will process the first header from the client
    for (int i = 0; i < n; i++) {
      lineBuf[msgInd++] = rcvBuf[i];
      if ( (msgInd > 1) && ( (lineBuf[msgInd-2] == '\r') &&
          (lineBuf[msgInd-1] == '\n') ) ) {
        if (killed) {
          // if we are killed, we don't respond by just closing the socket
          // see TODO above
          close(client_sock);
          pthread_exit(NULL);
        }
        // convert the lineBuf into a string to work with
        std::string line(lineBuf);
        // switch based on the command (dummy only responds to GET)
        if (line.compare(0, 3, "GET") == 0) {
          // check to make sure the client is requesting /index.html and ensure
          // they are using HTTP/1.1
          int firstSpace = line.find_first_of(' ');
          int secondSpace = line.find_first_of(' ', firstSpace+1);
          std::string path(line.substr(firstSpace+1,secondSpace-firstSpace-1));
          std::string version(line.substr(secondSpace+1,
            line.find_first_of('\r')-secondSpace-1));
          if (path.compare("/index.html") != 0 && path.compare("/") != 0) {
            // return a 404 Not Found error to the client (and close socket)
            std::string response("HTTP/1.1 404 Not Found\r\n\r\n");
            if (debug) {
              std::cerr << "Sending " << response.substr(0,response.length()-4)
                << " to: " << client_sock << std::endl;
            }
            if (write(client_sock, response.c_str(), response.length()) < 0) {
              perror("write");
              raise(SIGINT);
            }
            close(client_sock);
            pthread_exit(NULL);
          }
          if (version.compare("HTTP/1.1") != 0) {
            // return a 505 HTTP Version Not Supported error to the client
            // (and close socket)
            std::string response("HTTP/1.1 505 HTTP Version Not Suppported\r\n\r\n");
            if (debug) {
              std::cerr << "Sending " << response.substr(0,response.length()-4)
                << " to: " << client_sock << std::endl;
            }
            if (write(client_sock, response.c_str(), response.length()) < 0) {
              perror("write");
              raise(SIGINT);
            }
            close(client_sock);
            pthread_exit(NULL);
          }
          // find the best server to redirect to and respond with a 302 Found
          // code to the client and close client sock
          std::string addressToSend = findLeastLoadServer(frontEndLoadBalancer);
          sem_wait(&loadSem);
          frontEndLoadBalancer[addressToSend]++;
          sem_post(&loadSem);
          std::string response("HTTP/1.1 302 Found\r\n");
          response.append("Location: http://");
          response.append(addressToSend);
          response.append("/\r\n");
          if (debug) {
            std::cerr << "Sending " << response.substr(0,response.length()-2)
              << " to: " << client_sock << std::endl;
            sem_wait(&loadSem);
            std::cerr << "CURRENT LOAD: " << std::endl;
            for (auto it = frontEndLoadBalancer.begin(); it != frontEndLoadBalancer.end();
                ++it) {
              std::cout << "\tAddr:Port = " << it->first << " Load: " << it->second <<
                        std::endl;
            }
            sem_post(&loadSem);
          }
          if (write(client_sock, response.c_str(), response.length()) < 0) {
            perror("write");
            raise(SIGINT);
          }
          close(client_sock);
          pthread_exit(NULL);
        } else {
          // return a 501 Not Implemented error to the client (and close socket)
          std::string response("HTTP/1.1 501 Not Implemented\r\n\r\n");
          if (debug) {
            std::cerr << "Sending " << response.substr(0,response.length()-4)
              << " to: " << client_sock << std::endl;
          }
          if (write(client_sock, response.c_str(), response.length()) < 0) {
            perror("write");
            raise(SIGINT);
          }
          close(client_sock);
          pthread_exit(NULL);
        }
      }
    }
    // if we get here, we have an admin message in lineBuf
    handleAdminMessage(client_sock, lineBuf);
  }
  return NULL;
}

void ctrlc_handler(int signum) {
  close(server_sock);
  exit(0);
}

int main(int argc, char *argv[]) {
  // get input arguments for the dummy server
  std::string configFilename = getInputArgs(argc, argv, &debug);

  sem_init(&loadSem, 0, 1);
  signal(SIGINT, ctrlc_handler);

  // read the server configuration
  serverConfig = parse_config(configFilename);

  // initialize the frontEndLoadBalancer and print out the information if we're
  // debugging
  for (auto it = serverConfig.begin(); it != serverConfig.end(); ++it) {
    vector<string> theseServers = it->second;
    if (debug)
      cout << "Server Types: " << it->first << endl;
    for (auto it1 = theseServers.begin(); it1 != theseServers.end(); ++it1) {
      if (it->first == 'F') {
        // add this address to the frontEndLoadBalancer
        // need to first parse out the first port
        string fullAddr = *it1;
        int colon = fullAddr.find(":");
        int comma = fullAddr.find(",");
        string ip(fullAddr.substr(0, colon));
        string port(fullAddr.substr(colon+1, comma-colon-1));
        string toSet(ip);
        toSet.append(":");
        toSet.append(port);
        frontEndLoadBalancer[toSet] = 0;
      }
      // then if we're debugging, go ahead and print
      if (debug)
        cout << "\tAddr:Port = " << *it1 << endl;
    }
  }

  string fullAddr = serverConfig['D'][servNum-1];
  int colon = fullAddr.find(":");
  int comma = fullAddr.find(",");
  string ip(fullAddr.substr(0, colon));
  string firstPort(fullAddr.substr(colon+1, comma-colon-1));
  string secPort(fullAddr.substr(comma+1));
  string heartAddr(ip);
  heartAddr.append(":");
  heartAddr.append(secPort);
  int portNum = stoi(firstPort);

  struct heartbeatArgs args;
  args.heartAddr = heartAddr;
  args.killed = &killed;
  pthread_create(&heartThread, NULL, heartbeat_client, &args);
  pthread_detach(heartThread);

  // begin to listen for incoming connections on 8080 (for clients)
  struct sockaddr_in saddr_in;
  socklen_t saddr_len = sizeof(struct sockaddr_in);

  saddr_in.sin_family = AF_INET;
  inet_aton(ip.c_str(), &(saddr_in.sin_addr));
  // HTTP port 
  saddr_in.sin_port = htons(portNum);

  // open socket
  if ( (server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("server_sock");
    exit(1);
  }

  // allows for socket to be re-binded after we ctrl-c
  int enable = 1;
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &enable,
        sizeof(int)) < 0) {
    perror("setsockopt");
    exit(1);
  }

  // bind socket to our address
  if ( bind(server_sock, (struct sockaddr *) &saddr_in, saddr_len) < 0) {
    perror("bind");
    exit(1);
  }

  // listen on the socket allowing up to 100 connections to queue up
  if (listen(server_sock, 100) < 0) {
    perror("listen");
    raise(SIGINT);
  }

  saddr_len = sizeof(struct sockaddr_in); // ensure length is the same

  if (debug)
    std::cerr << "server sock listening: " << server_sock << std::endl;

  // begin accepting connections
  struct sockaddr_in client_saddr_in;
  int client_sock;

  while (true) {
    if ( (client_sock = accept(server_sock, (struct sockaddr*) &client_saddr_in,
          &saddr_len)) < 0) {
      perror("accept");
      raise(SIGINT);
    }

    if (debug)
      std::cerr << "[" << client_sock << "] New connection" << std::endl;
    
    // now handle the new client and redirect them to one of the front ends if
    // it's a browser client, we do something else if we receive a message from
    // an admin server (handleNewClient handles this)
    pthread_t thread;
    pthread_create(&thread, NULL, handleNewClient, &client_sock);
    pthread_detach(thread); // get more clients
    
    saddr_len = sizeof(struct sockaddr_in); // ensure length is the same
  }

  return 0;
}
