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

#include "../DATA_PACKET.pb.h"
#include "../utility.h"

#define BUF_SIZE 4096

using namespace std;

//socket we're talking with, header of http (map), and then the data vector of strings
#ifndef __mailclient__
#define __mailclient__

int sendmail(int sock, map<string, string> headerInfo, string from, 
    vector<string> to, string subject, string msg);
vector<string> receiveMail(int sock, string username, string password);
string driveTransaction(int sock, string command, 
    map<string, string> driveArgs, int data_flag, unsigned char* file_data);
string deleteMail(int sock, string username, string password,
    vector<int> msgIndexToDelete);
Packet receivePacket(int sock);
vector<string> getMiddleServerAddresses(int sock);
map<char, map<string, bool>> getServerStatus(int sock);
map<char, map<string, bool>> killServer(int sock, string addrToKill); 
map<char, map<string, bool>> restartServer(int sock, string addrToRestart); 
string getRowColVal(int sock, string row, string column, string kvAddr);
string getRowColsofKV(int sock, string kvAddr);

#endif
