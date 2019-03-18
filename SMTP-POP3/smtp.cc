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

#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <sys/wait.h>
#include <iostream>
#include <arpa/inet.h>
#include <signal.h>
#include <ctime>



using namespace std;

// GLOBAL VARIABLES
vector < pthread_t > THREADS;
vector < int > SOCKETS;
//
//pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; // global variable
//pthread_mutex_lock(&m); // start of Critical Section
//pthread_mutex_unlock(&m); //end of Critical Section

map < string , pthread_mutex_t > MBOX;

int LISTEN_SOCKET;
bool V_FLAG;
string DIRECTORY;
unsigned short PORT = 2500;
const char* DOMAIN = "localhost";




// COMMANDS
//HELO I AM <DOMAIN> (user, for identifying mailbox)
const char* HELO = "HELO";
const char* MAIL = "MAIL";
const char* RCPT = "RCPT";
const char* DATA = "DATA";
const char* RSET = "RSET";
const char* NOOP = "NOOP";
const char* QUIT = "QUIT";

// RESPONSES - SUCCESS
const char* HELO_SUCC = "250 localhost\r\n";
const char* CMD_SUCC = "250 OK\r\n";
const char* DATA_SUCC = "354 Start mail input; end with <CRLF>.<CRLF>\r\n";
const char* QUIT_SUCC = "221 localhost Service closing transmission channel\r\n";
const char* CONN_SUCC = "220 localhost Simple Mail Transfer Service Ready\r\n";

// RESPONSES - ERR
//const char* CONN_ERR = "421  Service not available, closing transmission channel\r\n";
const char* ARG_ERR = "501 Syntax error in parameters or arguments\r\n";
const char* OOO_ERR = "503 Bad sequence of commands\r\n";
const char* UNK_ERR = "500 Syntax error, command unrecognized\r\n";
const char* MAIL_ERR = "550 Requested action not taken: mailbox unavailable\r\n";

// META MSGS - SUCC
const char* NEW_CONN_SUCC = "New connection\r\n";
const char* CLOSE_CONN_SUCC =  "Connection closed\r\n";

const char* AUTH_SUCC = "*** Author: Gil Landau (glandau)\r\n";


// META MSGS - ERR
const char* CTRLC_ERR = "421 localhost Service not available, closing transmission channel\r\n";
const char* WRITE_ERR = "Write Err\r\n";
const char* PTHREAD_ERR = "ERR CREATING PTHREADS\r\n";
const char* PORT_ERR = "Invalid port\r\n";
const char* SETSOCKOPT_ERR = "setsockopt failed\r\n";
const char* SOCK_ERR = "Cannot open socket\r\n";
const char* MAIN_ARG_ERR = "Arguments error\r\n";






bool do_write (int fd, const char *buf, int len)
{
	int sent = 0;
	while (sent < len)
	{
		int n = write(fd, &buf[sent], len - sent);
		if (n < 0)
		{
			return false;
		}
		sent += n;
	}
	return true;
}

void handle_ctrlc(int signal)
{
// CLOSE
	for(int i =0; i < THREADS.size(); i++)
	{

		if (do_write(SOCKETS[i], CTRLC_ERR, strlen(CTRLC_ERR)) != false)
		{
			close(SOCKETS[i]);
			if (V_FLAG)
			{
				cerr << "[" << SOCKETS[i] << "] " << CLOSE_CONN_SUCC;
			}
			pthread_cancel(THREADS[i]);
		}

	}
	close(LISTEN_SOCKET);
	vector< int >().swap(SOCKETS);
	vector< pthread_t >().swap(THREADS);

}


void process_cmd(int comm_fd, char* cmd, int linelen, char* subbuff, bool &helo_rdy, bool &keep_reading, int &tran_state,  string  &data, set< string > &rcpt, string &snd){
	string buffs = string(subbuff);
	string cmds = string(cmd);
	//TODO REORDER

////////////////////////HELLO////////////////////////////////////////////////
	if(tran_state != 2){
		if ((strcasecmp(cmd, HELO)) == 0 && (subbuff[4] == ' ')){
			//CHECK IF HELO HAS ARG
			bool helo_arg = false;
			for(int i = 4; i < linelen; i++){
				if(subbuff[i] != ' ' && subbuff[i] != '\r' && subbuff[i] != '\n'){
					helo_arg = true;
					break;
				}
			}
			// CHECK SEQUENCE
			if (tran_state != 0){
				if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else if ((subbuff[5] == '\r' && subbuff[6] == '\n')  || !helo_arg){
				if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				helo_rdy = true;
				if (do_write(comm_fd, HELO_SUCC, strlen(HELO_SUCC)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: HELO\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << HELO_SUCC;
				}
			}
		}
	////////////////////RSET//////////////////////////////////////////////////////////
		else if((strcasecmp(cmd, RSET)) == 0 && (subbuff[4] == '\r' && subbuff[5] == '\n')){
			if(!helo_rdy){
				if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				data.clear();
				rcpt.clear();
				snd = "";
				tran_state = 0;

				if (do_write(comm_fd, CMD_SUCC, strlen(CMD_SUCC)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: RSET\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << CMD_SUCC;
				}
			}
		}
		/////////////////////////////NOOP////////////////////////////////////////////////////////////////////////////////
		else if ((strcasecmp(cmd, NOOP) == 0) && (subbuff[4] == '\r' && subbuff[5] == '\n')){
			// CHECK SEQUENCE
			if(!helo_rdy || tran_state != 0){
				if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				if (do_write(comm_fd, CMD_SUCC, strlen(CMD_SUCC)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: NOOP\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << CMD_SUCC;
				}
			}
		}
	/////////////////////////////////////QUIT////////////////////////////////////////////////////////////////////////
		// QUIT and not mid transaction
		else if ((strcasecmp(cmd, QUIT) == 0)  && (subbuff[4] == '\r' && subbuff[5] == '\n')){
			if (!helo_rdy){
				if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				if (do_write(comm_fd, QUIT_SUCC, strlen(QUIT_SUCC)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: QUIT\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << QUIT_SUCC;
				}
				keep_reading = false;

			}
		}

		///////////////////////// MAIL////////////////
		else if((strcasecmp(cmd, MAIL) == 0)){
			// check args
			int comp = -1;
			string ssubbuff = string(subbuff);
			int sub_len = ssubbuff.length();
			string prefix = "MAIL FROM:";
			int pre_len = prefix.length();
			int arg_len = 0;
			string arg = "";
			bool has_arg = false;
			if (pre_len<sub_len){
				string s_prefix = ssubbuff.substr(0,pre_len);
				comp = strcasecmp(s_prefix.c_str(), prefix.c_str());
			}
			if (comp != 0 ){
				if (do_write(comm_fd, UNK_ERR, strlen(UNK_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				if (!helo_rdy || tran_state != 0){
					if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
				}
				else{
					arg = ssubbuff.substr(pre_len, sub_len-3);
					arg_len = arg.length();

					if(arg.at(0) != '<' || arg.at(arg_len-3) != '>'){
						if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
							cerr << "[" << comm_fd << "]" << "Write Error\r\n";
						}
					}
					else{
						has_arg = true;
					}
				}
			}
			if(has_arg){
				string user ="";
				string dom = "";
				bool sawat = false;
				for(int i = 1; i < arg_len-3; i++){
					if(!sawat){
						if(arg.at(i) == '@'){
							sawat = true;
						}
						else{
							user = user + arg.at(i);
						}
					}
					else{
						dom = dom + arg.at(i);
					}
				}
				if(user.length() == 0 || dom.length() == 0){
					if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
				}
				else{
					for(int i = 0; i < arg_len-2; i++){
						snd = snd + arg.at(i);
					}
					tran_state = 1;
					if (do_write(comm_fd, CMD_SUCC, strlen(CMD_SUCC)) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
					if (V_FLAG){
						cerr << "[" << comm_fd << "]" << " C: " << ssubbuff;
						cerr << "[" << comm_fd << "]" << " S: " << CMD_SUCC;
					}
				}
			}
		}
///////////////////////////////// RCPT ///////////////////////
		else if((strcasecmp(cmd, RCPT) == 0)){

			string ssubbuff = string(subbuff);
			int sub_len = ssubbuff.length();
			string prefix = "RCPT TO:";
			int comp = -1;
			int pre_len = prefix.length();
			if (pre_len<sub_len){
				string s_prefix = ssubbuff.substr(0,pre_len);
				comp = strcasecmp(s_prefix.c_str(), prefix.c_str());
			}

			int arg_len = 0;
			string arg = "";
			bool has_arg = false;

			if (comp != 0 ){
				if (do_write(comm_fd, UNK_ERR, strlen(UNK_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				if (!helo_rdy || tran_state != 1){
					if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
				}
				else{
					arg = ssubbuff.substr(pre_len, sub_len-3);
					arg_len = arg.length();

					if(arg.at(0) != '<' || arg.at(arg_len-3) != '>'){
						if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
							cerr << "[" << comm_fd << "]" << "Write Error\r\n";
						}
					}
					else{
						has_arg = true;
					}
				}
			}
			if(has_arg){
				string user ="";
				string dom = "";
				bool sawat = false;
				for(int i = 1; i < arg_len-3; i++){
					if(!sawat){
						if(arg.at(i) == '@'){
							sawat = true;
						}
						else{
							user = user + arg.at(i);
						}
					}
					else{
						dom = dom + arg.at(i);
					}
				}
				if(user.length() == 0 || dom.length() == 0){
					if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
				}
				else if(dom.compare("localhost") != 0 || MBOX.find(user+".mbox")== MBOX.end()) {
						if (do_write(comm_fd, MAIL_ERR, strlen(MAIL_ERR)) == false && V_FLAG){
							cerr << "[" << comm_fd << "]" << "Write Error\r\n";
						}
				}
				else{
					rcpt.insert(user);
					if (do_write(comm_fd, CMD_SUCC, strlen(CMD_SUCC)) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
					if (V_FLAG){
						cerr << "[" << comm_fd << "]" << " C: " << ssubbuff;
						cerr << "[" << comm_fd << "]" << " S: " << CMD_SUCC;
					}
				}
			}
		}
		///////////////////////////////// DATA ////////////////////////////////
		else if ((strcasecmp(cmd, DATA) == 0)  && (subbuff[4] == '\r' && subbuff[5] == '\n')){
			if (!helo_rdy || tran_state != 1 || rcpt.size() == 0){
				if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				tran_state = 2;
				if (do_write(comm_fd, DATA_SUCC, strlen(DATA_SUCC)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: DATA\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << DATA_SUCC;
				}
			}
		}
		else{
			if (do_write(comm_fd, UNK_ERR, strlen(UNK_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
	}
	else{
		if(strcmp(subbuff,".\r\n") == 0){
			tran_state = 0;
			string sig = "";
			std::set< string >::iterator it;
			for (it = rcpt.begin(); it != rcpt.end(); ++it)
			{
			    string f = *it; // Note the "*" here
				fstream mbox;
				time_t t = time(0);
				tm* localtm = localtime(&t);
				string new_t = string(asctime(localtm));
				new_t= new_t.substr(0, new_t.size()-1);
				sig = "From " + snd + " " + new_t + "\r\n";
				string mbox_d = DIRECTORY + "/" + f+".mbox";
				//pthread_mutex_lock(&m); // start of Critical Section
				//pthread_mutex_unlock(&m); //end of Critical Section
				pthread_mutex_lock(&MBOX[f]);
				mbox.open(mbox_d.c_str(), ios_base::app);
				mbox << sig << data;
				mbox.close();
				pthread_mutex_unlock(&MBOX[f]);
			}
			data.clear();
			rcpt.clear();
			snd = "";
			tran_state = 0;
			if (do_write(comm_fd, CMD_SUCC, strlen(CMD_SUCC)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
			if (V_FLAG){
				cerr << "[" << comm_fd << "]" << " C: "<< sig << data;
				cerr << "[" << comm_fd << "]" << " S: " << CMD_SUCC;
			}

//			for(int i =0; i < rcpt.size(); i++){
//				fstream mbox;
//				time_t t = time(0);
//				tm* localtm = localtime(&t);
//				string new_t = string(asctime(localtm));
//				string sig = "From " + snd + " " + new_t;
//				//pthread_mutex_lock(&m); // start of Critical Section
//				//pthread_mutex_unlock(&m); //end of Critical Section
//				pthread_mutex_lock(&MBOX[rcpt[i]]);
//				mbox.open(DIRECTORY + "/" + rcpt[i]+".mbox", ios_base::app);
//				mbox << sig << data;
//				mbox.close();
//				pthread_mutex_unlock(&MBOX[rcpt[i]]);
//			}
		}
		else{
			string sub = string(subbuff);
			data = data + sub;
		}
	}
}

//WORKER handles META WORK (reading, clearing buffer): PROCESS COMMANDS handles command nonsense
void *worker(void *arg)
{
// SETUP ADMIN FOR THREADS
    int s = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (s != 0 && V_FLAG){
        cerr << "pthread_setcancelstate";
    }
	int comm_fd = *(int*)arg;

	if (do_write(comm_fd, CONN_SUCC, strlen(CONN_SUCC)) == false && V_FLAG){
		cerr << "[" << comm_fd << "]" << "Write Error\r\n";
	}
	if(V_FLAG){
		cerr << "[" << comm_fd << "] S: " << CONN_SUCC;
	}

	char buff[1100];
	char* buff_ptr = buff;
	int buff_size = 1100;
	int buff_free = 1100;
	bool keep_reading = true;
	bool helo_rdy = false;
	int tran_state = 0;

	string data = "";
	set < string > rcpt;
	string snd = "";

//	READ IN INPUTS
	while (keep_reading){
		// STORE LINE
		int rlen = read(comm_fd, &buff[buff_size-buff_free], buff_free);
		if (rlen > 0){
			buff_free -= rlen;
			char* linebreak ;
			while ((linebreak = strstr(buff, "\r\n")) != NULL){
				linebreak = linebreak + 2;
				int linelen = linebreak - buff_ptr;

	//			If the length of the line is shorter than any possible command + \r\n
				if (linelen < 6 && tran_state != 2){
					if (do_write(comm_fd, UNK_ERR, strlen(UNK_ERR)) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
				}
				else{
	//				0123456 7 8
	//				test a\r\nE
					char subbuff[linelen+1];
					char* subbuff_ptr = subbuff;
					strncpy( subbuff, &buff[0], linelen );
					subbuff[linelen] = '\0';

	//				PARSE
					char cmd[5];
					strncpy( cmd, &subbuff[0], 4 );
					cmd[5] = '\0';

					// PROCESS COMMANDS
					process_cmd(comm_fd, cmd, linelen, subbuff, helo_rdy, keep_reading, tran_state, data, rcpt, snd);

				}

				// REMOVE LINE
				int free_start = 0;
				for(int i = linelen; i < buff_size-buff_free; i++ ){
					buff[i-linelen] = buff[i];
					free_start++;
				}
				for(int i = free_start; i < buff_size; i++ ){
					buff[i] = '\0';

				}
				buff_free += linelen;
//
//				if(!keep_reading){
//					break;
//				}
			}
		}
	}
	close(comm_fd);
	if (V_FLAG){
		cerr << "[" << comm_fd << "] " << "Connection closed\r\n";
	}
    pthread_exit(0);
}




int main(int argc, char *argv[]){
	//	Default variables
	int c;
	int p;
	int opt_status;
	V_FLAG = false;
	int listen_socket;
	struct sigaction sigIntHandler;

//	CTRLC handler
	sigIntHandler.sa_handler = handle_ctrlc;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler,NULL);

// FLAG HANDLER
	if (argc < 2)
	{
		cerr << MAIN_ARG_ERR;
		exit(1);
	}
	else{
		while((c = getopt(argc, argv, "p:av")) != -1){
			switch(c){
				case 'p':
					PORT = atoi(optarg);
					if (PORT < 1){
						cerr << PORT_ERR;
						exit(1);
					}
					break;

				case 'a':
					cerr << AUTH_SUCC;
					exit(1);
					break;

				case 'v':
					V_FLAG = true;
					break;

				default:
					cerr << MAIN_ARG_ERR;
					exit(1);
			}
		}
	}

//		initilize socket and basic error check (exit 2 has to do with sockets
	listen_socket = socket(PF_INET, SOCK_STREAM,0);
	if(listen_socket < 0){
		if(V_FLAG){
			cerr << SOCK_ERR;
		}
		exit(2);
	}
	LISTEN_SOCKET = listen_socket;
	int enable = 1;
	opt_status = setsockopt(LISTEN_SOCKET, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	if (opt_status < 0 && V_FLAG){
	    cerr<< SETSOCKOPT_ERR;
	}

//		initialize bind
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(PORT);
	bind(listen_socket, (struct sockaddr*) &servaddr, sizeof(servaddr));

//		initialize listen
	listen(listen_socket, 120);

//	Initialize mailbox - get mailboxes
	if (optind == argc){
		cerr <<MAIN_ARG_ERR;
		exit(1);
	}

	DIRECTORY = string(argv[optind]);
	DIR* pdir;
	struct dirent* dp;

	if ((pdir = opendir(DIRECTORY.c_str())) != NULL ){
		if(V_FLAG){
			cerr << "OPENING THE DIRECTORY\r\n";
		}
	    while ((dp = readdir(pdir)) != NULL) {
	    	if(strlen(dp->d_name) > 2){
	    		MBOX[string(dp->d_name)] = PTHREAD_MUTEX_INITIALIZER;
	    	}
	    }
		closedir(pdir);
	}
	else{
		cerr<< "WRONG DIRECTORY\r\n";
		exit(1);
	}



//		initialize accept
	while (true){
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);

			//Accept
		int comm_fd = accept(listen_socket, (struct sockaddr*) &clientaddr, &clientaddrlen);
		if(comm_fd < 0){
			if(V_FLAG){
				cerr << SOCK_ERR;
			}
			exit(2);
		}
		SOCKETS.push_back(comm_fd);
		if(V_FLAG){
			cerr << "[" << comm_fd <<"] " << NEW_CONN_SUCC;
		}

//			Assign to threads
		pthread_t thread;
		THREADS.push_back(thread);
		p = pthread_create(&thread, NULL, worker, &comm_fd);
        if (p != 0 && V_FLAG){
            cerr << PTHREAD_ERR;
        }
	}
	exit(0);
}
