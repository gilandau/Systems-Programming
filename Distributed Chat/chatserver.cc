#include <stdlib.h>
#include <stdio.h>
#include <openssl/md5.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <sys/wait.h>
#include <iostream>
#include <arpa/inet.h>
#include <signal.h>
#include <ctime>

using namespace std;

// NUM CHATROOMS
const int NUM_CHATROOMS = 20;

// COMMANDS
const char* NICK = "/nick";
const char* JOIN = "/join";
const char* QUIT = "/quit";
const char* PART = "/part";

// ORDERINGS
// 0 unordered
// 1 fifo
// 2 total
// 3 causal
int MODE = 0;
const char* UNORDERED = "unordered"; //0
const char* FIFO = "fifo";//1
const char* CAUSAL = "causal"; //2
const char* TOTAL = "total";//3

//Total Order stages
string INIT = "INIT";
string PROP = "PROP";
string RESP = "RESP";



//SOCKETS AND META DATA
int LISTEN_SOCKET;
bool V_FLAG;
int INDEX;

//vector < sockaddr_in > CLIENTS;
//vector < string > CLIENTS;


// SRC_ID, (nick, chat room, sock object)
map < string, tuple < string, int, sockaddr_in > > USERS;

//vector < tuple < string, int > > USERS;
// Chatroom #, [srcids]
vector < set < string > > CHATROOMS;
vector < sockaddr_in > SERVERS;
map < string, int > SERVER2INDEX;

// ip and port for servers
sockaddr_in BIND;



//FIFO
//SEQ number for msg sent to group g on this node (node N)(SGN)
map<int, int > FIFO_SENT;
//SEQ number of the most recent message received from N for group g (RGN)
map <string, map< int, int >> FIFO_RECEIVED;
// holdback queue that contains messages received from N for group g
// map from user to sequence, room, message
// N -> G -> SEQ + MSG
map < string, map <int, map < int, string > > > FIFO_HOLDBACK;
//map < int, vector < string > > FIFO_HOLDBACK;


//TOTAL ORDERING
//Highest agreed for group g
vector < int > AGREED;
//Highest proposed for group g
vector < int > PROPOSED;
struct compare_msg {
	bool operator()(const tuple <int, int> msg1, const tuple <int, int>  msg2) const {
		// tie breaker
		if (get<0>(msg1) < get<0>(msg2) || (get<0>(msg1)  == get<0>(msg2)  && get<1>(msg1) < get<1>(msg2))) {
			return true;
		}
		else {
			return false;
		}
	}
};
// Priority QUEUE
// msg -> vecRoom -> (seq, server) -> deliver
map< int, map < tuple< int, int >, tuple< bool, string >, compare_msg >> TO_HOLDBACK;
// TS_msg -> room -> node -> prop
map < int, map < string ,map< int, int > > > TO_TS_RECEIVED;



// RESPONSES - SUCCESS
const char* CMD_SUCC = "+OK\n";
const char* PART_SUCC = "+OK You have left chat room ";
const char* ROOM_SUCC = "+OK You are now in chat room ";
const char* NICK_SUCC = "+OK Your name is changed to ";

// RESPONSES - ERR
const char* MAIN_ARG_ERR = "Arguments error";
const char* ARG_ERR = "-ERR Syntax error in parameters or arguments";
const char* ROOM_ERR = "-ERR Already in a room";
const char* NO_ROOM_ERR = "-ERR Room does not exist";
const char* PART_ERR = "-ERR You are not in a chat room";
const char* UNK_ERR = "-ERR Unknown command";
const char* MSG_ERR = "-ERR Not in room";

//DEBUG
string debug_prefix(){
	string prefix;
	char tbuf[256];
	struct timeval tv;
	struct timezone tz;
	struct tm *tm;
	gettimeofday(&tv, &tz);
	tm=localtime(&tv.tv_sec);
	sprintf (tbuf, "%02d:%02d:%02d:%06ld",tm->tm_hour, tm->tm_min, tm->tm_sec, (tv.tv_usec/1000) );
	string sbuf = "S" + to_string(INDEX) + " ";
	prefix = string(tbuf) + " " + sbuf;
	return prefix;
}

int from_server(string src_id){
	map<string,int>::iterator it = SERVER2INDEX.find(src_id);
	if(it != SERVER2INDEX.end())
	{
	   //element found;
	   return (it->second);
	}
	else{
		return -1;
	}
}



//TODO CHECK FOR NEWLINES?
void process_client_cmd(string src_id, string buff){
	string b = buff;
	if(buff.at(0) == '/'){
		if (b.length() >= 5){
			string cmd = b.substr(0, 5);
			if(strcmp(cmd.c_str(), QUIT) == 0  && b.length() == 5){

				int chat = get<1>(USERS[src_id]);
				if(chat != -1){
					CHATROOMS[chat-1].erase(src_id);
				}
				USERS.erase(src_id);
				string prefix = debug_prefix();
				if(V_FLAG){
					cout << prefix << "CLIENT " <<  get<0>(USERS[src_id])   << "QUITS: \"" << buff << "\"" << "\n";
				}

			}
			else if(strcmp(cmd.c_str(), NICK ) == 0 && b.length() > 6 && buff.at(5) == ' ' && buff.at(6) != '\n'){
				string nickname = b.substr(6, b.length()-6);
				tuple< string, int, sockaddr_in> new_nick = make_tuple(nickname, get<1>(USERS[src_id]) , get<2>(USERS[src_id]));
				USERS[src_id] = new_nick;
				string FINAL_SUCC = string(NICK_SUCC) + nickname;
				sendto(LISTEN_SOCKET, FINAL_SUCC.c_str(), strlen(FINAL_SUCC.c_str()), 0, (struct sockaddr*) &get<2>(USERS[src_id]), sizeof(get<2>(USERS[src_id])));
				string prefix = debug_prefix();
				if(V_FLAG){
					cout << prefix << "CLIENT " <<  get<0>(USERS[src_id])   << "sets nickname \"" << buff << "\"" << "\n";
				}
			}
			else if(strcmp(cmd.c_str(), JOIN) == 0 && b.length() > 6 && buff.at(5) == ' '){
				size_t found = b.find(" ");
				int chatroom = stoi(b.substr(found+1, b.length()-6));
				if(chatroom >20 || chatroom <= 0){
					sendto(LISTEN_SOCKET, NO_ROOM_ERR, strlen(NO_ROOM_ERR), 0, (struct sockaddr*) &get<2>(USERS[src_id]) , sizeof(get<2>(USERS[src_id])));
				}
				else if(get<1>(USERS[src_id]) != -1){
					sendto(LISTEN_SOCKET, ROOM_ERR, strlen(ROOM_ERR), 0, (struct sockaddr*) &get<2>(USERS[src_id]) , sizeof(get<2>(USERS[src_id])));

				}
				else{

					tuple< string, int, sockaddr_in> new_room = make_tuple(get<0>(USERS[src_id]), chatroom , get<2>(USERS[src_id]));
					USERS[src_id] = new_room;
					CHATROOMS[chatroom-1].insert(src_id);
					string FINAL_SUCC = string(ROOM_SUCC) + b.substr(found+1, b.length()-6);
					sendto(LISTEN_SOCKET, FINAL_SUCC.c_str(), strlen(FINAL_SUCC.c_str()), 0, (struct sockaddr*) &get<2>(USERS[src_id]) , sizeof(get<2>(USERS[src_id])));
					string prefix = debug_prefix();
					if(V_FLAG){
						cout << prefix << "CLIENT " <<  get<0>(USERS[src_id])   << "sets room \"" << buff << "\"" << "\n";
					}
				}

			}
			else if(strcmp(cmd.c_str(), PART ) == 0 && b.length() == 5){


				if(get<1>(USERS[src_id]) == -1){
					sendto(LISTEN_SOCKET, PART_ERR, strlen(PART_ERR), 0, (struct sockaddr*) &get<2>(USERS[src_id]) , sizeof(get<2>(USERS[src_id])));

				}
				else{
					int room = get<1>(USERS[src_id]);
					tuple< string, int, sockaddr_in> no_room = make_tuple(get<0>(USERS[src_id]), -1 , get<2>(USERS[src_id]));
					USERS[src_id] = no_room;
					CHATROOMS[room-1].erase(src_id);
					string FINAL_SUCC = string(PART_SUCC) + to_string(room);
					sendto(LISTEN_SOCKET, FINAL_SUCC.c_str(), strlen(FINAL_SUCC.c_str()), 0, (struct sockaddr*) &get<2>(USERS[src_id]) , sizeof(get<2>(USERS[src_id])));
					string prefix = debug_prefix();
					if(V_FLAG){
						cout << prefix << "CLIENT " <<  get<0>(USERS[src_id])   << "leaves room \"" << buff << "\"" << "\n";
					}
				}
			}
			//ERR
			else{
				sendto(LISTEN_SOCKET, UNK_ERR, strlen(UNK_ERR), 0, (struct sockaddr*) &get<2>(USERS[src_id]) , sizeof(get<2>(USERS[src_id])));
			}
		}
		//ERR
		else{
			sendto(LISTEN_SOCKET, UNK_ERR, strlen(UNK_ERR), 0, (struct sockaddr*) &get<2>(USERS[src_id]) , sizeof(get<2>(USERS[src_id])));
		}
	}
	//MESSAGE
	else{
		int room = get<1>(USERS[src_id]);
		if (room == -1){
			sendto(LISTEN_SOCKET, MSG_ERR, strlen(MSG_ERR), 0, (struct sockaddr*) &get<2>(USERS[src_id]) , sizeof(get<2>(USERS[src_id])));
		}
		else{
			string msg = "<" + get<0>(USERS[src_id]) + "> " + b.substr(0, b.length());

			//UNORDERED or FIFO
			if (MODE == 0 || MODE == 1){
				// send to clients
				set < string > current_room = CHATROOMS[room-1];
				set<string>::iterator it;
				for (it = current_room.begin(); it != current_room.end(); it++)
				{
					string usr = *it;
					sendto(LISTEN_SOCKET, msg.c_str(), strlen(msg.c_str()), 0, (struct sockaddr*) &get<2>(USERS[usr]) , sizeof(get<2>(USERS[usr])));
					string prefix = debug_prefix();
					if(V_FLAG){
						cout << prefix << "CLIENT " <<  get<0>(USERS[usr])  << "send \"" << buff << "\" to CHATROOM " << to_string(get<1>(USERS[usr])) << "and user "<< get<0>(USERS[usr]) <<"\n";
					}
				}
				//forward unordered msg
				if(MODE == 0){

					// forward message to other servers
					string unordered_msg = to_string(MODE)+","+ to_string(room) + "," + msg;
					for (int i = 0; i < SERVERS.size(); i++){
						if(i+1 != INDEX){
							sendto(LISTEN_SOCKET, unordered_msg.c_str(), strlen(unordered_msg.c_str()), 0, (struct sockaddr*) &SERVERS[i], sizeof(SERVERS[i]));
						}
						string prefix = debug_prefix();
						if(V_FLAG){
							cout << prefix << "SERVER " <<  INDEX  << "send \"" << unordered_msg << "\" to CHATROOM " << to_string(get<1>(USERS[src_id])) << "and SERVER "<< i <<"and an UNORDERED\n";
						}
					}
				}
				// forward FIFO ordered msg
				else{
					FIFO_SENT[room]++;
					string fifo_msg  = to_string(MODE) +","+  to_string(FIFO_SENT[room]) +"," + to_string(room) + "," + msg;
					for (int i = 0; i < SERVERS.size(); i++){
						if(i+1 != INDEX){
							sendto(LISTEN_SOCKET, fifo_msg.c_str(), strlen(fifo_msg.c_str()), 0, (struct sockaddr*) &SERVERS[i], sizeof(SERVERS[i]));
						}
						string prefix = debug_prefix();
						if(V_FLAG){
							cout << prefix << "SERVER " <<  INDEX  << "send \"" << fifo_msg << "\" to CHATROOM " << to_string(get<1>(USERS[src_id])) << "and SERVER "<< i <<" and a FIFO \n";
						}
					}

				}


			}
			else if(MODE == 2){


				string prefix = debug_prefix();
				string to_msg  = to_string(MODE) +","+ "INIT,"  +"-1," + to_string(INDEX) +"," + to_string(room) + "," + prefix + "$" +msg;

				map < int, int> nodeprop = map < int, int>();
				TO_TS_RECEIVED[room].insert({ to_msg,nodeprop});
				for (int i = 0; i < SERVERS.size(); i++){
					sendto(LISTEN_SOCKET, to_msg.c_str(), strlen(to_msg.c_str()), 0, (struct sockaddr*) &SERVERS[i], sizeof(SERVERS[i]));
					if(V_FLAG){
						cout << prefix << "SERVER " <<  INDEX  << "send INIT MSG \"" << to_msg << "\" to CHATROOM " << to_string(get<1>(USERS[src_id])) << "and SERVER "<< i <<" and a TO \n";
					}
				}
			}

			else{
				cerr << "Bad ordering argument\n";
				exit(5);
			}
		}

	}
}



//TODO FREE DATA
int main(int argc, char *argv[])
{
	//	Default variables
	int c;
	int p;
	int opt_status;
	V_FLAG = false;
	string config_file;

// PROCESS ARGS
	if (argc < 3) {
		cerr << "*** Author: Gil Landau (glandau)\n";
		exit(1);
	}
	while((c = getopt(argc, argv, "o:v")) != -1){
		switch(c){
		case 'o':
			if(strcmp(optarg, UNORDERED) == 0){
				MODE = 0;
			}
			else if(strcmp(optarg, FIFO) == 0){
				MODE = 1;
			}
			else if(strcmp(optarg, TOTAL) == 0){
				MODE = 2;
			}
			else if(strcmp(optarg, CAUSAL) == 0){
				MODE = 3;
			}
			else{
				cerr << "Bad ordering argument\n";
				exit(1);
			}
			break;

		case 'v':
			V_FLAG = true;
			break;

		default:
			cerr << MAIN_ARG_ERR;
			exit(1);
		}
	}
	config_file = string(argv[optind]);
	optind++;
	INDEX = atoi(argv[optind]);


	//	PROCESS CONFIG
	ifstream config;
	config.open(config_file.c_str());
	string line;
	int current_index = 1;
	//forward then bind
	while (getline(config,line)){
		string l = line;
		size_t found = l.find(",");
		string forward_addr;
		string bind_addr;
		// separate forward and bind
		if (found!=string::npos){
			forward_addr = l.substr(0, found);
			bind_addr = l.substr(found+1);

		}
		else{
			forward_addr = l;
			bind_addr = l;

		}
		if(forward_addr.length() == 0 || bind_addr.length() == 0){
			cerr <<" Bad config\n";
			exit(1);
		}

		int port;
		string ipaddr;

		struct sockaddr_in servaddr;
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		found = forward_addr.find(":");
		if (found!=string::npos){
			ipaddr = forward_addr.substr(0, found);
			string s_port = forward_addr.substr(found+1);
			if(ipaddr.length() == 0 || s_port.length() == 0 ){
				cerr <<" Bad argument\n";
				exit(1);
			}
			port = atoi(s_port.c_str());

		}

		inet_pton(AF_INET, ipaddr.c_str(), &servaddr.sin_addr);
		servaddr.sin_port = htons(port);
		SERVERS.push_back(servaddr);
		string server_id = string(inet_ntoa(SERVERS[current_index-1].sin_addr)) + ":" + to_string(SERVERS[current_index-1].sin_port);
		SERVER2INDEX.insert({server_id, current_index});

		if(current_index == INDEX){
			struct sockaddr_in servaddr;
			bzero(&servaddr, sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			size_t found = bind_addr.find(":");
			if (found!=string::npos){
				ipaddr = bind_addr.substr(0, found);
				string s_port = bind_addr.substr(found+1);
				if(ipaddr.length() == 0 || s_port.length() == 0 ){
					cerr <<" Bad argument\n";
					exit(1);
				}

				port = atoi(s_port.c_str());

			}

			inet_pton(AF_INET, ipaddr.c_str(), &servaddr.sin_addr);
			servaddr.sin_port = htons(port);
			BIND = servaddr;
		}				if(current_index == INDEX){
			struct sockaddr_in servaddr;
			bzero(&servaddr, sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			size_t found = bind_addr.find(":");
			if (found!=string::npos){
				ipaddr = bind_addr.substr(0, found);
				string s_port = bind_addr.substr(found+1);
				if(ipaddr.length() == 0 || s_port.length() == 0 ){
					cerr <<" Bad argument\n";
					exit(1);
				}

				port = atoi(s_port.c_str());

			}

			inet_pton(AF_INET, ipaddr.c_str(), &servaddr.sin_addr);
			servaddr.sin_port = htons(port);
			BIND = servaddr;
		}
		//INITIALIZE FIFO SGN AND RGN
		if(MODE ==1 ){
			map < int, int > RGN = map < int, int >();

			map <int, map< int, string > > group_holdback = map <int, map< int, string > >();
			//N -> G -> seq + msg
//			map < string, map <int, map < int, string > > > FIFO_HOLDBACK;

			FIFO_HOLDBACK.insert({server_id, group_holdback});
			FIFO_RECEIVED.insert({server_id, RGN});
			for(int i = 0; i < NUM_CHATROOMS; i++){
				map< int, string > seqmsg = map < int, string >();
				FIFO_HOLDBACK[server_id].insert({i+1, seqmsg});
				FIFO_SENT.insert({i+1,0});
				FIFO_RECEIVED[server_id].insert({i+1,0});
			}

		}
		current_index++;
	}
	BIND.sin_addr.s_addr = htons(INADDR_ANY);
	int listen_socket = socket(PF_INET, SOCK_DGRAM, 0);
	LISTEN_SOCKET = listen_socket;
	bind(listen_socket, (struct sockaddr*) &BIND, sizeof(BIND));


	//	INITIALIZING TO


	for(int i = 0; i < NUM_CHATROOMS; i++){
		CHATROOMS.push_back(set < string >());
		if(MODE ==2){
			AGREED.push_back(0);
			PROPOSED.push_back(0);



			map < tuple< int, int >, tuple<bool, string>, compare_msg > tohold = map < tuple< int, int >, tuple<bool, string>, compare_msg > ();
			map < string ,map< int, int > > totshold = map < string ,map< int, int > > ();
			TO_HOLDBACK.insert({i+1, tohold});
			TO_TS_RECEIVED.insert({i+1, totshold});
		}
//		map < int, string> seq2msg = map <int, string>();
	}



	while (true){
		struct sockaddr_in src;
		socklen_t srclen = sizeof(src);
		char bu[1100];
		int rlen = recvfrom(listen_socket, bu, sizeof(bu)-1, 0, (struct sockaddr*) &src, &srclen);
		bu[rlen] = 0;
		string prefix = debug_prefix();
		string src_id = string(inet_ntoa(src.sin_addr)) + ":" + to_string(src.sin_port);
		string buff = string(bu);
		size_t found = buff.find("\n");
		if (found!=string::npos){
			buff = buff.substr(0, found);
		}




		// If message rcved from server or client
		int src_index = from_server(src_id);
		if(src_index != -1){
			if(V_FLAG){
				cout << prefix << "SERVER " <<  to_string(src_index)   << "sends \"" << buff << "\"\n";
			}
			string server_msg = buff;
			size_t found = server_msg.find(",");
			int msg_mode = stoi(server_msg.substr(0, found));
			string unmoded_msg = server_msg.substr(found+1);
				//UNORDERED
			if(msg_mode == 0){

				size_t found = unmoded_msg.find(",");
				int room = stoi(unmoded_msg.substr(0, found));
				string msg = unmoded_msg.substr(found+1);
				set < string > current_room = CHATROOMS[room-1];
				set<string>::iterator it;
				for (it = current_room.begin(); it != current_room.end(); it++){
					string usr = *it;
					sendto(LISTEN_SOCKET, msg.c_str(), strlen(msg.c_str()), 0, (struct sockaddr*) &get<2>(USERS[usr]) , sizeof(get<2>(USERS[usr])));
					string prefix = debug_prefix();
					if(V_FLAG){
						cout << prefix << "CLIENT " <<  get<0>(USERS[src_id])  << "send \"" << buff << "\" to CHATROOM " << to_string(get<1>(USERS[src_id])) << "and user "<< get<0>(USERS[usr]) <<"\n";
					}
				}
			}
			else if(msg_mode ==1){

				vector < string > tokens = vector < string >();
				size_t pos = 0;
				for (int z = 0; z < 2; z++) {
					pos = unmoded_msg.find(",");
					if (pos != string::npos){
						tokens.push_back(unmoded_msg.substr(0, pos));
						unmoded_msg.erase(0, pos + 1);
					}
				}

				int seq= stoi(tokens[0]);
				int room = stoi(tokens[1]);
				string msg = unmoded_msg;


				set < string > current_room = CHATROOMS[room-1];
				// if right FIFO seq
				if(seq == (FIFO_RECEIVED[src_id][room])+1){
					FIFO_RECEIVED[src_id][room]++;
					//deliver
					set<string>::iterator it;
					for (it = current_room.begin(); it != current_room.end(); it++){
						string usr = *it;
						sendto(LISTEN_SOCKET, msg.c_str(), strlen(msg.c_str()), 0, (struct sockaddr*) &get<2>(USERS[usr]) , sizeof(get<2>(USERS[usr])));
						string prefix = debug_prefix();
						if(V_FLAG){
							cout << prefix << "CLIENT " <<  get<0>(USERS[src_id])  << "send \"" << buff << "\" to CHATROOM " << to_string(get<1>(USERS[src_id])) << "and user "<< get<0>(USERS[usr]) <<"\n";
						}
					}

					int current_seq = seq+1;
					//check holdback
					while(FIFO_HOLDBACK[src_id][room].find(current_seq) != FIFO_HOLDBACK[src_id][room].end()){
						FIFO_RECEIVED[src_id][room]++;
						//deliver
						set<string>::iterator it;
						for (it = current_room.begin(); it != current_room.end(); it++){
							string usr = *it;
							string holdback_msg = FIFO_HOLDBACK[src_id][room][current_seq];
							sendto(LISTEN_SOCKET, holdback_msg.c_str(), strlen(holdback_msg.c_str()), 0, (struct sockaddr*) &get<2>(USERS[usr]) , sizeof(get<2>(USERS[usr])));
							string prefix = debug_prefix();
							if(V_FLAG){
								cout << prefix << "CLIENT " <<  get<0>(USERS[src_id])  << "send \"" << buff << "\" to CHATROOM " << to_string(get<1>(USERS[src_id])) << "and user "<< get<0>(USERS[usr]) <<"\n";
							}
						}
						FIFO_HOLDBACK[src_id][room].erase(current_seq);
						current_seq++;
					}

				}
				//if wrong FIFO seq
				else{
					FIFO_HOLDBACK[src_id][room].insert({seq, msg});
				}

			}
			else if(msg_mode == 2){
				vector < string > tokens = vector < string >();
				size_t pos = 0;
				for (int z = 0; z < 4; z++) {
					pos = unmoded_msg.find(",");
					if (pos != string::npos){
						tokens.push_back(unmoded_msg.substr(0, pos));
						unmoded_msg.erase(0, pos + 1);
					}
				}

				string prop_type= tokens[0];
				int seq= stoi(tokens[1]);
				int sender = stoi(tokens[2]);
				int room = stoi(tokens[3]);
				string msg = unmoded_msg;

				if(prop_type.compare(INIT) == 0){


					PROPOSED[room-1]= max(AGREED[room-1], PROPOSED[room-1])+1;
					tuple < int, int> serverseq = make_tuple(PROPOSED[room-1], INDEX);
					tuple < bool, string> delivmsg = make_tuple(false, msg);

					TO_HOLDBACK[room].insert({serverseq, delivmsg});

					string to_msg  = to_string(msg_mode) +","+ "PROP,"  + to_string(PROPOSED[room-1]) + "," + to_string(INDEX) + "," + to_string(room) + "," + msg;
					sendto(LISTEN_SOCKET, to_msg.c_str(), strlen(to_msg.c_str()), 0, (struct sockaddr*) &SERVERS[sender-1], sizeof(SERVERS[sender-1]));
					string prefix = debug_prefix();
					if(V_FLAG){
						cout << prefix << "SERVER " <<  INDEX  << "send PROP MSG \"" << to_msg << "\" to CHATROOM " << to_string(room) << "and SERVER "<< to_string(sender) <<" and a TO \n";
					}

				}
				else if(prop_type.compare(PROP)==0){




					// Current proposals received per room:   room -> seq -> (server -> deliverable)
					TO_TS_RECEIVED[room][msg].insert({sender, seq});

					// IF ALL PROPOSALS ARE IN
					if (TO_TS_RECEIVED[room][msg].size() == SERVERS.size()) {
						int prop_max = 0;
						int max_idx = 0;

						for (int i = 0; i < SERVERS.size(); i++) {

							if ( TO_TS_RECEIVED[room][msg][i+1] > prop_max) {
								prop_max =  TO_TS_RECEIVED[room][msg][i+1];
								max_idx = i+1;
							}

							TO_TS_RECEIVED[room][msg].erase(i+1);

						}
						if(TO_TS_RECEIVED[room][msg].size() == 0){
							TO_TS_RECEIVED[room].erase(msg);
						}
						string to_msg  = to_string(msg_mode) +","+ "RESP,"  + to_string(prop_max) + "," + to_string(max_idx) + ","+ to_string(room) + "," + msg;
						for (int i = 0; i < SERVERS.size(); i++){
							sendto(LISTEN_SOCKET, to_msg.c_str(), strlen(to_msg.c_str()), 0, (struct sockaddr*) &SERVERS[i], sizeof(SERVERS[i]));
							if(V_FLAG){
								cout << prefix << "SERVER " <<  INDEX  << "send RESP MSG \"" << to_msg << "\" to CHATROOM " << to_string(get<1>(USERS[src_id])) << "and SERVER "<< i <<" and a TO \n";
							}
						}
					}
				}
				else if(prop_type.compare(RESP) == 0 ){
					for (auto iterator : TO_HOLDBACK[room]) {
						if (get<1>(iterator.second).compare(msg) == 0) {
							TO_HOLDBACK[room].erase(iterator.first);
							pos = msg.find("$");
							string unts_msg = msg.substr(pos+1);
							tuple < int, int> serverseq = make_tuple(seq, sender);
							tuple < bool, string> delivmsg = make_tuple(true, unts_msg);
							TO_HOLDBACK[room].insert({serverseq, delivmsg});
						}
					}

					AGREED[room-1] = max(AGREED[room-1], seq);

					// DELIVERY
					while (TO_HOLDBACK[room].size() >0 && get<0>(TO_HOLDBACK[room].begin()->second)) {

						string to_msg = get<1>(TO_HOLDBACK[room].begin()->second);

						// send to clients
						set < string > current_room = CHATROOMS[room-1];
						set<string>::iterator it;
						for (it = current_room.begin(); it != current_room.end(); it++)
						{
							string usr = *it;

							sendto(LISTEN_SOCKET, to_msg.c_str(), strlen(to_msg.c_str()), 0, (struct sockaddr*) &get<2>(USERS[usr]) , sizeof(get<2>(USERS[usr])));
							string prefix = debug_prefix();
							if(V_FLAG){
								cout << prefix << "CLIENT " <<  get<0>(USERS[usr])  << "send \"" << buff << "\" to CHATROOM " << to_string(get<1>(USERS[usr])) << "and user "<< get<0>(USERS[usr]) <<"\n";
							}
						}
						TO_HOLDBACK[room].erase(TO_HOLDBACK[room].begin());
					}


				}
			}
			else{
				cerr << "Bad ordering argument\n";
				exit(1);
			}
		}
		else{
			if(USERS.find(src_id)== USERS.end()){
				tuple < string, int, sockaddr_in > new_user = make_tuple(src_id, -1, src);
	    		USERS.insert({src_id, new_user});

			}
			if(V_FLAG){
				cout << prefix << "CLIENT " <<  get<0>(USERS[src_id])   << "posts \"" << buff << "\" to CHATROOM " << get<1>(USERS[src_id])  << "\n";
			}
			process_client_cmd(src_id, buff);

		}

	}

	return 0;
}  
