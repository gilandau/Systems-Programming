#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <strings.h>
#include <cstring>
#include <arpa/inet.h>
#include <pthread.h> 
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
#include <chrono>
#include <ctime>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "DATA_PACKET.pb.h"
#include "utility.h"
#include "smtp_forward_client.h"
#include <openssl/md5.h>
#include <nlohmann/json.hpp>


using namespace std;
using json = nlohmann::json;

// Authentication server time out
static int auth_time_out = 10;

// Self server number
static int SERVER_NUM;

// cache mapping cookies to user name

// Debug flag
static int debug = 0;
bool killed = false;

vector <int> open_conn;
int sockfd;
bool vflag = false;
void my_write(int fd, const char *data, int len);
// Master socket
static int master_sock;

// Master ip: port
int master_port;
string master_ip;

struct heartbeatArgs {
  string heartAddr;
  bool* killed;
};

void my_write(int fd,const char *data, int len){
	int r = 0;
	while(r < len){
		int w = write(fd, &data[r], len - r);
		if(w < 0){
			fprintf(stderr, "Send failed\n");
		}else if(w == 0){
			fprintf(stderr, "Connection closed unexpectedly\n");
		}else{
			r += w;
		}
	}
}

// Get the master for user info
Packet query_master_again(string user, string cmmd)
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
	cout << "Response: " << response << endl;

	char buff[2048];
	int idx = 0;
	idx = do_recv(&master_sock, buff, auth_time_out);
	if(idx == -1)
	{
		cerr << "Error reading COMMAND. Exiting...\n";
		close(master_sock);
	}
	cout << "Received packet from master\n";

	buff[idx] = 0;
	Packet packet_rcvd;

	// Parse the message to extract the content
	if(!packet_rcvd.ParseFromString(buff))
	{
		cerr << "Failed to parse received message!\n";
	}
	return packet_rcvd;
}


// Query the key value store for the appropriate data
Packet query_kv_store(string cmmd, string row, string col, vector<string> arg, string data, string cmd)
{
	// Initialize the packet to be sent
	transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
	Packet packet_to_send;
	packet_to_send.set_command(cmmd);
	packet_to_send.set_row(row);
	packet_to_send.set_column(col);
	packet_to_send.set_data(data);

	for(int i = 0; i < arg.size(); i++)
	{
		packet_to_send.add_arg(arg[i]);
	}

	// Query the master for the key-value store for the
	Packet packet_error;
	pair<string, int> addr;
	string ip;
	cout<< "In Q-KVS, COMMAND: "<< cmd<< '\n';

	if(cmd.compare("login") == 0)
	{
		cout << "Inside login, Querying master for address\n";
		Packet packet_rcvd_master = query_master_again(row, "WHERIS");
		string ip;
		if(packet_rcvd_master.command().compare("KVADDR") == 0)
		{
			cout << "User present\n";
			ip = packet_rcvd_master.arg(0);
			addr = get_ip_port(ip);
			cout << addr.first << addr.second << endl;
		}
		else if(packet_rcvd_master.command().compare("ABSENT") == 0)
		{
			//Send an error
			cout << "User absent\n";
			packet_error.set_command("error");
			packet_error.set_status_code("501");
			packet_error.set_status("No user");
			return packet_error;
		}
	}
	else if(cmd.compare("signup") == 0)
	{
		cout<<"Signing up new user\n";
		Packet packet_rcvd_master = query_master_again(row, "NEWCL");
		cout << "Reply from Master: " << packet_rcvd_master.command() << endl;
		if(packet_rcvd_master.command().compare("CLADDED") == 0)
		{
			ip = packet_rcvd_master.arg(0);
			addr = get_ip_port(ip); 
		}
		else if(packet_rcvd_master.command().compare("DUPCL") == 0){
			//Send an error
			ip = packet_rcvd_master.arg(0);
			addr = get_ip_port(ip);
		}
	}
	cout << "IP received from master: " << addr.first << addr.second << endl;

	// Initialize  socket to connect to KV-store
	int out_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in kvstore_addr;
	bzero(&kvstore_addr, sizeof(kvstore_addr));
	kvstore_addr.sin_family = AF_INET;
	kvstore_addr.sin_port = htons(addr.second);
	inet_pton(AF_INET, (addr.first).c_str(), &(kvstore_addr.sin_addr));
	
	int stat = connect(out_fd, (struct sockaddr*)&kvstore_addr, sizeof(kvstore_addr));

	while(stat < 0)
	{
		cout << "Stat: " << stat << endl;
		ip = query_master(row, "WHERIS", master_sock, master_port, master_ip);
		if(ip == "ABSENT")
		{
			Packet pack;
			pack.set_status_code("600");
			pack.set_status("User not present in system");
			return pack;
		}
		else if(ip == "SERVERFAIL")
		{
			Packet pack;
			pack.set_status_code("601");
			pack.set_status("ALL SERVERS FOR CLIENT DOWN");
			return pack;
		}
		addr = get_ip_port(ip); 
		kvstore_addr.sin_port = htons(addr.second);
		inet_pton(AF_INET, (addr.first).c_str(), &(kvstore_addr.sin_addr));
		stat = connect(out_fd, (struct sockaddr*)&kvstore_addr, sizeof(kvstore_addr));
	}
	cout << "Auth connected to KVS ? : " << stat << endl;
	
	string out;
	if(!packet_to_send.SerializeToString(&out))
	{
		cerr << "Could not serialize packet to send.\n";
	}
	string pre = generate_prefix(out.size());
	out = pre + out;
	char response[out.size()];
	out.copy(response, out.size());
	do_send(&out_fd, response, sizeof(response));

	// Receive response from KV-Store
	char buff[2048];
	int idx = 0;
	idx = do_recv(&out_fd, buff, auth_time_out);
	if(idx == -1)
	{
		cerr << "Error reading COMMAND. Exiting...\n";
		close(out_fd);
	}
	
	buff[idx] = 0;
	cout << "Packet RCVD KVS-END: " << buff << endl;
	Packet packet_rcvd;

	// Parse the message to extract the content
	cout<<"parsing\n";
	if(!packet_rcvd.ParseFromString(buff))
	{
		cerr << "Failed to parse received message!\n";
	}

	packet_to_send.Clear();
	packet_to_send.set_command("QUIT");
	send_packet(packet_to_send, out_fd);
	close(out_fd);
	
	return packet_rcvd;
}


void *worker(void *arg)
{
	int fd = *(int *)arg;
	string request, response, com;
	char t;
	int l;
	locale loc;
	Packet packet, packet_response, packet_from_kv;
	pthread_mutex_t mtx;
	char buffer[2048];

	while(true)
	{
		cout<<"Inside While...\n";
		memset(buffer,0,sizeof(buffer));
		int rcvdbytes = do_recv(&fd, &buffer[0], -1);
		cout<<" rcvd bytes: "<<rcvdbytes<<endl;

		if(rcvdbytes == -1)
		{
			cerr << "Error reading command. Closing connection.\n";
			close(fd);
			pthread_exit(NULL);
		}
		buffer[rcvdbytes] = 0;
		cout << "NEW BUFF:" << buffer << endl;
		packet.ParseFromString(buffer);
		com = packet.command();
		cout << "Command: " << com << endl;
		
		// Transform command to lower case
		transform(com.begin(), com.end(), com.begin(), ::tolower);
		if(com.compare("restart") !=0 && killed){
			close(fd);
			pthread_exit(NULL);
		}
		if(com.compare("login") == 0 && !killed){
			string username = packet.arg(0);
			cout << "Username: " << username << endl;
			string pwd = packet.arg(1);
			cout << "Password: " << pwd << endl;
			string cookie= packet.arg(2);
			cout << "Cookie: " << cookie << endl;

			vector<string> arg;
			packet_from_kv = query_kv_store("GET", username, "PASSWORD", arg, "", "login");
			cout << "Password received\n";

			// Check for positive response
			if(packet_from_kv.status_code() == "500")
			{
				send_error_pack(fd, "500", "Could not retrieve password from KVS.");
				close(fd);
				pthread_exit(NULL);
			}
			cout << "Password Returned by master: " << packet_from_kv.data() << endl;

			// Check password validity
			if(packet_from_kv.data().compare(pwd)==0)
			{
				packet_from_kv.Clear();

				// Store cookie in KVS
				// packet_from_kv = query_kv_store("PUT", username, "COOKIE", arg, "","login");
				// packet_from_kv.Clear();
				// cout << "Cookie added\n";

				// Corresponding to the cookie, add {usr, pwd}
				json arr = json::array();
				arr.push_back(username);
				arr.push_back(pwd);
				string j_arr = "$" + arr.dump();
				packet_from_kv = query_kv_store("PUT", cookie, "SESSID", arg, j_arr,"signup");
				if(packet_from_kv.status_code() != "200")
				{
					cerr << "Could not store session ID\n";
					send_error_pack(fd, "500", "Could not store session ID");
					close(fd);
					pthread_exit(NULL);
				}

				// Send acknowledgement to client
				packet_from_kv = query_kv_store("GET", username, "ADMIN", arg, "","login");
				cout << "Admin status received\n";
				packet_response.set_data(packet_from_kv.data());
				packet_response.set_status_code("200");
				packet_response.set_status("OK");
				send_packet(packet_response, fd);
			}
			else
			{
				// Password invalid
				cerr << "Password Invalid\n";
				send_error_pack(fd, "500", "Password Invalid");
				close(fd);
				pthread_exit(NULL);
			}

			if(vflag){
				fprintf(stderr, "[%d] S: %s", fd, response.c_str());
			}
			//buffer.clear();
			memset(buffer,0,sizeof(buffer));
			com.clear();
		}
		else if(com.compare("signup") == 0 && !killed)
		{
			string username = packet.arg(0);
			string pwd = packet.arg(1);
			string admin = packet.arg(2);
			cout << "User: " << username << endl;
			cout << "Password: " << pwd << endl;

			vector<string> arg;
			packet_from_kv = query_kv_store("PUT", username, "PASSWORD", arg, pwd,"signup");
			if(packet_from_kv.status_code() == "500")
			{
				cerr << "Could not store password...\n";
				send_error_pack(fd, "500", "Could not store PWD.");
				close(fd);
				pthread_exit(NULL);
			}
			packet_from_kv.Clear();
			packet_from_kv = query_kv_store("PUT", username, "ADMIN", arg, admin,"signup");
			if(packet_from_kv.status_code() == "500")
			{
				cerr << "Could not store ADMIN status...\n";
				send_error_pack(fd, "500", "Sign up failed");
				close(fd);
				pthread_exit(NULL);
			}
			packet_from_kv.Clear();

			packet_response.set_status_code("200");
			packet_response.set_status("OK");
			send_packet(packet_response, fd);

			if(vflag)
			{
				fprintf(stderr, "[%d] S: %s", fd, response.c_str());
			}
			com.clear();
			cout << "Done with signup" << endl;
		}
		else if(com == "getsession" && !killed)
		{
			cout << "Inside getsession\n";

			// Store the received input and get data from KVS
			string cookie = packet.arg(0);
			cout << "COOKIE: " << cookie << endl;
			vector<string> arg;

			packet_from_kv = query_kv_store("GET", cookie, "SESSID", arg, "", "login");

			// Check for positive response
			if(packet_from_kv.status_code() == "500")
			{
				cerr << "Invalid session ID. Exiting..\n";
				send_error_pack(fd, "500", "Could not retrieve USER-Password from KVS.");
				close(fd);
				pthread_exit(NULL);
			}

			cout << "Retrieved USR-PWD\n";
			string jstr = packet_from_kv.data();
			json arr = json::array();
			arr = json::parse(jstr.substr(1));

			packet_response.set_status_code("200");
			packet_response.set_status("OK");
			packet_response.add_arg(arr.at(0));
			packet_response.add_arg(arr.at(1));
			send_packet(packet_response, fd);

		}
		else if(com == "deletesession" && !killed)
		{
			cout << "Inside delete session\n";
			
			// Store the received input and get data from KVS
			string cookie = packet.arg(0);
			vector<string> arg;

			packet_from_kv = query_kv_store("DEL", cookie, "SESSID", arg, "", "login");

			// Check for error
			if(packet_from_kv.status_code() == "500")
			{
				cerr << "Could not delete cookie. Exiting...\n";
				send_error_pack(fd, "500", "Could not delete cookie");
				close(fd);
				pthread_exit(NULL);
			}

			packet_response.set_status_code("200");
			packet_response.set_status("OK");
			send_packet(packet_response, fd);
		}
		else if(com.compare("kill") == 0)
		{
			killed = true;
			packet.Clear();
      		packet_response.set_command("OK");
      		send_packet(packet_response, fd);
			close(fd);
			pthread_exit(NULL);
		}
		else if(com.compare("restart") == 0)
		{
			killed = false;
			packet_response.set_command("OK");
			send_packet(packet_response, fd);
      		close(fd);
      		pthread_exit(NULL);
		}
		else
		{
			cerr << "Invalid Argument.\n";
			send_error_pack(fd, "500", "Invalid Command");
			close(fd);
			packet.Clear();
			packet_response.Clear();
			pthread_exit(NULL);
		}
		packet.Clear();
		packet_response.Clear();
	}
}


int main(int argc, char *argv[])
{

	// Variable to store flag status for flags
	int option;

	int idx = 1;
	// Port to use
	int self_port;

	//Check for flags
	while((option = getopt(argc, argv, "avp:")) != -1)
	{
		switch(option)
		{
			case 'p':
				self_port = atoi(optarg);
				idx = idx + 2;
				break;
			case 'a':
				cerr << "Author: Ignorant human\n";
				idx = idx + 1;
				exit(0);
				break;
			case 'v':
				debug = 1;
				idx = idx + 1;
				break;
			case '?':
				if (optopt == 'p')
		          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
		        else if(isprint(optopt))
		          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		        else
		          fprintf(stderr,"Unknown option character `\\x%x'.\n", optopt);
		        return 1;
			default:
		    	cout << "varad" << endl;
		    	exit(1);
		}
	}

	// Check if arguments are in correct format
	if(argc != idx + 2)
	{
		cerr << "Incorrect Arguments.\nFormat: ./drive_server configfile index\nAdditional -v option can be specified.\n";
		exit(1);
	}
	cout << "Index: " << idx << endl;
	string configfile = string(argv[idx]);
	cout << "Config file: " << configfile << endl;
	SERVER_NUM = stoi(string(argv[idx + 1]));
	cout << "Authentication Server Number: " << SERVER_NUM << endl;


	// Map of all server addresses
	map <char, vector<string> > addr_list = parse_config(configfile);
	
	// Get the master address
	vector<pair<string, int> > master_addr = parse_server_info(addr_list['M'][0]);
	master_ip = master_addr[0].first;
	master_port = master_addr[0].second;

	// Get address of self
	vector<pair<string, int> > self_addr = parse_server_info(addr_list['A'][SERVER_NUM - 1]);
	string self_ip = self_addr[0].first;
	self_port = self_addr[0].second;

	// Master address string representation
	string master_addr_str = self_addr[1].first + ":" + to_string(self_addr[1].second);
	cout << "Heartbeat addr: " << master_addr_str << endl;

	// Heartbeat handler thread
	struct heartbeatArgs args;
	args.heartAddr = master_addr_str;
	args.killed = &killed;
	pthread_t heartbeat;
	pthread_create(&heartbeat, NULL, heartbeat_client, &args);
	pthread_detach(heartbeat);


	int iSetOption = 1;

	//Create a socket for listening and accepting connections from client
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption, sizeof(iSetOption));
	if(sockfd < 0)
		fprintf(stderr, "Error opening socket\n");

	// Setup auth server address
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(self_port);


	//Bind the socket to a port
	if(bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
		fprintf(stderr, "Failed to bind socket\n");

	//Listen for connections
	listen(sockfd, SOMAXCONN);

	//Loop through accepting connections
	while(true)
	{
		struct sockaddr_in cli_addr;
		socklen_t clilen = sizeof(cli_addr);
		int *commfd = (int *)malloc(sizeof(int));

		//Accept the connection
		*commfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if(*commfd < 0)
			fprintf(stderr, "Error accepting connection\n");
		else if(vflag)
			fprintf(stderr, "[%d] New Connection\n",*commfd);

		open_conn.push_back(*commfd);

		//Create a thread for the new connection
		pthread_t cli_thread;
		pthread_create(&cli_thread, NULL, worker, commfd);
    	pthread_detach(cli_thread);
	}

  return 0;
}
