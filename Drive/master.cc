#include "master.h"

#define BUF_SIZE 4096

bool debug;
int heartbeatPeriod;
int numReplications;
int standardTimeoutSec;
int standardTimeoutMilli;
int quickTimeoutSec;
int quickTimeoutMilli;
map<char, vector<string>> serverConfig;
pthread_t heartbeatThread, masterThread;
map<string, bool> serverStatus;
map<string, string> primToHeart;
map<string, string> heartToPrim;
map<string, int> kvLoad;
map<string, vector<string>> userToKVMap;
sem_t heartSem, quickHeartbeatSem, userFile, loadSem;
int master_sock, heartbeat_sock;
map<string, vector<string>> kvReps;

using namespace std;

string getInputArgs(int argc, char *argv[]) {
  int opt;
  debug = false; // debug not on by default
  heartbeatPeriod = 600; // 10 minutes by default
  float standardTimeout = 100.0; // 100 seconds by default
  float quickTimeout = 2.0; // 2 seconds by default
  numReplications = 2; // 2 copies of each user by default

  while ((opt = getopt(argc, argv, "vh:t:q:r:")) != -1) {
    switch (opt) {
      case 'v':
        debug = true;
        break;
      case 'h':
        heartbeatPeriod = stoi(optarg);
        break;
      case 't':
        standardTimeout = stof(optarg);
        break;
      case 'q':
        quickTimeout = stof(optarg);
        break;
      case 'r':
        numReplications = stoi(optarg);
        break;
      default:
        cerr << "Error invalid argument for " << argv[0] << endl;
        cerr << "Usage: " << argv[0] << " [-v] [-h <Heartbeat Period>] " <<
          "[-t <Standard Timeout>] [-q <Quick Timeout>] [-r <Replications>]" <<
          " </path/to/serverConfig>" << endl;
        exit(1);
    }
  }

  string configFilename;
  if (optind == argc-1) {
    configFilename.append(argv[optind]);
  } else {
    cerr << "Error invalid argument for " << argv[0] << endl;
    cerr << "Usage: " << argv[0] << " [-v] [-h <Heartbeat Period>] " <<
      "[-t <Standard Timeout>] [-q <Quick Timeout>] [-r <Replications>]" <<
      " </path/to/serverConfig>" << endl;
    exit(1);
  }

  // process timeout values
  standardTimeoutSec = (int) standardTimeout;
  standardTimeoutMilli = (int) ((standardTimeout-standardTimeoutSec)*1000);
  quickTimeoutSec = (int) quickTimeout;
  quickTimeoutMilli = (int) ((quickTimeout-quickTimeoutSec)*1000);

  return configFilename;
}

void ctrlc_handler(int signum) {
  close(master_sock);
  close(heartbeat_sock);
  exit(0);
}

void parseUserFile() {
  ifstream in("systemUsers");
  char lineBuf[BUF_SIZE];
  in.getline(lineBuf, BUF_SIZE);
  while (!in.eof()) {
    string line(lineBuf);
    // get username
    string username(line.substr(0, line.find(":")));
    // then, we get the primary KV addr for this user
    string primKV(line.substr(line.find(":")+1));
    kvLoad[primKV]++;
    // make a vector with the first element the primary KV
    vector<string> servers;
    servers.push_back(primKV);
    // then look up the replications for this server based on the map
    vector<string> reps = kvReps[primKV];
    for (auto it = reps.begin(); it != reps.end(); ++it)
      servers.push_back(*it);
    userToKVMap[username] = servers;

    in.getline(lineBuf, BUF_SIZE);
  }
  in.close();

  if (debug) {
    cerr << "Restored user state to: " << endl;
    for (auto it = userToKVMap.begin(); it != userToKVMap.end(); ++it) {
      cerr << "User: " << it->first << endl;
      vector<string> addr = it->second;
      cerr << "\t";
      for (auto s = addr.begin(); s != addr.end(); ++s)
        cerr << *s << " ";
      cerr << endl;
    }
    cerr << "KV Loads: " << endl;
    for (auto it = kvLoad.begin(); it != kvLoad.end(); ++it)
      cerr << "\tKV: " << it->first << " Prim Load: " << it->second << endl;
  }
}

char* packetToChar(Packet pkt) {
  string out;
  if (!pkt.SerializeToString(&out)) {
    cerr << "Could not serialize pakcet" << endl;
    raise(SIGINT);
  }
  string pre = generate_prefix(out.size());
  out = pre + out;
  char *msg = (char *) calloc(out.size(), sizeof(char));
  out.copy(msg, out.size());
  return msg;
}

void parseRepConfig() {
  ifstream in("repConfig");
  char lineBuf[BUF_SIZE];
  in.getline(lineBuf, BUF_SIZE);

  while (!in.eof()) {
    string line(lineBuf);
    size_t lastComma = 0;
    size_t nextComma = line.find_first_of(",", lastComma);
    string primAddr;

    while (nextComma != string::npos) {
      string addr = line.substr(lastComma, nextComma-lastComma);
      addr = addr.substr(0, addr.find(";"));
      if (lastComma == 0) {
        kvReps[addr] = vector<string>();
        primAddr.append(addr);
        kvLoad[addr] = 0;
      } else {
        kvReps[primAddr].push_back(addr);
      }
      lastComma = nextComma + 1;
      nextComma = line.find_first_of(",", lastComma);
    }
    string addr = line.substr(lastComma);
    addr = addr.substr(0, addr.find(";"));
    kvReps[primAddr].push_back(addr);
    in.getline(lineBuf, BUF_SIZE);
  }

  in.close();

  if (debug) {
    cerr << "Primary and Replicas" << endl;
    for (auto it = kvReps.begin(); it != kvReps.end(); ++it) {
      cerr << "Primary: " << it->first << endl;
      vector<string> reps = it->second;
      for (auto r = reps.begin(); r != reps.end(); ++r) {
        cerr << "\t " << *r << endl;
      }
    }
  }
}

int main(int argc, char *argv[]) {
  // get input args for master
  string configFilename = getInputArgs(argc, argv);

  if (debug) {
    cerr << "debug: " << debug << " heartPeriod: " << heartbeatPeriod << 
      " standardTimeS: " << standardTimeoutSec << " stdM: " << standardTimeoutMilli <<
      " quickS: " << quickTimeoutSec << " qM: " << quickTimeoutMilli << " rep: " <<
      numReplications << endl;
  }

  // read server config
  serverConfig = parse_config(configFilename);

  // initialize load balancer for the KV servers in serverConfig (only want the
  // primary port number) and the status data structure for the heartbeat
  // and the mapping from primary to secondary port for KV stores
  for (auto it = serverConfig.begin(); it != serverConfig.end(); ++it) {
    vector<string> servers = it->second;
    if (debug)
      cerr << "Server Type: " << it->first << endl;
    for (auto it1 = servers.begin(); it1 != servers.end(); ++it1) {
      if (it->first != 'M') {
        int colon = (*it1).find(":");
        int comma = (*it1).find(",");
        string ip((*it1).substr(0,colon));
        string firstPort((*it1).substr(colon+1, comma-colon-1));
        string secPort((*it1).substr(comma+1));
        string primAddr(ip);
        primAddr.append(":");
        primAddr.append(firstPort);
        string heartAddr(ip);
        heartAddr.append(":");
        heartAddr.append(secPort);

        if (it->first == 'K') {
          // set server status to true
          serverStatus[primAddr] = true;
          // set load to 0
          // for now, we aren't setting the load, this will be done when we read
          // in the replication config file
          // kvLoad[primAddr] = 0;
          // now extract second port to get the heartbeat port
          // map from prim to heart and back
          string modHeart(heartAddr.substr(0, heartAddr.find(",")));
          primToHeart[primAddr] = modHeart;
          heartToPrim[modHeart] = primAddr;
          
          if (debug) {
            cerr << "\tAddr: " << ip << " Prim: " << firstPort << 
              " Status: " <<
              (serverStatus[primAddr] ? "UP" : "DOWN") << endl;
          }
        } else {
          // other server, assume up
          serverStatus[primAddr] = true;
          primToHeart[primAddr] = heartAddr;
          heartToPrim[heartAddr] = primAddr;
          if (debug) {
            cerr << "\tAddr:Port: " << *it1 << " Status: " << 
              (serverStatus[primAddr] ? "UP" : "DOWN") << endl;
          }
        }
      }
    }
  }

  // initialize semaphores for accessing the server status (heartSem) and for
  // waiting until a quick heartbeat has completed (quickHeartSem) and the user
  // file 
  sem_init(&heartSem, 0, 1);
  sem_init(&quickHeartbeatSem, 0, 1);
  sem_init(&userFile, 0, 1);
  // register SIGINT for closing our sockets
  signal(SIGINT, ctrlc_handler);
  // registering SIGALRM and SIGUSR1 will be handled by the heartbeat thread (as
  // the master thread will be sending these signals to the heartbeat thread)

  // read in the KV replication config file to find the primary KV addrs to 
  parseRepConfig();

  // preload the stored users file
  sem_wait(&userFile);
  parseUserFile();
  // if debugging, print out the mapping
  if (debug) {
    for (auto it = userToKVMap.begin(); it != userToKVMap.end(); ++it) {
      vector<string> servers = it->second;
      cerr << "User: " << it->first << " ";
      for (auto it1 = servers.begin(); it1 != servers.end(); ++it1) {
        cerr << *it1 << " ";
      }
      cerr << endl;
    }
  }
  sem_post(&userFile);

  // check to make sure numReplications+1 (primary plus replications) is <=
  // number of KV stores in kvLoad
  int kvNums = kvLoad.size();
  if ( (kvNums/(numReplications+1)) < 1 ) {
    cerr << "Not enough KV stores for desired replications" << endl;
    exit(1);
  }

  // now we create a master thread and heartbeat thread
  pthread_create(&heartbeatThread, NULL, heartbeat_master, NULL);
  pthread_create(&masterThread, NULL, master_handler, NULL);
  //pthread_join(heartbeatThread, NULL);
  //pthread_join(masterThread, NULL);
  pthread_detach(heartbeatThread);
  pthread_detach(masterThread);

  while (true)
    pause();
  
  
  return 0;
}
