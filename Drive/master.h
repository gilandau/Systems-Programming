#ifndef __master_h__
#define __master_h__

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
#include <signal.h>
#include <sys/signal.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#include "DATA_PACKET.pb.h"
#include "utility.h"
#include "heartbeat.h"
#include "masterComms.h"

extern bool debug;
extern int heartbeatPeriod;
extern int numReplications;
extern int standardTimeoutSec;
extern int standardTimeoutMilli;
extern int quickTimeoutSec;
extern int quickTimeoutMilli;
extern map<char, vector<string>> serverConfig;
extern pthread_t heartbeatThread;
extern pthread_t masterThread;
extern map<string, bool> serverStatus;
extern map<string, string> primToHeart;
extern map<string, string> heartToPrim;
extern map<string, int> kvLoad;
extern map<string, vector<string>> userToKVMap;
extern sem_t heartSem;
extern sem_t quickHeartbeatSem;
extern sem_t userFile;
extern sem_t loadSem;
extern int master_sock;
extern int heartbeat_sock;
extern map<string, vector<string>> kvReps;

char* packetToChar(Packet pkt);

#endif
