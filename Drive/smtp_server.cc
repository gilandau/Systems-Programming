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
#include <netdb.h>
#include "smtp_forward_client.h"

using namespace std;
using json = nlohmann::json;

struct heartbeatArgs
{
  string heartAddr;
  bool* killed;
};

// Keeps count of number of active connections
static int num_active_connections = 0;

// Debug flag
static int debug = 0;

// Vector to store all client_fd
static vector<int> sock_list;

// Local cache of user:key_value_store_ip
static map<string, string> cache;

// ID of this server
static int SERVER_NUM;

// DO_RECV TIMEOUT
static int TIME_OUT = 10;

// Master socket
static int master_sock;

// Check if server is alive or dead
bool killed = false;

// Master ip: port
static int master_port;
static string master_ip;

// Query the key value store for the appropriate data
Packet query_kv_store(string cmmd, string row, string col, vector<string> arg, string data)
{
	// Initialize the packet to be sent
	Packet packet_to_send;
	packet_to_send.set_command(cmmd);
	packet_to_send.set_row(row);
	packet_to_send.set_column(col);
	packet_to_send.set_data(data);

	for(int i = 0; i < arg.size(); i++)
	{
		packet_to_send.add_arg(arg[i]);
	}
	cout << "Packet created in QKVS\n";
	// Check if client present in cache
	map<string, string>::iterator it;
	it = cache.find(row);
	pair<string, int> addr;
	string ip;
	if(it == cache.end())
	{
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
		cout << "IP FROM MASTER: " << ip << endl;
		addr = get_ip_port(ip); 
		cache.insert(make_pair(row, ip));
	}
	else
		addr = get_ip_port((it->second));

	cout << "KVS ADDR: " << addr.first << ":" << addr.second <<"\n";

	// Initialize  socket to connect to KV-store
	int out_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in kvstore_addr;
	bzero(&kvstore_addr, sizeof(kvstore_addr));
	kvstore_addr.sin_family = AF_INET;
	kvstore_addr.sin_port = htons(addr.second);
	inet_pton(AF_INET, (addr.first).c_str(), &(kvstore_addr.sin_addr));
	int stat = connect(out_fd, (struct sockaddr*)&kvstore_addr, sizeof(kvstore_addr));

	while(stat != 0)
	{
		cache.erase(row);
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
		cache.insert(make_pair(row, ip));
		kvstore_addr.sin_port = htons(addr.second);
		inet_pton(AF_INET, (addr.first).c_str(), &(kvstore_addr.sin_addr));
		stat = connect(out_fd, (struct sockaddr*)&kvstore_addr, sizeof(kvstore_addr));
	}
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
	cout << "Sending mail\n";
	
	// Receive response from KV-Store
	char buff[2048];
	int idx = 0;
	idx = do_recv(&out_fd, buff, TIME_OUT);
	buff[idx] = 0;

	while(idx == -1)
	{
		cerr << "KVS Probably Crashed :(...Closing connection.\n";
		close(out_fd);

		// Query master for replica
		stat = -1;
		memset(buff, 0 , sizeof(buff));
		while(stat != 0)
		{
			cache.erase(row);
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
			cache.insert(make_pair(row, ip));
			kvstore_addr.sin_port = htons(addr.second);
			inet_pton(AF_INET, (addr.first).c_str(), &(kvstore_addr.sin_addr));
			stat = connect(out_fd, (struct sockaddr*)&kvstore_addr, sizeof(kvstore_addr));
		}
		do_send(&out_fd, response, sizeof(response));
		cout << "Sending mail\n";
		idx = do_recv(&out_fd, buff, TIME_OUT);
		buff[idx] = 0;
	}

	Packet packet_rcvd;
	// Parse the message to extract the content
	if(!packet_rcvd.ParseFromString(buff))
	{
		cerr << "Failed to parse received message!\n";
	}
	cout << "Exiting KVS\n";

	// Send quit to KVS and close socket
	packet_to_send.Clear();
	packet_to_send.set_command("QUIT");
	send_packet(packet_to_send, out_fd);
	close(out_fd);

	return packet_rcvd;
}

// Add mail to sent mail
void add_to_sent_mail(string mail_id, int mail_size, string sender)
{
	// Add mail to sent mail field
	string sent_meta = "500";
	while(sent_meta != "200")
	{
		vector<string> arg;
		Packet pack = query_kv_store("GET", sender, "SENTMAIL", arg, "");

		json json_obj;
		string json_str;
		string old;

		// Check if response is positive
		if(pack.status_code() == "200")
		{
			string json_str = (pack.data()).substr(1);
			old = pack.data();
			json_obj = json::parse(json_str);
		}
		if(json_obj.find(mail_id) == json_obj.end())
				json_obj[mail_id] = to_string(mail_size);

		string to_send = "$" + json_obj.dump();
		pack.Clear();
		arg.push_back(old);
		pack = query_kv_store("CPUT", sender, "SENTMAIL", arg, to_send);
		sent_meta = pack.status_code();
		cout << "SENT status code: " << sent_meta << endl;
	}
}

// Add receiver to the contacts of the sender
void add_to_contacts(string sender, string receiver)
{
	// Add mail to sent mail field
	string addr_meta = "500";
	while(addr_meta != "200")
	{
		vector<string> arg;
		Packet pack = query_kv_store("GET", sender, "ADDRBK", arg, "");

		json json_obj = json::array();
		string json_str;
		string old;

		// Check if response is positive
		if(pack.status_code() == "200")
		{
			string json_str = (pack.data()).substr(1);
			old = pack.data();
			json_obj = json::parse(json_str);
		}
		if(json_obj.find(receiver) == json_obj.end())
				json_obj.push_back(receiver);

		string to_send = "$" + json_obj.dump();
		pack.Clear();
		arg.push_back(old);
		pack = query_kv_store("CPUT", sender, "ADDRBK", arg, to_send);
		addr_meta = pack.status_code();
		cout << "ADDRBK status code: " << addr_meta << endl;
	}
}


// Write mail stored in tmp file to the mailbox; 1:success, 2: usr not present, 3:client servers down, 0:transaction error
int write_to_mailbox(string tmp_file, string rcvr, vector<string> header, string sender, int flag)
{
	// Initialize the mail to be sent
	string mail = "";

	for(int i = 0; i < header.size(); i++)
		mail = mail + header[i];

	// Read mail from file to send to  KV-Store
	string line;
	ifstream myfile;
	myfile.open(tmp_file, ios::in);
	while(getline(myfile, line))
	{
		cout << line << endl;
		if(flag == 1)
		{
			if(line.substr(0,3) != "To:" && line.substr(0,5) != "From:" && line.substr(0,5) != "Date:" && line.substr(0,10) != "Message-ID" && line.substr(0,8) != "Subject:")
			{
				if(line.substr(0,11) == "User-Agent:")
				{
					mail = mail + line.substr(0, line.size() - 1);
					getline(myfile, line);
					mail = mail + line + "\n";
				}
				else
					mail = mail + line + "\n";
			}
		}
		else
			if(line.substr(0, 8) != "Subject:")
				mail = mail + line + "\n";
	}

	// Generate unique mail id
	string mail_id = generate_uidl(mail);
	cout << "Mail ID: " << mail_id << endl;
	cout << "Mail: \n";
	cout << mail << endl;

	vector<string> arg;
	Packet packet_rcvd = query_kv_store("PUT", rcvr, mail_id, arg, mail);

	if(packet_rcvd.status_code() == "200")
	{
		cout << "successfully sent mail to KVS\n";
		if(sender.find("penncloud") != string::npos)
		{
			add_to_sent_mail(mail_id, mail.size(), sender);
			add_to_contacts(sender, rcvr);
		}

		// Repeat until meta data successfully stored
		string meta_status = "500";

		while(meta_status != "200")
		{
			packet_rcvd.Clear();
			arg.clear();
			packet_rcvd = query_kv_store("GET", rcvr, "MAIL_META", arg, "");

			// Check if server is still up
			if(packet_rcvd.status_code() == "601")
			{
				cout << "All Client servers down\n";
				return 3;
			}

			// Check if mail data successfully received
			json json_obj;
			string old;
			if(packet_rcvd.status_code() == "200")
			{
				string json_str = (packet_rcvd.data()).substr(1);
				old = packet_rcvd.data();
				json_obj = json::parse(json_str);
			}
			if(json_obj.find(mail_id) == json_obj.end())
				json_obj[mail_id] = to_string(mail.size());

			string to_send = "$" + json_obj.dump();
			packet_rcvd.Clear();
			arg.push_back(old);
			packet_rcvd = query_kv_store("CPUT", rcvr, "MAIL_META", arg, to_send);
			meta_status = packet_rcvd.status_code();
		}
		return 1;
	}
	else if(packet_rcvd.status_code() == "600")
	{
		cout << "User not present in the system.\n";
		return 2;
	}
	else if(packet_rcvd.status_code() == "601")
	{
		cout << "All client servers down\n";
		return 3;
	}
	else
	{
		cout << "Failed sending mail\n";
		return 0;
	}
}

// Checks if a given adderss is valid user@domain
int is_valid(string e_addr)
{
	int sz = e_addr.size();
	int idx = e_addr.find("@");
	int cnt = count(e_addr.begin(), e_addr.end(), '@');
	string ip;
	pair<string, int> addr;

	if(idx != string::npos && cnt == 1)
	{
		if(e_addr.substr(idx + 1) == "penncloud")
		{
			// Check if user account valid
			cout << "Client to query: " << e_addr.substr(1, e_addr.size() - 1) << endl;
			ip = query_master(e_addr.substr(1, e_addr.size() - 1), "WHERIS", master_sock, master_port, master_ip);
			if(ip == "ABSENT")
			{
				cout << "Client not present in the system\n";
				return -1;
			}
			else if(ip == "SERVERFAIL")
			{
				cout << "Server Failure\n";
				return -1;
			}
			cout << "IP FROM MASTER: " << ip << endl;
			addr = get_ip_port(ip); 
			cache.insert(make_pair(e_addr, ip));
			return 1;
		}
		else if(idx < sz - 1 && idx > 0)
			return 1;
	}
	return -1;
}

// Write contents of a mail to file
void write_to_file(string filename, char *mail, int bytes)
{
	ofstream myfile;
	myfile.open(filename, ios::app);
	for(int i = 0; i < bytes; i++)
	{
		myfile << mail[i];
	}
	myfile.close();
}

// Extract the subject from the file
string extract_subject(string tmp_file)
{
	ifstream myfile;
	myfile.open(tmp_file, ios::in);
	string line;
	string sub;
	while(getline(myfile, line))
	{
		if(line.substr(0, 8) == "Subject:")
			sub = line.substr(8, line.size() - 8);
	}
	myfile.close();
	return sub;
}

// Generate mail header
vector<string> generate_mail_header(string sender, string time_stamp, string subject)
{
	cout << "Generating mail header...\n";
	vector<string> header;
	string from = "From: " + sender + "\r\n";
	string date = "Date: " + time_stamp.substr(0, time_stamp.size() - 1) + "\r\n";
	string prefix = generate_uidl(sender + time_stamp);
	string msg_id = "Message-ID: <" + prefix + "@seas.upenn.edu>\r\n";
	string sub = "Subject: " + subject + "\r\n";
	header.push_back(from);
	header.push_back(date);
	header.push_back(msg_id);
	//header.push_back(browser_info);
	header.push_back(sub);
	cout << "Mail headers generated\n";
	return header;
}

// Checks if the string is a valid binary string
int is_binary(string str)
{
	for(int i = 0; i < str.size(); i++)
	{
		if(str[i] != '0' && str[i] != '1')
			return 0;
	}
	return 1;
}

// Store contents of buff in packet format
Packet make_packet(char* buff)
{
	// IF the source is FES/Admin console, source = 0, else source = 1
	int source;
	string rcvd = string(buff);
	Packet pack;

	/*
	First 10 bytes of input stream
	is always a binary string indicating the no. of bytes that follow
	*/
	cout << "SUBSTR: " << rcvd.substr(0,22) << endl;
	cout << "Is binary: " << is_binary(rcvd.substr(0,22)) << endl;
	if(rcvd.size() >= 22 && is_binary(rcvd.substr(0,22)))
	{
		cout << "Pakcet received from middleload/admin\n";
		string sz_str = rcvd.substr(0,22);
		int sz = stoi(sz_str, nullptr, 2);
		buff[22 + sz] = 0;

		// Deserialize buff to packet
		if(!pack.ParseFromString(&buff[22]))
		{
			cerr << "Failed to parse received message!\n";
		}
		return pack; 
	}
	else
	{
		cout << "Packet received from remote client\n";
		// Input is from remote client, extract space separated tokens
		transform(rcvd.begin(), rcvd.end(), rcvd.begin(), ::tolower);
		if(rcvd.substr(0, 5) == "helo ")
		{
			pack.set_command("helo");
			pack.add_arg(rcvd.substr(5, rcvd.size() - 7));
			return pack;
		}
		else if(rcvd.substr(0, 10) == "mail from:")
		{
			pack.set_command("mail from");
			pack.add_arg(rcvd.substr(10, rcvd.size() - 12));
			return pack;
		}
		else if(rcvd.substr(0, 8) == "rcpt to:")
		{
			pack.set_command("rcpt to");
			pack.add_arg(rcvd.substr(8, rcvd.size() - 10));
			return pack;
		}
		else if(rcvd.substr(0, 4) == "data")
		{
			pack.set_command("data");
			return pack;
		}
		else if(rcvd.substr(0, 4) == "rset")
		{
			pack.set_command("rset");
			return pack;
		}
		else if(rcvd.substr(0, 4) == "noop")
		{
			pack.set_command("rset");
			return pack;
		}
		else if(rcvd.substr(0, 4) == "quit")
		{
			pack.set_command("rset");
			return pack;
		}
		else
		{
			pack.set_command("Unknown");
			return pack;
		}
	}
}

// Determine whether the input was received from within the system or remote user
int get_sender_source(char* buff)
{
	string rcvd = string(buff);
	if(rcvd.size() < 10)
		return 1;
	else
	{
		string sz_str = rcvd.substr(0,10);
		if(is_binary(sz_str))
			return 0;
		return 1;
	}
}

// Send response based on the flag
void send_response(string status_code, string status, int client_fd, int flag)
{
	if(flag == 0)
	{
		// Send response to frontend server/admin console
		Packet pack;
		pack.set_status_code(status_code);
		pack.set_status(status);
		send_packet(pack, client_fd);
	}
	else
	{
		// send response to remote client
		string to_send = status_code + " " + status + "\r\n";
		char buff[to_send.size()];
		to_send.copy(buff, to_send.size());
		send_t(&client_fd, buff, sizeof(buff));
	}
}

// Thread function to communicate with the client
void *communicate(void *arg)
{
	// Initialize the state of the client
	int client_fd = *(int *)arg;
	string mail_sender = "";
	vector<string> receivers;
	vector<string>non_local_recipients;
	Packet packet_to_send;

	// State of connection: connected, authorized, transaction
	string state = "connected";

	char msg[] = "220 Penncloud Simple Mail Transfer Service Ready\r\n";
	cout << "Sending greeting\n";
	send_t(&client_fd, msg, sizeof(msg)-1);
	
	while(true)
	{
		char buff[2048];
		int idx = 0;
		// Idx stores the number of bytes
		idx = recv_t(&client_fd, &buff[0], 10);
		if(idx == -1)
		{
			cerr << "Error reading COMMAND. Exiting...\n";
			close(client_fd);
			sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
			num_active_connections--;
			pthread_exit(NULL);
		}
		buff[idx] = 0;
		cout << "Received Input: " << buff << endl;

		// Tranfer contents of buff into a packet
		Packet packet_rcvd = make_packet(buff);

		// Flag = 0 if comm. from frontend or admin; 1 if from remote client
		int flag = get_sender_source(buff);

		// Obtain the relevant message fields
		string command = packet_rcvd.command();
		transform(command.begin(), command.end(), command.begin(), ::tolower);
		cout << "Command: " + command  + "\n";

		if(killed)
		{
			if(command == "restart")
			{
				killed = false;
				Packet pack;
				pack.set_command("OK");
				pack.set_status_code("+OK");
				pack.set_status("Bring me to life.");
				send_packet(pack, client_fd);
				close(client_fd);
				num_active_connections--;
				pthread_exit(NULL);
			}
			else
			{
				close(client_fd);
				num_active_connections--;
				pthread_exit(NULL);
			}
		}

		// Execute the appropreate commands
		if(command == "quit")
		{
			send_response("221", "Localhost Service closing transmission channel", client_fd, flag);
			close(client_fd);
			sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
			num_active_connections--;
			pthread_exit(NULL);
		}
		else if(command == "helo")
		{
			string domain_name = packet_rcvd.arg(0);
			
			if(domain_name.empty() || domain_name == " ")
			{
				send_response("501", "Invalid domain name", client_fd, flag);
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				num_active_connections--;
				pthread_exit(NULL);
			}
			else
			{
				send_response("250", "PennCloud_MX", client_fd, flag);
				state = "authorized";
			}
		}
		else if(command == "mail from")
		{
			string sender = packet_rcvd.arg(0);
			int status = is_valid(sender);
			cout << "Sender: " << sender << endl;

			if(state != "authorized")
			{
				send_response("503", "Bad sequence of commands", client_fd, flag);
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				num_active_connections--;
				pthread_exit(NULL);
			}
			else if(status == 1)
			{
				send_response("250", "OK Received Mail From", client_fd, flag);
				mail_sender = sender.substr(1, sender.size() - 2);
				state = "transaction";
			}
			else
			{
				send_response("501", "Invalid sender address", client_fd, flag);
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				num_active_connections--;
				pthread_exit(NULL);
			}
		}
		else if(command == "rcpt to")
		{
			string rcvr = packet_rcvd.arg(0);
			int recv_idx = is_valid(rcvr);
			cout << "Receiver: " << rcvr << endl;

			if(mail_sender == "" || state != "transaction")
			{
				send_response("503", "Bad sequence of commands", client_fd, flag);
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				num_active_connections--;
				pthread_exit(NULL);
			}
			else if(rcvr.find("penncloud") != string::npos && recv_idx != -1)
			{
				send_response("250", "OK Received Recipient", client_fd, flag);
				receivers.push_back(rcvr.substr(1, rcvr.size() - 2));
			}
			else
			{
				if(recv_idx != -1)
				{
					send_response("251", "Message will be forwarded to non-local recipient.", client_fd, flag);
					non_local_recipients.push_back(rcvr.substr(1, rcvr.size() - 2));
				}
				else
				{
					send_response("501", "Invalid email id.", client_fd, flag);
					close(client_fd);
					sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
					num_active_connections--;
					pthread_exit(NULL);
				}
			}
		}
		else if(command == "data")
		{
			if(mail_sender != "" && (receivers.size() != 0 || non_local_recipients.size() != 0))
			{
				if(state != "transaction")
				{
					send_response("503", "Bad sequence of commands", client_fd, flag);
					close(client_fd);
					sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
					num_active_connections--;
					pthread_exit(NULL);
				}
				else
				{
					send_response("354", "Start Mail Input", client_fd, flag);

					// Initialize buffer to store received data
					char mail[2048];
					int bytes = 0;
					string tmp_name = "tmp" + to_string(client_fd);
					ofstream myfile;
					myfile.open(tmp_name, ios::app);

					// Initailize string to store time stamp and packet to store received data
					stringstream st;
					string time_stamp;
					vector<string> header;
					string subject;
					cout<<"before\n";

					while(true)
					{
						bytes = recv_t(&client_fd, &mail[0], TIME_OUT);
						if(bytes == -1)
						{
							cerr << "Error reading DATA. Exiting...\n";
							close(client_fd);
							sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
							num_active_connections--;
							pthread_exit(NULL);
						}

						// Check for end of mail
						if(bytes == 3  && mail[bytes-1] == '\n' && mail[bytes-2] == '\r' && mail[bytes - 3] == '.')
						{
							// Get the time stamp after receiving all the data
							chrono::system_clock::time_point curr = chrono::system_clock::now();
							time_t t = chrono::system_clock::to_time_t(curr);
							st << ctime(&t);
							time_stamp = st.str();
							subject = extract_subject(tmp_name);
							//if(flag == 0)
							header = generate_mail_header(mail_sender, time_stamp, subject);
							break;
						}
						mail[bytes] = 0;
						cout << "[C]: " + string(mail);
						write_to_file(tmp_name, &mail[0], bytes);
					}
					
					cout << "Bytes of mail received: " << bytes << endl;
					cout << "Sending mail..." << endl;

					// m_flag is set to 0 if any mail is failed to be delivered
					int m_flag = 1;

					// Vector to maintain the status of each mail
					vector<int> sent_mail_status;

					// Write the contents of mail to a mailbox
					for(int x = 0; x < receivers.size(); x++)
					{
						//if(flag == 0)
						header.insert(header.begin() + 1, "To: " + receivers[x] + "\r\n");
						int stat = write_to_mailbox(tmp_name, receivers[x], header, mail_sender, flag);
						if(stat == 0 || stat == 2 || stat == 3)
							m_flag = 0;
						sent_mail_status.push_back(stat);
						//if(flag == 0)
						header.erase(header.begin() + 1);
					}

					// Forward mail to non-local clients
					vector<int> status = forward_mail(tmp_name, header, non_local_recipients, mail_sender, flag);

					// Check if any mails could not be delivered
					for(int i = 0; i < status.size(); i++)
					{
						if(status[i] == 0)
						{
							m_flag = 0;
							break;
						}
					}

					// Prepare response to mail sender
					string status_code;
					string status_t;
					if(m_flag == 1)
					{
						status_code = "250";
						status_t = "OK Mail Data received";
					}
					else
					{
						status_code = "554";
						status_t = "Mail Transaction Failed to the following clients";
					}

					// Clear the buffers i
					receivers.clear();
					non_local_recipients.clear();
					mail_sender = "";
					remove(tmp_name.c_str());

					send_response(status_code, status_t, client_fd, flag);

					// Mark this transaction as complete and go back to authorized state
					state = "authorized";
				}
			}
			else
			{
				send_response("550", "Sender/Receiver not specified", client_fd, flag);
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				num_active_connections--;
				pthread_exit(NULL);
			}
		}
		else if(command == "rset")
		{
			// If rset occurs before helo, return error
			if(state == "connected")
			{
				send_response("503", "Bad sequence of commands", client_fd, flag);
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				num_active_connections--;
				pthread_exit(NULL);
			}
			else
			{
				// Clear all buffers
				mail_sender = "";
				receivers.clear();
				non_local_recipients.clear();
				send_response("250", "OK", client_fd, flag);
				state = "authorized";
			}
		}
		else if(command == "noop")
		{
			send_response("250", "OK", client_fd, flag);
		}
		else if(command == "curload")
		{
			send_num_connections(client_fd, num_active_connections - 1);
			close(client_fd);
			sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
			num_active_connections--;
			pthread_exit(NULL);
		}
		else if(command == "kill")
		{
			killed = true;
			packet_to_send.set_command("OK");
			packet_to_send.set_status("Commiting suicide");
			packet_to_send.set_status_code("+OK");
			send_packet(packet_to_send, client_fd);
			close(client_fd);
			num_active_connections--;
			pthread_exit(NULL);
		}
		else
		{
			send_response("502", "Command not implemented", client_fd, flag);
		}
		packet_to_send.Clear();
		packet_rcvd.Clear();
	}
}

// Routine to handle SIGINT interrupt
void int_handler_routine(int sig)
{
	char msg[] = "-ERR Server shutting down\r\n";
	for(int i = 0; i < sock_list.size(); i++)
	{
		do_send(&sock_list[i], msg, sizeof(msg));
		close(sock_list[i]);
	}
	exit(0);
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
	if(argc != idx + 2)
	{
		cerr << "Incorrect Arguments.\nFormat: ./smtp_server configfile index\nAdditional -v and -o options can be specified.\n";
		exit(1);
	}
	string configfile = string(argv[idx]);
	SERVER_NUM = stoi(string(argv[idx + 1]));

	// Map of all server addresses
	map <char, vector<string> > addr_list = parse_config(configfile);

	// Get the master address
	vector<pair<string, int> > master_addr = parse_server_info(addr_list['M'][0]);
	master_ip = master_addr[0].first;
	master_port = master_addr[0].second;


	// Get address of self
	vector<pair<string, int> > self_addr = parse_server_info(addr_list['S'][SERVER_NUM - 1]);
	string self_ip = self_addr[0].first;
	self_port = self_addr[0].second;

	// Master address string representation
	string master_addr_str = self_addr[1].first + ":" + to_string(self_addr[1].second);
	cout << "Heartbeat addr: " << master_addr_str << endl;

	pthread_t heartThread;
	struct heartbeatArgs args;
	args.heartAddr = master_addr_str;
	args.killed = &killed;
	pthread_create(&heartThread, NULL, heartbeat_client, &args);
	pthread_detach(heartThread);

	//Setup to handle interrupts
	struct sigaction int_handler;
	int_handler.sa_handler = int_handler_routine;
	sigemptyset(&int_handler.sa_mask);
	int_handler.sa_flags = 0;
	
	// Setup smtp server socket to listen to connections
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(self_port);
	inet_pton(AF_INET, self_ip.c_str(), &(server_addr.sin_addr));
	bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	listen(listen_fd, SOMAXCONN);
	

	// Accept connections from multiple clients
	cout << "Ready to accept new connections...\n";
	while(true)
	{
		// Handling Ctrl + C interrupt
		sigaction(SIGINT, &int_handler, NULL);

		// Handling connections from front end servers
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		int client_fd;
		client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		sock_list.push_back(client_fd);
		Packet packet_to_send;

		if(client_fd >= 0)
		{
			//Printing connection
			string con = "New Connection\n";
			cout<<"New connection\n";
			// Pass connection to thread
			pthread_t thread;
			pthread_create(&thread, NULL, communicate, (void *)&client_fd);
			num_active_connections++;
		}
		else
		{
			cerr << "Connection could not be established.\n";
		}
	}
	return 0;
}
