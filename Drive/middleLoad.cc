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
#include <sys/poll.h>

#include "DATA_PACKET.pb.h"
#include "utility.h"

using namespace std;

#define BUF_SIZE 4096

int heartSock, loadSock;
map<string, int> loads;
sem_t loadSem;
bool debug;
map<char, vector<string>> serverConfig;
pthread_t heartThread;
int servNum = -1;
bool killed = false;

struct heartbeatArgs {
  string heartAddr;
  bool* killed;
};

string getInputArgs(int argc, char *argv[]) {
	int opt;
	debug = false;

	while ((opt = getopt(argc, argv, "vn:")) != -1) {
		switch (opt) {
			case 'v':
				debug = true;
				break;
			case 'n':
				servNum = stoi(optarg);
				break;
			default:
				cerr << "Error invalid argument for " << argv[0] << endl;
				cerr << "Usage: " << argv[0] << "[-v] [-n <servNum] " <<
					"</path/to/serverConfig>" << endl;
				exit(1);
		}
	}

	string configFilename;
	if (optind == argc-1) {
		configFilename.append(argv[optind]);
	} else {
		cerr << "Error invalid argument for " << argv[0] << endl;
		cerr << "Usage: " << argv[0] << "[-v] [-n servNum] " <<
			"</path/to/serverConfig>" << endl;
		exit(1);
	}

	if (servNum == -1) {
		cerr << "Input server number with -n <num>" << endl;
		exit(1);
	}

	return configFilename;
}

struct sockaddr_in getServerAddr(string config) {
	// need the ip and first port
	int colon = config.find(":");
	int comma = config.find(",");
	string ip(config.substr(0, colon));
	string portStr(config.substr(colon+1, comma-colon-1));
	int port = stoi(portStr);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	inet_aton(ip.c_str(), &(addr.sin_addr));
	addr.sin_port = htons(port);

	return addr;
}

void closeAndSIGINT(vector<struct pollfd>& fds) {
	for (auto it = fds.begin(); it != fds.end(); ++it) {
		struct pollfd f = *it;
		close(f.fd);
	}
	raise(SIGINT);
}

void connectAndSend(vector<string>& servers, map<int, string>& fdToAddr,
		vector<struct pollfd>& fds, char *msg, char c) {
	int size = servers.size();
	for (int i = 0; i < size; i++)
	{
		struct sockaddr_in servAddr = getServerAddr(servers[i]);
		// open socket
		int s;
		if ( (s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			perror("socket");
			closeAndSIGINT(fds);
		}
		// connect
		if (connect(s, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
		{
			// check to see if server is just down
 			if (errno == ECONNREFUSED)
 			{
				// just set this load's value to 100000 and continue
				sem_wait(&loadSem);
				loads[servers[i]] = 100000;
				sem_post(&loadSem);
				close(s);
				continue;
			} 
			else
			{
				// actual error
				perror("connect");
				closeAndSIGINT(fds);
			}
		}
		char blank[BUF_SIZE];
		if(c == 'S')
		{
			int n = recv_t(&s, blank, 4);
		    if (debug)
		    {
		      cerr << "n: " << n << endl;
		      cout << string(blank) << endl;
		    }
		    if (n < 0 && debug)
		      cerr << "no welcome message, continuing" << endl;

	  		send_t(&s, msg, strlen(msg));
	  	
			// add this fd to the poll struct
			struct pollfd thisFd;
			thisFd.fd = s;
			thisFd.events = POLLIN;
			fds.push_back(thisFd);
			// add the mapping from fd to address
			fdToAddr[s] = servers[i];
		}
		else
		{
		    
		    cout << "Before do recv" << endl;
		    int n = do_recv(&s, blank, 4);
		    if (debug)
		    {
		      cerr << "n: " << n << endl;
		      cout << string(blank) << endl;
		    }
		    if (n < 0 && debug)
		      cerr << "no welcome message, continuing" << endl;

			do_send(&s, msg, strlen(msg) - 2);
			// add this fd to the poll struct
			struct pollfd thisFd;
			thisFd.fd = s;
			thisFd.events = POLLIN;
			fds.push_back(thisFd);
			// add the mapping from fd to address
			fdToAddr[s] = servers[i];
		}
	}
}

bool serverLoadSorter(const pair<string,int>&a, const pair<string,int>& b) {
	if (a.second != b.second)
		return a.second < b.second;
	return a.first < b.first;
}

void sortLoads(vector<pair<string, int>>& popLoads, vector<pair<string, int>>& smtpLoads, 
    vector<pair<string, int>>& driveLoads, vector<pair<string, int>>& adminLoads) {
	// loop through the loads for the various servers and place into the 
	// respective vector
	sem_wait(&loadSem);
	for (auto it = serverConfig['P'].begin(); it != serverConfig['P'].end(); ++it) {
    if (loads[*it] == 100000)
      continue;
		pair<string, int> servPair(*it, loads[*it]);
		popLoads.push_back(servPair);
	}
	for (auto it = serverConfig['S'].begin(); it != serverConfig['S'].end(); ++it) {
		if (loads[*it] == 100000)
      continue;
    pair<string, int> servPair(*it, loads[*it]);
		smtpLoads.push_back(servPair);
	}
	for (auto it = serverConfig['R'].begin(); it != serverConfig['R'].end(); ++it) {
    if (loads[*it] == 100000)
      continue;
		pair<string, int> servPair(*it, loads[*it]);
		driveLoads.push_back(servPair);
	}
  for (auto it = serverConfig['C'].begin(); it != serverConfig['C'].end(); ++it) {
    if (loads[*it] == 100000)
      continue;
    pair<string, int> servPair(*it, loads[*it]);
    adminLoads.push_back(servPair);
  }

	sort(popLoads.begin(), popLoads.end(), serverLoadSorter);
	sort(smtpLoads.begin(), smtpLoads.end(), serverLoadSorter);
	sort(driveLoads.begin(), driveLoads.end(), serverLoadSorter);
  sort(adminLoads.begin(), adminLoads.end(), serverLoadSorter);
	sem_post(&loadSem);
}	

void * client_handler(void * args) {
	int sock = *( (int *) args);
	
	// read from socket to make sure we need to a check
	char buf[BUF_SIZE];
	int n = 0;
	n = do_recv(&sock, buf, -1);
	if (n < 0) {
		perror("read");
		raise(SIGINT);
	}
  buf[n] = 0;

	Packet r;
	if (!r.ParseFromString(buf)) {
		cerr << "couldn't parse" << endl;
		raise(SIGINT);
	}

	string command = r.command();
	transform(command.begin(), command.end(), command.begin(), ::tolower);

  // first check to see if we are in a killed state
  if (killed) {
    cout << "in killed state" << endl;
    if (command == "restart") {
      // time to start accepting messages again
      killed = false;
      // prepare packet for OK response
      Packet to_send;
      to_send.set_command("OK");
      string out;
      if (!to_send.SerializeToString(&out)) {
        cerr << "Could not serialize packet" << endl;
        raise(SIGINT);
      }
      string pre = generate_prefix(out.size());
      out = pre + out;
      char msg[out.size()];
      out.copy(msg, out.size());
      do_send(&sock, msg, out.size());
      close(sock);
      pthread_exit(NULL);
    } else {
      // ignoring this message, just close the socket
      close(sock);
      pthread_exit(NULL);
    }
  }

	if (command != "middle") {
		cerr << "unknown command " << command << endl;
		close(sock);
		return NULL;
	}

  cout << "middle received" << endl;

	// create file descriptor data structures
	map<int, string> fdToAddr;
	vector<struct pollfd> fds;

	// Prepare packet to send to each 
	Packet load;
	load.set_command("CURLOAD");
	string out;
	if (!load.SerializeToString(&out)) {
		cerr << "Could not serialize packet" << endl;
		exit(1);
	}
	string pre = generate_prefix(out.size());
	out = pre + out + "\r\n";
	char msg[out.size()];
	out.copy(msg, out.size());
	cout << "OUT: " << msg << endl;

	// attempt to connect to each server and then send the packet
	connectAndSend(serverConfig['P'], fdToAddr, fds, msg, 'P');
	connectAndSend(serverConfig['S'], fdToAddr, fds, msg, 'S');
	connectAndSend(serverConfig['R'], fdToAddr, fds, msg, 'R');
  connectAndSend(serverConfig['C'], fdToAddr, fds, msg, 'C');

	// now, we keep a count of fds we've read from and loop while we haven't read from all of them
	int readSoFar = 0, numFds = fds.size(), pollErr;
	// convert vector of fds to array (now that we know how many we care about)
	struct pollfd fdArr[numFds];
	for (int i = 0; i < numFds; i++)
		fdArr[i] = fds[i];

	while (readSoFar < numFds) {
		// indefinite timeout because we are using TCP connections, poll will stop blocking at some point
		pollErr = poll(fdArr, numFds, -1); 
		
		if (pollErr < 0) {
			perror("poll");
			closeAndSIGINT(fds);
		}

		// loop through the fds to see if we're ready to read
		for (int i = 0; i < numFds; i++) {
			if (fdArr[i].revents != POLLNVAL) {
				// lots of possibilities for poll to return (err doesnt necessarily mean the poll was in error, just the user closed the socket)
				// data to read from middle server
				char loadResp[BUF_SIZE];
				int n = 0;
				n = do_recv(&(fdArr[i].fd), loadResp, -1);
				// TODO timeout from do_recv might mean the server is overloaded, but need to check
				if (n < 0) {
          // we will assume that the load is "infinite" if we can't connect (in
          // the case the server just closes the socket)
          string addr = fdToAddr[fdArr[i].fd];
          sem_wait(&loadSem);
          loads[addr] = 100000;
          sem_post(&loadSem);
          readSoFar++;
          close(fdArr[i].fd);
          continue;
        }
        loadResp[n] = 0;
        cout << loadResp << " = loadResp" << endl;

				Packet loadP;
				if (!loadP.ParseFromString(loadResp)) {
					cerr << "couldn't parse" << endl;
					closeAndSIGINT(fds);
				}

				string command = loadP.command();
				transform(command.begin(), command.end(), command.begin(), ::tolower);
				if (command != "load") {
					cerr << "invalid command " << command << endl;
					closeAndSIGINT(fds);
				}

				// get the arg(0) from the packet for the load
				int servLoad;
				try {
					servLoad = stoi(loadP.arg(0));
					if (debug)
						cerr << "servLoad: " << servLoad << endl;
				} catch (invalid_argument e) {
					cerr << "bad load value" << endl;
					closeAndSIGINT(fds);
				}
				string addr = fdToAddr[fdArr[i].fd];
				sem_wait(&loadSem);
				loads[addr] = servLoad;
				sem_post(&loadSem);
				readSoFar++;
				close(fdArr[i].fd); // done with fd
			}
		}
	}

	// we have an up to date look at the loads for the server, now we will
	// sort a vector for each of the server types
	vector<pair<string, int>> popLoads, smtpLoads, driveLoads, adminLoads;
	sortLoads(popLoads, smtpLoads, driveLoads, adminLoads);
  if (debug) {
    cerr << "printing out the loads: " << endl;
    for (auto c = popLoads.begin(); c != popLoads.end(); ++c)
      cerr << (*c).first << " " << (*c).second << endl;
    for (auto c = smtpLoads.begin(); c != smtpLoads.end(); ++c) 
      cerr << (*c).first << " " << (*c).second << endl;
    for (auto c = driveLoads.begin(); c != driveLoads.end(); ++c)
      cerr << (*c).first << " " << (*c).second << endl;
    for (auto c = adminLoads.begin(); c != adminLoads.end(); ++c)
      cerr << (*c).first << " " << (*c).second << endl;
  }

	// ensure none of the server types have 0 servers in vector
	/*if (popLoads.size() == 0 || smtpLoads.size() == 0 || driveLoads.size() == 0
      || authLoads.size() == 0) {
		cerr << "error no servers to connect to" << endl;
		closeAndSIGINT(fds);
	}*/
  // send back a packet with no servers in the case of no servers to connect to
  // here, we make a distinction between pop/smtp and drive 
  /*if (popLoads.size() == 0 || smtpLoads.size() == 0 || driveLoads.size() == 0) {
    Packet toSend;
    toSend.set_command("MIDSRV");
    string respStr;
    if (!toSend.SerializeToString(&respStr)) {
      cerr << "Could not serialize packet" << endl;
      closeAndSIGINT(fds);
      string pre = generate_prefix(respStr.size());
      respStr = pre + respStr;
      char respMsg[respStr.size()];
      respStr.copy(respMsg, respStr.size());
      do_send(&sock, respMsg, respStr.size());
      close(sock);
      return NULL;
    }
  }
  */

  // if popLoads or smtpLoads == 0 and drive != 0: send drive only
  // if drive == 0 and (pop and smtp != 0): send pop and smtp only
  // else send all
  Packet toSend;
  toSend.set_command("MIDSRV");

  // we will send the address of the least loaded server as long as there is at
  // least one (except for pop/smtp, if one of those is down, we say all are
  // down to indicate all mail services are down)
  if (popLoads.size() == 0 || smtpLoads.size() == 0) {
    toSend.add_arg("POPDOWN");
    toSend.add_arg("SMTPDOWN");
  } else {
    toSend.add_arg(popLoads[0].first);
    toSend.add_arg(smtpLoads[0].first);
  }
  if (driveLoads.size() == 0)
    toSend.add_arg("DRIVEDOWN");
  else
    toSend.add_arg(driveLoads[0].first);
  if (adminLoads.size() == 0)
    toSend.add_arg("ADMINDOWN");
  else
    toSend.add_arg(adminLoads[0].first);
  
	// prepare packet to send to client
	string respStr;
	if (!toSend.SerializeToString(&respStr)) {
		cerr << "Could not serialize packet" << endl;
		closeAndSIGINT(fds);
	}
  pre = generate_prefix(respStr.size());
  respStr = pre + respStr;
	char respMsg[respStr.size()];
	respStr.copy(respMsg, respStr.size());
	do_send(&sock, respMsg, respStr.size());
	close(sock);
	return NULL;
}

void ctrlc_handler(int signum) {
	close(heartSock);
	close(loadSock);
	exit(0);
}

void updateConfig() {
	map<char, vector<string>> newConfig;
	for (auto it = serverConfig.begin(); it != serverConfig.end(); ++it) {
		vector<string> addrs = it->second;
		vector<string> newAddrs;
		for (auto a = addrs.begin(); a != addrs.end(); ++a) {
			size_t comma = (*a).find_first_of(",");
			if (comma != string::npos) {
				// comma is found, get substring up to comma
				string newAddr((*a).substr(0, comma));
				newAddrs.push_back(newAddr);
			} else {
				// no comma, just append address as is
				newAddrs.push_back(*a);
			}
		}
		newConfig[it->first] = newAddrs;
	}
	serverConfig = newConfig;
}

int main(int argc, char* argv[]) {
	signal(SIGINT, ctrlc_handler);
	sem_init(&loadSem, 0, 1);

	string configFilename = getInputArgs(argc, argv);

	if (debug) {
		cerr << "debug: " << debug << endl;
	}

	serverConfig = parse_config(configFilename);
	// parse out our address for the heartbeat
	int middleServs = serverConfig['L'].size();
	if (servNum > middleServs) {
		cerr << "Invalid server number, not enough servers" << endl;
		exit(1);
	}
	string fullAddr = serverConfig['L'][servNum-1];
	int colon = fullAddr.find(":");
	int comma = fullAddr.find(",");
	string ip(fullAddr.substr(0, colon));
	string firstPort(fullAddr.substr(colon+1, comma-colon-1));
	string secPort(fullAddr.substr(comma+1));
	string heartAddr(ip);
	heartAddr.append(":");
	heartAddr.append(secPort);
  if (debug)
    cerr << "My address: " << ip << ":" << firstPort << endl;

	// now get rid of all secondary ports (just keep the primary port)
	updateConfig();

	// create thread for heartbeat and communication with middle servers
  struct heartbeatArgs args;
  args.heartAddr = heartAddr;
  args.killed = &killed;
	pthread_create(&heartThread, NULL, heartbeat_client, &args);
	pthread_detach(heartThread);

	// open and accept connections to first port
	struct sockaddr_in loadAddr;
	socklen_t loadLen = sizeof(struct sockaddr_in);

	loadAddr.sin_family = AF_INET;
	inet_aton(ip.c_str(), &(loadAddr.sin_addr));
	int port = stoi(firstPort);
	loadAddr.sin_port = htons(port);

	if ( (loadSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("loadSock");
		raise(SIGINT);
	}

	int enable = 1;
	if (setsockopt(loadSock, SOL_SOCKET, SO_REUSEADDR, &enable,
				sizeof(int)) < 0) {
		perror("setsockopt");
		raise(SIGINT);
	}

	if ( bind(loadSock, (struct sockaddr *) &loadAddr, loadLen) < 0) {
		perror("bind");
		raise(SIGINT);
	}

	if (listen(loadSock, 100) < 0) {
		perror("listen");
		raise(SIGINT);
	}

	loadLen = sizeof(struct sockaddr_in);

	struct sockaddr_in cAddr;
	int c_sock;

	while (true) {
		if ( (c_sock = accept(loadSock, (struct sockaddr *)
						&cAddr, &loadLen)) < 0) {
			perror("accept");
			raise(SIGINT);
		}

		pthread_t cThread;
		pthread_create(&cThread, NULL, client_handler, &c_sock);
		pthread_detach(cThread);

		loadLen = sizeof(struct sockaddr_in);
	}

	return 0;
}
	
