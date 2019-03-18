#define _DEFAULT_SOURCE
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
#include "utility.h"


using namespace std;

// Debug flag
static int debug = 1;

// Write data stream to server
void write_stream(int sockfd, const char *str, const char *arg)
{
	char buff[4096];
	if (arg != NULL)
    	snprintf(buff, sizeof(buff), str, arg);        
    else
        snprintf(buff, sizeof(buff), str, NULL);
    send(sockfd, buff, strlen(buff), 0);
}

// Open transmission channel to server
void send_data(vector<string> v)
{
	// Create socket to communicate with mailserver
	int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd < 0)
	{
		cerr << "Cannot open socket\n";
		exit(1);
	}
	else
	{	
		// Initialize server data
		struct sockaddr_in server_addr;
		bzero(&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(8000);
		inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));

		// Buffer to store server response
		char response[2048];

		// Connect to mail server
		Packet packet_to_send;
		Packet packet_rcvd;

		char buff[2048];
		int idx = 0;
		string out;
		int f = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
		string status;
		string data;


		if(f != -1)
		{
			//Idx stores the number of bytes
			idx = do_recv(&sockfd, buff);
			if(idx == -1)
			{
				cerr << "Error reading COMMAND. Exiting...\n";
				close(sockfd);
			}
			buff[idx] = 0;
			// Set \r\n to 0 and then transfer contents of the buffer to a string
			buff[idx - 2] = 0;
			buff[idx - 1] = 0;

			// Parse the message to extract the content
			if(!packet_rcvd.ParseFromString(buff))
			{
				cerr << "Failed to parse received message!\n";
			}
			data = packet_rcvd.data();
			status = packet_rcvd.status_code();
			memset(buff, 0, 2048);
			cout << status + " " + data + "\n";
			idx = 0;

			packet_to_send.set_command("HELO");
			packet_to_send.add_arg("penncloud");
			if(!packet_to_send.SerializeToString(&out))
			{
				cerr << "Could not serialize packet to send.\n";
			}
			out = out + "\r\n";
			//cout << "Out: " + out;
			cout << "Out_size: " << out.size() << endl;
			char response[out.size()];
			out.copy(response, out.size());
			do_send(&sockfd, response, sizeof(response));
			idx = 0;
			out.erase(out.begin(), out.end());
			packet_to_send.Clear();
			packet_rcvd.Clear();


			// Idx storest the number of bytes
			idx = do_recv(&sockfd, buff);
			if(idx == -1)
			{
				cerr << "Error reading COMMAND. Exiting...\n";
				close(sockfd);
			}
			buff[idx] = 0;
			// Set \r\n to 0 and then transfer contents of the buffer to a string
			buff[idx - 2] = 0;
			buff[idx - 1] = 0;
			//const string *to_parse1 = string(buff);

			// Parse the message to extract the content
			if(!packet_rcvd.ParseFromString(buff))
			{
				cerr << "Failed to parse received message!\n";
			}
			data = packet_rcvd.status();
			status = packet_rcvd.status_code();
			memset(buff, 0, 2048);
			cout << status + " " + data + "\n";
			idx = 0;

			packet_to_send.set_command("MAIL FROM");
			packet_to_send.add_arg("linhphan@penncloud");
			if(!packet_to_send.SerializeToString(&out))
			{
				cerr << "Could not serialize packet to send.\n";
			}
			out = out + "\r\n";
			char response2[out.size()];
			out.copy(response2, out.size());
			do_send(&sockfd, response2, sizeof(response2));
			out.erase(out.begin(), out.end());
			idx = 0;
			packet_to_send.Clear();
			packet_rcvd.Clear();

			// Idx storest the number of bytes
			idx = do_recv(&sockfd, buff);
			if(idx == -1)
			{
				cerr << "Error reading COMMAND. Exiting...\n";
				close(sockfd);
			}

			// // Set \r\n to 0 and then transfer contents of the buffer to a string
			buff[idx - 2] = 0;
			buff[idx - 1] = 0;
			//const string to_parse2 = string(buff);

			// Parse the message to extract the content
			if(!packet_rcvd.ParseFromString(buff))
			{
				cerr << "Failed to parse received message!\n";
			}
			data = packet_rcvd.status();
			status = packet_rcvd.status_code();

			cout << status + " " + data + "\n";
			idx = 0;

			packet_to_send.set_command("RCPT TO");
			packet_to_send.add_arg("alice@penncloud");
			if(!packet_to_send.SerializeToString(&out))
			{
				cerr << "Could not serialize packet to send.\n";
			}
			out = out + "\r\n";
			char response3[out.size()];
			out.copy(response3, out.size());
			do_send(&sockfd, response3, sizeof(response3));
			out.erase(out.begin(), out.end());
			idx = 0;
			packet_to_send.Clear();
			packet_rcvd.Clear();

			// Idx storest the number of bytes
			idx = do_recv(&sockfd, buff);
			if(idx == -1)
			{
				cerr << "Error reading COMMAND. Exiting...\n";
				close(sockfd);
			}

			// Set \r\n to 0 and then transfer contents of the buffer to a string
			buff[idx - 2] = 0;
			buff[idx - 1] = 0;
			//const string to_parse3 = string(buff);

			// Parse the message to extract the content
			if(!packet_rcvd.ParseFromString(buff))
			{
				cerr << "Failed to parse received message!\n";
			}
			memset(buff, 0, 2048);
			data = packet_rcvd.status();
			status = packet_rcvd.status_code();

			cout << status + " " + data + "\n";
			idx = 0;

			packet_to_send.set_command("RCPT TO");
			packet_to_send.add_arg("bob@penncloud");
			if(!packet_to_send.SerializeToString(&out))
			{
				cerr << "Could not serialize packet to send.\n";
			}
			out = out + "\r\n";
			char response4[out.size()];
			out.copy(response4, out.size());
			do_send(&sockfd, response4, sizeof(response4));
			out.erase(out.begin(), out.end());
			idx = 0;
			packet_to_send.Clear();
			packet_rcvd.Clear();

			// Idx storest the number of bytes
			idx = do_recv(&sockfd, buff);
			if(idx == -1)
			{
				cerr << "Error reading COMMAND. Exiting...\n";
				close(sockfd);
			}

			// Set \r\n to 0 and then transfer contents of the buffer to a string
			buff[idx - 2] = 0;
			buff[idx - 1] = 0;
			//const string to_parse = string(buff);

			// Parse the message to extract the content
			if(!packet_rcvd.ParseFromString(buff))
			{
				cerr << "Failed to parse received message!\n";
			}
			memset(buff, 0, 2048);
			data = packet_rcvd.status();
			status = packet_rcvd.status_code();

			cout << status + " " + data + "\n";
			idx = 0;

			packet_to_send.set_command("DATA");
			if(!packet_to_send.SerializeToString(&out))
			{
				cerr << "Could not serialize packet to send.\n";
			}
			out = out + "\r\n";
			char response5[out.size()];
			out.copy(response5, out.size());
			do_send(&sockfd, response5, sizeof(response5));
			out.clear();
			idx = 0;
			packet_to_send.Clear();
			packet_rcvd.Clear();

			// Idx storest the number of bytes
			idx = do_recv(&sockfd, buff);
			if(idx == -1)
			{
				cerr << "Error reading COMMAND. Exiting...\n";
				close(sockfd);
			}

			// Set \r\n to 0 and then transfer contents of the buffer to a string
			buff[idx - 2] = 0;
			buff[idx - 1] = 0;
			//const string to_parse5 = string(buff);

			// Parse the message to extract the content
			if(!packet_rcvd.ParseFromString(buff))
			{
				cerr << "Failed to parse received message!\n";
			}
			memset(buff, 0, 2048);
			data = packet_rcvd.status();
			status = packet_rcvd.status_code();
			packet_to_send.Clear();

			cout << status + " " + data + "\n";
			idx = 0;

			for(int i = 2; i < v.size(); i++)
		    {
		    	packet_to_send.set_data(packet_to_send.data() + v[i]);
			}
	    	//packet_to_send.set_data("\r\n.\r\n");
	    	cout << packet_to_send.data();
			if(!packet_to_send.SerializeToString(&out))
			{
				cerr << "Could not serialize packet to send.\n";
			}
			out = out + "\r\n";
			char response10[out.size()];
			out.copy(response10, out.size());
			do_send(&sockfd, response10, sizeof(response10));
			out.clear();

		    // Idx storest the number of bytes
			idx = do_recv(&sockfd, buff);
			if(idx == -1)
			{
				cerr << "Error reading COMMAND. Exiting...\n";
				close(sockfd);
			}

			// Set \r\n to 0 and then transfer contents of the buffer to a string
			buff[idx - 2] = 0;
			buff[idx - 1] = 0;

			// Parse the message to extract the content
			if(!packet_rcvd.ParseFromString(buff))
			{
				cerr << "Failed to parse received message!\n";
			}
			memset(buff, 0, 2048);
			data = packet_rcvd.status();
			status = packet_rcvd.status_code();

			cout << status + " " + data + "\n";
			idx = 0;
			packet_to_send.Clear();
			packet_rcvd.Clear();
			
			packet_to_send.set_command("QUIT");
			if(!packet_to_send.SerializeToString(&out))
			{
				cerr << "Could not serialize packet to send.\n";
			}
			out = out + "\r\n";
			char response6[out.size()];
			out.copy(response6, out.size());
			do_send(&sockfd, response6, sizeof(response6));
			out.erase(out.begin(), out.end());
			idx = 0;
			packet_to_send.Clear();
			packet_rcvd.Clear();

			// Idx storest the number of bytes
			idx = do_recv(&sockfd, buff);
			if(idx == -1)
			{
				cerr << "Error reading COMMAND. Exiting...\n";
				close(sockfd);
			}

			// Set \r\n to 0 and then transfer contents of the buffer to a string
			buff[idx - 2] = 0;
			buff[idx - 1] = 0;

			// Parse the message to extract the content
			if(!packet_rcvd.ParseFromString(buff))
			{
				cerr << "Failed to parse received message!\n";
			}
			memset(buff, 0, 2048);
			data = packet_rcvd.status();
			status = packet_rcvd.status_code();

			cout << status + " " + data + "\n";
			idx = 0;
			packet_to_send.Clear();
			packet_rcvd.Clear();
		}
		else
		{
			// Write undiliverable msg to tmp file
			cerr << "Unable to connect to server...\n";
			close(sockfd);
		}
	}
}

void send_mail()
{
	string line;
	string address;
	ifstream myfile;
	myfile.open("mqueue", ios::in);
	vector<string> data;
	int flag = 0;
	while(getline(myfile, line))
	{
		if(line.substr(0, 5) == "From ")
		{
			// Send prev. mail
			if(data.size() > 0 && flag == 0)
			{
				send_data(data);
				data.clear();
			}
			else if(data.size() > 0 && flag == 1)
			{
				flag = 0;
				data.clear();
			}
			getline(myfile, line);
			
			data.push_back("<linh@penncloud>");
			data.push_back("<alice@penncloud>");
		}
		else
			data.push_back(line + "\n");
	}
	if(data.size() > 0 && flag == 0)
	{
		send_data(data);
		data.clear();
	}
	else if(data.size() > 0 && flag == 1)
	{
		flag = 0;
	}
	return;
}

int main(int argc, char const *argv[])
{
	send_mail();
	return 0;
}