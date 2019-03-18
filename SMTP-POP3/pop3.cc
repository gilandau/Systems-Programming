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
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <sys/wait.h>
#include <iostream>
#include <arpa/inet.h>
#include <signal.h>
#include <ctime>

using namespace std;

// COMMANDS
//HELO I AM <DOMAIN> (user, for identifying mailbox)
const char* USER = "USER";
const char* PASS = "PASS";
const char* STAT = "STAT";
const char* UIDL = "UIDL";
const char* RETR = "RETR";
const char* DELE = "DELE";
const char* QUIT = "QUIT";
const char* LIST = "LIST";
const char* RSET = "RSET";
const char* NOOP = "NOOP";

// RESPONSES - SUCCESS
const char* CMD_SUCC = "+OK\r\n";
const char* QUIT_SUCC = "+OK POP3 server signing off\r\n";
const char* CONN_SUCC = "+OK POP3 ready [localhost]\r\n";
const char* PASS_SUCC = "+OK maildrop locked and ready\r\n";
const char* USER_SUCC = "+OK name is a valid mailbox\r\n";
// RESPONSES - ERR
const char* ARG_ERR = "-ERR Syntax error in parameters or arguments\r\n";
const char* OOO_ERR = "-ERR Bad sequence of commands\r\n";
const char* UNK_ERR = "-ERR Not supported\r\n";
const char* LIST_ERR = "-ERR no such message\r\n";
const char* PASS_ERR = "-ERR invalid password\r\n";
const char* MSG_ERR = "-ERR no such message\r\n";
const char* USER_ERR = "-ERR never heard of mailbox name\r\n";

// META MSGS - SUCC
const char* NEW_CONN_SUCC = "New connection\r\n";
const char* CLOSE_CONN_SUCC =  "Connection closed\r\n";

const char* AUTH_SUCC = "*** Author: Gil Landau (glandau)\r\n";


// META MSGS - ERR
const char* CTRLC_ERR = "-ERR Server shutting down\r\n";
const char* WRITE_ERR = "Write Err\r\n";
const char* PTHREAD_ERR = "ERR CREATING PTHREADS\r\n";
const char* PORT_ERR = "Invalid port\r\n";
const char* SETSOCKOPT_ERR = "setsockopt failed\r\n";
const char* SOCK_ERR = "Cannot open socket\r\n";
const char* MAIN_ARG_ERR = "Arguments error\r\n";



// GLOBAL VARIABLES
vector < pthread_t > THREADS;
vector < int > SOCKETS;
map < string , pthread_mutex_t > MBOX;

//
//pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; // global variable
//pthread_mutex_lock(&m); // start of Critical Section
//pthread_mutex_unlock(&m); //end of Critical Section

int LISTEN_SOCKET;
bool V_FLAG;
string DIRECTORY;
unsigned short PORT = 11000;
const char* DOMAIN = "localhost";


void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer)
{
  /* The digest will be written to digestBuffer, which must be at least MD5_DIGEST_LENGTH bytes long */

  MD5_CTX c;
  MD5_Init(&c);
  MD5_Update(&c, data, dataLengthBytes);
  MD5_Final(digestBuffer, &c);
}


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
	for (map< string, pthread_mutex_t >::iterator it=MBOX.begin(); it!=MBOX.end(); ++it){
		 pthread_mutex_unlock(&(it->second));
	 }


	close(LISTEN_SOCKET);
	vector< int >().swap(SOCKETS);
	vector< pthread_t >().swap(THREADS);

}

void process_cmd(int comm_fd, char* cmd, int linelen, char* subbuff, bool &keep_reading, int &tran_state, vector < string >  &hdr, vector < string >  &msg, vector< bool > &del_msg, string &user, bool &d_flag){
	string buffs = string(subbuff);
	string cmds = string(cmd);
	//TODO REORDER

////////////////////////USER////////////////////////////////////////////////
	if ((strcasecmp(cmd, USER)) == 0 && (subbuff[4] == ' ')){

		string ssubbuff = string(subbuff);
		int sub_len = ssubbuff.length();
		string prefix = "USER ";
		int pre_len = prefix.length();
		int arg_len = 0;
		string arg = "";
		bool has_arg = false;
		if (tran_state != 0){
			if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
		else{
			arg = ssubbuff.substr(pre_len, sub_len-3);
			arg_len = arg.length();
			arg = arg.substr(0, arg_len-2);
			if(arg_len < 1){
				if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				has_arg = true;
			}
		}
		if(has_arg){
			arg = arg + ".mbox";
			if(MBOX.find(arg)== MBOX.end()){
				if (do_write(comm_fd, USER_ERR, strlen(USER_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}

			}
			else{
				user = arg;
				if (do_write(comm_fd, USER_SUCC, strlen(USER_SUCC)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: USER\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << USER_SUCC;
				}
			}
		}
	}
	////////////// NOOOP//////////////
	else if ((strcasecmp(cmd, NOOP) == 0) && (subbuff[4] == '\r' && subbuff[5] == '\n')){
		if (tran_state != 1){
			if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
		else{
			// CHECK SEQUENCE
			if (do_write(comm_fd, CMD_SUCC, strlen(CMD_SUCC)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
			if (V_FLAG){
				cerr << "[" << comm_fd << "]" << " C: NOOP\r\n";
				cerr << "[" << comm_fd << "]" << " S: " << CMD_SUCC;
			}
		}
	}
	////////// PASS /////////
	else if((strcasecmp(cmd, PASS) == 0)&& (subbuff[4] == ' ')){
		string ssubbuff = string(subbuff);
		int sub_len = ssubbuff.length();
		string prefix = "PASS ";
		int pre_len = prefix.length();
		int arg_len = 0;
		string arg = "";
		bool has_arg = false;
		if (tran_state != 0 || user.length() == 0){
			if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
		else{
			arg = ssubbuff.substr(pre_len, sub_len-3);
			arg_len = arg.length();
			arg = arg.substr(0, arg_len-2);
			if(arg_len < 1){
				if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				has_arg = true;
			}
		}
		if(has_arg){
			if(arg.compare("cis505") != 0){
				user = "";
				if (do_write(comm_fd, PASS_ERR, strlen(PASS_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}

			}
			else{
				string mbox_d = DIRECTORY + "/" + user;

				pthread_mutex_lock(&MBOX[user]);
				tran_state = 1;
				string prefix = "From ";
				ifstream mbox;
				mbox.open(mbox_d.c_str(), ios_base::app);
				string line;
				string current_msg = "";

				while (getline(mbox,line)){
					if(line.substr(0,5).compare(prefix) == 0){
						string l = line;
						hdr.push_back(l);
						if(current_msg.length() != 0){
							del_msg.push_back(false);
							msg.push_back(current_msg);
						}
						current_msg = "";
					}
					else{
						string s = line;
						s += "\n";
						current_msg += s;
					}
				}
				if (current_msg.length() != 0) {
					del_msg.push_back(false);
					msg.push_back(current_msg);
				}
				mbox.close();

				if (do_write(comm_fd, PASS_SUCC, strlen(PASS_SUCC)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: PASS\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << PASS_SUCC;
				}

			}
		}
	}

/////////////////////STAT//////////////
	else if((strcasecmp(cmd, STAT) == 0)&& (subbuff[4] == '\r' && subbuff[5] == '\n')){
		if(tran_state != 1){
			if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
		else{
			string STAT_SUCC = "+OK";
			int msg_count = 0;
			int octet_count = 0;

			for(int i = 0; i < msg.size(); i++){
				if (!del_msg[i]){
					msg_count++;
					octet_count += msg[i].length();
				}
			}
			STAT_SUCC = STAT_SUCC + " " + to_string(msg_count) + " " +  to_string(octet_count) + "\r\n";
			if (do_write(comm_fd, STAT_SUCC.c_str(), strlen(STAT_SUCC.c_str())) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
			if (V_FLAG){
				cerr << "[" << comm_fd << "]" << " C: STAT\r\n";
				cerr << "[" << comm_fd << "]" << " S: " << STAT_SUCC;
			}
		}
	}
	/////////////// LIST //////////////////
	else if((strcasecmp(cmd, LIST) == 0)){
		if(tran_state != 1){
			if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
		else{
			if((subbuff[4] == '\r' && subbuff[5] == '\n')){
				string LIST_SUCC = "+OK ";
				int msg_count = 0;
				int octet_count = 0;
				vector < string > succ_msg;
				for(int i = 0; i < msg.size(); i++ ){
					if (!del_msg[i]){
						msg_count++;
						octet_count += msg[i].length();
						string list_msg = to_string(i+1) + " " + to_string(msg[i].length()) + "\r\n";
						succ_msg.push_back(list_msg);
					}
				}
				LIST_SUCC = LIST_SUCC + " " + to_string(msg_count) + " messages (" +  to_string(octet_count) + " octets)\r\n";
				if (do_write(comm_fd,  LIST_SUCC.c_str(), strlen(LIST_SUCC.c_str())) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: LIST\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << LIST_SUCC;
				}


				for(int i = 0; i < succ_msg.size(); i++){
					if (do_write(comm_fd,  succ_msg[i].c_str(), strlen(succ_msg[i].c_str())) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
					if (V_FLAG){
						cerr << "[" << comm_fd << "]" << " S: " << succ_msg[i].c_str();
					}
				}
				string dot = ".\r\n";
				if (do_write(comm_fd,  dot.c_str(), strlen(dot.c_str())) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				string ssubbuff = string(subbuff);
				int sub_len = ssubbuff.length();
				string prefix = "LIST ";
				int pre_len = prefix.length();
				int arg_len = 0;
				string arg = "";
				bool has_arg = false;
				arg = ssubbuff.substr(pre_len, sub_len-3);
				arg_len = arg.length();
				arg = arg.substr(0, arg_len-2);
				if(arg_len < 1){
					if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
				}
				else{
					has_arg = true;
				}
				if(has_arg){
					int msg_i = stoi(arg)-1;
					if(msg_i < 0 || msg_i > msg.size()-1 || del_msg.size() == 0 || del_msg[msg_i]){
						if (do_write(comm_fd, MSG_ERR, strlen(MSG_ERR)) == false && V_FLAG){
							cerr << "[" << comm_fd << "]" << "Write Error\r\n";
						}
					}
					else{
						string LIST_SUCC = "+OK " + to_string(msg_i+1) + " " + to_string(msg[msg_i].length()) + "\r\n";
						if (do_write(comm_fd,  LIST_SUCC.c_str(), strlen(LIST_SUCC.c_str())) == false && V_FLAG){
							cerr << "[" << comm_fd << "]" << "Write Error\r\n";
						}
						if (V_FLAG){
							cerr << "[" << comm_fd << "]" << " C: LIST\r\n";
							cerr << "[" << comm_fd << "]" << " S: " << LIST_SUCC;
						}
					}
				}
			}
		}
	}

	/// UIDL

	else if((strcasecmp(cmd, UIDL) == 0)){
		if(tran_state != 1){
			if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
		else{
			if((subbuff[4] == '\r' && subbuff[5] == '\n')){
				string UIDL_SUCC = "+OK\r\n";
				int msg_count = 0;
				int octet_count = 0;
				vector < string > succ_msg;
				for(int i = 0; i < msg.size(); i++ ){
					if (!del_msg[i]){
						string data_msg = hdr[i] + "\n" + msg[i];
						int data_len = data_msg.length();
						unsigned char* digest_buff = (unsigned char*) malloc(MD5_DIGEST_LENGTH) ;

						computeDigest((char*)data_msg.c_str(), data_len, digest_buff);
						char buf[MD5_DIGEST_LENGTH *2];
						for (int j=0;j<MD5_DIGEST_LENGTH;j++){
						    sprintf(buf+j, "%02x", digest_buff[j]);
						}


						string uidl_msg = to_string(i+1) + " " +  string(buf) + "\r\n";
						succ_msg.push_back(uidl_msg);
					}
				}
				if (do_write(comm_fd,  UIDL_SUCC.c_str(), strlen(UIDL_SUCC.c_str())) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: UIDL\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << UIDL_SUCC;
				}


				for(int i = 0; i < succ_msg.size(); i++){
					if (do_write(comm_fd,  succ_msg[i].c_str(), strlen(succ_msg[i].c_str())) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
					if (V_FLAG){
						cerr << "[" << comm_fd << "]" << " S: " << succ_msg[i].c_str();
					}
				}
				string dot = ".\r\n";
				if (do_write(comm_fd,  dot.c_str(), strlen(dot.c_str())) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}


			}
			else{
				string ssubbuff = string(subbuff);
				int sub_len = ssubbuff.length();
				string prefix = "UIDL ";
				int pre_len = prefix.length();
				int arg_len = 0;
				string arg = "";
				bool has_arg = false;
				arg = ssubbuff.substr(pre_len, sub_len-3);
				arg_len = arg.length();
				arg = arg.substr(0, arg_len-2);

				if(arg_len < 1){
					if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
						cerr << "[" << comm_fd << "]" << "Write Error\r\n";
					}
				}
				else{
					has_arg = true;
				}
				if(has_arg){
					int msg_i = stoi(arg)-1;
					if(msg_i < 0 || msg_i > msg.size()-1 || del_msg.size() == 0 || del_msg[msg_i]){
						if (do_write(comm_fd, MSG_ERR, strlen(MSG_ERR)) == false && V_FLAG){
							cerr << "[" << comm_fd << "]" << "Write Error\r\n";
						}
					}
					else{
						string data_msg = hdr[msg_i] + "\n" + msg[msg_i];
						int data_len = data_msg.length();
						unsigned char* digest_buff = (unsigned char*) malloc(MD5_DIGEST_LENGTH) ;

						computeDigest((char*)data_msg.c_str(), data_len, digest_buff);

						char buf[MD5_DIGEST_LENGTH *2];
						for (int j=0;j<MD5_DIGEST_LENGTH;j++){
						    sprintf(buf+j, "%02x", digest_buff[j]);
						}
						string UIDL_SUCC = "+OK " + to_string(msg_i+1) + " " + string(buf) + "\r\n";


						if (do_write(comm_fd,  UIDL_SUCC.c_str(), strlen(UIDL_SUCC.c_str())) == false && V_FLAG){
							cerr << "[" << comm_fd << "]" << "Write Error\r\n";
						}
						if (V_FLAG){
							cerr << "[" << comm_fd << "]" << " C: UIDL\r\n";
							cerr << "[" << comm_fd << "]" << " S: " << UIDL_SUCC;
						}
						free(digest_buff);
					}
				}
			}
		}
	}

	//////////// RSET//////////////////////
	else if((strcasecmp(cmd, RSET) == 0) && (subbuff[4] == '\r' && subbuff[5] == '\n')){
		if(tran_state != 1){
			if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
		else{
			d_flag = false;
			for(int i = 0; i < del_msg.size(); i++){
				del_msg[i] = false;
			}

			if (do_write(comm_fd, CMD_SUCC, strlen(CMD_SUCC)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
			if (V_FLAG){
				cerr << "[" << comm_fd << "]" << " C: RSET\r\n";
				cerr << "[" << comm_fd << "]" << " S: " << CMD_SUCC;
			}
		}
	}

///////////////DELE////////////////
	else if((strcasecmp(cmd, DELE) == 0) && (subbuff[4] == ' ')){
		string ssubbuff = string(subbuff);
		int sub_len = ssubbuff.length();
		string prefix = "DELE ";
		int pre_len = prefix.length();
		int arg_len = 0;
		string arg = "";
		bool has_arg = false;

		if(tran_state != 1){

			if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
		else{

			arg = ssubbuff.substr(pre_len, sub_len-3);
			arg_len = arg.length();
			arg = arg.substr(0, arg_len-2);

			if(arg_len < 1){

				if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				has_arg = true;
			}
		}
		if(has_arg){
			int msg_i = stoi(arg)-1;
			if(msg_i < 0 || msg_i > del_msg.size()-1 || del_msg.size() == 0){
				if (do_write(comm_fd, MSG_ERR, strlen(MSG_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else if(del_msg[msg_i]){
				string DELE_ERR = "-ERR message " +  arg + " already deleted\r\n";
				if (do_write(comm_fd, DELE_ERR.c_str(), strlen(DELE_ERR.c_str())) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				del_msg[msg_i] = true;
				d_flag = true;
				string DELE_SUCC = "+OK message " + arg + " deleted\r\n";
				if (do_write(comm_fd, DELE_SUCC.c_str(), strlen(DELE_SUCC.c_str())) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: DELE\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << DELE_SUCC;
				}
			}
		}
	}
	////////// RETR /////////
	else if((strcasecmp(cmd, RETR) == 0)&& (subbuff[4] == ' ')){
		string ssubbuff = string(subbuff);
		int sub_len = ssubbuff.length();
		string prefix = "PASS ";
		int pre_len = prefix.length();
		int arg_len = 0;
		string arg = "";
		bool has_arg = false;

		if (tran_state != 1){
			if (do_write(comm_fd, OOO_ERR, strlen(OOO_ERR)) == false && V_FLAG){
				cerr << "[" << comm_fd << "]" << "Write Error\r\n";
			}
		}
		else{
			arg = ssubbuff.substr(pre_len, sub_len-3);
			arg_len = arg.length();
			arg = arg.substr(0, arg_len-2);

			if(arg_len < 1){
				if (do_write(comm_fd, ARG_ERR, strlen(ARG_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				has_arg = true;
			}
		}
		if(has_arg){
			int msg_i = stoi(arg)-1;
			if(msg_i < 0 || msg_i > msg.size()-1 || del_msg.size() == 0 || del_msg[msg_i]){
				if (do_write(comm_fd, MSG_ERR, strlen(MSG_ERR)) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
			}
			else{
				string RETR_SUCC = "+OK " + to_string(msg[msg_i].length()) + " octets\r\n";
				if (do_write(comm_fd,  RETR_SUCC.c_str(), strlen(RETR_SUCC.c_str())) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}
				if (V_FLAG){
					cerr << "[" << comm_fd << "]" << " C: RETR\r\n";
					cerr << "[" << comm_fd << "]" << " S: " << RETR_SUCC;
				}
				if (do_write(comm_fd,  msg[msg_i].c_str(), strlen(msg[msg_i].c_str())) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}


				string dot = ".\r\n";
				if (do_write(comm_fd,  dot.c_str(), strlen(dot.c_str())) == false && V_FLAG){
					cerr << "[" << comm_fd << "]" << "Write Error\r\n";
				}

			}
		}
	}
	////////// QUIT /////////
	else if((strcasecmp(cmd, QUIT) == 0)&&(subbuff[4] == '\r' && subbuff[5] == '\n')){
		string mbox_d = DIRECTORY + "/" + user;
		string mbox_d_temp = DIRECTORY + "/" + user + "_temp";
//		if correct state

		if(d_flag){
			ofstream mbox_temp;
			mbox_temp.open(mbox_d_temp.c_str(), ios::app );
			for(int i = 0; i < msg.size(); i++){
				if(!del_msg[i]){
					string data_msg = hdr[i] + "\n" + msg[i];
					mbox_temp << data_msg;

				}
			}

			mbox_temp.close();
			remove(mbox_d.c_str());
			rename(mbox_d_temp.c_str(), mbox_d.c_str());
		}
		///////////

		keep_reading = false;
		if (do_write(comm_fd, CMD_SUCC, strlen(CMD_SUCC)) == false && V_FLAG){
			cerr << "[" << comm_fd << "]" << "Write Error\r\n";
		}
		if (V_FLAG){
			cerr << "[" << comm_fd << "]" << " C: QUIT\r\n";
			cerr << "[" << comm_fd << "]" << " S: " << CMD_SUCC;
		}
		pthread_mutex_unlock(&MBOX[user]);

	}
	else{
		if (do_write(comm_fd, UNK_ERR, strlen(UNK_ERR)) == false && V_FLAG){
			cerr << "[" << comm_fd << "]" << "Write Error\r\n";
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
	bool d_flag = false;
	int tran_state = 0;

	vector < string > msg;
	vector < string >  hdr;
	vector < bool > del_msg;
	string user = "";

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
				if (linelen < 6 && tran_state != 1){
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
					process_cmd(comm_fd, cmd, linelen, subbuff, keep_reading, tran_state, hdr, msg, del_msg, user, d_flag);

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
