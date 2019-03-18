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

using namespace std;
using json = nlohmann::json;

struct heartbeatArgs
{
  string heartAddr;
  bool* killed;
};

// Static status if the server is dead or alive
bool killed = false;

// Keeps count of number of active connections
static int num_active_connections = 0;

// Debug flag
static int debug = 0;

// Vector to store all client_fd
static vector<int> sock_list;

// Local cache of user:key_value_store_ip
static map<string, string> cache;

// DO_RECV TIMEOUT
static int TIME_OUT = 10;

// Store self server
static int SERVER_NUM;

// Master socket
static int master_sock;

// Master ip: port
static int master_port;
static string master_ip;

// Generate a unique file identifier
string get_unique_id(string file_name, string client_name)
{
	// Get the current time stamp
	stringstream st;
	string time_stamp;
	chrono::system_clock::time_point curr = chrono::system_clock::now();
	time_t t = chrono::system_clock::to_time_t(curr);
	st << ctime(&t);
	time_stamp = st.str();

	return generate_uidl(file_name + time_stamp + client_name);
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

	cout << "Ready to query kvs\n";
	// Check if client present in cache
	map<string, string>::iterator it;
	it = cache.find(row);
	pair<string, int> addr;
	string ip;
	if(it == cache.end())
	{
		cout << "Querying master for IP..\n";
		ip = query_master(row, "WHERIS", master_sock, master_port, master_ip);
		if(ip == "ABSENT")
		{
			cout << "Client not present in the system\n";
			Packet pack;
			pack.set_status_code("600");
			pack.set_status("User not present in system");
			return pack;
		}
		else if(ip == "SERVERFAIL")
		{
			cout << "Server Failure\n";
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

	cout << "KVS IP: " << addr.first << ":" << addr.second << endl;
	
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
	cout << "Connected to KVS\n";
	//send_packet(packet_to_send, out_fd);
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
	cout << "Packet sent\n";

	// Receive response from KV-Store
	char buff[2048];
	int idx = 0;
	idx = do_recv(&out_fd, buff, TIME_OUT);
	buff[idx] = 0;

	cout << "Received bytes: " << idx << endl;
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
		cout << "Sending packet\n";
		idx = do_recv(&out_fd, buff, TIME_OUT);
		buff[idx] = 0;
	}
	
	Packet packet_rcvd;
	cout << "Packet Received...\n";
	cout << "BUFF: " << string(buff) << endl;

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

// Delete user file
int delete_file(string client_name, string file_name)
{
	vector<string> arg;
	// Get unique file identifier
	Packet pack = query_kv_store("GET", client_name, file_name, arg, "");
	if(pack.status_code() == "500")
	{
		cerr << "Error in locating file.\n";
		return -1;
	}
	string f_id = pack.data();

	// Get number of splits
	pack.Clear();
	arg.push_back(client_name);
	pack = query_kv_store("GET", client_name, f_id, arg, "");
	if(pack.status_code() == "500")
	{
		cerr << "Error in locating file.\n";
		return -1;
	}
	int splits = stoi(pack.data());

	// Delete all splits from KVS
	for(int i = 0; i < splits; i++)
	{
		pack.Clear();
		pack = query_kv_store("DEL", client_name, f_id + "&" + to_string(i) + "&", arg, "");
		if(pack.status_code() == "500")
			return -1;	
	}

	// Delete file meta deta
	pack = query_kv_store("DEL", client_name, f_id, arg, "");
	pack.Clear();
	arg.clear();
	pack = query_kv_store("DEL", client_name, file_name, arg, "");
	return 1;
}

// Update the file meta
int update_dir_meta(json old_meta, json new_meta, string row, string col, int client_fd)
{
	vector<string> arg;
	string old_m;
	string new_m;
	string old = "";
	// Packet tmp = query_kv_store("GET", row, col, arg, new_m);
	// old_m = "$" + old_meta.dump();
	// new_m = "$" + new_meta.dump();
	
	// arg.push_back(old);

	// Packet pack = query_kv_store("CPUT", row, col, arg, new_m);
	// cout << "***KVS-UPDATE-1***\n";
	// string status = pack.status_code();
	Packet pack;
	string status = "500";
	while(status != "200")
	{
		pack.Clear();
		arg.clear();

		pack = query_kv_store("GET", row, col, arg, "");
		cout << "***KVS-UPDATE-2****\n";

		json JsonObjects = json::array();
		string json_str;

		if(pack.status_code() == "601")
			return -1;

		if(pack.status_code() == "200")
		{
			string json_str = (pack.data()).substr(1);
			old = pack.data();
			JsonObjects = json::parse(json_str);
		}
		else
			old = "";

		// If new contencts present in old meta, add them to the new meta
		json arr = json::array();
		json tmp = json::array();
		for(int i = 0; i < JsonObjects.size(); i++)
		{
			arr = JsonObjects.at(i);
			tmp = old_meta.at(i);

			if(tmp.at(0) != arr.at(0) || tmp.at(1) != arr.at(1))
				new_meta.push_back(arr);
		}
		new_m = "$" + new_meta.dump();
		pack.Clear();
		arg.clear();
		arg.push_back(old);
		pack = query_kv_store("CPUT", row, col, arg, new_m);
		cout << "***KVS-UPDATE-3\n***";
		status = pack.status_code();
	}
	cout << "****UPDATED THE DIRECTORY****\n";
	return 1;
}

// Thread function to communicate with the client
void *communicate(void *arg)
{
	// Initialize the state of the client
	int client_fd = *(int *)arg;
	string client_name;
	
	// Initialize the packet to send
	Packet packet_rcvd;
	Packet packet_to_send; 
	packet_to_send.set_status_code("+OK");
	packet_to_send.set_status("Drive Server Ready");

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
	do_send(&client_fd, response, sizeof(response));

	while(true)
	{
		char* buff = (char *)malloc(1024 * 1024 + 100);
		int idx = 0;

		// Idx stores the number of bytes
		idx = do_recv(&client_fd, &buff[0], TIME_OUT);
		if(idx == -1)
		{
			cerr << "Error reading COMMAND. Exiting...\n";
			close(client_fd);
			sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
			num_active_connections--;
			pthread_exit(NULL);
		}
		buff[idx] = 0;
		
		// Parse the message to extract the content
		if(!packet_rcvd.ParseFromString(buff))
		{
			cerr << "Failed to parse received message!\n";
			num_active_connections--;
			pthread_exit(NULL);
		}

		// Obtain the relevant message fields
		string command = packet_rcvd.command();
		transform(command.begin(), command.end(), command.begin(), ::tolower);
		cout << "Command Received: " << command << endl;

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

		if(command == "display")
		{
			cout << "--------IN DISPLAY--------\n";
			client_name = packet_rcvd.arg(0);
			string dir = packet_rcvd.arg(1);
			cout << "Client: " << client_name << endl;
			cout << "Directory: " << dir << endl;
			vector<string> arg;
			Packet pack = query_kv_store("GET", client_name, dir, arg, "");

			json json_obj;
			string json_str;
			cout << pack.data() << endl;			

			// // Check if home dir present
			// if(dir == "home")
			// {
			// 	// If home absent, create one
			// 	if(pack.status_code() == "500")
			// 	{
			// 		Packet tmp = query_kv_store("PUT", client_name, dir, arg, "$");
			// 	}
			// }

			// Forward the json string to FES
			packet_to_send.set_status_code("+OK");
			if(pack.data().size() == 0)
				packet_to_send.set_data("$");
			else
				packet_to_send.set_data(pack.data());

			send_packet(packet_to_send, client_fd);
		}
		else if(command == "download")
		{
			cout << "--------IN DOWNLOAD----------\n";
			string file_name = packet_rcvd.arg(1);
			string client_name = packet_rcvd.arg(0);
			cout << "Argument: " << client_name << endl;
			cout << "Argument: " << file_name << endl;
			vector<string> arg;

			// Get the FID associated with the filename
			Packet pack = query_kv_store("GET", client_name, file_name, arg, "");
			cout << "Obtained FID\n";
			string f_id = pack.data();

			if(pack.status_code() == "200")
			{
				pack.Clear();
				// Get the number of file splits
				pack = query_kv_store("GET", client_name, f_id, arg, "");
				cout << "Obtained file FID-SPLITS\n";

				// Get all parts of the file and send to fes
				// Initialize the packet to send
				packet_to_send.set_status_code("+OK");
				packet_to_send.set_status("File splits coming up");
				packet_to_send.set_data(pack.data());

				//Serialize the packet as a string
				send_packet(packet_to_send, client_fd);
				packet_to_send.Clear();

				// Send the file splits
				int splits = stoi(pack.data());
				for(int i = 0; i < splits; i++)
				{
					Packet tmp = query_kv_store("GET", client_name, f_id + "&" + to_string(i) + "&", arg, "");
					packet_to_send.set_status_code("+OK");
					packet_to_send.set_data(tmp.data());
					send_packet(packet_to_send, client_fd);
					packet_to_send.Clear();
				}
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Unable to download file");
				close(client_fd);
				num_active_connections--;
				pthread_exit(NULL);
			}
		}
		else if(command == "new")
		{
			cout << "--------IN NEW-------------\n";
			string file_name = packet_rcvd.arg(1);
			string type = packet_rcvd.arg(2);
			string client_name = packet_rcvd.arg(0);
			vector<string> arg;
			cout << "File name: " << file_name << endl;
			cout << "Type: " << type << endl;
			cout << "User: " << client_name << endl;

			Packet pack = query_kv_store("PUT", client_name, file_name, arg, "");
			cout << "PUT COMPLETED\n";

			if(pack.status_code() == "200")
			{
				// Get the parent directory of the file at it to the parent directory
				pack.Clear();
				vector<string> path = get_path(file_name);

				// Add file to parent
				string dir_meta = "500";

				while(dir_meta != "200")
				{
					pack = query_kv_store("GET", client_name, path[0], arg, "");
					cout << "Received Parent Directory Contents\n";

					json JsonObjects = json::array();
					string json_str;
					string old;

					if(pack.status_code() == "601")
					{
						send_error_pack(client_fd, "-ERR", "KVS Failed to respond");
						do_send(&client_fd, response, sizeof(response));
						close(client_fd);
						num_active_connections--;
						pthread_exit(NULL);
					}

					if(pack.status_code() == "200")
					{
						string json_str = (pack.data()).substr(1);
						old = pack.data();
						JsonObjects = json::parse(json_str);
					}
					else
						old = "";
					JsonObjects.push_back({type, file_name});
					string j_array = "$" + JsonObjects.dump();

					pack.Clear();
					arg.clear();
					arg.push_back(old);
					pack = query_kv_store("CPUT", client_name, path[0], arg, j_array);
					dir_meta = pack.status_code();
					cout << "DIR META: " << dir_meta << endl;
				}
				cout << "New directory created\n";
				// Initialize the packet to send
				packet_to_send.set_status_code("+OK");
				packet_to_send.set_status("New Folder created");
				send_packet(packet_to_send, client_fd);
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Could not create new folder.");
				close(client_fd);
				num_active_connections--;
				pthread_exit(NULL);
			}
		}
		else if(command == "rename")
		{
			cout << "--------------RENAME--------------\n";
			string new_name = packet_rcvd.arg(2);
			string old_name = packet_rcvd.arg(1);
			string type = packet_rcvd.arg(3);
			string client_name = packet_rcvd.arg(0);
			cout << "New Name: " << new_name << endl;
			cout << "Old Name: " << old_name << endl;
			cout << "Type: " << type << endl;
			cout << "User: " << client_name;
			vector<string> arg;
			Packet pack = query_kv_store("GET", client_name, old_name, arg, "");
			cout << "OLD META Received\n";
			
			vector<string> path_old = get_path(old_name);
			vector<string> path_new = get_path(new_name);

			string par_meta = "500";
			string old;
			while(par_meta != "200")
			{	
				Packet pack1 = query_kv_store("GET", client_name, path_old[0], arg, "");
				cout << "Path of old file\n";

				json JsonObjects = json::array();
				json updated = json::array();
				string json_str;

				if(pack1.status_code() == "200")
				{
					string json_str = (pack1.data()).substr(1);
					old = pack1.data();
					JsonObjects = json::parse(json_str);
				}
				else
					old = "";

				for(int i = 0; i < JsonObjects.size(); i++)
				{
					json file = json::array();
					file = JsonObjects.at(i);
					if(file.at(1) != old_name)
					{
						updated.push_back(file);
					}
					if(file.at(1) == new_name)
					{
						send_error_pack(client_fd, "-ERR", "File name already exists");
						close(client_fd);
						num_active_connections--;
						pthread_exit(NULL);
					}
				}
				string j_array = "$" + updated.dump();
				pack1.Clear();
				arg.clear();
				arg.push_back(old);
				pack1 = query_kv_store("CPUT", client_name, path_old[0], arg, j_array);
				par_meta = pack1.status_code();
			}
			cout << "Removed old file name from parent meta\n";

			par_meta = "500";
			while(par_meta != "200")
			{	
				Packet pack1 = query_kv_store("GET", client_name, path_new[0], arg, "");
				cout << "Get parent of new file\n";

				json JsonObjects = json::array();
				string json_str;

				if(pack1.status_code() == "200")
				{
					string json_str = (pack1.data()).substr(1);
					old = pack1.data();
					JsonObjects = json::parse(json_str);
				}
				else
					old = "";

				// Check if file with same name already present in the parent directory
				for(int i = 0; i < JsonObjects.size(); i++)
				{
					json name = json::array();
					name = JsonObjects.at(i);
					if(name.at(1) == new_name)
					{
						send_error_pack(client_fd, "-ERR", "File name already exists");
						close(client_fd);
						num_active_connections--;
						pthread_exit(NULL);
					}
				}
				JsonObjects.push_back({type, new_name});
				
				string j_array = "$" + JsonObjects.dump();
				pack1.Clear();
				arg.clear();
				arg.push_back(old);
				pack1 = query_kv_store("CPUT", client_name, path_new[0], arg, j_array);
				par_meta = pack1.status_code();
			}
			cout << "File added to parent of new file\n";

			// Set the value of the new file name to be the fid obtained from old name
			Packet pack1 = query_kv_store("PUT", client_name, new_name, arg, pack.data());
			if(pack1.status_code() == "200")
			{
				// Delete old file name from kvs
				pack1.Clear();
				pack1 = query_kv_store("DEL", client_name, old_name, arg, "");
				pack1.Clear();
				cout << "FID/DIR META matched to new file\n";
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Error in renaming");
				close(client_fd);
				num_active_connections--;
				pthread_exit(NULL);
			}

			// CHANGE THIS!!! ADD NEW FILE NAME TO THE DIR META PROPERLY
			if(type == "d")
			{
				vector<pair<string, string> > queue;
				queue.push_back(make_pair(new_name, new_name));
				
				// Recurse through directory to change the name of all the files contained in it
				while(queue.size() > 0)
				{
					pair<string, string> file = queue[0];
					queue.erase(queue.begin());

					// Obtain the file/dir names stored in file.first
					Packet pack2 = query_kv_store("GET", client_name, file.first, arg, "");
					Packet tmp;
					// If oldname == new name, no need to change
					if(file.first != file.second)
						tmp = query_kv_store("DEL", client_name, file.first, arg, "");
					else
						cout << "----------------HERE----------------------------\n";
					if((pack2.data()).size() > 0)
					{
						json dir = json::array();
						json updated = json::array();
						dir = json::parse((pack2.data()).substr(1));

						if(pack2.status_code() == "200")
						{
							// Iterate through all files. Update name and add any directories present to the queue
							for(int i = 0; i < dir.size(); i++)
							{
								string f_old_name = (dir.at(i)).at(1);
								string f_new_name = file.second + "/" + get_path(f_old_name)[1];
								if((dir.at(i)).at(0) == "d")
								{
									queue.push_back(make_pair(f_old_name, f_new_name));
									updated.push_back({"d", f_new_name});
								}
								else
								{
									updated.push_back({"f", f_new_name, (dir.at(i)).at(2)});
									// if(tmp.status_code() == "200")
									// {
										// Get fid from old file name
										pack2.Clear();
										pack2 = query_kv_store("GET", client_name, f_old_name, arg, "");
										string f_id = pack2.data();

										// Set the value of new file name to be fid
										pack2.Clear();
										pack2 = query_kv_store("PUT", client_name, f_new_name, arg, f_id);

										// Delete old file name from kvs
										pack2.Clear();
										pack2 = query_kv_store("DEL", client_name, f_old_name, arg, "");
									//}
								}
							}
							// Update the directory meta data
							int status;
							if(updated.size() > 0)
								status = update_dir_meta(dir, updated, client_name, file.second, client_fd);
							if(status == -1)
							{
								cout << "Rename failed.\n";
								send_error_pack(client_fd, "-ERR", "Could not rename file/directory.");
								close(client_fd);
								num_active_connections--;
								pthread_exit(NULL);
							}
						}
						else
						{
							send_error_pack(client_fd, "-ERR", "Could not rename file/directory.");
							close(client_fd);
							num_active_connections--;
							pthread_exit(NULL);
						}
					}
				}
			}
			packet_to_send.Clear();
			packet_to_send.set_status_code("+OK");
			packet_to_send.set_status("File renamed/moved successfully");
			send_packet(packet_to_send, client_fd);
		}
		else if(command == "upload")
		{
			cout << "-------------UPLOAD------------------\n";
			string file_name = packet_rcvd.arg(1);
			string type = packet_rcvd.arg(2);
			int splits = stoi(packet_rcvd.arg(3));
			string client_name = packet_rcvd.arg(0);
			string data_sz = to_string((packet_rcvd.data()).size());
			cout << "File to upload: " << file_name << endl;
			cout << "Client Name: " << client_name << endl;
			cout << "Number of splits: " << splits << endl;
			vector<string> arg;
			int file_sz = 0;

			// Generate unique identifier for file
			string f_id = get_unique_id(file_name, client_name);

			// Store unique id corresponding to the file
			Packet pack = query_kv_store("PUT", client_name, file_name, arg, f_id);
			pack.Clear();
			cout << "FID Added to KVS\n";

			// Add number of splits to file_name meta
			pack = query_kv_store("PUT", client_name, f_id, arg, to_string(splits));
			if(pack.status_code() == "200")
			{
				cout << "PUT File meta successfully\n";
				int idx = 0;
				pack.Clear();
				pack = query_kv_store("PUT", client_name, f_id + "&" + to_string(idx) + "&", arg, packet_rcvd.data());
				file_sz = file_sz + (packet_rcvd.data()).size();
				for(idx = 1; idx < splits; idx++)
				{
					Packet tmp;
					pack.Clear();
					char *file_data;
					file_data = (char *)malloc(1024 * 1024);
					int bytes = do_recv(&client_fd, file_data, 10);
					file_data[bytes] = 0;
					if(bytes == -1)
					{
						// undo all the put operations
						int k = idx;
						for(int j = 0; j < k; j++)
						{
							tmp.Clear();
							arg.clear();
							tmp = query_kv_store("DEL", client_name, f_id + "&" + to_string(j) + "&", arg, "");
						}
						tmp = query_kv_store("DEL", client_name, f_id, arg, "");
						tmp.Clear();
						tmp = query_kv_store("DEL", client_name, file_name, arg, "");
						cerr << "Error reading COMMAND. Exiting...\n";
						close(client_fd);
						sock_list.erase(remove(sock_list.begin(), sock_list.end(), client_fd), sock_list.end());
						num_active_connections--;
						pthread_exit(NULL);
					}

					// Parse the message to extract the content
					if(!tmp.ParseFromString(file_data))
					{
						cerr << "Failed to parse received message!\n";
						num_active_connections--;
						pthread_exit(NULL);
					}
					pack = query_kv_store("PUT", client_name, f_id + "&" + to_string(idx) + "&", arg, tmp.data());
					file_sz = file_sz + (pack.data()).size();
				}
			}

			cout << "File uploaded to KVS.\n";
			if(pack.status_code() == "200")
			{
				string par_meta = "500";

				while(par_meta != "200")
				{
					// Update drive meta
					pack.Clear();

					// Get parent meta
					vector<string> path = get_path(file_name);
					pack = query_kv_store("GET", client_name, path[0], arg, "");

					json dir = json::array();
					string json_str;
					cout << "Received File Meta\n";

					string old = "";
					if(pack.status_code() == "200")
					{
						json_str = (pack.data()).substr(1);
						old = pack.data();
						dir = json::parse(json_str);
					}
					dir.push_back({type, file_name, to_string(file_sz)});
					string to_send = "$" + dir.dump();
					pack.Clear();
					arg.clear();
					arg.push_back(old);
					pack = query_kv_store("CPUT", client_name, path[0], arg, to_send);
					par_meta = pack.status_code();
				}

				cout << "Succesfully uploaded file.\n";
				// Initialize the packet to send
				packet_to_send.set_status_code("+OK");
				packet_to_send.set_status("New file uploaded");
				send_packet(packet_to_send, client_fd);
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "");
				close(client_fd);
				num_active_connections--;
				pthread_exit(NULL);
			}
		}
		else if(command == "delete")
		{
			cout << "--------------DELETE------------\n";
			string file_name = packet_rcvd.arg(1);
			string client_name = packet_rcvd.arg(0);
			string type = packet_rcvd.arg(2);
			cout << "File name: " << file_name << endl;
			cout << "Client Name: " << client_name << endl;
			cout << "Type: " << type << endl;
			vector<string> arg;
			
			Packet pack;
			string file_meta = "500";
			while(file_meta != "200")
			{
				pack.Clear();
				arg.clear();

				// Get parent meta
				vector<string> path = get_path(file_name);

				// Delete file from drive meta data
				Packet pack = query_kv_store("GET", client_name, path[0], arg, "");
				json JsonObjects = json::array();
				json updated = json::array();
				string json_str;
				string old = "";

				if(pack.status_code() == "200")
				{
					string json_str = (pack.data()).substr(1);
					old = pack.data();
					JsonObjects = json::parse(json_str);
				}

				for(int i = 0; i < JsonObjects.size(); i++)
				{
					json file = json::array();
					file = JsonObjects.at(i);
					if(file.at(1) != file_name)
					{
						updated.push_back(file);
					}
				}
				string j_array = "$" + updated.dump();
				pack.Clear();
				arg.clear();
				arg.push_back(old);
				pack = query_kv_store("CPUT", client_name, path[0], arg, j_array);
				file_meta = pack.status_code();
			}
			cout << "****File deleted from parent directory****\n";
			if(type == "f")
			{
				int status = delete_file(client_name, file_name);
				
				if(status == -1)
				{
					send_error_pack(client_fd, "-ERR", "File could not be deleted");
					close(client_fd);
					num_active_connections--;
					pthread_exit(NULL);
				}
				cout << "***File successfully deleted***\n";
			}
			else if(type == "d")
			{
				vector<pair<string, string> > queue;
				queue.push_back(make_pair("d", file_name));
				
				while(queue.size() > 0)
				{
					pair<string, string> item = queue[0];
					queue.erase(queue.begin());
					json arr = json::array();

					Packet tmp = query_kv_store("GET", client_name, item.second, arg, "");
					cout << "***Received DIR META from KVS***\n";
					if(tmp.status_code() == "200")
					{
						arr = json::parse((tmp.data()).substr(1));

						for(int i = 0; i < arr.size(); i++)
						{
							if((arr.at(i)).at(0) == "d")
								queue.push_back(make_pair("d", (arr.at(i)).at(1)));
							else
							{
								int status = delete_file(client_name, (arr.at(i)).at(1));
								cout << "***File deleted from directory***\n";
								//Packet tmp1 = query_kv_store("DEL", client_name, (arr.at(i)).at(1), arg, "");
								if(status == -1)
								{
									send_error_pack(client_fd, "-ERR", "File could not be deleted");
									close(client_fd);
									num_active_connections--;
									pthread_exit(NULL);
								}
							}
						}
					}
					tmp.Clear();
					tmp = query_kv_store("DEL", client_name, item.second, arg, "");
				}
			}

			// Initialize the packet to send
			packet_to_send.set_status_code("+OK");
			packet_to_send.set_status("File successfully deleted");
			send_packet(packet_to_send, client_fd);
		}
		else if(command == "share")
		{
			cout << "---------------IN SHARE--------------\n";
			string client_name = packet_rcvd.arg(0);
			string share_with = packet_rcvd.arg(1);
			string file_name = packet_rcvd.arg(2);
			string type = packet_rcvd.arg(2);
			cout << "User: " << client_name << endl;
			cout << "File name: " << file_name << endl;
			cout << "Share with: " << share_with << endl;
			cout << "Type: " << type << endl;
			vector<string> arg;

			vector<string> path = get_path(file_name);

			// Get the size of the file
			Packet pack =  query_kv_store("GET", client_name, path[0], arg, "");
			json parent_dir = json::array();
			string jstr;
			int file_sz = 0;
			if(pack.status_code() == "200")
			{
				jstr = (pack.data()).substr(1);
				parent_dir = json::parse(jstr);
			}
			for(int i = 0; i < parent_dir.size(); i++)
			{
				json arr = json::array();
				arr = parent_dir.at(i);
				if(arr.at(0) == type && arr.at(1) == file_name)
				{
					file_sz = arr.at(2);
					break;
				}
			}
			pack.Clear();

			// Get unique file identifier
			pack = query_kv_store("GET", client_name, file_name, arg, "");
			string f_id;
			if(pack.status_code() != "500")
			{
				f_id = pack.data();
				pack.Clear();

				// Add the file along with unique identifier to the receiveres shared dir
				pack = query_kv_store("PUT", share_with, "Home/Shared With Me/" + path[1], arg, f_id);

				// Add file name to share with me folder
				string dir_meta = "500";
				while(dir_meta != "200")
				{
					pack.Clear();
					arg.clear();

					// Get shared with me folder
					pack = query_kv_store("GET", share_with, "home/Shared With Me", arg, "");
					if(pack.status_code() == "500")
					{
						string par_meta = "500";
						while(par_meta != "200")
						{
							json dir = json::array();
							string json_str;
							string old = "";

							pack.Clear();
							pack = query_kv_store("GET", share_with, "home", arg, "");
					
							if(pack.status_code() == "200")
							{
								json_str = (pack.data()).substr(1);
								old = pack.data();
								dir = json::parse(json_str);
							}

							int flag = 0;
							for(int i = 0; i < dir.size(); i++)
							{
								json arr = json::array();
								arr = dir.at(i);
								if(arr.at(0) == "d" && arr.at(1) == "Home/Shared With Me")
								{
									flag = 1;
									break;
								}
							}
							if(flag == 0)
							{
								dir.push_back({"d", "home/Shared With Me"});
								string to_send = "$" + dir.dump();
								pack.Clear();
								arg.clear();
								arg.push_back(old);
								pack = query_kv_store("CPUT", share_with, "home", arg, to_send);
								par_meta = pack.status_code();
							}
							else
								break;
						}
					}
					pack.Clear();
					pack = query_kv_store("GET", share_with, "home/Shared With Me", arg, "");
					json j_obj = json::array();
					string json_str;
					string old = "";

					if(pack.status_code() == "200")
					{
						json_str = (pack.data()).substr(1);
						old = pack.data();
						j_obj = json::parse(json_str);
					}
					j_obj.push_back({"f", "home/Shared With Me/" + path[1], file_sz});
					string to_send = "$" + j_obj.dump();
					pack.Clear();
					arg.clear();
					arg.push_back(old);
					pack = query_kv_store("CPUT", share_with,"home/Shared With Me", arg, to_send);
					dir_meta = pack.status_code();
				}

				// Get number of file splits
				pack.Clear();
				pack = query_kv_store("GET", client_name, f_id, arg, "");
				int splits = stoi(pack.data());

				// Transfer data to the other user
				for(int i = 0; i < splits; i++)
				{
					pack.Clear();
					pack = query_kv_store("GET", client_name, f_id + "&" + to_string(i) + "&", arg, "");
					string file_data = pack.data();
					pack.Clear();
					pack = query_kv_store("PUT", share_with, f_id + "&" + to_string(i) + "&", arg, file_data);
				}

				// Initialize the packet to send
				packet_to_send.set_status_code("+OK");
				packet_to_send.set_status("File shared successfully.");
				send_packet(packet_to_send, client_fd);
			}
			else
			{
				send_error_pack(client_fd, "-ERR", "Could not share file.");
				close(client_fd);
				num_active_connections--;
				pthread_exit(NULL);
			}
			
		}
		else if(command == "quit")
		{
			// Initialize the packet to send
			packet_to_send.set_status_code("+OK");
			packet_to_send.set_status("Closing connection");
			send_packet(packet_to_send, client_fd);
			close(client_fd);
			num_active_connections--;
			pthread_exit(NULL);
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
			send_error_pack(client_fd, "-ERR", "Invalid Command.");
			close(client_fd);
			num_active_connections--;
			pthread_exit(NULL);
		}

		// Clear the packets
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
		cerr << "Incorrect Arguments.\nFormat: ./drive_server configfile index\nAdditional -v option can be specified.\n";
		exit(1);
	}
	cout << "Index: " << idx << endl;
	string configfile = string(argv[idx]);
	cout << "Config file: " << configfile << endl;
	SERVER_NUM = stoi(string(argv[idx + 1]));
	cout << "Drive Number: " << SERVER_NUM << endl;

	// Map of all server addresses
	map <char, vector<string> > addr_list = parse_config(configfile);

	// Get the master address
	vector<pair<string, int> > master_addr = parse_server_info(addr_list['M'][0]);
	master_ip = master_addr[0].first;
	master_port = master_addr[0].second;


	// Get address of self
	vector<pair<string, int> > self_addr = parse_server_info(addr_list['R'][SERVER_NUM - 1]);
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
			//pretty_print(*client_fd, con, debug);

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