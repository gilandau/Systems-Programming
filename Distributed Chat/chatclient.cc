#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <fstream>
#include <vector>
#include <queue>
#include <time.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>

using namespace std;

const char* QUIT = "/quit\n";

int main(int argc, char *argv[])
{
	int port = -1;
	string ipaddr;
	bool keep_listening = true;

	if (argc != 2) {
		cerr << "*** Author: Gil Landau (glandau)\n";
		exit(1);
	}
	else{
		//		PARSE ARGS
		string ip_arg = string(argv[1]);
		size_t found = ip_arg.find(":");
		if (found!=string::npos){
			ipaddr = ip_arg.substr(0, found);
			string s_port = ip_arg.substr(found+1, ip_arg.length());
			if(ipaddr.length() == 0 || s_port.length() == 0 ){
				cerr <<" Bad argument\n";
				exit(1);
			}
			cout << ipaddr << "\n";
			cout << s_port << "\n";
			port = atoi(s_port.c_str());

		}
		else{
			cerr <<" Bad argument\n";
			exit(1);
		}
	}
	int listen_socket = socket(PF_INET, SOCK_DGRAM,0);

	if (listen_socket < 0){
		cerr << "Cannot open main socket\n";
		exit(2);
	}

	// SETUP DATAGRAM SOCKET

	struct sockaddr_in dest;
	bzero(&dest, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(port);

	inet_pton(AF_INET, ipaddr.c_str(), &(dest.sin_addr));

	// RECEIVER SOCKET
	struct sockaddr_in src;
	socklen_t srcSize = sizeof(src);

	while(keep_listening){
		char buf[1100];
//		char rbuf[1100];

		fd_set r;
//		fd_set w;
		FD_ZERO(&r);
//		FD_ZERO(&w);
//		Listen socket is server
//		STD_FILENO is user input
		FD_SET(listen_socket, &r);
		FD_SET(STDIN_FILENO, &r);

		int ret = select(max(STDIN_FILENO,listen_socket)+1, &r, NULL, NULL, NULL);

		//socket
		if (FD_ISSET(listen_socket,&r)){
			int rlen = recvfrom(listen_socket, buf, sizeof(buf), 0, (struct sockaddr*) &src, &srcSize);
			buf[rlen]='\0';
			cout << buf << "\n";
		}

		//user
		else if (FD_ISSET(STDIN_FILENO, &r)){
			int r = read(STDIN_FILENO,  buf, sizeof(buf));
			buf[r] = '\0';
			sendto(listen_socket, buf, sizeof(buf), 0, (struct sockaddr*) &dest, sizeof(dest));
			if (strcmp(buf, QUIT) == 0){
				keep_listening = false;
			}
		}
		else{
			//wrong socket?
			exit(5);
		}
	}
	close(listen_socket);
	return 0;
}
