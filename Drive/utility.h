#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <strings.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <map>
#include <openssl/md5.h>
#include <netdb.h>
#include <vector>
#include "DATA_PACKET.pb.h"
#include <sstream>

using namespace std;

#ifndef _UTILITY_INCLUDE
#define _UTILITY_INCLUDE

string generate_prefix(int sz);
void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer);
string generate_uidl(string to_hash);
void pretty_print(string entity, string msg, int debug);
int do_recv(int *sockfd, char* cmmd, int timeout);
int do_send(int *sockfd, char* buff, int len);
int recv_t(int *sockfd, char* cmmd, int timeout);
int send_t(int *sockfd, char* buff, int len);
pair<string, int> get_ip_port(string inp);
int is_valid_int(string arg);
string get_name(string id);
map < char, vector < string >  > parse_config( string file_name);
string buff_to_hex(unsigned char* digest, int bytes);
string query_master(string user, string cmmd, int master_sock, int master_port, string master_ip);
void send_error_pack(int client_fd, string status_code, string status);
void send_packet(Packet pack, int sockfd);
vector<string> get_path(string abs_path);
void * heartbeat_client(void * args);
vector<pair<string, int> > parse_server_info(string addr);
void send_num_connections(int client_fd, int num_active_connections);
#endif
