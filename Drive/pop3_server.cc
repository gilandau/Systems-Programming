#include <openssl/md5.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> 
#include <algorithm>
#include <signal.h>
#include <iostream>
#include <getopt.h>
#include <vector>
#include <sys/types.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include "utility.h"
#include "DATA_PACKET.pb.h"
#include <openssl/md5.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
struct heartbeatArgs
{
  string heartAddr;
  bool* killed;
};

using namespace std;

// Debug flag
static int debug = 0;

// Keeps count of number of active connections
static int num_active_connections = 0;

// Vector to store all client_fd
static vector<int> sock_list;

// Cache to store the IP of the KV-store where the client data is present
map<string, string> cache;

// Store client password
map<string, string> pass;

// Mutex to write to file
map<string, pthread_mutex_t> locks;

// DO_RECV TIMEOUT
static int TIME_OUT = 10;

// Master socket
static int master_sock;

// Check if the server is alive or dead
bool killed = false;

// Master ip: port
static int master_port;
static string master_ip;

// Self ID number
static int SERVER_NUM;


// void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer)
// {
//    The digest will be written to digestBuffer, which must be at least MD5_DIGEST_LENGTH bytes long 

//   MD5_CTX c;
//   MD5_Init(&c);
//   MD5_Update(&c, data, dataLengthBytes);
//   MD5_Final(digestBuffer, &c);
// }

// Check if user is present in the system
int is_present(string user)
{
	// Check if client present in cache
	map<string, string>::iterator it;
	it = cache.find(user);
	pair<string, int> addr;

	if(it == cache.end())
	{
		cout << "Querying mastr\n";
		string ip = query_master(user, "WHERIS", master_sock, master_port, master_ip);
		if(ip == "ABSENT")
			return -1;
		cout << "IP: " << ip << endl;

		addr = get_ip_port(ip); 
		cout << "IP: " << ip << endl;

		cache.insert(make_pair(user, ip));
	}
	return 1;
}

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
	cout << addr.first << ":" << addr.second <<"\n";

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
	cout << "Connected\n";
	
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
			memset(buff,0,sizeof(buff));
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
	cout << "Packet received\n";
	
	Packet packet_rcvd;
	cout<<"Buff: "<<buff<<endl;
	// Parse the message to extract the content
	if(!packet_rcvd.ParseFromString(buff))
	{
		cerr << "Failed to parse received message!\n";
	}

	// Send quit to KVS and close socket
	packet_to_send.Clear();
	packet_to_send.set_command("QUIT");
	send_packet(packet_to_send, out_fd);
	close(out_fd);

	return packet_rcvd;
}


// Function returns the drop listing (Number_of_messages : total size)
string get_drop_listing(map<int, pair<string, string> > mail_meta_data, vector<int> marked_as_deleted)
{
	int num_msg = 0;
	int tot_sz = 0;
	string listing;
	for(int i = 0; i < mail_meta_data.size(); i++)
	{
		if(marked_as_deleted[i] != 1)
		{
			num_msg = num_msg + 1;
			tot_sz = tot_sz + stoi(mail_meta_data[i + 1].second);
		}
	}
	listing = to_string(num_msg) + " " + to_string(tot_sz);
	return listing;
}

// Function returns the scan listing
vector<string> get_scan_listing(map<int, pair<string, string> > mail_meta_data, int arg, vector<int> marked_as_deleted)
{
	vector<string> res;
	pair<string, string> tmp;
	if(arg != -1)
	{
		// Check if arg refers to a valid email
		if(mail_meta_data.size() >= arg && marked_as_deleted[arg - 1] != 0)
		{
			tmp = mail_meta_data[arg];
			string s = to_string(arg) + " " + tmp.second;
			res.push_back(s);
		}
		else
			res.push_back("INVALID");
	}
	else
	{
		int i = 0;
		for(map<int, pair<string, string> >::iterator it = mail_meta_data.begin(); it != mail_meta_data.end(); it++)
		{
			if(marked_as_deleted[i] != 1)
			{
				string s = to_string(i + 1) + " " + (it->second).second;
				res.push_back(s);
			}
			i = i + 1;
		}
	}
	return res;
}

// Function to update marked messages as important
int update_mbox(string client_name, vector<int> marked_as_imp, map<int, pair<string, string> > mail_meta_data)
{
	cout << "In update mbox\n";
	// Add the important mails to IMP
	Packet packet_rcvd;
	string meta_status = "500";
	vector<string> arg;
	while(meta_status != "200")
	{
		json json_obj = json::array();
		string json_str;
		packet_rcvd.Clear();
		arg.clear();
		string old;

		packet_rcvd = query_kv_store("GET", client_name, "IMP", arg, "");

		// Check if server is still up
		if(packet_rcvd.status_code() == "601")
		{
			cout << "All Client servers down\n";
			return 0;
		}
		else if(packet_rcvd.status_code() == "200")
		{
			string json_str = (packet_rcvd.data()).substr(1);
			old = packet_rcvd.data();
			json_obj = json::parse(json_str);
		}

		// Add the starred mails to existing set
		for(int i = 0; i < marked_as_imp.size(); i++)
		{
			if(marked_as_imp[i] == 1 && json_obj.find(mail_meta_data[i + 1].first) != json_obj.end())
			{
				json_obj.push_back(mail_meta_data[i + 1].first);
			}
		}
		string to_send = "$" + json_obj.dump();
		packet_rcvd.Clear();
		arg.push_back(old);
		packet_rcvd = query_kv_store("CPUT", client_name, "IMP", arg, to_send);
		meta_status = packet_rcvd.status_code();
		cout << "Meta status: " << meta_status << endl;
	}
	return 1;
}

//Function to remove the messages marked as DELETE
int clean_up_mbox(string client_name, vector<int> marked_as_deleted, map<int, pair<string, string> > mail_meta_data)
{
	vector<string> arg;
	for(int i = 0; i < marked_as_deleted.size(); i++)
	{
		if(marked_as_deleted[i] == 1)
		{
			Packet response = query_kv_store("DEL", client_name, mail_meta_data[i+1].first, arg, "");
		}
	}
	cout << "Mails Deleted\n";

	// Delete the deleted mails from meta data
	Packet packet_rcvd;
	string meta_status = "500";

	while(meta_status != "200")
	{
		json json_obj;
		packet_rcvd.Clear();
		arg.clear();
		string old;

		packet_rcvd = query_kv_store("GET", client_name, "MAIL_META", arg, "");

		// Check if server is still up
		if(packet_rcvd.status_code() == "601")
		{
			cout << "All Client servers down\n";
			return 0;
		}

		if(packet_rcvd.status_code() == "200")
		{
			string json_str = (packet_rcvd.data()).substr(1);
			old = packet_rcvd.data();
			json_obj = json::parse(json_str);
		}
		if(packet_rcvd.status_code() == "500")
		{
			return 1;
		}

		for(int i = 0; i < marked_as_deleted.size(); i++)
		{
			if(marked_as_deleted[i] == 1 && json_obj.find(mail_meta_data[i + 1].first) != json_obj.end())
			{
				json_obj.erase(mail_meta_data[i + 1].first);
			}
		}
		string to_send = "$" + json_obj.dump();
		packet_rcvd.Clear();
		arg.push_back(old);
		packet_rcvd = query_kv_store("CPUT", client_name, "MAIL_META", arg, to_send);
		meta_status = packet_rcvd.status_code();
		cout << "Meta status: " << meta_status << endl;
	}
	return 1;
}

// Retrieves message for client
Packet retrieve_message(map<int, pair<string, string> > mail_meta_data, int msg_num, string client_name)
{
	vector<string> arg;
	Packet packet_rcvd = query_kv_store("GET", client_name, mail_meta_data[msg_num].first, arg, "");
	return packet_rcvd;
}

// Thread function to communicate with the client
void *communicate(void *arg)
{
	int client_fd = *(int *)arg;

	// Initialize client variables
	string client_name;
	string prev_cmmd = "";
	map<int, pair<string, string> > mail_meta_data;
	vector<int> marked_as_deleted;
	vector<int> marked_as_imp;

	// Buffer to store input
	char buff[1024];

	// Flag to maintain state of the client: unauthorized, transaction, update
	string state = "authorization";

	while(true)
	{
		// Clear previous command
		memset(buff, 0, 1024);

		int idx = 0;

		// Idx storest the number of bytes
		idx = do_recv(&client_fd, buff, TIME_OUT);
		if(idx == -1)
		{
			cerr << "Error reading COMMAND. Exiting...\n";
			close(client_fd);
			sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
			pthread_exit(NULL);
		}

		// Set \r\n to 0 and then transfer contents of the buffer to a string
		buff[idx] = 0;
		Packet packet_rcvd;
		Packet packet_to_send;
		// Parse the message to extract the content
		if(!packet_rcvd.ParseFromString(buff))
		{
			cerr << "Failed to parse received message!\n";
			num_active_connections--;
			pthread_exit(NULL);
		}

		// Obtain the relevant message fields
		string command = packet_rcvd.command();
		cout<<"CMD: "<<command<<endl;
		transform(command.begin(), command.end(), command.begin(), ::tolower);
		cout << command << endl;

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

		if(command == "quit")
		{
			if(state == "transaction")
			{
				state = "update";
				// Inform KV-store to delete the marked messages
				clean_up_mbox(client_name, marked_as_deleted, mail_meta_data);

				// Inform KV-store to add important messages to IMP
				update_mbox(client_name, marked_as_imp, mail_meta_data);
			}
			pthread_mutex_unlock(&locks[client_name]);

			// Initialize the packet to send
			packet_to_send.set_status_code("+OK");
			packet_to_send.set_status(" POP3 [localhost] closing connection");
			send_packet(packet_to_send, client_fd);

			// Close connection with client
			close(client_fd);
			sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
			pthread_exit(NULL);

			prev_cmmd = "QUIT";
		}
		else if(command == "stat")
		{
			if(state != "transaction")
			{
				send_error_pack(client_fd, "-ERR", "Invalid Command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			else
			{
				string listing;
				cout << "Size of mail_meta_data: " << mail_meta_data.size() << endl;
				if(mail_meta_data.size() == 0)
					listing = "0 0";
				else
					listing = get_drop_listing(mail_meta_data, marked_as_deleted);
				cout << "Listing: " << listing << endl;

				// Initialize the packet to send
				packet_to_send.set_status_code("+OK");
				packet_to_send.set_status(listing);
				send_packet(packet_to_send, client_fd);
			}
			prev_cmmd = "STAT";
		}
		else if(command == "user")
		{

			// Check if in correct state
			if(state != "authorization")
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			else
			{
				// Check if user is present on localhost
				string user = packet_rcvd.arg(0);
				int user_idx = is_present(user);
				cout << "USER: " << user << endl;
				if(user_idx != -1)
				{
					client_name = user;
					cout << "Client Name: " << client_name << endl;
					// Initialize lock for this user if not already initialized
					if(locks.find(user) == locks.end())
					{
						pthread_mutex_t lock  = PTHREAD_MUTEX_INITIALIZER;
						locks.insert(make_pair(client_name, lock));
					}

					// Initialize the packet to send
					packet_to_send.set_status_code("+OK");
					packet_to_send.set_status("MBOX is valid");
					send_packet(packet_to_send, client_fd);
				}
				else
				{
					send_error_pack(client_fd, "-ERR", "Invalid User");
					close(client_fd);
					sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
					pthread_exit(NULL);
				}
			}
			prev_cmmd = "USER";
		}
		else if(command == "pass")
		{
			// Validate user by comparing password and state
			string pwd = packet_rcvd.arg(0);
			cout << pwd <<"\n";
			if(!client_name.empty() && prev_cmmd == "USER" && state == "authorization")
			{
				string password;
				vector<string> arg;
				cout << "CHECKING PASSWORD " << "\n";
				// Check if the password is cached on the server
				// Otherwise get pass word from KV-Store
				if(pass.find(client_name) == pass.end())
				{
					cout << "Getting PASSWORD " << "\n";

					password = (query_kv_store("GET", client_name, "PASSWORD", arg, "")).data();
					pass.insert(make_pair(client_name, password));

				}
				else
					password = pass[client_name];
				cout << "Password: " << password << endl;
				// Check is the two passwords match
				if(password == pwd)
				{
					int flag = pthread_mutex_trylock(&locks[client_name]);
					if(flag != 0)
					{
						send_error_pack(client_fd, "-ERR", "Mailbox already locked");
						close(client_fd);
						sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
						pthread_exit(NULL);

						// Wait for lock to be released
						while(true)
						{
							if(pthread_mutex_trylock(&locks[client_name]) == 0)
								break;
						}
					}
					// Initialize the packet to send
					packet_to_send.set_status_code("+OK");
					packet_to_send.set_status("Mailbox locked");
					send_packet(packet_to_send, client_fd);

					state = "transaction";

					// On successful login, query the KV-Store for the mail metadata
					vector<string> arg;
					Packet pack = query_kv_store("GET", client_name, "MAIL_META", arg, "");
					json json_obj;
					if(pack.status_code() == "200")
					{
						string json_str = (pack.data()).substr(1);
						json_obj = json::parse(json_str);

						int i = 1;
						for(json::iterator it = json_obj.begin(); it != json_obj.end(); it++)
						{
							mail_meta_data.insert(make_pair(i, make_pair(it.key(), it.value())));
							i = i + 1;
						}
					}

					// Initialize the marked as deleted vector
					for(int i = 0; i < mail_meta_data.size(); i++)
					{
						marked_as_deleted.push_back(0);
						marked_as_imp.push_back(0);
					}
					cout << "Password validated..\n";
				}
				else
				{
					send_error_pack(client_fd, "-ERR", "Invalid password");
					close(client_fd);
					sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
					pthread_exit(NULL);
				}
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "PASS";
		}
		else if(command == "list")
		{
			if(state == "transaction")
			{
				string arg = packet_rcvd.arg(0);;

				// -1 indicates that no argument given
				if(arg.empty())
					arg = "-1";

				if(arg == "-1" || is_valid_int(arg) == 1)
				{
					// Get scan listing
					vector<string> listing = get_scan_listing(mail_meta_data, stoi(arg), marked_as_deleted);

					if(listing[0] == "INVALID")
					{
						send_error_pack(client_fd, "-ERR", "Reference to invalid message");
						close(client_fd);
						sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
						pthread_exit(NULL);
					}
					else
					{
						// Initialize the packet to send
						packet_to_send.set_status_code("+OK");
						packet_to_send.set_status(to_string(listing.size()) + " messages");
						send_packet(packet_to_send, client_fd);
						packet_to_send.Clear();

						for(int i = 0; i < listing.size(); i++)
						{
							packet_to_send.set_data(listing[i]);
							send_packet(packet_to_send, client_fd);
						}
						packet_to_send.set_data(".");
						send_packet(packet_to_send, client_fd);
					}
				}
				else
				{
					send_error_pack(client_fd, "-ERR", "Invalid argument");
					close(client_fd);
					sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
					pthread_exit(NULL);
				}
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "LIST";
		}
		else if(command == "retr")
		{
			if(state == "transaction")
			{
				// Get msg number
				string arg = packet_rcvd.arg(0);
				if(is_valid_int(arg) == 1)
				{
					Packet pack = retrieve_message(mail_meta_data, stoi(arg), client_name);

					if(pack.status_code() == "200")
					{
						// Initialize the packet to send
						packet_to_send.set_status_code("+OK");
						packet_to_send.set_status("Message In Data Field");
						packet_to_send.set_data(pack.data());
						send_packet(packet_to_send, client_fd);
					}
					else
					{
						send_error_pack(client_fd, "-ERR", "Reference to invalid mail");
						close(client_fd);
						sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
						pthread_exit(NULL);
					}
				}
				else
				{
					send_error_pack(client_fd, "-ERR", "Invalid argument");
					close(client_fd);
					sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
					pthread_exit(NULL);
				}
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "RETR";
		}
		else if(command == "dele")
		{
			if(state == "transaction")
			{
				string msg_no = packet_rcvd.arg(0);
				cout << "MESSAGE NO: " << msg_no << endl;
				if(!msg_no.empty())
				{
					//Flag: 1: Msg marked as deleted, 0: Ref to deleted msg, -1: no such msg in mbox
					//int flag = mark_as_delete(client_name, stoi(msg_no), mail_meta_data);
					if(stoi(msg_no) > marked_as_deleted.size())
					{
						send_error_pack(client_fd, "-ERR", "Invalid message");
						close(client_fd);
						sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
						pthread_exit(NULL);
					}
					else
					{
						// Get status of message
						int flag = marked_as_deleted[stoi(msg_no) - 1];
						if(flag == 0)
						{
							// Mark the message as deleted
							marked_as_deleted[stoi(msg_no) - 1] = 1;

							// Initialize the packet to send
							packet_to_send.set_status_code("+OK");
							send_packet(packet_to_send, client_fd);

							cout << "Message marked as deleted\n";
						}
						else
						{
							send_error_pack(client_fd, "-ERR", "Reference to deleted message");
							close(client_fd);
							sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
							pthread_exit(NULL);
						}
					}
				}
				else
				{
					send_error_pack(client_fd, "-ERR", "Invalid argument");
					close(client_fd);
					sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
					pthread_exit(NULL);
				}
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "DELE";
		}
		else if(command == "rset\r")
		{
			// If in correct state, unmark deleted messages
			if(state == "transaction")
			{
				// Reset all messages as not deleted
				for(int i = 0; i < marked_as_deleted.size(); i++)
					marked_as_deleted[i] = 0;

				// Initialize the packet to send
				packet_to_send.set_status_code("+OK");
				send_packet(packet_to_send, client_fd);
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "RSET";
		}
		else if(command == "markimp")
		{
			if(state == "transaction")
			{
				string msg_no = packet_rcvd.arg(0);
				cout << "MESSAGE NO: " << msg_no << endl;
				if(!msg_no.empty())
				{
					//Flag: 1: Msg marked as deleted, 0: Ref to deleted msg, -1: no such msg in mbox
					//int flag = mark_as_delete(client_name, stoi(msg_no), mail_meta_data);
					if(stoi(msg_no) > marked_as_imp.size())
					{
						send_error_pack(client_fd, "-ERR", "Invalid message");
						close(client_fd);
						sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
						pthread_exit(NULL);
					}
					else
					{
						// Get status of message
						int flag = marked_as_imp[stoi(msg_no) - 1];
						if(flag == 0)
						{
							// Mark the message as important
							marked_as_deleted[stoi(msg_no) - 1] = 1;

							// Initialize the packet to send
							packet_to_send.set_status_code("+OK");
							send_packet(packet_to_send, client_fd);

							cout << "Message marked as important\n";
						}
						else
						{
							send_error_pack(client_fd, "-ERR", " Message already marked as important.");
							close(client_fd);
							sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
							pthread_exit(NULL);
						}
					}
				}
				else
				{
					send_error_pack(client_fd, "-ERR", "Invalid argument");
					close(client_fd);
					sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
					pthread_exit(NULL);
				}
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "MARKIMP";
		}
		else if(command == "IMP")
		{
			if(state == "transaction")
			{
				vector<string> arg;
				Packet pack = query_kv_store("GET", client_name, "IMP", arg, "");

				// Forward the json string to FES
				packet_to_send.set_status_code("+OK");
				if(pack.data().size() == 0)
					packet_to_send.set_data("$");
				else
					packet_to_send.set_data(pack.data());

				send_packet(packet_to_send, client_fd);
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "IMP";
		}
		else if(command == "sentmail")
		{
			if(state == "transaction")
			{
				vector<string> arg;
				Packet pack = query_kv_store("GET", client_name, "SENTMAIL", arg, "");

				// Forward the json string to FES
				packet_to_send.set_status_code("+OK");
				if(pack.data().size() == 0)
					packet_to_send.set_data("$");
				else
					packet_to_send.set_data(pack.data());

				send_packet(packet_to_send, client_fd);
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "SENTMAIL";
		}
		else if(command == "contacts")
		{
			if(state == "transaction")
			{
				vector<string> arg;
				Packet pack = query_kv_store("GET", client_name, "ADDRBK", arg, "");

				// Forward the json string to FES
				packet_to_send.set_status_code("+OK");
				if(pack.data().size() == 0)
					packet_to_send.set_data("$");
				else
					packet_to_send.set_data(pack.data());

				send_packet(packet_to_send, client_fd);
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "SENTMAIL";
		}
		else if(command == "noop\r")
		{
			if(state == "transaction")
			{
				// Initialize the packet to send
				packet_to_send.set_status_code("+OK");
				send_packet(packet_to_send, client_fd);
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Invalid command");
				close(client_fd);
				sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
				pthread_exit(NULL);
			}
			prev_cmmd = "NOOP";
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
			send_error_pack(client_fd, "-ERR", "Command not supported.");
			close(client_fd);
			sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
			pthread_exit(NULL);
		}
	}
}

// Routine to handle SIGINT interrupt
void int_handler_routine(int sig)
{
	// Initialize the packet to send
	Packet packet_to_send;
	packet_to_send.set_status_code("-ERR");
	packet_to_send.set_status("Server shutting down");

	//Serialize the packet as a string
	string out;
	if(!packet_to_send.SerializeToString(&out))
	{
		cerr << "Could not serialize packet to send.\n";
	}
	string pre = generate_prefix(out.size());
	out = pre + out;
	char response[out.size()];
	out.copy(response, out.size());
	for(int i = 0; i < sock_list.size(); i++)
	{
		do_send(&sock_list[i], response, sizeof(response));
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
		cerr << "Incorrect Arguments.\nFormat: ./pop3_server configfile index\nAdditional -v and -o options can be specified.\n";
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
	vector<pair<string, int> > self_addr = parse_server_info(addr_list['P'][SERVER_NUM - 1]);
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
	// Heartbeat handler thread
	// pthread_t heartbeat;
	// pthread_create(&heartbeat, NULL, heartbeat_client, &master_addr_str);
	// pthread_detach(heartbeat);



	//Setup to handle interrupts
	struct sigaction int_handler;
	int_handler.sa_handler = int_handler_routine;
	sigemptyset(&int_handler.sa_mask);
	int_handler.sa_flags = 0;
	
	// Setup server socket to listen to connections
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(self_port);
	inet_pton(AF_INET, self_ip.c_str(), &(server_addr.sin_addr));
	bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	listen(listen_fd, SOMAXCONN);

	// Accept connections from multiple clients
	pthread_t threads[SOMAXCONN];
	int i = 0;
	while(true)
	{
		// Handling Ctrl + C interrupt
		sigaction(SIGINT, &int_handler, NULL);

		// Create client socket
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		int *client_fd = (int *)malloc(sizeof(int));
		*client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		sock_list.push_back(*client_fd);

		//Printing connection
		string con = "New Connection\n";
		//pretty_print(*client_fd, con, debug);

		Packet packet_to_send;

		// Initialize the packet to send
		packet_to_send.set_status_code("+OK");
		packet_to_send.set_status("POP3 ready [localhost]");
		send_packet(packet_to_send, *client_fd);
		num_active_connections++;

		// Pass connection to thread
		pthread_create(&threads[i], NULL, communicate, (void *)client_fd);
		i++;
	}
	return 0;
}
