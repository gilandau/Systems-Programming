#include <bits/stdc++.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <locale.h>
#include <fcntl.h>

using namespace std;

int sockfd, sockudp;

int build_fd_sets(int *sock, fd_set *readfds){
	FD_ZERO(readfds);
	FD_SET(STDIN_FILENO, readfds);
	FD_SET(*sock, readfds);

	return 0;
}

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

int main(int argc, char *argv[]){

	int c;
	opterr = 0;

	int portno = 10000;
	struct sockaddr_in serv_addr, serv_addr_udp;

	string input;
	while((c = getopt(argc, argv, "p:a")) != -1){
		switch(c){
		case 'p':
			portno = atoi(optarg);
			break;
		case 'a':
			fprintf(stderr, "*** Author: Saket Milind Karve (saketk)");
			exit(0);
			break;
		case '?':
			if(optind != argc){
				if(optopt == c){
					fprintf(stderr, "Option %c requires an argument\n", optopt);
					break;
				}
				else if(isprint(optopt)){
					fprintf(stderr, "Unknown option '-%c'\n", optopt);
					break;
				}
				else{
					fprintf(stderr, "Unknown option character");
					break;
				}
			}
			break;
		default:
			fprintf(stderr, "*** Author: Saket Milind Karve (saketk)");
			abort();
		}
	}
	long a,b;
	a = sysconf(_SC_PHYS_PAGES);
	b = sysconf(_SC_AVPHYS_PAGES);
	cout<<"Total: "<<a<<"Available: "<<b<<endl; 

	int iSetOption = 1;
	//Create a socket for connecting to the respective mail server
	cout<<"Creating socket\n";
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption, sizeof(iSetOption));
	if(sockfd < 0)
		fprintf(stderr, "Error opening socket\n");

	bzero((char *) &serv_addr, sizeof(serv_addr));

	//Set up address structure
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	sockudp = socket(AF_INET, SOCK_DGRAM, 0);
	setsockopt(sockudp, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption, sizeof(iSetOption));
	if(sockudp < 0)
		fprintf(stderr, "Error opening socket\n");

	bzero((char *) &serv_addr_udp, sizeof(serv_addr_udp));

	//Set up address structure
	serv_addr_udp.sin_family = AF_INET;
	serv_addr_udp.sin_addr.s_addr = INADDR_ANY;
	serv_addr_udp.sin_port = htons(portno);


	/* Set nonblock for stdin. */
  	int flag = fcntl(STDIN_FILENO, F_GETFL, 0);
	flag |= O_NONBLOCK;
	fcntl(STDIN_FILENO, F_SETFL, flag);

	fd_set readfds;
	int maxfd = sockfd;

	cout<<"Connecting...\n";
					if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
						fprintf(stderr, "Error connecting\n");
					}else{
						cout<<"Connected"<<endl;
					}
	
	
	while(1){
		//Packet packet;
		char buf[4096];
		build_fd_sets(&sockfd, &readfds);
		
		
		int activity = select(maxfd+1, &readfds, NULL, NULL, NULL);
		cout<<"Select returned: "<<activity<<endl;
		int choice;
		//cout<<"Enter 0:GET 1:PUT 2:CPUT 3:DELETE 4:QUIT\n";
		a = sysconf(_SC_PHYS_PAGES);
		b = sysconf(_SC_AVPHYS_PAGES);
		cout<<"Total: "<<a<<"Available: "<<b<<endl; 
		
		switch(activity){
			case -1:
				fprintf(stderr, "Error in selecting stream. Terminating\n");
				exit(1);
			case 0:
				fprintf(stderr, "Error in selecting stream. Terminating\n");
				exit(1);
			default:
				if(FD_ISSET(sockfd, &readfds)){
					//Read from server
					string request;
					char t;
					do{
						//cout<<"Inside while...\n";
						if(read(sockfd, &t, 1) < 0){
							fprintf(stderr, "Could not read command from client. Try again!");
						}
						if(t != '\n' || t != '\r')
							request += t;
					}while(t != '\n');
					cout<<"[S] "<<request<<endl;
					
				}else if(FD_ISSET(STDIN_FILENO, &readfds)){
					//Read from user input (console)
					getline(cin, input);
					cout<<"Sending to TCP..."<<input<<endl;
					input = input + "\n";
					
					my_write(sockfd, input.c_str(), input.length());
					
					//cin>>choice;
					/*string row, column, data, arg, msg;
					switch(choice){
						case 0:
							cout<<"Enter row\n";
							cin>>row;
							cout<<"Enter column\n";
							cin>>column;
							packet.set_command("get");
							packet.set_row(row);
							packet.set_column(column);
							packet.SerializeToString(&msg);
							msg += "\r\n";
							sprintf(buf, "%s", msg.c_str());
							my_write(sockfd, buf, sizeof(buf));
							break;
						case 1:
							cout<<"Enter row\n";
							cin>>row;
							cout<<"Enter column\n";
							cin>>column;
							cout<<"Enter Data 1\n";
							cin>>data;
							cout<<"Enter Data 2\n";
							cin>>arg;
							packet.set_command("put");
							packet.set_row(row);
							packet.set_column(column);
							packet.set_data(data);
							packet.set_arg(arg);
							packet.SerializeToString(&msg);
							msg += "\r\n";
							sprintf(buf, "%s", msg.c_str());
							my_write(sockfd, buf, sizeof(buf));
							break;
						case 2:
							cout<<"Enter row\n";
							cin>>row;
							cout<<"Enter column\n";
							cin>>column;
							cout<<"Enter Data\n";
							cin>>data;
							packet.set_command("cput");
							packet.set_row(row);
							packet.set_column(column);
							packet.set_data(data);
							packet.SerializeToString(&msg);
							msg += "\r\n";
							sprintf(buf, "%s", msg.c_str());
							my_write(sockfd, buf, sizeof(buf));
							break;
						case 3: 
							cout<<"Enter row\n";
							cin>>row;
							cout<<"Enter column\n";
							cin>>column;
							packet.set_command("del");
							packet.set_row(row);
							packet.set_column(column);
							packet.SerializeToString(&msg);
							msg += "\r\n";
							sprintf(buf, "%s", msg.c_str());
							my_write(sockfd, buf, sizeof(buf));
							break;
						case 4:
							packet.set_command("quit");
							packet.SerializeToString(&msg);
							msg += "\r\n";
							sprintf(buf, "%s", msg.c_str());
							my_write(sockfd, buf, sizeof(buf));
							break;
					}*/					
					
					/*cout<<"In in\n";
					packet.set_command("put");
					packet.set_row("A");
					packet.set_column("B");
					packet.set_data("C");
					string msg;
					packet.SerializeToString(&msg);
					msg = msg + "\r\n";
					char input[msg.size()];
					msg.copy(input,msg.size());
					my_write(sockfd, input, sizeof(input));

					msg.clear();
					packet.Clear();
					packet.set_command("put");
					packet.set_row("A");
					packet.set_column("B");
					packet.set_data("D");
					//string msg;
					packet.SerializeToString(&msg);
					msg = msg + "\r\n";
					char input1[msg.size()];
					msg.copy(input1,msg.size());
					my_write(sockfd, input1, sizeof(input1));

					msg.clear();
					packet.Clear();
					packet.set_command("put");
					packet.set_row("X");
					packet.set_column("Y");
					packet.set_data("Z");
					//string msg;
					packet.SerializeToString(&msg);
					msg = msg + "\r\n";
					char input2[msg.size()];
					msg.copy(input2,msg.size());
					my_write(sockfd, input2, sizeof(input2));

					msg.clear();
					packet.Clear();
					packet.set_command("GET");
					packet.set_row("A");
					packet.set_column("B");
					//string msg;
					packet.SerializeToString(&msg);
					msg = msg + "\r\n";
					char input3[msg.size()];
					msg.copy(input3,msg.size());
					my_write(sockfd, input3, sizeof(input3));*/
				}
		}
	}
	return 0;
}