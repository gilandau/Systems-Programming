#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <strings.h>
#include <cstring>
#include <pthread.h> 
#include <algorithm>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <vector>
#include <sys/types.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include "DATA_PACKET.pb.h"

using namespace std;

#ifndef _SFC_INCLUDE
#define _SFC_INCLUDE

string extract_domain_name(string address);
string get_domain_server(string address);
void write_stream(int sockfd, const char *str, const char *arg);
void write_to_tmp(vector<string> data);
string get_mail_id(string header);
int send_data(vector<string> data);
int send_mail(string tmp_name, string recipient, string sender, vector<string> header);
vector<string> reorder_header(vector<string> header);
vector<int> forward_mail(string tmp_name, vector<string> header, vector<string> non_local_recipients, string sender, int flag);
#endif