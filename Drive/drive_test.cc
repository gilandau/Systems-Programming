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
#include <nlohmann/json.hpp>
#include <openssl/md5.h>

using namespace std;
using json = nlohmann::json;

// Debug flag
static int debug = 0;

Packet get_response(int sockfd)
{
	Packet pack_recv;
	char buff[2048];
	int idx;
	idx = do_recv(&sockfd, buff, 10);
	if(idx < 0)
	{
		cerr << "Error receiving packet. Exiting...\n";
		close(sockfd);
		exit(1);
	}
	buff[idx] = 0;
	cout << "BUFF: " << string(buff) << endl;
	if(!pack_recv.ParseFromString(buff))
	{
		cerr << "Failed to parse received message!\n";
		exit(1);
	}
	return pack_recv;
}

void send_data(Packet pack, int sockfd)
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

int main(int argc, char *argv[])
{
	// Variable to store flag status for flags
	int option;

	int idx = 1;
	// Port to use
	int port;

	//Check for flags
	while((option = getopt(argc, argv, "avp:")) != -1)
	{
		switch(option)
		{
			case 'p':
				port = atoi(optarg);
				idx = idx + 2;
				break;
			case 'a':
				cerr << "Author: Varad Deshpande (varad)\n";
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
	if(argc != idx)
	{
		cerr << "Incorrect Arguments.\nFormat: ./drive_test  \nOptional -v can be specified.\n";
		exit(1);
	}

	// Create socket to connect to drive server
	int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd < 0)
	{
		cerr << "Cannot open socket\n";
		exit(1);
	}

	// Initialize server data
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));

	int status = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(status < 0)
	{
		cerr << "Error connecting. Exiting...\n";
		exit(1);
	}

	Packet pack = get_response(sockfd);
	Packet to_send;

	pretty_print("S", pack.data(), debug);
	pack.Clear();

	string name = "dog@penncloud";

	// // Create home
	// to_send.set_command("new");
	// to_send.add_arg(name);
	// to_send.add_arg("home");
	// to_send.add_arg("d");
	// send_data(to_send, sockfd);
	// pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	// to_send.Clear();

	// pack = get_response(sockfd);
	// pretty_print("S", pack.status_code(), debug);
	// pack.Clear();

	// Put data in KVS
	to_send.set_command("upload");
	to_send.add_arg(name);
	to_send.add_arg("home/file1");
	to_send.add_arg("f");
	to_send.add_arg("1");
	to_send.set_data("This is \r\nfile 1.");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pack.Clear();

	// Create new directory
	to_send.set_command("new");
	to_send.add_arg(name);
	to_send.add_arg("home/dir1");
	to_send.add_arg("d");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pack.Clear();

	// Upload files to dir1
	to_send.set_command("upload");
	to_send.add_arg(name);
	to_send.add_arg("home/dir1/file1");
	to_send.add_arg("f");
	to_send.add_arg("1");
	to_send.set_data("This is file 1 in dir1.");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pack.Clear();

	to_send.set_command("upload");
	to_send.add_arg(name);
	to_send.add_arg("home/dir1/file2");
	to_send.add_arg("f");
	to_send.add_arg("1");
	to_send.set_data("This is file 2 in dir1.");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pack.Clear();

	// Create dir2
	to_send.set_command("new");
	to_send.add_arg(name);
	to_send.add_arg("home/dir1/dir2");
	to_send.add_arg("d");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pack.Clear();

	// Upload file to dir2
	to_send.set_command("upload");
	to_send.add_arg(name);
	to_send.add_arg("Home/dir1/dir2/file1");
	to_send.add_arg("f");
	to_send.add_arg("1");
	to_send.set_data("This is file 1 in dir2.");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pack.Clear();

	// Download file from KVS
	to_send.set_command("download");
	to_send.add_arg(name);
	to_send.add_arg("home/file1");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pretty_print("S", pack.data(), debug);
	pack.Clear();

	// Display contents of dir1
	to_send.set_command("display");
	to_send.add_arg(name);
	to_send.add_arg("Home/dir1");
	to_send.add_arg("d");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pretty_print("S", pack.data(), debug);
	pack.Clear();

	// Rename dir1 as dir3
	to_send.set_command("rename");
	to_send.add_arg(name);
	to_send.add_arg("home/dir1");
	to_send.add_arg("home/dir3");
	to_send.add_arg("d");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1) + " to " + to_send.arg(2), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pack.Clear();

	// Display contents of dir3
	to_send.set_command("display");
	to_send.add_arg(name);
	to_send.add_arg("home/dir3");
	to_send.add_arg("d");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pretty_print("S", pack.data(), debug);
	pack.Clear();

	//Delete dir3
	to_send.set_command("delete");
	to_send.add_arg(name);
	to_send.add_arg("home/dir3");
	to_send.add_arg("d");
	send_data(to_send, sockfd);
	pretty_print("C", to_send.command() + " " + to_send.arg(1), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pack.Clear();

	//Quit
	to_send.set_command("quit");

	send_data(to_send, sockfd);
	pretty_print("C", to_send.command(), debug);
	to_send.Clear();

	pack = get_response(sockfd);
	pretty_print("S", pack.status_code(), debug);
	pack.Clear();

	return 0;
}