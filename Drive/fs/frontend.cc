#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <queue>
#include <time.h>
#include <sys/wait.h>
#include <iostream>
#include <arpa/inet.h>
#include <signal.h>
#include "../DATA_PACKET.pb.h"
#include "../utility.h"
#include <set>
#include <ctime>
#include "middleServerComms.h"
#include <nlohmann/json.hpp>
#include <openssl/md5.h>

using namespace std;
using json = nlohmann::json;


// Standard  Server Structures
map < int,  pthread_t > THREADS;
vector < int > CLIENTS;

int LISTEN_SOCKET;
bool V_FLAG;
int INDEX;


//vector < string > FRONT;
// SERVER LOCATIONS STORED IN IP:PORT STRING. BOOL INDICATES KNOWN STATUS
sockaddr_in MASTER;
vector < tuple<string, bool> > L_SERVERS;
vector < tuple<string, bool>  > AUTH;
set <string> IS_LOGGED_IN;
string POP3;
string SMTP;
string ADMIN_SERVER;
string DRIVE;
map < char, vector < string > > SERVER_NODES;


// Master ip: port
static int master_port;
static string master_ip;


// ADMIN DATA :

//vector < tuple<string, bool>  > AUTH;


// SERVER STATUSES
bool MAIL_SERVER_STATUS;
bool DRIVE_SERVER_STATUS;
bool ADMIN_SERVER_STATUS;
bool AUTH_SERVER_STATUS;
bool MIDDLE_SERVER_STATUS;


//USER AUTH: string key is cookies. USER holds user, pass.
//Bool is whether user is logged in or admin
map< string, tuple<string, string>> USER;
map < string, bool> USER_ADMIN;

// DRIVEBOX AND MAILBOX
map<string, vector < tuple<string, string> > >DRIVEBOX_FILE;
map<string, vector < tuple<string, string> > >DRIVEBOX_DIR;

map < string, vector < tuple< string, string, string, string, string, string>> >MAILBOX;
map < string, string > CURRENT_DRIVE_DIR;



//HTTP CODES
string HTML_DIR = "./html";
const char* HTTP_PREFIX11 = "HTTP/1.1";
const char* HTTP_PREFIX10 = "HTTP/1.0";

const char* OK_REQUEST = "200 OK";
const char* NOT_FOUND = "404 Not Found";
const char* BAD_REQUEST = "400 Bad Request";
const char* NO_AUTH = "403 Forbidden";
const char* SERVER_ERROR = "500 Server Error";

const char* MAIN_ARG_ERR = "Arguments error";

const char* HTTP_GET = "GET";
const char* HTTP_HEAD = "HEAD";
const char* HTTP_POST = "POST";

const string HTTP_BODY_HEADER = "Content-Type:";
const string HTTP_SIZE_HEADER = "Content-Length:";
const string HTTP_COOKIE_HEADER = "Set-Cookie:";

const string HTTP_DATE_HEADER = "Date:";


const string CRLF = "\r\n";
const string FILLER = "\n\n\n>>>>>>>>>>>>>>>>>>>>>>>";
time_t current_dt;

struct heartbeatArgs{
	string heartAddr;
	bool* killed;
};
bool killed = false;


///////////////// HELPERS ////////////////////////
/// Socket Write ///
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



/// HTTP RESPONSES

void write_get_ok(int comm_fd, string cookie, bool cookie_reset){
	string ok_msg = string(HTTP_PREFIX11) + " " + string(OK_REQUEST) + CRLF;
	do_write(comm_fd, ok_msg.c_str(), strlen(ok_msg.c_str()));

	char* time_string;
	time(&current_dt);
	time_string = ctime(&current_dt);
	string date_msg = HTTP_DATE_HEADER + " " + string(time_string);
	do_write(comm_fd, date_msg.c_str(), strlen(date_msg.c_str()));


	string content_msg = HTTP_BODY_HEADER + " text/html" + CRLF;
	do_write(comm_fd, content_msg.c_str(), strlen(content_msg.c_str()));

	if( cookie.compare("-1") != 0 && cookie_reset){
		string cookie_msg = string(HTTP_COOKIE_HEADER) + " " + cookie + CRLF;
		cout << cookie_msg;
		do_write(comm_fd, cookie_msg.c_str(), strlen(cookie_msg.c_str()));

	}

}


void write_download_ok(int comm_fd, string fn, string data){
	string ok_msg = string(HTTP_PREFIX11) + " " + string(OK_REQUEST) + CRLF;
	do_write(comm_fd, ok_msg.c_str(), strlen(ok_msg.c_str()));

	char* time_string;
	time(&current_dt);
	time_string = ctime(&current_dt);
	string date_msg = HTTP_DATE_HEADER + " " + string(time_string);
	do_write(comm_fd, date_msg.c_str(), strlen(date_msg.c_str()));


//	string content_msg = HTTP_BODY_HEADER + " text/html" + CRLF;
//	do_write(comm_fd, content_msg.c_str(), strlen(content_msg.c_str()));

	size_t path_end = fn.find_last_of("/");


	string content_disp = "Content-Disposition: attachment; filename=" +fn.substr(path_end+1) +CRLF;
	string pragma= "Pragma: no-cache" + CRLF;
	string exp = "Expires: 0" + CRLF;
	do_write(comm_fd, content_disp.c_str(), strlen(content_disp.c_str()));
	do_write(comm_fd, pragma.c_str(), strlen(pragma.c_str()));
	do_write(comm_fd, exp.c_str(), strlen(exp.c_str()));

	string content_msg = HTTP_SIZE_HEADER + " " + to_string(strlen(data.c_str())) + CRLF;
	do_write(comm_fd, content_msg.c_str(), strlen(content_msg.c_str()));
	do_write(comm_fd, CRLF.c_str(), strlen(CRLF.c_str()));
	do_write(comm_fd,(data + CRLF).c_str(), strlen((data + CRLF).c_str()));

	//
	//Content-type: text/plain
	//Content-Disposition: attachment; filename=foo.txt
	//Pragma: no-cache
	//Expires: 0

}

void write_get_not_found(int comm_fd, int status){
	if(status == 404){
		string nf_msg = string(HTTP_PREFIX11) + " " + string(NOT_FOUND) + CRLF;
		do_write(comm_fd, nf_msg.c_str(), strlen(nf_msg.c_str()));
	}
	else{
		string nf_msg = string(HTTP_PREFIX11) + " " + string(NO_AUTH) + CRLF;
		do_write(comm_fd, nf_msg.c_str(), strlen(nf_msg.c_str()));
	}

	char* time_string;
	time(&current_dt);
	time_string = ctime(&current_dt);
	string date_msg = HTTP_DATE_HEADER + " " + string(time_string);
	do_write(comm_fd, date_msg.c_str(), strlen(date_msg.c_str()));


	string content_msg = HTTP_BODY_HEADER + " text/html" + CRLF;
	do_write(comm_fd, content_msg.c_str(), strlen(content_msg.c_str()));
}

void write_bad_request(int comm_fd){
	string nf_msg = string(HTTP_PREFIX11) + " " + string(BAD_REQUEST) + CRLF;
	do_write(comm_fd, nf_msg.c_str(), strlen(nf_msg.c_str()));

	char* time_string;
	time(&current_dt);
	time_string = ctime(&current_dt);
	string date_msg = HTTP_DATE_HEADER + " " + string(time_string);
	do_write(comm_fd, date_msg.c_str(), strlen(date_msg.c_str()));
	do_write(comm_fd, CRLF.c_str(), strlen(CRLF.c_str()));

}

void write_server_error(int comm_fd){
	string nf_msg = string(HTTP_PREFIX11) + " " + string(SERVER_ERROR) + CRLF;
	do_write(comm_fd, nf_msg.c_str(), strlen(nf_msg.c_str()));

	char* time_string;
	time(&current_dt);
	time_string = ctime(&current_dt);
	string date_msg = HTTP_DATE_HEADER + " " + string(time_string);
	do_write(comm_fd, date_msg.c_str(), strlen(date_msg.c_str()));
	do_write(comm_fd, CRLF.c_str(), strlen(CRLF.c_str()));

}


void write_redirect(int comm_fd, int status_code, string cookie, bool cookie_reset){
	string redir_msg = string(HTTP_PREFIX11) + " " + string("303 See Other") + CRLF;
	do_write(comm_fd, redir_msg.c_str(), strlen(redir_msg.c_str()));

	char* time_string;
	time(&current_dt);
	time_string = ctime(&current_dt);
	string date_msg = HTTP_DATE_HEADER + " " + string(time_string);
	do_write(comm_fd, date_msg.c_str(), strlen(date_msg.c_str()));

	if( cookie.compare("-1") != 0 && cookie_reset){
		string cookie_msg = string(HTTP_COOKIE_HEADER) + " " + cookie + CRLF;
		cout << cookie_msg;
		do_write(comm_fd, cookie_msg.c_str(), strlen(cookie_msg.c_str()));

	}


	if(status_code == 404){
		string loc_msg = "Location: /404" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));
	}
	else if(status_code == 403){
		string loc_msg = "Location: /403" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));
	}
	// AYYYYYYYYY
	else if(status_code == 420){
		string loc_msg = "Location: /failedemail" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));
	}

	else if(status_code == 421){
		string loc_msg = "Location: /failedinbox" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));
	}
	else if(status_code == 422){
		string loc_msg = "Location: /faileddelete" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));

	}
	else if(status_code == 423){
		string loc_msg = "Location: /faileddrivebox" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));

	}
	else if(status_code == 424){
		string loc_msg = "Location: /faileddeletedrive" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));

	}
	//LOGIN SUCESS
	else if(status_code == 200){

		string loc_msg = "Location: /home" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));

	}
	//LOGOUT SUCESS
	else if(status_code == -200){
		string cookie_msg = string(HTTP_COOKIE_HEADER) + " " + cookie + "; expires=Thu, 01 Jan 1970 00:00:00 GMT" + CRLF;
		do_write(comm_fd, cookie_msg.c_str(), strlen(cookie_msg.c_str()));
		string loc_msg = "Location: /index" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));
	}
	else if(status_code == 221){
		string loc_msg = "Location: /sentemail" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));

	}
	else if(status_code == 222){
		string loc_msg = "Location: /inbox" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));

	}
	else if(status_code == 223){
		cout << "DRIIIIIIIIIVE"  <<"\n";

		string loc_msg = "Location: /drive" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));
		cout << "DRIIIIIIIIIVE22222222"  <<"\n";

	}
	else if(status_code == 999){
		string loc_msg = "Location: /admin" + CRLF;
		do_write(comm_fd, loc_msg.c_str(), strlen(loc_msg.c_str()));

	}
}




// COOKIES
void genSessionID(char *data, int dataLengthBytes, unsigned char *digestBuffer)
{
	MD5_CTX c;
	MD5_Init(&c);
	MD5_Update(&c, data, dataLengthBytes);
	MD5_Final(digestBuffer, &c);
}



// login code: 0 is success, -1, is an unauthorized error, -2 is a auth_server error
string login(int comm_fd, int sockfd, tuple<string, string> user_pass){
	unsigned char* sessionID = (unsigned char*) malloc(MD5_DIGEST_LENGTH);
	char* time_string;
	time(&current_dt);
	time_string = ctime(&current_dt);
	string usr = get<0>(user_pass) + string(time_string);
	int usr_len = usr.length();
	genSessionID((char*)usr.c_str(), usr_len, sessionID);
	char buf[MD5_DIGEST_LENGTH *2];
	for (int j=0;j<MD5_DIGEST_LENGTH;j++){
		sprintf(buf+j, "%02x", sessionID[j]);
	}
	string sid = string(buf);
	cout << "LOGGING IN WITH: " << get<0>(user_pass) << "\n";
	cout << "LOGGING IN WITH: " << get<1>(user_pass) << "\n";

	cout << "LOGGING IN WITH: " << sid << "\n";
	Packet auth_packet;
	auth_packet.set_command("login");
	auth_packet.add_arg(get<0>(user_pass));
	auth_packet.add_arg(get<1>(user_pass));
	auth_packet.add_arg(sid);
	string auth_string;
	if (!auth_packet.SerializeToString(&auth_string)){
		cerr << "COULD NOT SERIALIZE";
		exit(1);
	}
	else{
		string pre = generate_prefix(auth_string.size());
		auth_string = pre + auth_string;
		char tosend[auth_string.size()];
		auth_string.copy(tosend, auth_string.size());
		do_send(&sockfd, tosend, sizeof(tosend));
	}

	char buffer[2048];
	Packet auth_resp;
	int rcvdbytes = do_recv(&sockfd, &buffer[0], 10);
	buffer[rcvdbytes] = 0;
	auth_resp.ParseFromString(buffer);
	string auth_status = auth_resp.status_code();
	cout << "logging in " << auth_status << "\n";
	if(auth_status.compare("200") == 0){
		USER[sid] = user_pass;
		MAILBOX[sid] = vector < tuple< string, string, string, string, string, string>>();
		DRIVEBOX_FILE[sid] = vector< tuple < string, string >>();
		DRIVEBOX_DIR[sid] = vector< tuple < string, string >>();
		CURRENT_DRIVE_DIR[sid] = "Home";
		if(auth_resp.data().compare("1") == 0){
			USER_ADMIN[sid] = true;
		}
		else{
			USER_ADMIN[sid] = false;
		}
		return sid;
	}
	else{
		return "-1";
	}
}

string get_session(string cookie, int comm_fd){
	cout << "GETTING SESSION\n";

	int connection_works = -1;
	int rand_auth;
	string authmeta;
	size_t colon;
	string ipaddr;
	string port;
	int sockfd ;
	struct sockaddr_in authaddr;
	int p;
	bool authstatus;
	if(AUTH_SERVER_STATUS){
		while(connection_works < 0 && AUTH_SERVER_STATUS){
			rand_auth = rand() % AUTH.size();
			authmeta = get<0>(AUTH[rand_auth]);
			authstatus = get<1>(AUTH[rand_auth]);
			if(authstatus){
				colon = authmeta.find(":");
				ipaddr = authmeta.substr(0,colon);
				port = authmeta.substr(colon+1);
				sockfd = socket(PF_INET, SOCK_STREAM, 0);
				if (sockfd < 0){
					cerr << "SOCKET ERROR\n";
					exit(1);
				}
				bzero(&authaddr, sizeof(authaddr));
				authaddr.sin_family = AF_INET;
				p = stoi(port);
				authaddr.sin_port = htons(p);
				inet_pton(AF_INET,ipaddr.c_str() , &(authaddr.sin_addr));
				cout << "SEG\n";
				connection_works = connect(sockfd, (struct sockaddr*)&authaddr, sizeof(authaddr));
				if(connection_works < 0){
					cout << "Fault\n";

					AUTH[rand_auth] = make_tuple(authmeta, false);
					for(int i = 0; i <AUTH.size(); i++){
						AUTH_SERVER_STATUS = AUTH_SERVER_STATUS || get<1>(AUTH[i]);
					}
				}
			}
		}
		//			CONNECTED to auth server

		if(AUTH_SERVER_STATUS){
			Packet auth_packet;
			auth_packet.set_command("getsession");
			auth_packet.add_arg(cookie);
			string auth_string;
			if (!auth_packet.SerializeToString(&auth_string)){
				cerr << "COULD NOT SERIALIZE";
				exit(1);
			}
			else{
				string pre = generate_prefix(auth_string.size());
				auth_string = pre + auth_string;
				char tosend[auth_string.size()];
				auth_string.copy(tosend, auth_string.size());
				do_send(&sockfd, tosend, sizeof(tosend));
			}
			char buffer[2048];
			Packet auth_resp;
			int rcvdbytes = do_recv(&sockfd, &buffer[0], 10);
			buffer[rcvdbytes] = 0;
			auth_resp.ParseFromString(buffer);
			string auth_status = auth_resp.status_code();
			cout<< "AUTH STATUS RESULT>:" << auth_status <<"\n";

			if(auth_status.compare("200") == 0){
				tuple<string, string> user_pass = make_tuple(auth_resp.arg(0), auth_resp.arg(1));
				string logged_in = login(comm_fd,sockfd, user_pass);
				if(logged_in.compare("-1") !=  0){
					Packet del_packet;
					del_packet.set_command("deletesession");
					del_packet.add_arg(cookie);
					string del_string;
					if (!del_packet.SerializeToString(&del_string)){
						cerr << "COULD NOT SERIALIZE";
						exit(1);
					}
					else{
						string pre = generate_prefix(del_string.size());
						del_string = pre + del_string;
						char tosend[del_string.size()];
						del_string.copy(tosend,del_string.size());
						do_send(&sockfd, tosend, sizeof(tosend));
					}
					char buffer[2048];
					Packet auth_resp;
					int rcvdbytes = do_recv(&sockfd, &buffer[0], 10);
					buffer[rcvdbytes] = 0;
					auth_resp.ParseFromString(buffer);
					string auth_status = auth_resp.status_code();
					close(sockfd);
					return logged_in;
				}
				else{
					close(sockfd);

					return logged_in;
				}
			}
			else{
				close(sockfd);

				write_server_error(comm_fd);
				return "-1";
			}
		}
		else{
			close(sockfd);

			write_server_error(comm_fd);
			return "-1";

		}
	}
	else{
		close(sockfd);

		write_server_error(comm_fd);
		return "-1";
	}
}


//SERVER CODES
// 0 POP3
// 1 SMTP
// 2 DRIVE
// 3 AUTH

void genServers(){
	cout<<"IN GENSERVERS\n";
	int connection_works = -1;
	int rand_server;
	string servermeta;
	size_t colon;
	bool serverstatus;
	string ipaddr;
	string port;
	int sockfd ;
	bool server_status;
	struct sockaddr_in authaddr;
	int p;
	while(connection_works < 0){
		rand_server = rand() % L_SERVERS.size();
		servermeta = get<0>(L_SERVERS[rand_server]);
		serverstatus = get<1>(L_SERVERS[rand_server]);
		if(serverstatus){
			colon = servermeta.find(":");
			ipaddr = servermeta.substr(0,colon);
			port = servermeta.substr(colon+1);
			sockfd = socket(PF_INET, SOCK_STREAM, 0);
			if (sockfd < 0){
				cerr << "SOCKET ERROR\n";
				exit(1);
			}
			bzero(&authaddr, sizeof(authaddr));
			authaddr.sin_family = AF_INET;
			p = stoi(port);
			authaddr.sin_port = htons(p);
			inet_pton(AF_INET,ipaddr.c_str() , &(authaddr.sin_addr));
			cout<<ipaddr<<":"<<port<<endl;
			connection_works = connect(sockfd, (struct sockaddr*)&authaddr, sizeof(authaddr));
			cout<<connection_works<<endl;
			if( connection_works < 0){
				L_SERVERS[rand_server] = make_tuple(servermeta, false);
			}
		}


	}
	//		   * [0]: POP3
	//		   * [1]: SMTP
	//		   * [2]: Drive
	//		   * [3]: Admin

	vector<string> mid_addr =  getMiddleServerAddresses(sockfd);
	cout<<"0: "<<mid_addr[0]<<" 1: "<<mid_addr[1]<<" 2: "<<mid_addr[2]<<" 3: "<<mid_addr[3]<<endl;
	if(mid_addr.size() > 0){
		if(mid_addr[0].compare("POPDOWN") == 0 || mid_addr[1].compare("SMTPDOWN") == 0 ){
			MAIL_SERVER_STATUS = false;
		}
		else{
			POP3 = mid_addr[0];
			SMTP = mid_addr[1];
		}

		if(mid_addr[2].compare("DRIVEDOWN") == 0 ){
			DRIVE_SERVER_STATUS = false;
		}
		else{
			DRIVE = mid_addr[2];
		}

		if(mid_addr[3].compare("ADMINDOWN") == 0){
			ADMIN_SERVER_STATUS = false;
		}
		else{
			ADMIN_SERVER = mid_addr[3];
		}
	}
	else{
		DRIVE_SERVER_STATUS = false;
		ADMIN_SERVER_STATUS = false;
		MAIL_SERVER_STATUS = false;

	}

	close(sockfd);


}

/// CTRL-C Handler ///

void handle_ctrlc(int signal)
{
	const char* ctrlc_err = "-ERR Server shutting down\r\n";
	// CLOSE

	for(int i =0; i < THREADS.size(); i++)
	{

		if (do_write(CLIENTS[i], ctrlc_err, strlen(ctrlc_err)) != false)
		{
			close(CLIENTS[i]);
			if (V_FLAG)
			{
				cerr << "[" << CLIENTS[i] << "] " << "Connection closed\r\n";
			}
			pthread_cancel(THREADS[CLIENTS[i]]);
			THREADS.erase(CLIENTS[i]);
		}

	}

	close(LISTEN_SOCKET);
	vector< int >().swap(CLIENTS);
	map< int, pthread_t >().swap(THREADS);

}

string decode(string url) {
	string d_url;
	char ch;
	int i;
	int j;
	for (i=0; i<url.length(); i++) {
		if (int(url[i])==37) {
			sscanf(url.substr(i+1,2).c_str(), "%x", &j);
			ch=static_cast<char>(j);
			d_url+=ch;
			i=i+2;
		} else if (url[i] == '+') {
			d_url+=' ';
		}
		else {
			d_url+=url[i];
		}
	}

	return d_url;
}


//PARSE
tuple< string, string, bool> parse_user_pass(vector<string> data){


	string userpass_msg = data[0];
	size_t amp = userpass_msg.find("&");
	string user = userpass_msg.substr(0,amp);
	string pass = userpass_msg.substr(amp+1);
	string admin = "";
	amp = pass.find("&");
	if(amp != string::npos){
		admin = pass.substr(amp+1);
		pass = pass.substr(0,amp);
	}

	size_t equ = user.find("=");
	user = user.substr(equ+1);

	equ = pass.find("=");
	pass = pass.substr(equ+1);
	tuple< string, string, bool > usps;
	string d_user;
	string d_pass;
	d_user = decode(user+"@penncloud");
	d_pass = decode(pass);




	if(strlen(admin.c_str()) == 0){
		usps = make_tuple(d_user, d_pass, false);
	}
	else{
		usps = make_tuple(d_user, d_pass, true);
	}
	return usps;

}

vector<string> parse_to(string to_data){
	size_t equ = to_data.find("=");
	string to = decode(to_data.substr(equ+1));
	size_t comma = to.find(",");

	vector < string > to_list = vector<string>();
	while(comma != string::npos){
		string new_to = to.substr(0, comma);
		to = to.substr(comma+1);
		string d_new_to;
		d_new_to = decode(new_to);
		to_list.push_back(d_new_to);
		comma = to.find(",");



	}
	string d_to;
	d_to = decode(to);
	to_list.push_back(d_to);
	return to_list;

}



map<string, string> parse_data(string data, string cookie, bool is_first, string bookend){
	map<string, string> file_data = map<string, string>();
	// parse filename
	if(is_first){
		size_t newline = data.find("\r\n");
		string n_bookend = data.substr(0,newline);
		cout << n_bookend << "\n";

		file_data["bookend"] = n_bookend;

		size_t file_name_header = data.find("filename");
		string new_data = data.substr(file_name_header);

		size_t equ = new_data.find("=");
		size_t cr = new_data.find("\r\n");

		string file_name = new_data.substr(equ+2, cr-(equ+3));
		cout << "FILE NAME " << file_name<< "\n";
		//	file_name = file_name.substr(equ+2,newline);
		string d_file_name;
		d_file_name = decode(file_name);
		file_data["filename"] = CURRENT_DRIVE_DIR[cookie] + "/" + d_file_name;
		size_t rnrn=data.find("\r\n\r\n");
		file_data["start"] = to_string(rnrn + 4);
		//string d_data;
		//string all_data = data.substr(rnrn+4);
		//d_data = decode(all_data);
		//file_data["fileData"] = d_data;

	}

	if(!is_first){
		size_t end = data.find(bookend);
		data = data.substr(0, end);
		string d_data;
		d_data = decode(data);
		file_data["fileData"] = d_data;
		cout << "FILE DATA " << d_data<< "\n";
	}

	return file_data;




}

tuple< string, vector<string> , string, string> parse_email(vector<string> data, string cookie){
	string email_msg = data[0];
	string from =  get<0>(USER[cookie]);

	size_t amp = email_msg.find("&");

	string to = email_msg.substr(0, amp);
	cout << "TOOOO:" << to << "\n";
	vector<string> to_list = parse_to(to);

	string rest_of_email = email_msg.substr(amp+1);
	amp = rest_of_email.find("&");
	string subj = rest_of_email.substr(0, amp);
	size_t equ = subj.find("=");
	subj = subj.substr(equ+1);


	string msg_body = rest_of_email.substr(amp+1);
	equ = msg_body.find("=");
	msg_body = msg_body.substr(equ+1);


	string d_from;
	string d_subj;
	string d_msg_body;


	d_from = decode(from) ;
	d_subj = decode(subj) ;
	d_msg_body = decode(msg_body);

	tuple< string, vector<string> , string, string> email_data = make_tuple(d_from, to_list, d_subj, d_msg_body);
	return email_data;
}


tuple< string, string, string, string, string, string> parse_inbox(string data){

	cout<< "\n\nHERES THE DATA: " << data << "\n\n";

	//	From
	//	To
	//	Date
	//	Subj
	//	MSG ID
	// Browser
	//	MSG

	//FROM
	size_t space = data.find(" ");
	size_t newline = data.find("\r\n");
	string from = data.substr(space+1, newline-space);

	data = data.substr(newline+2);
	//	TO
	space = data.find(" ");
	newline = data.find("\r\n");
	string to = data.substr(space+1, newline-space);

	data = data.substr(newline+2);
	//DATE
	space = data.find(" ");
	newline = data.find("\r\n");
	string date = data.substr(space+1, newline-space);

	data = data.substr(newline+2);

	// MSG_ID
	space = data.find(" ");
	newline = data.find("\r\n");
	string id = data.substr(space, newline-space);

	data = data.substr(newline+2);



	//SUBJ
	space = data.find("Subject:");
	string a_subj = data.substr(space+8);
	newline = a_subj.find("\r\n");
	string subj = a_subj.substr(2, newline);

	data = a_subj.substr(newline+3);
	newline = data.find("\r\n\r\n");
	data = data.substr(newline+4);


	data = data.substr(0, data.length()-4);



	// MSG BODY
	string msg = data;
	//			From, To, Date, Subj, MSG
	string d_from;
	string d_msg;
	string d_subj;
	string d_date;
	string d_to;

	d_from = decode( from) ;
	d_msg = decode( msg) ;
	d_subj = decode( subj) ;
	d_date = decode(date) ;
	d_to = decode(to) ;



	cout << "0d_from:" << d_from  << "\n";
	cout << "1d_to:" << d_to  << "\n";
	cout << "2d_date:" << d_date << "\n";
	cout << "3d_subj:" << d_subj  << "\n";
	cout << "4d_msg:" << d_msg << "\n";

	tuple< string, string, string, string, string, string> email_data = make_tuple(d_from, d_to, d_date, d_subj, id, d_msg);
	return email_data;
}

/////// MSG handler ///////

void handle_get(int comm_fd,string resource, map< string, string> headers, vector< string > data, bool ishead){
	// PARSE
	//SIGNUP
	cout << "GET RESOURCE:" << resource  <<"\n";

	bool logged_in = false;
	bool reset_cookie = false;
	string cookie = "-1";
	map<string,string>::iterator it;
	it = headers.find("Cookie:");
	if (it != headers.end()){
		cookie = headers["Cookie:"];
		set<string>::iterator lit;
		lit = IS_LOGGED_IN.find(cookie);
		if(lit != IS_LOGGED_IN.end()){
			logged_in = true;
		}
		else{
			string sid = get_session(cookie, comm_fd);
			if(sid.compare("-1") != 0){
				IS_LOGGED_IN.insert(sid);
				logged_in = true;
				reset_cookie = true;
				cookie = sid;
			}
			else{
				cookie = "-1";
				logged_in = false;
			}

		}
	}

	//	If front-end crashed, restore user init

	cout << "loggin done\n";

	ifstream webpage;
	string line;
	string web_html = "";
	string htmlfile = "";
	bool is_inbox = false;
	bool is_drivebox = false;
	bool is_admin = false;
	bool no_redirect = true;
	map<char, map<string, bool>> full_server_list;
	string all_kv_pairs;


	//	email settings: 0 = new email, 1 = reply, 2 = forward

	cout<<"res: "<<resource<<endl;
	cout<<"admin cookie: "<<USER_ADMIN[cookie]<<endl;
	cout<<"logged_in: "<<logged_in<<endl;
//		Get homepage
	if(resource.compare("/favicon.ico") == 0){
		write_get_not_found(comm_fd, 404);
		no_redirect = false;
	}
	else if (resource.compare("/") == 0 || resource.compare("/index.html") == 0 || resource.compare("/index") == 0){
		if(!logged_in){
			cout << cookie << "\n";
			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/index.html";
		}
		else{
			cout << "has cookie\n";

			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/home.html";
		}
	}
	else if(resource.compare("/404") == 0 || resource.compare("/404.html") == 0){
		write_get_not_found(comm_fd, 404);
		htmlfile = "/404.html";
	}
	else if(resource.compare("/403") == 0 || resource.compare("/403.html") == 0){
		write_get_not_found(comm_fd, 403);
		htmlfile = "/403.html";
	}
	else if(resource.compare("/login") == 0 || resource.compare("/login.html") == 0){
		write_get_ok(comm_fd, cookie, reset_cookie);
		htmlfile = "/login.html";
	}
	else if(resource.compare("/register") == 0 || resource.compare("/register.html") == 0){
		write_get_ok(comm_fd, cookie, reset_cookie);
		htmlfile = "/register.html";
	}
	//	//No authorization
	else if(((resource.compare("/admin") == 0) && !USER_ADMIN[cookie])){
		cout<<"In admin\n";
		write_redirect(comm_fd, 403, cookie, reset_cookie);
		no_redirect = false;
	}
	else if(logged_in){
		if (resource.compare("/logout") == 0){

			//		Connect to AUTH to delete cookie
			int connection_works = -1;
			int rand_auth;
			string authmeta;
			size_t colon;
			string ipaddr;
			string port;
			int sockfd ;
			struct sockaddr_in authaddr;
			int p;
			bool authstatus;
			if(AUTH_SERVER_STATUS){
				while(connection_works < 0 && AUTH_SERVER_STATUS){
					rand_auth = rand() % AUTH.size();
					authmeta = get<0>(AUTH[rand_auth]);
					authstatus = get<1>(AUTH[rand_auth]);
					if(authstatus){
						colon = authmeta.find(":");
						ipaddr = authmeta.substr(0,colon);
						port = authmeta.substr(colon+1);
						sockfd = socket(PF_INET, SOCK_STREAM, 0);
						if (sockfd < 0){
							cerr << "SOCKET ERROR\n";
							exit(1);
						}
						bzero(&authaddr, sizeof(authaddr));
						authaddr.sin_family = AF_INET;
						p = stoi(port);
						authaddr.sin_port = htons(p);
						inet_pton(AF_INET,ipaddr.c_str() , &(authaddr.sin_addr));
						connection_works = connect(sockfd, (struct sockaddr*)&authaddr, sizeof(authaddr));
						if(connection_works < 0){
							AUTH[rand_auth] = make_tuple(authmeta, false);
							for(int i = 0; i <AUTH.size(); i++){
								AUTH_SERVER_STATUS = AUTH_SERVER_STATUS || get<1>(AUTH[i]);
							}
						}
					}
				}
				//			CONNECTED to auth server
				if(AUTH_SERVER_STATUS){
					Packet auth_packet;
					auth_packet.set_command("deletesession");
					auth_packet.add_arg(cookie);
					string auth_string;
					if (!auth_packet.SerializeToString(&auth_string)){
						cerr << "COULD NOT SERIALIZE";
						exit(1);
					}
					else{
						string pre = generate_prefix(auth_string.size());
						auth_string = pre + auth_string;
						char tosend[auth_string.size()];
						auth_string.copy(tosend, auth_string.size());
						do_send(&sockfd, tosend, sizeof(tosend));
					}
					char buffer[2048];
					Packet auth_resp;
					int rcvdbytes = do_recv(&sockfd, &buffer[0], 10);

					buffer[rcvdbytes] = 0;
					auth_resp.ParseFromString(buffer);
					string auth_status = auth_resp.status_code();
					cout << "logging out " << auth_status << "\n";
					close(sockfd);
					if(auth_status.compare("200") == 0){
						USER_ADMIN.erase(cookie);
						USER.erase(cookie);
						MAILBOX.erase(cookie);
						DRIVEBOX_FILE.erase(cookie);
						DRIVEBOX_DIR.erase(cookie);
						CURRENT_DRIVE_DIR.erase(cookie);

						write_redirect(comm_fd, -200, cookie, reset_cookie);
						no_redirect = false;
					}
					else{
						write_server_error(comm_fd);
					}
				}
				else{
					write_server_error(comm_fd);

				}
			}
			else{
				write_server_error(comm_fd);
			}


		}
		else if(resource.compare("/sentemail") == 0){
			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/sentemail.html";
		}
		else if(resource.compare("/failedemail") == 0){
			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/failedemail.html";
		}
		else if(resource.compare("/failedinbox") == 0){
			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/failedinbox.html";
		}
		else if(resource.compare("/faileddelete") == 0){
			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/faileddelete.html";
		}
		else if(resource.compare("/faileddrivebox") == 0){
			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/faileddrivebox.html";
		}
		else if(resource.compare("/faileddeletedrive") == 0){
			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/faileddeletedrive.html";
		}
		//
		else if((resource.compare("/inbox") == 0)){
			cout << cookie << "\n";
			MAILBOX[cookie] = vector < tuple< string, string, string, string, string, string>>();
			int connection_works= -1;
			int sockfd;
			if(MAIL_SERVER_STATUS){

				while(connection_works < 0 && MAIL_SERVER_STATUS ){

					size_t colon = POP3.find(":");
					string ipaddr = POP3.substr(0,colon);
					string port = POP3.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);

					}
					struct sockaddr_in pop3addr;
					bzero(&pop3addr, sizeof(pop3addr));
					pop3addr.sin_family = AF_INET;
					int p = stoi(port);
					pop3addr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(pop3addr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&pop3addr, sizeof(pop3addr));
					if(connection_works < 0){
						genServers();
					}
				}
				if(MAIL_SERVER_STATUS){

					Packet packet_rcvd = receivePacket(sockfd);


					vector <string> load_inbox = receiveMail(sockfd, get<0>(USER[cookie]), get<1>(USER[cookie]));
					if (load_inbox[0].compare("ERROR") == 0){
						no_redirect = false;
						write_redirect(comm_fd, 421, cookie, reset_cookie);

					}
					else{
						//			From, To, Date, Subj, msgid, MSG
						for(int i = 1; i < load_inbox.size(); i++){
							MAILBOX[cookie].push_back(parse_inbox(load_inbox[i]));
						}
						is_inbox = true;
						write_get_ok(comm_fd, cookie, reset_cookie);

						htmlfile = "/inbox.html";
					}
					close(sockfd);
				}
				else{
					write_server_error(comm_fd);

				}
			}
			else{
				write_server_error(comm_fd);

			}
		}


		else if((resource.compare("/home") == 0) && (cookie.compare("-1") != 0)){
			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/home.html";

		}
		else if((resource.compare("/newemail") == 0)){
			cout << "NEW EMAIL/n";
			write_get_ok(comm_fd, cookie, reset_cookie);
			htmlfile = "/newemail.html";
		}
		else if((resource.compare("/drive") == 0)){
			cout << "NEW EMAIL/n";

			DRIVEBOX_FILE[cookie] = vector<tuple<string, string>>();
			DRIVEBOX_DIR[cookie] = vector<tuple<string, string>>();
			if(CURRENT_DRIVE_DIR[cookie].compare("Home") != 0){

				size_t path_end =  CURRENT_DRIVE_DIR[cookie].find_last_of("/");

				string prev_dir = CURRENT_DRIVE_DIR[cookie].substr(0,path_end);
				tuple < string, string> d_d_s = make_tuple(prev_dir, string("d"));
				DRIVEBOX_DIR[cookie].push_back(d_d_s);
			}
			int connection_works= -1;
			int sockfd;
			if(DRIVE_SERVER_STATUS){

				while(connection_works < 0 && DRIVE_SERVER_STATUS){
					size_t colon = DRIVE.find(":");
					string ipaddr = DRIVE.substr(0,colon);
					string port = DRIVE.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);

					}
					struct sockaddr_in driveaddr;
					bzero(&driveaddr, sizeof(driveaddr));
					driveaddr.sin_family = AF_INET;
					int p = stoi(port);
					driveaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(driveaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&driveaddr, sizeof(driveaddr));
					if(connection_works < 0){
						genServers();
					}
				}

				if(DRIVE_SERVER_STATUS){
					Packet packet_rcvd = receivePacket(sockfd);
					map<string, string> d_args = map<string, string>();
					d_args["username"] = get<0>(USER[cookie]);
					d_args["filename"] = CURRENT_DRIVE_DIR[cookie];
					string curr_dir = "";
					//				d_args["filename"] = genDirectory();
					d_args["type"] = "d";

					unsigned char* d;
					string load_drivebox = driveTransaction(sockfd, "DISPLAY",d_args, 0, d);
					if (load_drivebox.compare("ERROR") == 0){
						no_redirect = false;
						cout << "IN dsdsds8"  <<"\n";

						write_redirect(comm_fd, 423, cookie, reset_cookie);

					}
					else{
						json json_obj = json::array();
						string json_str = (load_drivebox).substr(1);
						cout << "gggggJSON: " <<json_str << "\n";
						if(json_str.length() > 0){
							json_obj = json::parse(json_str);
							cout<<"size: "<<json_obj.size()<<endl;
							//for (json::iterator it = json_obj.begin(); it != json_obj.end(); ++it) {
							for(int i = 0; i < json_obj.size(); i++){
								json arr = json::array();
								arr = json_obj.at(i);
								//							cout << *it[0]<< "\n";
								//							cout << *it[1] << "\n";

								if(string(arr.at(0)).compare("d") == 0){
									tuple < string, string> d_d_s = make_tuple(string(arr.at(1)), string(arr.at(0)));

									DRIVEBOX_DIR[cookie].push_back(d_d_s);
								}
								else{

									tuple < string, string> d_f_s = make_tuple(string(arr.at(1)), string(arr.at(2)));
									DRIVEBOX_FILE[cookie].push_back(d_f_s);

								}
							}
						}
						write_get_ok(comm_fd, cookie, reset_cookie);
						htmlfile = "/drive.html";
						is_drivebox = true;
					}
					close(sockfd);
				}
				else{
					cout << "IN aaaPOST8"  <<"\n";

					write_server_error(comm_fd);

				}
			}
			else{
				cout << "IN aaaaaaaddsdsdsdsdPOST8"  <<"\n";

				write_server_error(comm_fd);

			}

		}
		else if((resource.compare("/admin") == 0) && USER_ADMIN[cookie]){
			cout<<"IN ADMIN\n";
			//		DRIVEBOX_FILE[cookie] = vector<tuple<string, string>>();
			int connection_works= -1;
			int sockfd;
			if(ADMIN_SERVER_STATUS){
				cout<<"ADMIN UP\n";
				while(connection_works < 0 && ADMIN_SERVER_STATUS){
					size_t colon = ADMIN_SERVER.find(":");
					string ipaddr = ADMIN_SERVER.substr(0,colon);
					string port = ADMIN_SERVER.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);
					}
					struct sockaddr_in adminaddr;
					bzero(&adminaddr, sizeof(adminaddr));
					adminaddr.sin_family = AF_INET;
					int p = stoi(port);
					adminaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(adminaddr.sin_addr));
					cout<<"connecting to admin..."<<ipaddr<<":"<<port<<endl;
					connection_works = connect(sockfd, (struct sockaddr*)&adminaddr, sizeof(adminaddr));
					if(connection_works < 0){
						genServers();
					}
				}
			}
			if(ADMIN_SERVER_STATUS){
//				Packet packet_rcvd = receivePacket(sockfd);
				full_server_list= getServerStatus(sockfd);
				close(sockfd);

				if(full_server_list.size() == 0){
					write_server_error(comm_fd);
					no_redirect = false;
				}
				else{
					write_get_ok(comm_fd, cookie, reset_cookie);
					htmlfile = "/admin.html";
					is_admin = true;
				}
				//		}
			}
			else{
				write_server_error(comm_fd);
				no_redirect = false;
			}
		}
		//404 not found
		else{
			write_redirect(comm_fd, 404, cookie, reset_cookie);
			no_redirect = false;
		}
	}
	else{
		write_redirect(comm_fd, 403, cookie, reset_cookie);
		no_redirect = false;
	}
	if(no_redirect){
		cout << "LOAD WEB\n";

		webpage.open ((HTML_DIR+htmlfile).c_str());
		while (getline(webpage,line)){
			cout << line;
			size_t ind = line.find("$uname");
			if(ind != string::npos){
				line.replace(ind, 6, get<0>(USER[cookie]));
			}
			size_t iind = line.find("$INBOX");
			size_t dind = line.find("$DRIVEFILE");
			size_t currdirind = line.find("$DIRNAME");
			size_t drivedirind = line.find("$DRIVEDIR");
			size_t aind = line.find("$ADMIN");

			size_t eind = line.find("$TO");
			if(eind != string::npos){
				line.replace(eind, 3, "");
			}
			eind = line.find("$SUBJ");
			if(eind != string::npos){
				line.replace(eind, 5, "");
			}
			eind = line.find("$MSG");
			if(eind != string::npos){
				line.replace(eind, 4,"");
			}

			if(is_inbox && iind != string::npos ){
				for(int i = 0; i < MAILBOX[cookie].size(); i++){
					// From, To, Date, Subj, MSG
					web_html += "<tr>";

					web_html +=  "<form method=\"post\" action=\"reademail\">" ;
					web_html += " <input type=\"hidden\" value=\"" + to_string(i+1) +"\" name=\"readinput\"/>";
					web_html += "<th> <button type=\"submit\" name=\"read\"></button> </th>";
					web_html += "</form>";


					web_html +=  "<form method=\"post\" action=\"deleteemail\">";
					web_html += " <input type=\"hidden\" value=\"" + to_string(i+1) +"\" name=\"deleinput\"/>";
					web_html += "<th> <button type=\"submit\" name=\"delete\"> </button> </th>";

					web_html += "</form>";

					web_html += "<th> " + get<0>(MAILBOX[cookie][i]) +"</th>";
					web_html += "<th> " + get<3>(MAILBOX[cookie][i]) + "</th>";
					web_html += "<th> " + get<2>(MAILBOX[cookie][i]) +"</th>";
					web_html += "</tr>";
				}
				is_inbox = false;
			}
			else if(is_drivebox &&((currdirind != string::npos) || (dind != string::npos) || (drivedirind != string::npos))){
				//					string curr_dir = "";
				////					for( int i = 0; i < CURRENT_DRIVE_DIR.size(); i++){
				////						curr_dir = curr_dir + CURRENT_DRIVE_DIR[i] + "/";
				////
				////					}
				//					curr_dir = curr_dir.substr(0, curr_dir.length()-1);


				if(currdirind != string::npos){
					line.replace(currdirind, 8, CURRENT_DRIVE_DIR[cookie]);
					web_html += line;
				}


				if(dind != string::npos){
					cout << "\nDRIVEBOX FILE DIR SIZE:" <<DRIVEBOX_FILE[cookie].size() << "\n";
					for(int i = 0; i < DRIVEBOX_FILE[cookie].size(); i++){
						//			ID, From, To, Date, Subj, MSG
						web_html += "<tr>";
						web_html +=  "<form method=\"post\" action=\"downloadfile\">" ;
						web_html += " <input type=\"hidden\" value=\"" + to_string(i+1) +"\" name=\"readinput\"/>";
						web_html += "<th> <button type=\"submit\" name=\"read\"></button> </th>";
						web_html += "</form>";
						web_html +=  "<form method=\"post\" action=\"deletefile\">";
						web_html += " <input type=\"hidden\" value=\"" + to_string(i+1) +"\" name=\"deleinput\"/>";
						web_html += "<th> <button type=\"submit\" name=\"delete\"> </button> </th>";
						web_html += "</form>";
						size_t path_end =  get<0>(DRIVEBOX_FILE[cookie][i]).find_last_of("/");

						web_html += "<th> " + get<0>(DRIVEBOX_FILE[cookie][i]).substr(path_end+1) +"</th>";
						web_html += "<th> " + get<1>(DRIVEBOX_FILE[cookie][i]) +"</th>";

						web_html += "</tr>";
					}
				}

				if(drivedirind != string::npos){

					for(int i = 0; i < DRIVEBOX_DIR[cookie].size(); i++){
						//			ID, From, To, Date, Subj, MSG
						web_html += "<tr>";
						if(CURRENT_DRIVE_DIR[cookie].compare("Home") != 0 && i == 0  ){
							web_html += "<th> </th>";

						}
						else{
							web_html +=  "<form method=\"post\" action=\"deletedir\">";
							web_html += " <input type=\"hidden\" value=\"" + to_string(i+1) +"\" name=\"deleinput\"/>";
							web_html += "<th> <button type=\"submit\" name=\"delete\"> </button> </th>";
							web_html += "</form>";
						}
						web_html +=  "<form method=\"post\" action=\"gotodir\">";

						size_t path_end =  get<0>(DRIVEBOX_DIR[cookie][i]).find_last_of("/");
						web_html += " <input type=\"hidden\" value=\"" + to_string(i+1) +"\" name=\"gotoinput\"/>";
						web_html += "<th> <button type=\"submit\" name=\"goto\">" +  get<0>(DRIVEBOX_DIR[cookie][i]).substr(path_end+1) +  " </button> </th>";
						web_html += "</form>";
						web_html += "</tr>";
					}
					is_drivebox = false;

				}

			}

			else if(is_admin && aind != string::npos){
				map<char, map<string, bool>>::iterator fsl_it;
				for(fsl_it = full_server_list.begin(); fsl_it != full_server_list.end(); fsl_it++ ){
					map<string, bool>::iterator ss_it;
					for(ss_it = full_server_list[fsl_it->first].begin(); ss_it  != full_server_list[fsl_it->first].end(); ss_it++)

						//					for
						//					Headers are:  Server Type, Server Name, Status, Kill/Restart, View
						web_html += "<tr>";

//					TYPE
						web_html += "<th>" + to_string(fsl_it->first) + "</th>";

//						NAME

						web_html += "<th>" +  string(ss_it->first) +  "</th>";


//						STATUS
						if(ss_it->second){
							web_html += "<th  style=\"color:green;\"> UP </th>";
						}
						else{
							web_html += "<th style=\"color:red;\"> DOWN </th>";

						}




//						KILL/RESTART

						web_html +=  "<form method=\"post\" action=\"killrestart\">";

						web_html += " <input type=\"hidden\" value=\"" + string(ss_it->first)  +"\" name=\"server\"/>";
						if(ss_it->second){
							web_html += " <input type=\"hidden\" value=\"" + to_string(1) +"\" name=\"status\"/>";
							web_html += "<th> <button type=\"submit\" name=\"kill/restart\"> Kill </button> </th>";

						}
						else{
							web_html += " <input type=\"hidden\" value=\"" +to_string(0) +"\" name=\"status\"/>";
							web_html += "<th> <button type=\"submit\" name=\"kill/restart\"> Restart </button> </th>";


						}

						web_html += "</form>";


//						VIEW
						if(to_string(fsl_it->first).compare("K") == 0 && ss_it->second){
							web_html +=  "<form method=\"get\" action=\"viewdata\">";
							web_html += " <input type=\"hidden\" value=\"" + string(ss_it->first)  +"\" name=\"server\"/>";
							web_html += "<th> <button type=\"submit\" name=\"view\"> View </button> </th>";
							web_html += "</form>";
						}
						else{
							web_html += "<th> </th>";

						}
						web_html += "</tr>";
				}
				is_admin = false;

			}
//			else if(is_view && vind != string::npos){
//
//
//			}
			else{
				web_html += line;
			}
		}
		cout<< web_html <<"\n";
		string content_msg = HTTP_SIZE_HEADER + " " + to_string(strlen(web_html.c_str())) + CRLF;
		do_write(comm_fd, content_msg.c_str(), strlen(content_msg.c_str()));

		if(!ishead){

			do_write(comm_fd, CRLF.c_str(), strlen(CRLF.c_str()));
			do_write(comm_fd,(web_html + CRLF).c_str(), strlen((web_html + CRLF).c_str()));
		}
	}
}










//POST
void handle_post(int comm_fd,string resource,  map< string, string> headers, vector< string > data, unsigned char* file_data, int data_size){
	// PARSE
	//SIGNUP
	cout << "POST RESOURCE:" << resource  <<"\n";

	bool logged_in = false;
	bool reset_cookie = false;
	string cookie = "-1";
	map<string,string>::iterator it;
	it = headers.find("Cookie:");
	if (it != headers.end()){
		cookie = headers["Cookie:"];
		set<string>::iterator lit;
		lit = IS_LOGGED_IN.find(cookie);
		if(lit != IS_LOGGED_IN.end()){
			logged_in = true;
		}
		else{
			string sid = get_session(cookie, comm_fd);
			if(sid.compare("-1") != 0){
				IS_LOGGED_IN.insert(sid);
				logged_in = true;
				reset_cookie = true;
				cookie = sid;
			}
			else{
				cookie = "-1";
				logged_in = false;
			}

		}
	}

	//	If front-end crashed, restore user init
	if (resource.compare("/newuser") == 0){
		tuple< string, string, bool > temp_user_pass = parse_user_pass(data);
		tuple < string, string> user_pass = make_tuple(get<0>(temp_user_pass), get<1>(temp_user_pass));
		cout << "NEW USER: " << get<0>(temp_user_pass) << "\n";
		cout << "NEW PASS: " << get<1>(temp_user_pass) << "\n";

		bool new_user_auth = get<2>(temp_user_pass);

		int connection_works = -1;
		int rand_auth;
		string authmeta;
		size_t colon;
		string ipaddr;
		string port;
		int sockfd ;
		struct sockaddr_in authaddr;
		int p;
		bool authstatus;
		if(AUTH_SERVER_STATUS){
			while(connection_works < 0 && AUTH_SERVER_STATUS){
				rand_auth = rand() % AUTH.size();
				authmeta = get<0>(AUTH[rand_auth]);
				authstatus = get<1>(AUTH[rand_auth]);
				if(authstatus){

					colon = authmeta.find(":");
					ipaddr = authmeta.substr(0,colon);
					port = authmeta.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);
					}
					bzero(&authaddr, sizeof(authaddr));
					authaddr.sin_family = AF_INET;
					p = stoi(port);
					authaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(authaddr.sin_addr));

					connection_works = connect(sockfd, (struct sockaddr*)&authaddr, sizeof(authaddr));
					if(connection_works < 0){
						AUTH[rand_auth] = make_tuple(authmeta, false);
						for(int i = 0; i <AUTH.size(); i++){
							AUTH_SERVER_STATUS = AUTH_SERVER_STATUS || get<1>(AUTH[i]);
						}
					}
				}
			}
			if(AUTH_SERVER_STATUS){
				Packet auth_packet;
				auth_packet.set_command("signup");
				auth_packet.add_arg(get<0>(user_pass));
				auth_packet.add_arg(get<1>(user_pass));
				if (new_user_auth){
					auth_packet.add_arg("1");

				}
				else{
					auth_packet.add_arg("0");

				}
				string auth_string;

				if (!auth_packet.SerializeToString(&auth_string)){
					cerr << "COULD NOT SERIALIZE";
					exit(1);
				}
				else{
					string pre = generate_prefix(auth_string.size());
					auth_string = pre + auth_string;
					char tosend[auth_string.size()];
					auth_string.copy(tosend, auth_string.size());
					do_send(&sockfd, tosend, sizeof(tosend));

				}


				char buffer[2048];
				Packet auth_resp;
				int rcvdbytes = do_recv(&sockfd, &buffer[0], 10);
				buffer[rcvdbytes] = 0;
				auth_resp.ParseFromString(buffer);

				string auth_status = auth_resp.status_code();
				if(auth_status.compare("200") == 0){
					string result = login(comm_fd,sockfd, user_pass);
					close(sockfd);
					cookie = result;
					reset_cookie = true;
					IS_LOGGED_IN.insert(cookie);

					if(result.compare("-1") != 0){
						write_redirect(comm_fd, 200, cookie, reset_cookie);
					}
					else{
						write_redirect(comm_fd, 403, cookie, reset_cookie);
					}
				}
				else{
					write_redirect(comm_fd, 403, cookie, reset_cookie);
				}
			}
			else{
				write_server_error(comm_fd);

			}
		}
		else{
			write_server_error(comm_fd);

		}
	}
	//LOGIN
	else if (resource.compare("/auth") == 0){
		tuple< string, string, bool > temp_user_pass = parse_user_pass(data);
		tuple < string, string> user_pass = make_tuple(get<0>(temp_user_pass), get<1>(temp_user_pass));

		int connection_works = -1;
		int rand_auth;
		string authmeta;
		size_t colon;
		string ipaddr;
		string port;
		int sockfd ;
		struct sockaddr_in authaddr;
		int p;
		bool authstatus;
		if(AUTH_SERVER_STATUS){
			while(connection_works < 0 && AUTH_SERVER_STATUS){
				rand_auth = rand() % AUTH.size();
				authmeta = get<0>(AUTH[rand_auth]);
				authstatus = get<1>(AUTH[rand_auth]);
				if(authstatus){

					colon = authmeta.find(":");
					ipaddr = authmeta.substr(0,colon);
					port = authmeta.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);
					}
					bzero(&authaddr, sizeof(authaddr));
					authaddr.sin_family = AF_INET;
					p = stoi(port);
					authaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(authaddr.sin_addr));

					connection_works = connect(sockfd, (struct sockaddr*)&authaddr, sizeof(authaddr));
					if(connection_works < 0){
						AUTH[rand_auth] = make_tuple(authmeta, false);
						for(int i = 0; i <AUTH.size(); i++){
							AUTH_SERVER_STATUS = AUTH_SERVER_STATUS || get<1>(AUTH[i]);
						}
					}
				}
			}
			if(AUTH_SERVER_STATUS){
				string logged_in = login(comm_fd,sockfd ,user_pass);
				close(sockfd);
				cookie = logged_in;
				reset_cookie = true;
				IS_LOGGED_IN.insert(cookie);

				if(logged_in.compare("-1") != 0){
					write_redirect(comm_fd, 200, cookie, reset_cookie);
				}
				else{
					write_redirect(comm_fd, 403, cookie, reset_cookie);
				}
			}
			else{
				write_server_error(comm_fd);

			}
		}
		else{
			write_server_error(comm_fd);

		}
	}
	else if(logged_in){

		//SEND EMAIL
		if (resource.compare("/sendemail") == 0){
			//From, To, subj, msg
			tuple< string, vector<string>, string, string > email_data = parse_email(data, cookie);
			int sockfd;
			int connection_works = -1;
			if(MAIL_SERVER_STATUS){
				while(connection_works < 0 && MAIL_SERVER_STATUS){
					size_t colon = SMTP.find(":");
					string ipaddr = SMTP.substr(0,colon);
					string port = SMTP.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);
					}
					struct sockaddr_in smtpaddr;
					bzero(&smtpaddr, sizeof(smtpaddr));
					smtpaddr.sin_family = AF_INET;
					int p = stoi(port);
					smtpaddr.sin_port = htons(p);
					cout<< "PORT: " << port << "\n";
					cout<< "IP: " << ipaddr << "\n";

					inet_pton(AF_INET,ipaddr.c_str() , &(smtpaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&smtpaddr, sizeof(smtpaddr));
					cout<<"conn works: "<<connection_works<<endl;
					if(connection_works < 0){
						cout<<"genservers called\n";
						genServers();
					}
				}
				if(MAIL_SERVER_STATUS){

					cout << "NOT CONNECTED TO SMTP\n";
					char buff[1024];
					int num = recv_t(&sockfd, buff, 5);
					cout << "[S]: " + string(buff);
					cout << "CONNECTED TO SMTP\n";
					int status_code = sendmail(sockfd, headers, get<0>(email_data), get<1>(email_data), get<2>(email_data), get<3>(email_data));
					cout << "SENT TO SMTP\n";

					close(sockfd);
					if(status_code == 221){
						write_redirect(comm_fd, 221, cookie, reset_cookie);
					}
					else{
			//			AYYYYYYYYYYYYYYYYY
						write_redirect(comm_fd, 420, cookie, reset_cookie);
					}
				}
				else{
					write_server_error(comm_fd);

				}
			}
			else{
				write_server_error(comm_fd);

			}

		}


		//READ EMAIL
		else if (resource.compare("/reademail") == 0){


			ifstream webpage;

			write_get_ok(comm_fd, cookie, reset_cookie);
			cout << data[0] << "DATA\n";
			string e_id = data[0];
			size_t equ = e_id.find("=");
			int email_id = stoi(e_id.substr(equ+1))-1;
			string htmlfile = "/reademail.html";
			webpage.open ((HTML_DIR+htmlfile).c_str());
			string line;
			string web_html = "";
			//			From, To, Date, Subj, msgid, MSG
	//		string a = get<0>(MAILBOX[email_id]);
			tuple< string, string, string, string, string, string> mbox= MAILBOX[cookie][email_id];
			while (getline(webpage,line)){

				size_t ind = line.find("$NUM");
				if(ind != string::npos){
					line.replace(ind, 4, e_id.substr(equ+1).c_str());
				}


				ind = line.find("$FROM");
				if(ind != string::npos){
					line.replace(ind, 5, get<0>(mbox).c_str());
				}
				ind = line.find("$SUBJ");
				if(ind != string::npos){
					line.replace(ind, 5,  get<3>(mbox).c_str());
				}
				ind = line.find("$MSG");
				if(ind != string::npos){
					line.replace(ind, 4, get<5>(mbox).c_str());
				}
				web_html += line;

			}


			cout<< web_html <<"\n";
			string content_msg = HTTP_SIZE_HEADER + " " + to_string(strlen(web_html.c_str())) + CRLF;
			do_write(comm_fd, content_msg.c_str(), strlen(content_msg.c_str()));
			do_write(comm_fd, CRLF.c_str(), strlen(CRLF.c_str()));
			do_write(comm_fd,(web_html + CRLF).c_str(), strlen((web_html + CRLF).c_str()));
		}
		else if (resource.compare("/replyemail") == 0){
			ifstream webpage;

			write_get_ok(comm_fd, cookie, reset_cookie);
			cout << data[0] << "DATA\n";
			string e_id = data[0];
			size_t equ = e_id.find("=");
			int email_id = stoi(e_id.substr(equ+1))-1;
			string htmlfile = "/newemail.html";
			webpage.open ((HTML_DIR+htmlfile).c_str());
			string line;
			string web_html = "";
			//			From, To, Date, Subj, msgid, MSG
	//		string a = get<0>(MAILBOX[email_id]);
			tuple< string, string, string, string, string, string> mbox= MAILBOX[cookie][email_id];
			while (getline(webpage,line)){

				size_t ind = line.find("$TO");
				if(ind != string::npos){
					cout << "STRING: " << get<0>(mbox).length() << ":" << get<0>(mbox) << "\n";
					line.replace(ind, 3, get<0>(mbox).c_str());
				}
				ind = line.find("$SUBJ");
				if(ind != string::npos){
					line.replace(ind, 5,  ("RE: " +get<3>(mbox)).c_str());
				}
				ind = line.find("$MSG");
				if(ind != string::npos){
					line.replace(ind, 4, (FILLER + "\nFROM: " + get<0>(mbox) + "\n" + get<5>(mbox)).c_str());
				}
				web_html += line;

			}


			cout<< web_html <<"\n";
			string content_msg = HTTP_SIZE_HEADER + " " + to_string(strlen(web_html.c_str())) + CRLF;
			do_write(comm_fd, content_msg.c_str(), strlen(content_msg.c_str()));
			do_write(comm_fd, CRLF.c_str(), strlen(CRLF.c_str()));
			do_write(comm_fd,(web_html + CRLF).c_str(), strlen((web_html + CRLF).c_str()));
		}
		else if (resource.compare("/fwdemail") == 0){
			ifstream webpage;

			write_get_ok(comm_fd, cookie, reset_cookie);
			cout << data[0] << "DATA\n";
			string e_id = data[0];
			size_t equ = e_id.find("=");
			int email_id = stoi(e_id.substr(equ+1))-1;
			string htmlfile = "/newemail.html";
			webpage.open ((HTML_DIR+htmlfile).c_str());
			string line;
			string web_html = "";
			//			From, To, Date, Subj, msgid, MSG
	//		string a = get<0>(MAILBOX[email_id]);
			tuple< string, string, string, string, string, string> mbox= MAILBOX[cookie][email_id];
			while (getline(webpage,line)){


				size_t ind = line.find("$TO");
				if(ind != string::npos){
					line.replace(ind, 3, "");
				}
				ind = line.find("$SUBJ");
				if(ind != string::npos){
					line.replace(ind, 5,  ("FW: " +get<3>(mbox)).c_str());
				}
				ind = line.find("$MSG");
				if(ind != string::npos){
					line.replace(ind, 4, (FILLER + "\nFROM: " + get<0>(mbox) + "\n" + get<5>(mbox)).c_str());
				}
				web_html += line;
			}


			cout<< web_html <<"\n";
			string content_msg = HTTP_SIZE_HEADER + " " + to_string(strlen(web_html.c_str())) + CRLF;
			do_write(comm_fd, content_msg.c_str(), strlen(content_msg.c_str()));
			do_write(comm_fd, CRLF.c_str(), strlen(CRLF.c_str()));
			do_write(comm_fd,(web_html + CRLF).c_str(), strlen((web_html + CRLF).c_str()));
		}

		//DELETE EMAIL

		else if (resource.compare("/deleteemail") == 0){
				//From, To, subj, msg

			string email_id = data[0];
			cout << data[0] << "DATA\n";
			size_t equ = email_id.find("=");
			email_id = email_id.substr(equ+1);


			int connection_works= -1;
			int sockfd;
			if(MAIL_SERVER_STATUS){

				while(connection_works < 0 && MAIL_SERVER_STATUS ){

					size_t colon = POP3.find(":");
					string ipaddr = POP3.substr(0,colon);
					string port = POP3.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);

					}
					struct sockaddr_in pop3addr;
					bzero(&pop3addr, sizeof(pop3addr));
					pop3addr.sin_family = AF_INET;
					int p = stoi(port);
					pop3addr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(pop3addr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&pop3addr, sizeof(pop3addr));
					if(connection_works < 0){
						genServers();
					}
				}
				if(MAIL_SERVER_STATUS){
					vector<int> delete_msg = vector<int>();
					delete_msg.push_back(stoi(email_id));
					Packet packet_rcvd = receivePacket(sockfd);

					string status_code = deleteMail(sockfd, get<0>(USER[cookie]), get<1>(USER[cookie]),delete_msg);
					close(sockfd);

					if(status_code.compare("SUCCESS") == 0){
						write_redirect(comm_fd, 222, cookie, reset_cookie);
					}
					else{
				//			AYYYYYYYYYYYYYYYYY
						write_redirect(comm_fd, 422, cookie, reset_cookie);
					}
				}
				else{
					write_server_error(comm_fd);

				}
			}
			else{
				write_server_error(comm_fd);

			}

		}
		else if (resource.compare("/uploadfile") == 0)
		{
			// int num_packets = data_size/1000000;
			// num_packets += 1;
			// if((data_size % 1000000) > 0){
			// 	num_packets += 1;
			// }
			cout << "FILE DATA: " << file_data << endl;

			string fname;
			map <string, string> init_data = parse_data(data[0], cookie, true, "");
			string book = init_data["bookend"];

			fname = init_data["filename"];

			// 2 added to account for \r\n
			int bookend_sz = book.size() + 2;
			int i = bookend_sz + 1;
			int begin = 0;

			while(true)
			{
				if( i >= 4 && file_data[i] == '\n' && file_data[i-1] == '\r' && file_data[i - 2] == '\n' && file_data[i-3] == '\r')
				{
					begin = begin + 1;
					break;
				}
				i = i + 1;
				begin = begin + 1;
			}
			begin = begin + bookend_sz + 1;
			cout << "Begin: " << begin << endl;

			cout << "Content: " << file_data[begin] << file_data[begin + 1] << file_data[begin + 2] << file_data[begin + 3] << endl;

			// Excluding bookend size
			int true_data_sz = data_size - (2*bookend_sz) - 6;
			cout << "Data Size:  "<< true_data_sz << "\n";

			// Set packet size as ~1MB and get the # of packets required
			int pack_num = true_data_sz/1000000;
			if(true_data_sz% 1000000 > 0)
				pack_num = pack_num + 1;

			// Skip the first bookend
			int idx = begin;

			// Last index of data
			//int end = true_data_sz;
			int end = data_size - bookend_sz - 4;

			// Establish connection with the drive server
			int connection_works= -1;
 			int sockfd;
			cout << to_string(pack_num) <<"\n";

			if(DRIVE_SERVER_STATUS)
			{
				while(connection_works < 0 && DRIVE_SERVER_STATUS )
				{
					size_t colon = DRIVE.find(":");
					string ipaddr = DRIVE.substr(0,colon);
					string port = DRIVE.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0)
					{
						cerr << "SOCKET ERROR\n";
						exit(1);
					}
					struct sockaddr_in driveaddr;
					bzero(&driveaddr, sizeof(driveaddr));
					driveaddr.sin_family = AF_INET;
					int p = stoi(port);
					driveaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(driveaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&driveaddr, sizeof(driveaddr));

					if(connection_works < 0)
					{
						genServers();
					}
				}
				if(DRIVE_SERVER_STATUS)
				{
					Packet packet_rcvd = receivePacket(sockfd);

					// Send data chunk to middlecomms
					string load_drivebox = "ERROR";
					for(int i = 0; i < pack_num; i++)
					{
						cout << "IN POST7"  <<"\n";

						unsigned char* chunk = (unsigned char*)malloc(1000000);
						int j = 0;
						while(true)
						{
							chunk[j] = file_data[idx + j];
							j = j + 1;

							// If 1MB written or end of file reached, break
							if(j == 1000000 || j == end)
								break;
							if(string((char*)&file_data[idx+j]) == book + "--\r\n")
								break;
							// if(file_data[idx + j] == '-' && file_data[idx + j + 1] == '-' && file_data[idx + j +2] == '-' && file_data[idx + j + 3] == '-')
							// {
							// 	// Found the bottom bookend remove the last 4 bytes
							// 	break;
							// }
						}
						cout << "CHUNK: " << chunk << endl;

						// Prepare arguments to send to middlecomms
						map<string, string> arg = map<string, string>();;
						arg["username"] = get<0>(USER[cookie]);
						arg["filename"] =  fname;
						arg["type"] = "f";
						arg["num_packets"] = to_string(pack_num);

						cout << "Call Drive Transaction\n";
						load_drivebox = driveTransaction(sockfd, "UPLOAD", arg, j - 1, chunk);

					}
					cout << "IN POST8"  <<"\n";
					if(load_drivebox.compare("ERROR") != 0){
						write_redirect(comm_fd, 223, cookie, reset_cookie);
					}
					else{
				//			AYYYYYYYYYYYYYYYYY
						write_redirect(comm_fd, 422, cookie, reset_cookie);
					}
	 				close(sockfd);

				}
				else
				{
					cout << "IN POST99"  <<"\n";

					write_server_error(comm_fd);
				}
			}
			else
			{
				cout << "IN POST1212"  <<"\n";

 				write_server_error(comm_fd);
 			}
 		

		}




			// Index where the actual data starts, might also contain headers
// 			int data_sz = data["start"];
// 			//data[0] = init_data["fileData"];
// //			data[num_packets-1] = parse_data(data[num_packets-1], cookie, false, book)["fileData"];
// 			int ds = data_size;
// 	//		map< string, string> d_args = parse_data(data[0], cookie,);
// 			int connection_works= -1;
// 			int sockfd;
// 			if(DRIVE_SERVER_STATUS){

// 				while(connection_works < 0 && DRIVE_SERVER_STATUS ){
// 					size_t colon = DRIVE.find(":");
// 					string ipaddr = DRIVE.substr(0,colon);
// 					string port = DRIVE.substr(colon+1);
// 					sockfd = socket(PF_INET, SOCK_STREAM, 0);
// 					if (sockfd < 0){
// 						cerr << "SOCKET ERROR\n";
// 						exit(1);

// 					}
// 					struct sockaddr_in driveaddr;
// 					bzero(&driveaddr, sizeof(driveaddr));
// 					driveaddr.sin_family = AF_INET;
// 					int p = stoi(port);
// 					driveaddr.sin_port = htons(p);
// 					inet_pton(AF_INET,ipaddr.c_str() , &(driveaddr.sin_addr));
// 					connection_works = connect(sockfd, (struct sockaddr*)&driveaddr, sizeof(driveaddr));

// 					if(connection_works < 0){
// 						genServers();
// 					}
// 				}

// 				if(DRIVE_SERVER_STATUS){
// 					Packet packet_rcvd = receivePacket(sockfd);

// 					for(int i = 0; i < num_packets; i++){
// 						cout << "UPLOADING PACKET:" << to_string(i+1) <<"\n";

// 						map< string, string> d_args = map< string, string>();
// 						d_args["username"] =  get<0>(USER[cookie]);
// 						d_args["filename"] =  fname;
// 						d_args["type"] = "f";
// 						if(i == 0){
// 							cout << "SENDING FIRST PACKET" << decode(data[0]) << "\n";
// //							d_args["fileData"] = decode(data[0]);
// 						}
// 						else{
// 							string fd;
// 							size_t copied = fd.copy((char*)file_data,1000000,i*1000000);
// 							if(i < (num_packets-1)){
// //								d_args["fileData"] = decode(fd);
// 							}
// 							else{
// //								d_args["fileData"] = parse_data(fd, cookie, false, book)["fileData"];
// 							}

// 						}
// 						d_args["curr_n"] = to_string(i+1);
// 						d_args["num_packets"] = to_string(num_packets);
// 						cout << "LOADING DRIVEBOX \n";
// 						string load_drivebox = driveTransaction(sockfd, "UPLOAD",d_args, true,  file_packet_data);
// 						cout << "LOAD DRIVEBOX " << load_drivebox << "\n";
// 					}
// 					write_redirect(comm_fd, 223, cookie, reset_cookie);
// 					close(sockfd);


// 				}
// 				else{
// 					write_server_error(comm_fd);

// 				}
// 			}
// 			else{
// 				write_server_error(comm_fd);

// 			}


		else if (resource.compare("/deletefile") == 0){
				//From, To, subj, msg

			string drive_id = data[0];
			size_t amp = drive_id.find("&");
			if(amp != string::npos){
				drive_id = drive_id.substr(0,amp);
			}
			size_t equ = drive_id.find("=");


			drive_id = drive_id.substr(equ+1);

			int connection_works= -1;
			int sockfd;
			if(DRIVE_SERVER_STATUS){

				while(connection_works < 0 && DRIVE_SERVER_STATUS){
					size_t colon = DRIVE.find(":");
					string ipaddr = DRIVE.substr(0,colon);
					string port = DRIVE.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);

					}
					struct sockaddr_in driveaddr;
					bzero(&driveaddr, sizeof(driveaddr));
					driveaddr.sin_family = AF_INET;
					int p = stoi(port);
					driveaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(driveaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&driveaddr, sizeof(driveaddr));
					if(connection_works < 0){
						genServers();
					}
				}

				if(DRIVE_SERVER_STATUS){

					map<string, string> d_args = map<string, string>();
					d_args["username"] =  get<0>(USER[cookie]);
					d_args["filename"] = get<0>(DRIVEBOX_FILE[cookie][stoi(drive_id)-1]);
					d_args["type"] = "f";
					cout << d_args["filename"] << " FILE NAME\n";
					Packet packet_rcvd = receivePacket(sockfd);
					unsigned char* d;
					string status_code = driveTransaction(sockfd, "DELETE", d_args, 0, d);
					cout << status_code << "\n";
					close(sockfd);

					if(status_code.compare("ERROR") == 0){
						write_redirect(comm_fd, 424, cookie, reset_cookie);
					}
					else{
				//			AYYYYYYYYYYYYYYYYY
						write_redirect(comm_fd, 223, cookie, reset_cookie);
					}

				}
				else{
					write_server_error(comm_fd);

				}
			}
			else{
				write_server_error(comm_fd);

			}
		}
		else if (resource.compare("/downloadfile") == 0){

			string drive_id = data[0];
			size_t amp = drive_id.find("&");
			if(amp != string::npos){
				drive_id = drive_id.substr(0,amp);
			}
			size_t equ = drive_id.find("=");
			drive_id = drive_id.substr(equ+1);


			int connection_works= -1;
			int sockfd;
			if(DRIVE_SERVER_STATUS){

				while(connection_works < 0 && DRIVE_SERVER_STATUS ){
					size_t colon = DRIVE.find(":");
					string ipaddr = DRIVE.substr(0,colon);
					string port = DRIVE.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);

					}
					struct sockaddr_in driveaddr;
					bzero(&driveaddr, sizeof(driveaddr));
					driveaddr.sin_family = AF_INET;
					int p = stoi(port);
					driveaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(driveaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&driveaddr, sizeof(driveaddr));
					if(connection_works < 0){
						genServers();
					}
				}

				if(DRIVE_SERVER_STATUS){

					map<string, string> d_args = map<string, string>();
					d_args["username"] = get<0>(USER[cookie]);
					d_args["filename"] = get<0>(DRIVEBOX_FILE[cookie][stoi(drive_id)-1]);
					Packet packet_rcvd = receivePacket(sockfd);
					unsigned char* d;
					string status_code = driveTransaction(sockfd, "DOWNLOAD", d_args, 0, d);
					cout << status_code << "\n";
					close(sockfd);
					if(status_code.compare("ERROR") == 0){
						write_redirect(comm_fd, 423, cookie, reset_cookie);
					}
					else{
						write_download_ok(comm_fd, get<0>(DRIVEBOX_FILE[cookie][stoi(drive_id)-1]), status_code);
					}

				}
				else{
					write_server_error(comm_fd);

				}
			}
			else{
				write_server_error(comm_fd);

			}

		}
		else if (resource.compare("/newdir") == 0){

			string drive_name= data[0];
			size_t amp = drive_name.find("&");
			if(amp != string::npos){
				drive_name = drive_name.substr(0,amp);
			}
			size_t equ = drive_name.find("=");


			drive_name = drive_name.substr(equ+1);

			int connection_works= -1;
			int sockfd;
			if(DRIVE_SERVER_STATUS){

				while(connection_works < 0 && DRIVE_SERVER_STATUS){
					size_t colon = DRIVE.find(":");
					string ipaddr = DRIVE.substr(0,colon);
					string port = DRIVE.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);

					}
					struct sockaddr_in driveaddr;
					bzero(&driveaddr, sizeof(driveaddr));
					driveaddr.sin_family = AF_INET;
					int p = stoi(port);
					driveaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(driveaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&driveaddr, sizeof(driveaddr));
					if(connection_works < 0){
						genServers();
					}
				}

				if(DRIVE_SERVER_STATUS){

					map<string, string> d_args = map<string, string>();
					d_args["username"] =  get<0>(USER[cookie]);
					d_args["filename"] = CURRENT_DRIVE_DIR[cookie]+"/" + drive_name;
					d_args["type"] = "d";
					Packet packet_rcvd = receivePacket(sockfd);
					unsigned char* d;
					string status_code = driveTransaction(sockfd, "NEW", d_args, 0, d);
					cout << status_code << "\n";
					close(sockfd);

					if(status_code.compare("ERROR") == 0){
						write_redirect(comm_fd, 424, cookie, reset_cookie);
					}
					else{
				//			AYYYYYYYYYYYYYYYYY
						write_redirect(comm_fd, 223, cookie, reset_cookie);
					}

				}
				else{
					write_server_error(comm_fd);

				}
			}
			else{
				write_server_error(comm_fd);

			}
		}

		else if (resource.compare("/deletedir") == 0){

			string drive_id = data[0];
			size_t amp = drive_id.find("&");
			if(amp != string::npos){
				drive_id = drive_id.substr(0,amp);
			}
			size_t equ = drive_id.find("=");


			drive_id = drive_id.substr(equ+1);

			int connection_works= -1;
			int sockfd;
			if(DRIVE_SERVER_STATUS){

				while(connection_works < 0 && DRIVE_SERVER_STATUS){
					size_t colon = DRIVE.find(":");
					string ipaddr = DRIVE.substr(0,colon);
					string port = DRIVE.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);

					}
					struct sockaddr_in driveaddr;
					bzero(&driveaddr, sizeof(driveaddr));
					driveaddr.sin_family = AF_INET;
					int p = stoi(port);
					driveaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(driveaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&driveaddr, sizeof(driveaddr));
					if(connection_works < 0){
						genServers();
					}
				}

				if(DRIVE_SERVER_STATUS){

					map<string, string> d_args = map<string, string>();
					d_args["username"] =  get<0>(USER[cookie]);
					d_args["filename"] = get<0>(DRIVEBOX_DIR[cookie][stoi(drive_id)-1]);
					d_args["type"] = "d";
					Packet packet_rcvd = receivePacket(sockfd);
					unsigned char* d;
					string status_code = driveTransaction(sockfd, "DELETE", d_args, 0, d);
					cout << status_code << "\n";
					close(sockfd);

					if(status_code.compare("ERROR") == 0){
						write_redirect(comm_fd, 424, cookie, reset_cookie);
					}
					else{
				//			AYYYYYYYYYYYYYYYYY
						write_redirect(comm_fd, 223, cookie, reset_cookie);
					}

				}
				else{
					write_server_error(comm_fd);

				}
			}
			else{
				write_server_error(comm_fd);

			}
		}


		else if (resource.compare("/renamedir") == 0){

			string drive_name= data[0];
			size_t amp = drive_name.find("&");
			if(amp != string::npos){
				drive_name = drive_name.substr(0,amp);
			}
			size_t equ = drive_name.find("=");


			drive_name = drive_name.substr(equ+1);

			int connection_works= -1;
			int sockfd;
			if(DRIVE_SERVER_STATUS){

				while(connection_works < 0 && DRIVE_SERVER_STATUS){
					size_t colon = DRIVE.find(":");
					string ipaddr = DRIVE.substr(0,colon);
					string port = DRIVE.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);

					}
					struct sockaddr_in driveaddr;
					bzero(&driveaddr, sizeof(driveaddr));
					driveaddr.sin_family = AF_INET;
					int p = stoi(port);
					driveaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(driveaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&driveaddr, sizeof(driveaddr));
					if(connection_works < 0){
						genServers();
					}
				}

				if(DRIVE_SERVER_STATUS){

					map<string, string> d_args = map<string, string>();
					d_args["username"] =  get<0>(USER[cookie]);
					d_args["oldname"] = CURRENT_DRIVE_DIR[cookie];
					size_t path_end = CURRENT_DRIVE_DIR[cookie].find_last_of("/");
					if(path_end != string::npos && CURRENT_DRIVE_DIR[cookie].compare("Home") != 0){
						d_args["newname"] = CURRENT_DRIVE_DIR[cookie].substr(0, path_end+1) + drive_name;
					}
					if(CURRENT_DRIVE_DIR[cookie].compare("Home") == 0){
						write_redirect(comm_fd, 424, cookie, reset_cookie);

					}
					else{

						d_args["type"] = "d";
						Packet packet_rcvd = receivePacket(sockfd);
						unsigned char* d;
						string status_code = driveTransaction(sockfd, "RENAME", d_args, 0, d);
						cout << status_code << "\n";
						close(sockfd);

						if(status_code.compare("ERROR") == 0){
							write_redirect(comm_fd, 424, cookie, reset_cookie);
						}
						else{
					//			AYYYYYYYYYYYYYYYYY
							CURRENT_DRIVE_DIR[cookie] = CURRENT_DRIVE_DIR[cookie].substr(0, path_end+1) + drive_name;
							write_redirect(comm_fd, 223, cookie, reset_cookie);
						}
					}

				}
				else{
					write_server_error(comm_fd);

				}
			}
			else{
				write_server_error(comm_fd);

			}

		}

		else if (resource.compare("/gotodir") == 0){
			string drive_id = data[0];
			cout << "NEW GOTO: " << drive_id << "\n";

			size_t amp = drive_id.find("&");
			if(amp != string::npos){
				drive_id = drive_id.substr(0,amp);
			}
			size_t equ = drive_id.find("=");


			drive_id = drive_id.substr(equ+1);

			CURRENT_DRIVE_DIR[cookie] = get<0>(DRIVEBOX_DIR[cookie][stoi(drive_id)-1]);
			write_redirect(comm_fd, 223, cookie, reset_cookie);

		}
		else if (resource.compare("/killrestart") == 0 && USER_ADMIN[cookie]){
			string serv_id = data[0];
			size_t amp = serv_id.find("&");
			string status = "0";
			if(amp != string::npos){
				status = serv_id.substr(amp+1);
				serv_id = serv_id.substr(0,amp);
			}
			size_t equ = serv_id.find("=");
			serv_id = serv_id.substr(equ+1);

			equ = status.find("=");
			int serv_status = stoi(status.substr(equ+1));
			map<char, map<string, bool>> res = map<char, map<string, bool>>();
			int connection_works= -1;
			int sockfd;
			if(ADMIN_SERVER_STATUS){
				while(connection_works < 0 && ADMIN_SERVER_STATUS){
					size_t colon = ADMIN_SERVER.find(":");
					string ipaddr = ADMIN_SERVER.substr(0,colon);
					string port = ADMIN_SERVER.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);
					}
					struct sockaddr_in adminaddr;
					bzero(&adminaddr, sizeof(adminaddr));
					adminaddr.sin_family = AF_INET;
					int p = stoi(port);
					adminaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(adminaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&adminaddr, sizeof(adminaddr));
					if(connection_works < 0){
						genServers();
					}
				}
			}
			if(ADMIN_SERVER_STATUS){

				if(serv_status == 0){
					res = restartServer(sockfd, serv_id);
				}
				else{
					res = killServer(sockfd, serv_id);

				}

//				GO TO ADMIN

				if(res.size() > 0){
					write_redirect(comm_fd, 999, cookie, reset_cookie);


				}
//				ERROR
				else{
					write_redirect(comm_fd, 404, cookie, reset_cookie);
				}
				close(sockfd);
			}
			else{
				write_server_error(comm_fd);
			}



		}
		else if (resource.compare("/viewdata") == 0 && USER_ADMIN[cookie]){
			ifstream webpage;
			string all_kv_pairs;
			string serv_id = data[0];
			size_t amp = serv_id.find("&");
			if(amp != string::npos){
				serv_id = serv_id.substr(0,amp);
			}
			size_t equ = serv_id.find("=");
			serv_id = serv_id.substr(equ+1);

			int connection_works= -1;
			int sockfd;
			if(ADMIN_SERVER_STATUS){
				while(connection_works < 0 && ADMIN_SERVER_STATUS){
					size_t colon = ADMIN_SERVER.find(":");
					string ipaddr = ADMIN_SERVER.substr(0,colon);
					string port = ADMIN_SERVER.substr(colon+1);
					sockfd = socket(PF_INET, SOCK_STREAM, 0);
					if (sockfd < 0){
						cerr << "SOCKET ERROR\n";
						exit(1);
					}
					struct sockaddr_in adminaddr;
					bzero(&adminaddr, sizeof(adminaddr));
					adminaddr.sin_family = AF_INET;
					int p = stoi(port);
					adminaddr.sin_port = htons(p);
					inet_pton(AF_INET,ipaddr.c_str() , &(adminaddr.sin_addr));
					connection_works = connect(sockfd, (struct sockaddr*)&adminaddr, sizeof(adminaddr));
					if(connection_works < 0){
						genServers();
					}
				}
			}
			if(ADMIN_SERVER_STATUS){
//				Packet packet_rcvd = receivePacket(sockfd);
				all_kv_pairs = getRowColsofKV(sockfd, serv_id);
				//close(sockfd);
				json kv_j = json::array();
				kv_j = json::parse(all_kv_pairs);

				string htmlfile = "/reademail.html";
				webpage.open ((HTML_DIR+htmlfile).c_str());
				string line;
				string web_html = "";
				while(getline(webpage,line)){
					size_t ind = line.find("$VIEW");
					if(ind != string::npos){
						string kv_table = "";
						for(int i=0;kv_j.size();i++){
							json pairj = json::array();
							pairj = kv_j.at(i);
							string row = pairj.at(0);
							string col = pairj.at(1);
							kv_table += "<tr>";
							kv_table += "<th>" + row + "</th>";
							kv_table += "<th>" + col + "</th>";
							kv_table +=  "<form method=\"get\" action=\"viewdatafinal\">";
							kv_table += " <input type=\"hidden\" value=\"" + row  +"\" name=\"row\"/>";
							kv_table += " <input type=\"hidden\" value=\"" + col  +"\" name=\"column\"/>";
							kv_table += "<th> <button type=\"submit\" name=\"view\"> View </button> </th>";
							kv_table += "</form>";
							kv_table += "</tr>";
						}
						line.replace(ind, 5, kv_table);
					}
					web_html += line;
				}
				string content_msg = HTTP_SIZE_HEADER + " " + to_string(strlen(web_html.c_str())) + CRLF;
				do_write(comm_fd, content_msg.c_str(), strlen(content_msg.c_str()));
				do_write(comm_fd, CRLF.c_str(), strlen(CRLF.c_str()));
				do_write(comm_fd,(web_html + CRLF).c_str(), strlen((web_html + CRLF).c_str()));
				//		}
			}

		}
		else{
	//		write_redirect(comm_fd, 5);
			write_redirect(comm_fd, 404, cookie, reset_cookie);

		}
	}
	else{
//		write_redirect(comm_fd, 5);
		write_redirect(comm_fd, 403, cookie, reset_cookie);

	}
}
void handle_head(int comm_fd,string resource,  map< string, string> headers, vector< string > data){

	handle_get(comm_fd, resource, headers, data, true);

}

////// connection handler //////////////



void *client_handler(void *arg)
{
// SETUP ADMIN FOR THREADS
    int s = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (s != 0 && V_FLAG)
        cerr << "pthread_setcancelstate";
	int comm_fd = *(int*)arg;
	bool keep_reading = true;
//	READ IN INPUTS
	string msg = "";


	//MUST RESET THESE AFTER MSG DONE
	bool has_data = false;
	bool read_data = false;
	bool headers_done = false;
	int data_size = -1;
	map < string, string > headers = map < string, string >();
	vector < string > data = vector < string >();
	//SET State for command reading (-1 is error, 1 is get, 2 is post, 3 is head
	int msg_state = -1;
	string cmd = "";
	string resource = "";
	string http_ver = "";
	unsigned char buff[4096];
	int buff_size = 4096;
	char http_head[4];
	char remain_head[18];


	bool is_uploadfile = false;
	int rlen = 0;
	bool read_first = false;
	bool is_http = false;
	unsigned char* file_data;
	unsigned char* rest_of_file_data;

	unsigned char* file_data_prefix;

	while (keep_reading){

		if(!is_http || killed){
			rlen = read(comm_fd, &http_head, 4);
			if((string(http_head).compare("HEAD") == 0) || (string(http_head).compare("GET ") == 0) || (string(http_head).compare("POST") == 0 )){
				is_http = true;
			}
			else if(isdigit(http_head[0])){
				rlen = read(comm_fd, &remain_head, 18);
				string admin_packet_size = string(http_head) + string(remain_head);
				cout << "ADMIN PACKET: " << admin_packet_size << to_string(comm_fd) <<"\n";
				cout << "MAYBE CRASH\n";
				int sz = stoi(string(admin_packet_size), nullptr, 2);
				cout << "NO CRASH\n";

				// Read in 'sz' bytes
				char admin_packet[sz];
				Packet packet_rcvd;
				Packet packet_to_send;


				rlen = read(comm_fd, &admin_packet, sz);
				admin_packet[sz] = 0;
				if(!packet_rcvd.ParseFromString(admin_packet))
				{
					cerr << "Failed to parse received message!\n";
					pthread_exit(NULL);
				}
				string command = packet_rcvd.command();

				if(command.compare("kill") == 0 && !killed)
				{
					killed = true;
					packet_to_send.set_command("OK");
					packet_to_send.set_status("Commiting suicide");
					packet_to_send.set_status_code("+OK");
					send_packet(packet_to_send, comm_fd);
					close(comm_fd);
					pthread_exit(NULL);
				}
//
				if(killed)
				{
					if(command == "restart")
					{
						killed = false;
						Packet pack;
						pack.set_command("OK");
						pack.set_status_code("+OK");
						pack.set_status("Bring me to life.");
						send_packet(pack, comm_fd);
						close(comm_fd);
						pthread_exit(NULL);
					}
					else
					{
						close(comm_fd);
						pthread_exit(NULL);
					}
				}


			}
			else{
				close(comm_fd);
				THREADS.erase(comm_fd);
				if (V_FLAG)
				{
					cerr << "[" << comm_fd << "] " << "Connection closed\r\n";
				}
			    pthread_exit(0);
			}


		}
		else{
			bool rnrn_seen = 0;

			rlen = read(comm_fd, &buff, buff_size);

			if (rlen > 0){

				if(read_data){
					msg = data[0];
				}
				else{
					if(read_first){
						msg += string((char*)buff);
					}
					else{
						msg += (string(http_head) + string((char*)buff));
						read_first = true;
					}
				}
				size_t crlfbreak ;
				size_t linebreak ;

				//If the line ends:
				while (read_data || ( ( ((crlfbreak = msg.find(CRLF)) != string::npos )|| ((linebreak = msg.find("\n")) != string::npos) ) && !headers_done)){

					string current_msg = "";
					if(!read_data){
						if(crlfbreak != string::npos){
							current_msg = msg.substr(0,crlfbreak+2);
							msg = msg.substr(crlfbreak+2);
						}
						else{
							current_msg = msg.substr(0,linebreak+1);
							msg = msg.substr(linebreak+1);
						}
					}

					if(current_msg.compare(CRLF ) == 0){
						cout << "\nHEADERS DONE"  <<"\n";
						cout << "STARTING DATA SIZE" << to_string(data_size)  <<"\n";
						map<string, string>::iterator it = headers.find("Content-Type:");
						if (it != headers.end()){
							string content_type_header = headers["Content-Type:"];
							size_t multiform = content_type_header.find("multipart/form-data");
							if(multiform != string::npos){
								is_uploadfile = true;
							}
						}
						headers_done = true;

	//						SPLIT DATA INTO CHUNKS
						if(has_data && (strlen(msg.c_str()) > 0) ){
							data.push_back(msg);
						}
						if(is_uploadfile){
//							SEARCH THROUGH BUFFER TO FIND RELEVANT DATA
							cout << "PARSING PREFIX" <<"\n";

							int prefix_data_size = 0;
							int data_start = 0;
							for(int i = 0; i < sizeof(buff); i++){
								if(buff[i] == '\n' && buff[i+1] == '-' && buff[i+2] == '-'){
									data_start = i;
									break;
								}
							}

							file_data_prefix = &buff[data_start];
							cout << "PREFIX\n";
//							cout << "Buff: " << file_data_prefix << endl;
//							cout << "DONE PREFIX\n";

//							cout << "Buff: " << buff_to_hex(file_data_prefix,1) << endl;
							if(data_size > 4096){
								prefix_data_size = sizeof(buff) - data_start;

							}
							else{
								prefix_data_size = data_size;
							}


//							READ IN THE REST OF THE DATA
							if(data_size > prefix_data_size){
								cout << "PARSING REST OF DATA" <<"\n";

								rest_of_file_data = new unsigned char[data_size-prefix_data_size];

								rlen = read(comm_fd, rest_of_file_data, data_size-prefix_data_size);
								if (rest_of_file_data == nullptr){
									cout << "Error: memory could not be allocated";
									exit(1);
								}
//								cout << "READING REST OF FILE_DATA\n\n\n" <<"\n";
//								cout << rest_of_file_data << "\n";
								file_data = new unsigned char[rlen + prefix_data_size+1];
								strcpy((char*)file_data, (char*)file_data_prefix);
								strcat((char*)file_data, (char*)rest_of_file_data);

								cout << "DONE PARSING DATA " << to_string(data_size)  <<"\n";
								cout << file_data << "\n";
//								cout << "DONE PARSING OUT\n\n\n";
//								delete rest_of_file_data;
//								delete file_data_prefix;

							}
							else{
								cout << "COPY STRING\n";
								file_data = new unsigned char[prefix_data_size];
								data_size = prefix_data_size;
								strcpy((char*)file_data, (char*)file_data_prefix);
//								delete file_data_prefix;


							}


						}

						read_data = true;
					}
					cout << current_msg <<"\n";

	//				MSG IS DONE
					if((headers_done && !has_data) || read_data){
						if(msg_state == 1){
							handle_get(comm_fd, resource, headers, data, false);
							cout << "GET DONE\n";

						}
						else if(msg_state == 2){
							handle_post(comm_fd, resource, headers, data, file_data, data_size);
							delete file_data;

						}
						else if (msg_state == 3){
							handle_head(comm_fd, resource, headers, data);


						}
						//RESET

	//					msg_state = -1;
	//					data.clear();
	//					headers.clear();
	//					cmd = "";
	//					resource = "";
	//					http_ver = "";
	//					read_data = false;
	//					data_size = -1;
	//					has_data = false;
	//					headers_done = false;
						close(comm_fd);
						THREADS.erase(comm_fd);
						if (V_FLAG)
						{
							cerr << "[" << comm_fd << "] " << "Connection closed\r\n";
						}
					    pthread_exit(0);

					}
					else{

						size_t space = current_msg.find(" ");


		//				INIT LINE
						if( msg_state == -1){
							//get init line

							if (space!=string::npos){
								cmd = current_msg.substr(0,space);
								string uncmd_init_line = current_msg.substr(space+1);
								size_t second_space = uncmd_init_line.find(" ");

								if(second_space!=string::npos){
									resource = uncmd_init_line.substr(0, second_space);
									http_ver = uncmd_init_line.substr(second_space+1);
									if(crlfbreak != string::npos){
										http_ver = http_ver.substr(0, http_ver.size()-2);
									}
									else{
										http_ver = http_ver.substr(0, http_ver.size()-1);
									}
								}
								else{
									write_bad_request(comm_fd);
								}

							}
							else{
								write_bad_request(comm_fd);
							}
							if(http_ver.compare(HTTP_PREFIX11) != 0  && http_ver.compare(HTTP_PREFIX10) != 0  ){
								write_bad_request(comm_fd);
							}
							else{

								if(cmd.compare(HTTP_GET) == 0){
									msg_state = 1;
								}
								else if(cmd.compare(HTTP_POST) == 0){
									msg_state = 2;
								}
								else if(cmd.compare(HTTP_HEAD) == 0){
									msg_state = 3;
								}
								else{
									write_bad_request(comm_fd);

								}

							}
						}
		//				GET, POST, HEAD get headers
						else if (!headers_done){
							if (space!=string::npos){
								string new_header = current_msg.substr(0, space);
								string new_value = current_msg.substr(space+1);
								size_t crlf = new_value.find(CRLF);
								size_t lf = new_value.find("\n");
								if(crlf != string::npos){
									new_value = new_value.substr(0,crlf);
								}
								else if(lf != string::npos){
									new_value = new_value.substr(0, lf);
								}

								headers.insert({new_header, new_value});
								if(new_header.compare(HTTP_BODY_HEADER) == 0){
									has_data = true;
								}

								if(new_header.compare(HTTP_SIZE_HEADER) == 0){
									data_size = atoi(new_value.c_str());
								}

							}

						}
					}
				}
				bzero(buff, 4096);
			}
		}
	}

	close(comm_fd);
	THREADS.erase(comm_fd);
	if (V_FLAG)
	{
		cerr << "[" << comm_fd << "] " << "Connection closed\r\n";
	}
    pthread_exit(0);
}

//////////////////// MAIN ///////////////////
int main(int argc, char *argv[])
{
	//	Default variables
	int c;
	int p;
	int opt_status;
	V_FLAG = false;
	int listen_socket;
	struct sigaction sigIntHandler;
	string config_file;
	MAIL_SERVER_STATUS = true;
	DRIVE_SERVER_STATUS = true;
	ADMIN_SERVER_STATUS = true;
	AUTH_SERVER_STATUS = true;
	MIDDLE_SERVER_STATUS = true;
	// DRIVEBOX AND MAILBOX
	DRIVEBOX_FILE = map<string, vector < tuple<string, string> > >();
	DRIVEBOX_DIR = map<string, vector < tuple<string, string> > >();

	MAILBOX = map < string, vector < tuple< string, string, string, string, string, string>> >();
	IS_LOGGED_IN = set <string>();


	// PROCESS ARGS
	if (argc < 2) {
		cerr << "*** Author: T11\n";
		exit(1);
	}
	while((c = getopt(argc, argv, "v")) != -1){
		switch(c){
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
	SERVER_NODES = parse_config(config_file);
	for(int i = 0; i < SERVER_NODES['A'].size(); i++ ){
		vector < pair <string, int >> curr_serv = parse_server_info(SERVER_NODES['A'][i]);
		string current_server = curr_serv[0].first+":"+ to_string(curr_serv[0].second);
		AUTH.push_back(make_tuple(current_server, true));
	}
	for(int i = 0; i < SERVER_NODES['L'].size(); i++ ){
		vector < pair <string, int >> curr_serv = parse_server_info(SERVER_NODES['L'][i]);
		string current_server = curr_serv[0].first+":"+ to_string(curr_serv[0].second);
		L_SERVERS.push_back(make_tuple(current_server, true));
	}

	// Get the master address
	vector<pair<string, int> > master_addr = parse_server_info(SERVER_NODES['M'][0]);
	master_ip = master_addr[0].first;
	master_port = master_addr[0].second;


	// Get address of self
	vector<pair<string, int> > self_addr = parse_server_info(SERVER_NODES['F'][INDEX - 1]);
	string self_ip = self_addr[0].first;
	int self_port = self_addr[0].second;
	cout << self_ip << ":" << to_string(self_port) << "\n";

	// Master address string representation
	string master_addr_str = self_addr[1].first + ":" + to_string(self_addr[1].second);
	cout << "Heartbeat addr: " << master_addr_str << endl;
	// Heartbeat handler thread
	pthread_t heartThread;
	struct heartbeatArgs args;
	args.heartAddr = master_addr_str;
	args.killed = &killed;
	pthread_create(&heartThread, NULL, heartbeat_client, &args);
	pthread_detach(heartThread);

	genServers();

//	for(int i =0; i< SERVER_NODES[])
//	for(int i = 0; i < SERVER_NODES['S'].size(); i++ ){
//		SMTP.push_back(SERVER_NODES['S'][i]);
//	}
//	for(int i = 0; i < SERVER_NODES['P'].size(); i++ ){
//		POP3.push_back(SERVER_NODES['P'][i]);
//	}
//	for(int i = 0; i < SERVER_NODES['R'].size(); i++ ){
//		DRIVE.push_back(SERVER_NODES['R'][i]);
//	}
//	CTRLC handler
	sigIntHandler.sa_handler = handle_ctrlc;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler,NULL);


//		initilize socket and basic error check (exit 2 has to do with sockets)
	listen_socket = socket(PF_INET, SOCK_STREAM,0);
	if(listen_socket < 0 && V_FLAG)
	{
		cerr << "Cannot open main socket\r\n";
		exit(2);
	}
	LISTEN_SOCKET = listen_socket;

	int enable = 1;
	opt_status = setsockopt(LISTEN_SOCKET, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	if (opt_status < 0 && V_FLAG)
	{
	    cerr<< "setsockopt failed\r\n";

	}



	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;



//		initialize bind
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(self_port);
	inet_pton(AF_INET, self_ip.c_str(), &servaddr.sin_addr);

	bind(listen_socket, (struct sockaddr*) &servaddr, sizeof(servaddr));

//		initialize listen
	listen(listen_socket, 120);

	cout << "SOCKET AND SUCH SET UP\n";

//		initialize accept
	while (true)
	{
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);
		cout << "LISTENING\n";

			//Accept
		int comm_fd = accept(listen_socket, (struct sockaddr*) &clientaddr, &clientaddrlen);
		cout << "PORT:" <<to_string(clientaddr.sin_port);

		if(comm_fd < 0)
		{
			exit(2);
		}



		CLIENTS.push_back(comm_fd);
		if(V_FLAG)
		{
			cerr << "[" << comm_fd <<"] New connection\r\n";
		}

//			Assign to threads
		pthread_t thread;
		THREADS.insert({comm_fd,thread});
		p = pthread_create(&thread, NULL, client_handler, &comm_fd);
        if (p != 0 && V_FLAG)
            cerr << "pthread create error\r\n";

	}
	exit(0);
}
