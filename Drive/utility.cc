#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <strings.h>
#include <cstring>
#include <arpa/inet.h>
#include <algorithm>
#include <signal.h>
#include <iostream>
#include <getopt.h>
#include <unistd.h>
#include <vector>
#include <sys/types.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <openssl/md5.h>
#include <netdb.h>
#include "DATA_PACKET.pb.h"
#include "utility.h"
#include <bits/stdc++.h>


using namespace std;

// Used to set timeout while receiving input in do_recv
struct timeval tv;

struct heartbeatArgs {
  string heartAddr;
  bool* killed;
};

// Generate prefix to append to out string
string generate_prefix(int sz)
{
	string s = bitset<22>(sz).to_string();
	return s;
}

void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer)
{
  // The digest will be written to digestBuffer, which must be at least MD5_DIGEST_LENGTH bytes long 

  MD5_CTX c;
  MD5_Init(&c);
  MD5_Update(&c, data, dataLengthBytes);
  MD5_Final(digestBuffer, &c);
}


//Get the UIDL Listing
string generate_uidl(string to_hash)
{
	unsigned char md5_digest[MD5_DIGEST_LENGTH]; 
	MD5_CTX context;
	MD5_Init(&context);
	char buff[to_hash.size()];
	to_hash.copy(buff, to_hash.size());
	MD5_Update(&context, buff, to_hash.size());
	MD5_Final(&md5_digest[0], &context);
	string digest = buff_to_hex(md5_digest, MD5_DIGEST_LENGTH);
	return digest;
}

//Function to print output
void pretty_print(string entity, string msg, int debug)
{	
	if(debug == 1)
	{
		string err = "[" + entity + "] " + msg + "\n";
		cerr << err;
	}
}

// Receive data 
int do_recv(int *sockfd, char* cmmd, int timeout)
{
	int rlen = 0;
	int client_fd = *sockfd;
	int idx = 0;

	// Set the specified timeout. If timeout = -1, no timeout
	if(timeout != -1)
	{
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
	}

	// Read the number of bytes to expected
	rlen = read(client_fd, &cmmd[idx], 22);
	if(rlen <= 0)
		return -1;
	cmmd[rlen] = 0;

	// Convert the binary representation to int
	int sz = stoi(string(cmmd), nullptr, 2);
	memset(cmmd, 0, 22);

	// Read in 'sz' bytes
	while(sz > 0)
	{
		rlen = read(client_fd, &cmmd[idx], 1);
		if(rlen < 0)
		{
			cerr << "Error reading input.\n";
			perror("read");
			return -1;
		}
		idx = idx + rlen;
		sz = sz - rlen;
	}
	return idx;
}

// Send data
int do_send(int *sockfd, char* buff, int len)
{
	int sent = 0;
	int client_fd = *sockfd;

	while(sent < len)
	{
		int n_sent = write(client_fd, &buff[sent], len - sent);
		if(n_sent < 0)
			return -1;
		sent = sent + n_sent;
	}
	return sent;
}

// Receive data 
int recv_t(int *sockfd, char* cmmd, int timeout)
{
	int rlen = 0;
	int client_fd = *sockfd;
	int idx = 0;
	// Set the specified timeout. If timeout = -1, no timeout
	if(timeout != -1)
	{
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
	}
	while(true)
	{
		rlen = read(client_fd, &cmmd[idx], 1);
		if(rlen < 0)
		{
			cerr << "Error reading input.\n";
			return -1;
		}
		idx = idx + rlen;
		if(idx >= 2 && cmmd[idx - 2] == '\r' && cmmd[idx - 1] == '\n')
		{
			break;
		}
	}
	return idx;
}

// Send data
int send_t(int *sockfd, char* buff, int len)
{
	int sent = 0;
	int client_fd = *sockfd;
	while(sent < len)
	{
		int n_sent = write(client_fd, &buff[sent], len - sent);
		if(n_sent < 0)
			return -1;
		sent = sent + n_sent;
	}
	return sent;
}


//Separate out the IP and Port# from the given string
pair<string, int> get_ip_port(string inp)
{
	string ip = "";
	string port = "";
	pair<string, int> res;

	size_t i = 0;
	while(true)
	{
		if(inp[i] == ':')
			break;
		ip = ip + inp[i];
		i = i + 1;
	}
	i = i + 1;
	while(i < inp.size())
	{
		port = port + inp[i];
		i = i + 1;
	}
	res = make_pair(ip, stoi(port));
	return res;
}

// Checks if a given string represents a valid integer
int is_valid_int(string arg)
{
	int sz = arg.size();
	for(int i = 0; i < sz; i++)
	{
		// ascii(0) = 48 ascii(9) = 57
		if(arg[i] < 48 || arg[i] > 57)
		{
			return -1;
		}
	}
	return 1;
}

// Extract the username from email: username@ddomain
string get_name(string id)
{
	int i = 0;
	string name = "";
	while(id[i] != '@')
	{
		name = name + id[i];
		i = i + 1;
	}
	return name;
}

// Parses the config file and returns a map of M,D,F, and B
map < char, vector < string >  > parse_config( string file_name){
	map < char, vector < string >  > server_nodes;
	server_nodes.insert({'M', vector < string >()});
	server_nodes.insert({'D', vector < string >()});
	server_nodes.insert({'F', vector < string >()});
	server_nodes.insert({'S', vector < string >()});
	server_nodes.insert({'P', vector < string >()});
	server_nodes.insert({'K', vector < string >()});
	server_nodes.insert({'R', vector < string >()});

	ifstream config;
	config.open(file_name.c_str());

	string line;
	while (getline(config,line)){
		char node_type = line.at(0);
		string node_address = line.substr(1);
		if(node_address.length() == 0){
			cerr <<" Bad config\n";
			exit(1);
		}
		server_nodes[node_type].push_back(node_address);
	}

	return server_nodes;


}
//Return the hex string representation of a char buff
string buff_to_hex(unsigned char* digest, int bytes)
{
	stringstream hex_str;
	for(int i = 0; i < bytes; i++)
	{
		hex_str << hex << (int)digest[i];
	}
	return hex_str.str();
}

// Query master for the user info (cmmd : WHERIS)
string query_master(string user, string cmmd, int master_sock, int master_port, string master_ip)
{
	// Initialize  master socket to connect to master
	master_sock = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in master_addr;
	bzero(&master_addr, sizeof(master_addr));
	master_addr.sin_family = AF_INET;
	master_addr.sin_port = htons(master_port);
	inet_pton(AF_INET, (master_ip).c_str(), &(master_addr.sin_addr));
	connect(master_sock, (struct sockaddr*)&master_addr, sizeof(master_addr));

	// Initialize the packet to send
	Packet packet_to_send;
	packet_to_send.set_command(cmmd);
	packet_to_send.add_arg(user);

	//Serialize the packet as a string
	string out;
	if(!packet_to_send.SerializeToString(&out))
	{
		cerr << "Could not serialize packet to send.\n";
	}
	string pre = generate_prefix(out.size());
	out = pre + out;
	char response[out.size()];
	out.copy(response, out.size());
	do_send(&master_sock, response, sizeof(response));

	char buff[2048];
	int master_timeout = 10;
	int idx = 0;
	idx = do_recv(&master_sock, buff, master_timeout);
	if(idx == -1)
	{
		cerr << "Error reading COMMAND. Exiting...\n";
		close(master_sock);
	}
	buff[idx] = 0;
	

	Packet packet_rcvd;
	// Parse the message to extract the content
	if(!packet_rcvd.ParseFromString(buff))
	{
		cerr << "Failed to parse received message!\n";
	}
	if(packet_rcvd.command() == "ABSENT")
		return "ABSENT";
	else if(packet_rcvd.command() == "SERVERFAIL")
		return "SERVERFAIL";
	else
		return packet_rcvd.arg(0);
}

// Send error packet
void send_error_pack(int client_fd, string status_code, string status)
{
	Packet pack;
	pack.set_status_code(status_code);
	pack.set_status(status);
	string out;
	if(!pack.SerializeToString(&out))
	{
		cerr << "Could not serialize packet to send.\n";
	}
	string pre = generate_prefix(out.size());
	out = pre + out;
	char response[out.size()];
	out.copy(response, out.size());
	do_send(&client_fd, response, sizeof(response));
}

// Send the packet to the poor soul
void send_packet(Packet pack, int sockfd)
{
	string out;
	if(!pack.SerializeToString(&out))
	{
		cerr << "Could not serialize packet to send.\n";
	}
	string pre = generate_prefix(out.size());
	out = pre + out;
	char response[out.size()];
	out.copy(response, out.size());
	do_send(&sockfd, response, sizeof(response));
}

// Return the file name and its parent name 
vector<string> get_path(string abs_path)
{
	int sz = abs_path.size() - 1;
	string parent;
	string file = "";
	vector<string> res;
	int i = sz;

	while(abs_path[i] != '/')
	{
		file = abs_path[i] + file;
		i--;
	}

	parent = abs_path.substr(0, i);
	res.push_back(parent);
	res.push_back(file);
	return res;
}

/* argument must be string with ip and port of the heartbeat address in the
 * config file in the form: "127.0.0.1:8000"
 * To get this working simply run the following command snippet (in main):
 * pthread_t thread;
 * pthread_create(&thread, NULL, heartbeat_handler, &addr);
 * pthread_detach(thread);
void * heartbeat_client(void * args) {
  string addrStr = *( (string *) args);

  int sock;
  if ( (sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  string ip(addrStr.substr(0, addrStr.find(":")));
  string port(addrStr.substr(addrStr.find(":")+1));
  int portNum = stoi(port);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  inet_aton(ip.c_str(), &(addr.sin_addr));
  addr.sin_port = htons(portNum);

  if ( bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("bind");
    exit(1);
  }

  int numResp = 0;

  // listen for packets forever
  while (true) {
    char buf[4096];
    int n = 0;
    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    n = recvfrom(sock, buf, 4095, 0, (struct sockaddr *) &src, &srclen);
    if (n < 0) {
      cerr << "Error receiving packet from front-end" << endl;
      close(sock);
      exit(1);
    }
    buf[n-2] = 0;

    Packet packet_rcvd;
    if (!packet_rcvd.ParseFromString(buf)) {
      cerr << "Unable to parse received message: " << buf << endl;
      close(sock);
      exit(1);
    }

    string command = packet_rcvd.command();
    transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (command == "heart") {
      // send response to the received server
      Packet toSend;
      toSend.set_command("UP");
      string out;
      if (!toSend.SerializeToString(&out)) {
        cerr << "Could not serialize packet" << endl;
        exit(1);
      }
      out = out + "\r\n";
      char msg[out.size()];
      out.copy(msg, out.size());

      numResp++;

      if (sendto(sock, msg, strlen(msg), 0,
            (struct sockaddr *) &src, sizeof(src)) < 0) {
        perror("sendto");
        exit(1);
      }
    }
  }
}
*/

void * heartbeat_client(void * heartArgs) {
  struct heartbeatArgs args = *( (struct heartbeatArgs *) heartArgs);
  string addrStr = args.heartAddr;
  bool* killed = args.killed;

  int sock;
  if ( (sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  string ip(addrStr.substr(0, addrStr.find(":")));
  string port(addrStr.substr(addrStr.find(":")+1));
  int portNum = stoi(port);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  inet_aton(ip.c_str(), &(addr.sin_addr));
  addr.sin_port = htons(portNum);

  if ( bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("bind");
    exit(1);
  }

  int numResp = 0;

  // listen for packets forever
  while (true) {
    char buf[4096];
    int n = 0;
    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    n = recvfrom(sock, buf, 4095, 0, (struct sockaddr *) &src, &srclen);
    if (n < 0) {
      cerr << "Error receiving packet from front-end" << endl;
      close(sock);
      exit(1);
    }
    buf[n-2] = 0;

    Packet packet_rcvd;
    if (!packet_rcvd.ParseFromString(buf)) {
      cerr << "Unable to parse received message: " << buf << endl;
      close(sock);
      exit(1);
    }

    string command = packet_rcvd.command();
    transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (command == "heart") {
      // send response to the received server if we aren't in a killed state
      if ((*killed)) {
        // killed, set message and continue loop
        cerr << "Server in killed state, not responding to heartbeat" << endl;
        continue;
      }
      Packet toSend;
      toSend.set_command("UP");
      string out;
      if (!toSend.SerializeToString(&out)) {
        cerr << "Could not serialize packet" << endl;
        exit(1);
      }
      out = out + "\r\n";
      char msg[out.size()];
      out.copy(msg, out.size());

      numResp++;

      if (sendto(sock, msg, strlen(msg), 0,
            (struct sockaddr *) &src, sizeof(src)) < 0) {
        perror("sendto");
        exit(1);
      }
    }
  }
}


vector<pair<string, int> > parse_server_info(string addr)
{
	vector<pair<string, int>> res;
	int colon = addr.find(":");
	int comma = addr.find(",");
	string ip(addr.substr(0, colon));
	string firstPort(addr.substr(colon+1, comma-colon-1));
	string secPort(addr.substr(comma+1));
	int firstPortN = stoi(firstPort);
	int secPortN = stoi(secPort);

	pair<string, int> add1 (ip, firstPortN);
	pair<string, int> add2 (ip, secPortN);
	res.push_back(add1);
	res.push_back(add2);
	return res;
}

// Send back the number of active connections to the server back to the load balancer
void send_num_connections(int client_fd, int num_active_connections)
{
	Packet pack;
	pack.set_command("LOAD");
	pack.add_arg(to_string(num_active_connections));
	string out;
	if(!pack.SerializeToString(&out))
	{
		cerr << "Could not serialize packet to send.\n";
	}
	string pre = generate_prefix(out.size());
	out = pre + out;
	char response[out.size()];
	out.copy(response, out.size());
	do_send(&client_fd, response, sizeof(response));
}