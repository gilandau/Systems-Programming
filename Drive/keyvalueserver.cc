#include <bits/stdc++.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <locale.h>
#include <nlohmann/json.hpp>
#include "DATA_PACKET.pb.h"
#include "replication_packet.pb.h"
#include "ordering_packet.pb.h"
#include "utility.h"

using namespace std;
using json = nlohmann::json;

vector <int> open_conn;
int sockfd, sockreplica;
bool vflag = false;
int logsize = 0;
int KVS_NO;
int self_port, replica_port, heartbeat_port;
string self_ip;
int msgid = 0;
const int N = 3;
const int R = 1;
const int W = 1;
int log_seq = 0;
bool killed = false;

pthread_mutex_t storemtx;
pthread_mutex_t logmtx;
pthread_mutex_t cache_read_mtx;
pthread_mutex_t cache_rw_mtx;
int cache_readcount = 0;
pthread_mutex_t queue_read_mtx;
pthread_mutex_t queue_rw_mtx;
int queue_readcount = 0;

struct heartbeatArgs {
  string heartAddr;
  bool* killed;
};

map <string, map<string, pair<string, int>>> keyvaluecache; //map to store the tablet. Key contains the 'Row Key' and value contains a map of the column entries.
map <string, map<string, string>> deleted; //Stores all deleted row-column pairs. Will be helpful when transferring cache to disk.
map <string, pair<int, int>> ordering; //Stores the P and A values for each user so far for total ordering to ensure sequential consistency.
map <string, map<pair<int, int>,pair<string,bool>>> holdbackqueue; //Queue for ordering commands to ensure sequential consistency
map <pair<int, int>, string> to_forward;
map <string, map<string, pair<string, int>>> old_keyvaluecache;
vector <string> replicas;

void my_write(int fd, const char *data, int len);
Packet get_latest_data(int sfd, string r, string c);
int* request_proposed_number(string ip, int port, int id, string req, string r, string c, string d, string d2c);
void send_final_data(int sfd, string mid, string req, string r, string c, string data, int ver_no, int maxpn, int maxpnsender);

void sigint_handler(int sig){
	string errormsg = "-ERR Server shutting down\r\n";
	int l = errormsg.length();
	for(int i=0;i<open_conn.size();i++){
		int fd = open_conn[i];
		my_write(fd, errormsg.c_str(),l);
		if(vflag){
			fprintf(stderr, "[%d] S: %s", open_conn[i], errormsg.c_str());
		}
		close(open_conn[i]);
		//open_conn.erase(open_conn.begin()+i);
	}
	close(sockfd);
	exit(1);
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

void read_lock(pthread_mutex_t* myrw, pthread_mutex_t* mymtx, int* myreadcount){
	pthread_mutex_lock(mymtx);
	(*myreadcount)++;
	if ((*myreadcount) == 1){
		pthread_mutex_lock(myrw);
	}
	pthread_mutex_unlock(mymtx);
}

void read_unlock(pthread_mutex_t* myrw, pthread_mutex_t* mymtx, int* myreadcount){
	pthread_mutex_lock(mymtx);
	(*myreadcount)--;
	if ((*myreadcount) == 0){
		pthread_mutex_unlock(myrw);
	}
	pthread_mutex_unlock(mymtx);
}

void write_lock(pthread_mutex_t* myrw){
	pthread_mutex_lock(myrw);
}

void write_unlock(pthread_mutex_t* myrw){
	pthread_mutex_unlock(myrw);
}

void printcache(){
	map <string, map<string, pair<string, int>>>::iterator it1;
	map <string, pair<string, int>>::iterator it2;
	for(it1 = keyvaluecache.begin(); it1!=keyvaluecache.end(); ++it1){
		cout<<it1->first<<": [";
		for(it2 = it1->second.begin(); it2!=it1->second.end(); ++it2){
			cout<<" ( "<<it2->first<<", "<<it2->second.first<<", "<<it2->second.second<<")";
		}
		cout<<"]"<<endl;
	}
}


string get_value_from_disk(string r, string c){
	//Open file
	cout<<"Getting lock\n";
	pthread_mutex_lock(&storemtx);
	cout<<"Opening file\n";
	fstream f("kvstore" + to_string(KVS_NO), ios::in);
	if(f.is_open()){
		string line;
		while(getline(f,line)){ //Each line in the json contains a single JSON object corresponding to one row (user)
			if(line.length()!=0){
				json j = json::parse(line);
				
				cout<<j<<endl;
				json::iterator it = j.begin();
				if(it.key().compare(r) == 0){ //If row key matches, check for corresponding column
					cout<<"row match\n";
					for(json::iterator it1 = (*it).begin(); it1 != (*it).end(); ++it1){ //Loop through all columns of the row
						if(it1.key().compare(c) == 0){
							cout<<"column match\n";
							pthread_mutex_unlock(&storemtx);
							return it1.value()[0]; //Each value is stored as a pair [VALUE, VERSION_NO]
						}
					}
				}
			}
		}
	}
	pthread_mutex_unlock(&storemtx);
	return "";
}

int get_version_from_disk(string r, string c){
	//Open file
	cout<<"Getting version from disk\n";
	pthread_mutex_lock(&storemtx);
	cout<<"Opening kvstore\n";
	fstream f("kvstore" + to_string(KVS_NO), ios::in);
	if(f.is_open()){
		string line;
		while(getline(f,line)){ //Each line in the json contains a single JSON object corresponding to one row (user)
			if(line.length()!=0){
				json j = json::parse(line);
				
				cout<<j<<endl;
				json::iterator it = j.begin();
				if(it.key().compare(r) == 0){ //If row key matches, check for corresponding column
					cout<<"row match\n";
					for(json::iterator it1 = (*it).begin(); it1 != (*it).end(); ++it1){ //Loop through all columns of the row
						if(it1.key().compare(c) == 0){
							cout<<"column match\n";
							pthread_mutex_unlock(&storemtx);
							return it1.value()[1]; //Each value is stored as a pair [VALUE, VERSION_NO]
						}
					}
				}
			}
		}
	}
	pthread_mutex_unlock(&storemtx);
	return 0;
}

void put_to_disk(map <string, map<string, pair<string, int>>> keyvaluecache){
	
	pthread_mutex_lock(&storemtx);
	fstream f("kvstore" + to_string(KVS_NO));
	ofstream temp("kvstoretemp" + to_string(KVS_NO), ios::app);
	vector<string> rowsadded;
	if(f.is_open()){
			string line;
			bool del = false;
			while(getline(f,line)){
				if(line.length()!=0){
				json j = json::parse(line);
				json::iterator it = j.begin();
				string row = it.key();
				
				if(keyvaluecache.find(row) != keyvaluecache.end()){ //If row present in cache
					
					map <string, map<string, pair<string, int>>> tempmap;
					map<string, pair<string, int>> temprow;
					//tempmap.insert(pair<string, map<string, pair<string, int>>>(it.key(),temprow));
					rowsadded.push_back(row);
					for(json::iterator it1 = (*it).begin(); it1 != (*it).end(); ++it1){ //Loop through all columns of the row
						del = false;
						string col = it1.key();
						
						if(deleted[it.key()].find(it1.key()) != deleted[it.key()].end()){
							
							del = true;
						}
						if(keyvaluecache[it.key()].find(it1.key()) != keyvaluecache[it.key()].end()){ //If column in disk is also present in cache, use the value in cache (With replication, agree on a value from the replicas and then set the value)
							//Update value on disk. With replication, this value will be decided by the WRITE QUORAM after executing the required protocol
							
							if(tempmap.find(row)!=tempmap.end()){
								tempmap[row].insert(pair<string, pair<string, int>>(col,make_pair(keyvaluecache[row][col].first, keyvaluecache[row][col].second)));
							}else{
								temprow.insert(pair<string, pair<string, int>>(col,make_pair(keyvaluecache[row][col].first, keyvaluecache[row][col].second)));
								tempmap.insert(pair<string, map<string, pair<string, int>>>(row,temprow));
							}
							//tempmap[it.key()].insert(pair<string, pair<string, int>>(it1.key(),make_pair(keyvaluecache[it.key()][it1.key()], 0)));
						}else{
							
							if(!del){
								
								//Column only present in disk. Not on cache and not deleted, leave it as it is.
								if(tempmap.find(row)!=tempmap.end()){
									string val = it1.value()[0];
									tempmap[row].insert(pair<string, pair<string, int>>(col, make_pair(val, it1.value()[1])));
								}else{
									temprow.insert(pair<string, pair<string, int>>(col, make_pair(it1.value()[0], it1.value()[1])));
									tempmap.insert(pair<string, map<string, pair<string, int>>>(row,temprow));
								}
								//tempmap[it.key()].insert(pair<string, pair<string, int>>(it1.key(), make_pair(it1.value()[0], 0)));
							}
						}
					}
					//Insert remaining columns of that row in cache to the tempmap to store to disk
					for(map<string, pair<string, int>>::iterator itm=keyvaluecache[it.key()].begin();itm!=keyvaluecache[it.key()].end();++itm){
						if(tempmap[it.key()].find(itm->first)!=tempmap[it.key()].end()){
							//Do nothing
						}else{
							//If not found in tempmap, insert it
							tempmap[it.key()].insert(pair<string, pair<string, int>>(itm->first,itm->second));
						}
					}

					//Convert map to JSON and store it in the file
					if(tempmap.size()!=0){
						json j(tempmap);
						
						temp<<j<<"\n";
						tempmap.clear();
					}
				}else{
					//Row not present in cache, copy it as it is to temporary file
					cout<<"row not in cache\n";
					temp<<line<<"\n";
				}
			}
		}
		//Insert remaining rows in cache to the tempmap to store to disk
		for(auto itr = keyvaluecache.begin(); itr!=keyvaluecache.end();++itr){
			vector<string>::iterator vit = find(rowsadded.begin(), rowsadded.end(),itr->first);
			if(vit == rowsadded.end()){ //If row in keyvaluecache is not present in added rows, add it to file (disk)
				map <string, map<string, pair<string, int>>> tempmap;
				map<string, pair<string, int>> temprow;
				temprow.insert(itr->second.begin(), itr->second.end());
				tempmap.insert(pair<string, map<string, pair<string, int>>>(itr->first,temprow));
				json j(tempmap);
				temp<<j<<"\n";
				temprow.clear();
				tempmap.clear();
			}	
		}
	}
	f.close();
	temp.close();
	string filename = "kvstore" + to_string(KVS_NO);
	string tempfilename = "kvstoretemp" + to_string(KVS_NO);
	remove(filename.c_str()); //Delete the previous version of the kvstore
	rename(tempfilename.c_str(), filename.c_str()); //Rename the temp file to kvstore
	pthread_mutex_unlock(&storemtx);

}

string get_from_cache(int fd, string r, string c, bool ret){
	string response;
	Packet packet_response;
	int l;
	//Retreive the value at the specified row/column
	string val;
	bool found = false;
	bool skip = false;
	//Check if the requested entry is deleted.
	if(deleted.find(r)!=deleted.end()){
		if(deleted[r].find(c)!=deleted[r].end()){
			found = false;
			skip = true;
		}
	}
	if(!skip){
		write_lock(&cache_rw_mtx);
		if(keyvaluecache.find(r) != keyvaluecache.end()){
			//Row present in cache.
			if(keyvaluecache[r].find(c) != keyvaluecache[r].end()){
				val = keyvaluecache[r][c].first;
				//cout<<val<<endl;
				cout<<"Found data in cache "<<val<<endl;
				found = true;
			}else{
				//Column not found in cache.
				val = get_value_from_disk(r,c);
				if(val.length() !=0 ){
					keyvaluecache[r].insert(pair<string, pair<string, int>>(c, make_pair(val,1)));
					found = true;
					cout<<"Found data in disk "<<val<<endl;
				}
				else{
					found = false;
					cout<<"Not found in disk\n";
				}
			}
		}else{
			//Row not found in cache.
			cout<<"Calling get value from disk\n";
			val = get_value_from_disk(r,c);
			
			if(val.length() !=0 ){
				map <string, pair<string, int>> cacherow; //map to store each row of the tablet. Key contains the 'Column Key' and value contains the actual value in the column.
				cacherow.insert(pair<string, pair<string, int>>(c, make_pair(val,1)));
				keyvaluecache.insert(pair<string, map<string, pair<string, int>>>(r, cacherow));
				cout<<"Found data in disk "<<val<<endl;
				found = true;
			}
			else{
				found = false;
				cout<<"Not found in disk\n";
			}
		}
		write_unlock(&cache_rw_mtx);
	}
	if(!ret){
		if(found){
			packet_response.set_status_code("200");
			packet_response.set_status("success");
			packet_response.set_data(val);
			cout<<"Found "<<val<<endl;
			if(!packet_response.SerializeToString(&response)){
				fprintf(stderr, "Could not serialize response\n");
			}
			response = generate_prefix(response.size()) + response;
			char tosend[response.size()];
			response.copy(tosend, response.size());
			//l = response.length();
			do_send(&fd, tosend, sizeof(tosend));
			return val;
			/*response = "+OK VALUE FOUND " + val + "\n";
			l = response.length();
			my_write(fd, response.c_str(), l);*/
		}else{
			packet_response.set_status_code("500");
			packet_response.set_status("not found");
			cout<<"Not found\n";
			if(!packet_response.SerializeToString(&response)){
				fprintf(stderr, "Could not serialize response\n");
			}
			response = generate_prefix(response.size()) + response;
			char tosend[response.size()];
			response.copy(tosend, response.size());
			//l = response.length();
			do_send(&fd, tosend, sizeof(tosend));
			return "";
			/*response = "-ERR NOT FOUND\n";
			l = response.length();
			my_write(fd, response.c_str(), l);*/
		}
	}else{
		if(found)
			return val;
		else
			return "";
	}
			
	//printcache();
	if(vflag){
		fprintf(stderr, "[%d] S: %s", fd, response.c_str());
	}
}

int get_version_from_cache(string r, string c){
	//Retreive the version of the value at the specified row/column
	cout<<"Getting version\n";
	//Check if the requested entry is deleted.
	if(deleted.find(r)!=deleted.end()){
		if(deleted[r].find(c)!=deleted[r].end()){
			return 0;
		}
	}
	read_lock(&cache_rw_mtx, &cache_read_mtx , &cache_readcount);
	if(keyvaluecache.find(r) != keyvaluecache.end()){
		//Row present in cache.
		if(keyvaluecache[r].find(c) != keyvaluecache[r].end()){
			cout<<"Found data in cache "<<endl;
			read_unlock(&cache_rw_mtx, &cache_read_mtx , &cache_readcount);
			return keyvaluecache[r][c].second;
		}else{
			//Column not found in cache.
			read_unlock(&cache_rw_mtx, &cache_read_mtx , &cache_readcount);
			return get_version_from_disk(r,c);
		}
	}else{
		//Row not found in cache.
		read_unlock(&cache_rw_mtx, &cache_read_mtx , &cache_readcount);
		return get_version_from_disk(r,c);
	}
	read_unlock(&cache_rw_mtx, &cache_read_mtx , &cache_readcount);
	return 0;
}


int put_to_cache(int fd, string r, string c, string v, int vno, bool replay){
	string response;
	Packet packet_response;
	int l;
	write_lock(&cache_rw_mtx);
	//Insert to map
	if(keyvaluecache.find(r) != keyvaluecache.end()){
		//Row already present in cache
		if(keyvaluecache[r].find(c) != keyvaluecache[r].end()){
			//Add previous version to old cache
			if(old_keyvaluecache.find(r) != old_keyvaluecache.end()){
				//Row already present in cache
				if(old_keyvaluecache[r].find(c) != old_keyvaluecache[r].end()){
					//Column already present in row. Update the value
					old_keyvaluecache[r][c] = keyvaluecache[r][c];
				}else{
					//Column not present in row. Add new column.
					old_keyvaluecache[r].insert(pair<string, pair<string, int>>(c, keyvaluecache[r][c]));
				}
			}else{
				//Row not present. Insert new row.
				map <string, pair<string, int>> old_cacherow; //map to store each row of the tablet. Key contains the 'Column Key' and value contains the actual value in the column.
				old_cacherow.insert(pair<string, pair<string, int>>(c, keyvaluecache[r][c]));
				old_keyvaluecache.insert(pair<string, map<string, pair<string, int>>>(r, old_cacherow));
				//Insert user to 'ordering' list with initial values for P and A set to zero.
				//ordering.insert(pair<string, pair<int, int>>(r,make_pair(0,0)));
			}
			//Column already present in row. Update the value
			keyvaluecache[r][c] = make_pair(v,vno);
		}else{
			//Column not present in row. Add new column.
			keyvaluecache[r].insert(pair<string, pair<string, int>>(c, make_pair(v,vno)));
		}
	}else{
		//Row not present. Insert new row.
		map <string, pair<string, int>> cacherow; //map to store each row of the tablet. Key contains the 'Column Key' and value contains the actual value in the column.
		cacherow.insert(pair<string, pair<string, int>>(c, make_pair(v,vno)));
		keyvaluecache.insert(pair<string, map<string, pair<string, int>>>(r, cacherow));
		//Insert user to 'ordering' list with initial values for P and A set to zero.
		//ordering.insert(pair<string, pair<int, int>>(r,make_pair(0,0)));
	}
	write_unlock(&cache_rw_mtx);
	return 1;
}

void cput_to_cache(int fd, string r, string c, string v1, string v2, bool replay){
	string response;
	Packet packet_response;
	int l;
	bool success = false;

	//Request for version number from all replicas. Also start total ordering to ensure sequential consistency.
	//Get addresses of all replicas for the respective user (i.e. row)
	map <string,pair<int*,bool>> current_replicas; //Stores IP:Port of all replicas as key and a boolean (initially set to false) to indicate if we have got a response from that replica
	//current_replicas = get_my_replicas(r);
	int *a = 0;
	int *b = 0;
	current_replicas.insert(pair<string,pair<int*,bool>>("127.0.0.1:10004",make_pair(a,false)));
	current_replicas.insert(pair<string,pair<int*,bool>>("127.0.0.1:10005",make_pair(b,false)));

	//Create and send packet requesting from version number (to get the latest data) and proposed number (for ordering)
	int maxfd = 0;
	for(auto it = current_replicas.begin();it!=current_replicas.end();++it){
		pair<string, int> add = get_ip_port(it->first);
		(it->second).first = request_proposed_number(add.first, add.second,msgid,"GET", r, c, "", "");
		if(*((it->second).first) > maxfd){
			maxfd = *((it->second).first);
		}
	}

	//Get version number from own cache/disk
	int myvn = get_version_from_cache(r,c);
	//Wait for proposed number from all replicas
	int count = 0;
	int maxvn = myvn;
	string maxsender = self_ip + ":" + to_string(replica_port);
	fd_set readfd;
	
	while(count < current_replicas.size()){
		FD_ZERO(&readfd);
		for(auto it = current_replicas.begin();it!=current_replicas.end();++it){
			FD_SET(*((it->second).first),&readfd);
		}
		int activity = select(maxfd+1, &readfd, NULL, NULL, NULL);

		switch(activity){
			case -1:
				fprintf(stderr, "Error in selecting stream. Terminating\n");
				exit(1);
			case 0:
				fprintf(stderr, "Error in selecting stream. Terminating\n");
				exit(1);
			default:
			for(auto it = current_replicas.begin();it!=current_replicas.end();++it){
				if(FD_ISSET(*(current_replicas[it->first].first),&readfd)){
					count++;
					char* rcvd = (char *)malloc(1024 * 1024 + 200);
					int rcvdb = do_recv(current_replicas[it->first].first,&rcvd[0],-1);
					
					RepPacket rcvdpacket;
					rcvdpacket.ParseFromString(rcvd);
					//cout<<"VN RCVD: "<<rcvdpacket.version_number()<<endl;
					if(rcvdpacket.version_number() > maxvn){
						maxvn = rcvdpacket.version_number();
						maxsender = it->first;
					}
					//cout<<"MAX VN: "<<maxvn<<endl;
					//cout<<"PN: "<<rcvdpacket.proposed_number()<<endl;
				}
			}
		}
	}
	string myval;
	if(maxsender.compare(self_ip + ":" + to_string(replica_port)) == 0){
		//Node itself has the latest data
		if(keyvaluecache.find(r) != keyvaluecache.end()){
			//Row already present in cache
			if(keyvaluecache[r].find(c) != keyvaluecache[r].end()){
				//column found in cache
				myval = keyvaluecache[r][c].first;
			}else{
				//Column not present in cache. Find on disk.
				myval = get_value_from_disk(r,c);
			}
		}else{
			//Row not found in cache
			myval = get_value_from_disk(r,c);
		}
	}else{
		//Get value from the node which sent the highest version number
		pair<string, int> add1 = get_ip_port(maxsender);
		Packet response_packet; 
		myval = response_packet.data();
	}

	//Compare value with given value in the request
	if(v1.compare(myval) == 0 || myval.length() == 0){
		//Put to cache
		put_to_cache(-1, r, c, v2, maxvn+1, true);
		success = true;
	}else{
		//Send error
		success = false;
	}
	//printcache();
	if(!replay){
		if(success){
			packet_response.set_status_code("200");
			packet_response.set_status("success");
			cout<<"Data added to kvstore\n";
			if(!packet_response.SerializeToString(&response)){
				fprintf(stderr, "Could not serialize response\n");
			}
			response = generate_prefix(response.size()) + response;
			char tosend[response.size()];
			response.copy(tosend, response.size());
			//l = response.length();
			do_send(&fd, tosend, sizeof(tosend));
			cout<<"Updated cache is: ";
			printcache();
			/*response = "+OK INSERTED\n";
			l = response.length();
			my_write(fd, response.c_str(), l);*/
		}else{
			packet_response.set_status_code("500");
			packet_response.set_status("error");
			cout<<"could not add data to kvstore\n";
			if(!packet_response.SerializeToString(&response)){
				fprintf(stderr, "Could not serialize response\n");
			}
			response = generate_prefix(response.size()) + response;
			char tosend[response.size()];
			response.copy(tosend, response.size());
			//l = response.length();
			do_send(&fd, tosend, sizeof(tosend));
			/*response = "-ERR NOT FOUND IN CACHE\n";
			l = response.length();
			my_write(fd, response.c_str(), l);*/
		}
		if(vflag){
			fprintf(stderr, "[%d] S: %s", fd, response.c_str());
		}
	}
}

bool delete_from_cache(int fd, string r, string c, bool replay){
	string val, response;
	Packet packet_response;
	int l;
	bool found = false;
	write_lock(&cache_rw_mtx);
	if(keyvaluecache.find(r) != keyvaluecache.end()){
		//Row present in cache.
		if(keyvaluecache[r].find(c) != keyvaluecache[r].end()){
			val = keyvaluecache[r][c].first;
			//Add to deleted
			if(deleted.find(r)!=deleted.end()){
				deleted[r].insert(pair<string, string>(c, val));
			}else{
				map <string, string> delrow;
				delrow.insert(pair<string, string>(c,val));
				deleted.insert(pair<string, map<string, string>>(r, delrow));
			}
			//Add previous version to old cache
			if(old_keyvaluecache.find(r) != old_keyvaluecache.end()){
				//Row already present in cache
				if(old_keyvaluecache[r].find(c) != old_keyvaluecache[r].end()){
					//Column already present in row. Update the value
					old_keyvaluecache[r][c] = keyvaluecache[r][c];
				}else{
					//Column not present in row. Add new column.
					old_keyvaluecache[r].insert(pair<string, pair<string, int>>(c, keyvaluecache[r][c]));
				}
			}else{
				//Row not present. Insert new row.
				map <string, pair<string, int>> old_cacherow; //map to store each row of the tablet. Key contains the 'Column Key' and value contains the actual value in the column.
				old_cacherow.insert(pair<string, pair<string, int>>(c, keyvaluecache[r][c]));
				old_keyvaluecache.insert(pair<string, map<string, pair<string, int>>>(r, old_cacherow));
				//Insert user to 'ordering' list with initial values for P and A set to zero.
				//ordering.insert(pair<string, pair<int, int>>(r,make_pair(0,0)));
			}
			
			//Delete from cache
			keyvaluecache[r].erase(keyvaluecache[r].find(c));
			found = true;
		}else{
			//Column not found in cache.
			found = false;
		}
	}else{
		//Row not found in cache.
		found = false;
	}
	write_unlock(&cache_rw_mtx);
	return found;
	/*if(!replay){
		if(found){
			packet_response.set_status_code("200");
			packet_response.set_status("success");
			cout<<"Deleted \n";
			if(!packet_response.SerializeToString(&response)){
				fprintf(stderr, "Could not serialize response\n");
			}
			response = generate_prefix(response.size()) + response;
			char tosend[response.size()];
			response.copy(tosend, response.size());
			//l = response.length();
			do_send(&fd, tosend, sizeof(tosend));
			cout<<"Updated cache is: ";
			printcache();
			/*response = "+OK DELETED " + val + "\n";
			l = response.length();
			my_write(fd, response.c_str(), l);
		}else{
			packet_response.set_status_code("500");
			packet_response.set_status("could not delete");
			if(!packet_response.SerializeToString(&response)){
				fprintf(stderr, "Could not serialize response\n");
			}
			response = generate_prefix(response.size()) + response;
			char tosend[response.size()];
			response.copy(tosend, response.size());
			//l = response.length();
			do_send(&fd, tosend, sizeof(tosend));
			/*response = "-ERR NOT FOUND IN CACHE\n";
			l = response.length();
			my_write(fd, response.c_str(), l);
		}
		if(vflag){
			fprintf(stderr, "[%d] S: %s", fd, response.c_str());
		}
	}*/
}

void undo_operation(string r, string c){
	write_lock(&cache_rw_mtx);
	if(old_keyvaluecache.find(r) != old_keyvaluecache.end()){
		//Row already present in cache
		if(old_keyvaluecache[r].find(c) != old_keyvaluecache[r].end()){
			//Column already present in row. Restore the value
			keyvaluecache[r][c] = old_keyvaluecache[r][c];
			old_keyvaluecache[r].erase(old_keyvaluecache[r].find(c));
		}else{
			keyvaluecache[r].erase(keyvaluecache[r].find(c));
		}
	}else{
		keyvaluecache[r].erase(keyvaluecache[r].find(c));
	}
	write_unlock(&cache_rw_mtx);
}

int* request_proposed_number(string ip, int port, int id, string req, string r, string c, string d, string d2c){
	RepPacket orderingpacket;
	string serialized;
	struct sockaddr_in temp_addr;
	int *tempsock = (int *)malloc(sizeof(int));

	orderingpacket.set_command("PROPOSE");
	orderingpacket.set_request(req);
	orderingpacket.set_row(r);
	orderingpacket.set_column(c);
	orderingpacket.set_msgid(to_string(KVS_NO) + ":" + to_string(id));
	if(d.length()!=0)
		orderingpacket.set_data(d);
	if(d2c.length()!=0)
		orderingpacket.set_data_to_compare(d2c); 
	cout<<"proposing: "<<orderingpacket.msgid()<<endl;
	//orderingpacket.set_senderid(sid);

	if(!orderingpacket.SerializeToString(&serialized)){
		fprintf(stderr, "Could not serialize response\n");
	}
	serialized = generate_prefix(serialized.size()) + serialized;
	char tosend[serialized.size()];
	serialized.copy(tosend, serialized.size());
	//Set up address for sending UDP packet to all replica
	*tempsock = socket(AF_INET, SOCK_STREAM, 0);
	
	if(*tempsock < 0)
		fprintf(stderr, "Error opening socket\n");

	bzero((char *) &temp_addr, sizeof(temp_addr));

	//Set up address structure
	temp_addr.sin_family = AF_INET;
	temp_addr.sin_port = htons(port);
	inet_pton(AF_INET, ip.c_str(), &(temp_addr.sin_addr));
	if(connect(*tempsock, (struct sockaddr *)&temp_addr, sizeof(temp_addr)) < 0){
		fprintf(stderr, "Error connecting\n");
		return 0;
	}
	do_send(tempsock, tosend, sizeof(tosend));
	return tempsock;
}

Packet get_latest_data(int sfd, string r, string c){
	RepPacket orderingpacket; 
	Packet response_packet;
	string serialized;

	orderingpacket.set_command("GETLATEST");
	orderingpacket.set_row(r);
	orderingpacket.set_column(c);

	if(!orderingpacket.SerializeToString(&serialized)){
		fprintf(stderr, "Could not serialize response\n");
	}
	serialized = generate_prefix(serialized.size()) + serialized;
	char tosend[serialized.size()];
	serialized.copy(tosend, serialized.size());
	cout<<"Sending GET LATEST to "<<sfd<<" "<<tosend<<endl;
	int sent = do_send(&sfd, tosend, sizeof(tosend));
	cout<<"sent: "<<sent<<endl;
	cout<<"Waiting for response\n";
	char *buffer = (char*)malloc(1024 * 1024 + 200);
	int rcvdbytes = do_recv(&sfd, &buffer[0],10);
	if(rcvdbytes < 0){
		response_packet.set_status_code("500");
		return response_packet;
	}
	response_packet.ParseFromString(buffer);
	return response_packet;
}

void send_final_data(int sfd, string mid, string req, string r, string c, string data, int ver_no, int maxpn, int maxpnsender){
	RepPacket orderingpacket;
	string serialized;

	orderingpacket.set_command("MAXPROPOSAL");
	orderingpacket.set_request(req);
	orderingpacket.set_row(r);
	orderingpacket.set_column(c);
	orderingpacket.set_proposed_number(maxpn);
	orderingpacket.set_version_number(ver_no);
	orderingpacket.set_deliverable(1);
	orderingpacket.set_senderid(maxpnsender);
	orderingpacket.set_msgid(mid);
	if(data.length()!=0)
		orderingpacket.set_data(data);

	if(!orderingpacket.SerializeToString(&serialized)){
		fprintf(stderr, "Could not serialize response\n");
	}
	serialized = generate_prefix(serialized.size()) + serialized;
	char tosend[serialized.size()];
	serialized.copy(tosend, serialized.size());
	
	do_send(&sfd, tosend, sizeof(tosend));
}

void send_undo(int sfd, string r, string c){
	RepPacket orderingpacket;
	string serialized;

	orderingpacket.set_command("UNDO");
	orderingpacket.set_row(r);
	orderingpacket.set_column(c);

	if(!orderingpacket.SerializeToString(&serialized)){
		fprintf(stderr, "Could not serialize response\n");
	}
	serialized = generate_prefix(serialized.size()) + serialized;
	char tosend[serialized.size()];
	serialized.copy(tosend, serialized.size());
	
	do_send(&sfd, tosend, sizeof(tosend));
}

void forward_to_remaining(vector<string> remaining_replicas, pair<int, int> seq, string req){
	cout<<"In forward\n";
	for(int i=0;i<remaining_replicas.size();i++){
		cout<<"In loop\n";
		pair<string, int> addr = get_ip_port(remaining_replicas[i]);
		string ip = addr.first;
		int port = addr.second;

		RepPacket orderingpacket;
		string serialized;
		struct sockaddr_in temp_addr;
		int *tempsock = (int *)malloc(sizeof(int));

		Queuepacket tempq;
		tempq.ParseFromString(req);

		orderingpacket.set_command("forward");
		orderingpacket.set_request(tempq.command());
		orderingpacket.set_row(tempq.row());
		orderingpacket.set_column(tempq.column());
		orderingpacket.set_data(tempq.data());
		orderingpacket.set_version_number(tempq.version_number()); 
		orderingpacket.set_proposed_number(seq.first);
		orderingpacket.set_senderid(seq.second);
		//orderingpacket.set_senderid(sid);

		if(!orderingpacket.SerializeToString(&serialized)){
			fprintf(stderr, "Could not serialize response\n");
		}
		serialized = generate_prefix(serialized.size()) + serialized;
		char tosend[serialized.size()];
		serialized.copy(tosend, serialized.size());
		//Set up address for sending UDP packet to all replica
		*tempsock = socket(AF_INET, SOCK_STREAM, 0);
		
		if(*tempsock < 0)
			fprintf(stderr, "Error opening socket\n");

		bzero((char *) &temp_addr, sizeof(temp_addr));

		//Set up address structure
		temp_addr.sin_family = AF_INET;
		temp_addr.sin_port = htons(port);
		inet_pton(AF_INET, ip.c_str(), &(temp_addr.sin_addr));
		if(connect(*tempsock, (struct sockaddr *)&temp_addr, sizeof(temp_addr)) < 0){
			fprintf(stderr, "Error connecting\n");
		}
		do_send(tempsock, tosend, sizeof(tosend));	
	}

}

int do_put(int fd, string r, string c, string v, int id){
	Packet packet_response;
	string response;
	int maxvn;

	vector<string> current_replicas; //Stores IP:Port of all replicas as key and a boolean (initially set to false) to indicate if we have got a response from that replica
	vector<pair<int*,bool>> replicas_connected_to; //Stores socket FD of all those replicas (R/W out of N) to whom the proposal request was sent
	vector<string> remaining_replicas;
	current_replicas = replicas;

	//Create and send packet requesting version number (to get the latest data) and proposed number (for ordering)
	int maxfd = 0;
	int connected = 0;
	bool over = false;
	for (int i=0;i<current_replicas.size();i++){
		pair<string, int> add = get_ip_port(current_replicas[i]);
		int *fd_rcvd;
		if(over){
			remaining_replicas.push_back(current_replicas[i]);
		}else{
			if((fd_rcvd = request_proposed_number(add.first, add.second,id,"PUT", r, c, v, "")) == 0){
				//Could not connect to this replica
				remaining_replicas.push_back(current_replicas[i]);
			}else{
				replicas_connected_to.push_back(make_pair(fd_rcvd,false));
				connected++;
				if(*fd_rcvd > maxfd){
					maxfd = *fd_rcvd;
				}
			}
		} 
		if(connected == W){
			over = true;
		}
	}
	cout<<"connected to: "<<replicas_connected_to.size()<<endl;
	cout<<"rem: "<<remaining_replicas.size()<<endl;
	if(replicas_connected_to.size() < W){
		//Send error to user
		packet_response.set_status_code("500");
		packet_response.set_status("could not connect to replica");
		cout<<"could not connect to replica\n";
		if(!packet_response.SerializeToString(&response)){
			fprintf(stderr, "Could not serialize response\n");
		}
		response = generate_prefix(response.size()) + response;
		char tosend[response.size()];
		response.copy(tosend, response.size());
		do_send(&fd, tosend, sizeof(tosend));
		return -1;
	}else{
		//Add command to own holdback queue
		int mymaxP, mymaxA;
		if(ordering.find(r) != ordering.end()){
			mymaxP = ordering[r].first;
			mymaxA = ordering[r].second;
		}else{
			mymaxP = 0;
			mymaxA = 0;
			ordering.insert(pair<string, pair<int, int>>(r,make_pair(0,0)));
		}
		int mypn = max(mymaxA,mymaxP)+1;
		ordering[r].first = mypn;
		string queuedata;
		Queuepacket q;
		q.set_command("PUT");
		q.set_row(r);
		q.set_column(c);
		q.set_data(v);
		q.set_msgid(to_string(KVS_NO) + ":" + to_string(msgid));
		q.set_sender_socket(fd);
		if(!q.SerializeToString(&queuedata)){
				fprintf(stderr, "Could not serialize response\n");
		}
		write_lock(&queue_rw_mtx);
		if(holdbackqueue.find(r) != holdbackqueue.end()){
			//User already present in holdbackqueue
			holdbackqueue[r].insert(pair<pair<int, int>,pair<string, bool>>(make_pair(mypn,KVS_NO),make_pair(queuedata, false)));
		}else{
			map <pair<int, int>, pair<string, bool>> t;
			t.insert(pair<pair<int, int>,pair<string, bool>>(make_pair(mypn,KVS_NO),make_pair(queuedata, false)));
			holdbackqueue.insert(pair<string,map<pair<int, int>, pair<string, bool>>>(r,t));
		}
		write_unlock(&queue_rw_mtx);

		//Get version number from own cache/disk
		int myvn = get_version_from_cache(r,c);
		//Wait for proposed number from all replicas
		int count = 0;
		int maxpn = mypn;
		maxvn = myvn;
		cout<<"maxvn: "<<maxvn<<endl;
		int maxsender = -1; //IP + PORT of the node which sent the highest version number
		int maxpnsender = KVS_NO; //ID (KVS NO) of the node which sent the highest Proposed number (for ordering)
		fd_set readfd;
		string mid;

		while(count < replicas_connected_to.size()){
			FD_ZERO(&readfd);
			for(int i=0;i<replicas_connected_to.size();i++){
				FD_SET(*(replicas_connected_to[i].first),&readfd);
			}
			int activity = select(maxfd+1, &readfd, NULL, NULL, NULL);

			switch(activity){
				case -1:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					exit(1);
				case 0:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					exit(1);
				default:
				for(int i=0;i<replicas_connected_to.size();i++){
					if(FD_ISSET(*(replicas_connected_to[i].first),&readfd)){
						count++;
						char *rcvd = (char*)malloc(1024 * 1024 + 200);
						int rcvdb = do_recv(replicas_connected_to[i].first,&rcvd[0],-1);
						
						RepPacket rcvdpacket;
						rcvdpacket.ParseFromString(rcvd);
						mid = rcvdpacket.msgid();
						cout<<"mid rcvd: "<<rcvdpacket.msgid()<<endl;
						cout<<"VN RCVD: "<<rcvdpacket.version_number()<<endl;
						if(rcvdpacket.version_number() > maxvn){
							maxvn = rcvdpacket.version_number();
							maxsender = *(replicas_connected_to[i].first);
						}
						if(rcvdpacket.proposed_number() > maxpn){
							maxpn = rcvdpacket.proposed_number();
							maxpnsender = rcvdpacket.senderid();
						}
						cout<<"MAX VN: "<<maxvn<<endl;
						cout<<"PN: "<<rcvdpacket.proposed_number()<<endl;
					}
				}
			}
		}
		
		cout<<"MaxPN: "<<maxpn<<" MaxVN: "<<maxvn<<endl;
		ordering[r].second = maxpn;

		//Send data with maximum proposed number and latest version number to all replicas
		for(int i=0;i<replicas_connected_to.size();i++){
			send_final_data(*(replicas_connected_to[i].first),mid,"PUT", r, c, v, maxvn+1, maxpn, maxpnsender);
		}

		//Wait for acknowledgement from all replicas and then update own holdback queue.
		count = 0;
		bool err = false;
		struct timeval tv,tv1;
		tv.tv_sec = 10;
		vector <int> ack_rcvd_from;
		cout<<"replicas_connected_to: "<<replicas_connected_to.size()<<endl;
		while(count < replicas_connected_to.size()){
			cout<<"In while\n";
			memcpy(&tv1, &tv, sizeof(tv));
			FD_ZERO(&readfd);
			for(int i=0;i<replicas_connected_to.size();i++){
				FD_SET(*(replicas_connected_to[i].first),&readfd);
			}
			int activity = select(maxfd+1, &readfd, NULL, NULL, &tv1);
			cout<<"activity: "<<activity<<endl;

			switch(activity){
				case -1:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					err = true;
					break;
				case 0:
					//Terminating because of time out.
					fprintf(stderr, "Error in selecting stream because of time out. Terminating\n");
					err = true;
					break;
				default:
				cout<<"In switch default\n";
				for(int i=0;i<replicas_connected_to.size();i++){
					if(FD_ISSET(*(replicas_connected_to[i].first),&readfd)){
						char *rcvd = (char*)malloc(1024 * 1024 + 200);
						cout<<"Waiting to rcv\n";
						int rcvdb = do_recv(replicas_connected_to[i].first,&rcvd[0],-1);
						rcvd[rcvdb] = 0;
						cout<<"rcvddddd: "<<rcvd<<endl;
						Packet mypacket;
						mypacket.ParseFromString(rcvd);
						cout<<"hhhh:"<<mypacket.status_code()<<endl;
						if(mypacket.status_code().compare("200") == 0){
							count++;
							cout<<"ACK RCVD\n";
							ack_rcvd_from.push_back(*(replicas_connected_to[i].first));
						}else{
							//Send error to user
							err = true;
							break;
						}
					}
				}
				if(err)
					break;
			}
			if(err)
				break;
		}
		if(err){
			//Send an undo to all those who acknowledged
			for(int i=0;i<ack_rcvd_from.size();i++){
				send_undo(ack_rcvd_from[i],r,c);
			}
		}
		/*for(int i=0;i<replicas_connected_to.size();i++){
			close(*(replicas_connected_to[i].first));
		}*/
		if(!err){
			//Update own holdback queue
			write_lock(&queue_rw_mtx);
			cout<<"Holdbackqueue\n";
			for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
				cout<<it->first.first<<" "<<it->first.second<<" "<<it->second.first<<" "<<it->second.second<<endl;
			}
			ordering[r].second = max(ordering[r].second, maxpn);
			for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
				Queuepacket tempq;
				tempq.ParseFromString(it->second.first);
				cout<<"mid: "<<mid<<" In queue: "<<tempq.msgid()<<endl;
				if(tempq.msgid().compare(mid) == 0){
					cout<<"In If\n";
					tempq.set_version_number(maxvn+1);
					string t;
					if(!tempq.SerializeToString(&t)){
						fprintf(stderr, "Could not serialize response\n");
					}
					cout<<"Adding to holdbackqueue\n";
					holdbackqueue[r].erase(it);
					holdbackqueue[r].insert(pair<pair<int, int>, pair<string, bool>>(make_pair(maxpn, maxpnsender),make_pair(t,true)));
					break;
				}
			}
			cout<<"Out of loop\n";
			
			//Check if request is deliverable
			cout<<"Holdbackqueue\n";
			for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
				cout<<it->first.first<<" "<<it->first.second<<" "<<it->second.first<<" "<<it->second.second<<endl;
			}
			while(!holdbackqueue[r].empty() && holdbackqueue[r].begin()->second.second){
				Queuepacket tempq;
				tempq.ParseFromString(holdbackqueue[r].begin()->second.first);
				int putret = put_to_cache(tempq.sender_socket(), tempq.row(), tempq.column(), tempq.data(), tempq.version_number(), false);
				
				if(putret > 0){
					//Add entry to log with a global sequence number
					//Log commands to log file (as JSON)
					pthread_mutex_lock(&logmtx);
					fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
					map <string, string> tempjsonmap;
					tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
					tempjsonmap.insert(pair<string, string>("command", "PUT"));
					tempjsonmap.insert(pair<string, string>("row", tempq.row()));
					tempjsonmap.insert(pair<string, string>("col", tempq.column()));
					tempjsonmap.insert(pair<string, string>("val", tempq.data()));
					tempjsonmap.insert(pair<string, string>("version", to_string(maxvn+1)));
					log_seq++;
					tempjsonmap.insert(pair<string, string>("sequence_number", to_string(log_seq)));
					tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
					json logjson(tempjsonmap);
					mylog<<logjson<<"\n";
					tempjsonmap.clear();
					logsize += mylog.tellg();
					
					if(logsize > 1000){
						
						//Dump data to disk (file)
						put_to_disk(keyvaluecache);
						//Clear log file
						ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
						ofs.close();
						logsize = 0;
					}
					mylog.close();
					pthread_mutex_unlock(&logmtx);
					//Send an acknowledgement to the socket present in the queuepacket
					Queuepacket tempq;
					tempq.ParseFromString(holdbackqueue[r].begin()->second.first);
					Packet packet_response;
					packet_response.set_status_code("200");
					packet_response.set_status("success");
					string serialized;
					if(!packet_response.SerializeToString(&serialized)){
						fprintf(stderr, "Could not serialize response\n");
					}
					serialized = generate_prefix(serialized.size()) + serialized;
					char tosend[serialized.size()];
					serialized.copy(tosend, serialized.size());
					int tfd = tempq.sender_socket();
					do_send(&tfd,tosend, sizeof(tosend));
					cout<<"remaining_replicas size: "<<remaining_replicas.size()<<endl;
					forward_to_remaining(remaining_replicas, holdbackqueue[r].begin()->first, holdbackqueue[r].begin()->second.first);
				}
				holdbackqueue[r].erase(holdbackqueue[r].begin());
			}
			write_unlock(&queue_rw_mtx);
		}else{
			write_lock(&queue_rw_mtx);
			//Remove the packet from the holdbackqueue
			for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
				Queuepacket tempq;
				tempq.ParseFromString(it->second.first);
				cout<<"mid: "<<mid<<" In queue: "<<tempq.msgid()<<endl;
				if(tempq.msgid().compare(mid) == 0){
					string t = it->second.first;
					holdbackqueue[r].erase(it);
					break;
				}
			}
			write_unlock(&queue_rw_mtx);
			return -1;
		}
	}
	return maxvn;
}

void do_get(int fd, string r, string c, int id){
	Packet packet_response;
	string response;

	//Request for version number from all replicas. Also start total ordering to ensure sequential consistency.
	//Get addresses of all replicas for the respective user (i.e. row)
	vector<string> current_replicas; //Stores IP:Port of all replicas as key and a boolean (initially set to false) to indicate if we have got a response from that replica
	vector<pair<int*,bool>> replicas_connected_to; //Stores socket FD of all those replicas (R/W out of N) to whom the proposal request was sent
	current_replicas = replicas;

	//Create and send packet requesting version number (to get the latest data) and proposed number (for ordering)
	int maxfd = 0;
	int connected = 0;
	for (int i=0;i<current_replicas.size();i++){
		pair<string, int> add = get_ip_port(current_replicas[i]);
		int *fd_rcvd;
		if((fd_rcvd = request_proposed_number(add.first, add.second,id,"GET", r, c, "", "")) == 0){
			//Could not connect to this replica
		}else{
			cout<<"fd_rcvd: "<<*fd_rcvd<<endl;
			replicas_connected_to.push_back(make_pair(fd_rcvd,false));
			connected++;
			if(*fd_rcvd > maxfd){
				maxfd = *fd_rcvd;
			}
		}
		if(connected == R){
			break;
		}
	}
	cout<<"connected to: "<<replicas_connected_to.size()<<endl;
	if(replicas_connected_to.size() < R){
		//Send error to user
		packet_response.set_status_code("500");
		packet_response.set_status("could not connect to replica");
		cout<<"could not connect to replica\n";
		if(!packet_response.SerializeToString(&response)){
			fprintf(stderr, "Could not serialize response\n");
		}
		response = generate_prefix(response.size()) + response;
		char tosend[response.size()];
		response.copy(tosend, response.size());
		do_send(&fd, tosend, sizeof(tosend));
	}else{
		//Get version number from own cache/disk
		int myvn = get_version_from_cache(r,c);
		//Wait for proposed number from all replicas
		int count = 0;
		int maxvn = myvn;
		int maxsender = -1;
		fd_set readfd;
		
		while(count < replicas_connected_to.size()){
			FD_ZERO(&readfd);
			for(int i=0;i<replicas_connected_to.size();i++){
				FD_SET(*(replicas_connected_to[i].first),&readfd);
			}
			int activity = select(maxfd+1, &readfd, NULL, NULL, NULL);

			switch(activity){
				case -1:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					exit(1);
				case 0:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					exit(1);
				default:
				for(int i=0;i<replicas_connected_to.size();i++){
					if(FD_ISSET(*(replicas_connected_to[i].first),&readfd)){
						count++;
						char *rcvd = (char*)malloc(1024 * 1024 + 200);
						int rcvdb = do_recv(replicas_connected_to[i].first,&rcvd[0],-1);
						
						RepPacket rcvdpacket;
						rcvdpacket.ParseFromString(rcvd);
						cout<<"VN RCVD: "<<rcvdpacket.version_number()<<" from: "<<*(replicas_connected_to[i].first)<<endl;
						if(rcvdpacket.version_number() > maxvn){
							maxvn = rcvdpacket.version_number();
							maxsender = *(replicas_connected_to[i].first);
						}
						cout<<"MAX VN: "<<maxvn<<endl;
						cout<<"PN: "<<rcvdpacket.proposed_number()<<endl;
					}
				}
			}
		}
		cout<<"outside while \n";
		cout<<"Got max from: "<<maxsender<<endl;
		if(maxsender < 0){
			//Node itself has the latest data
			cout<<"In if\n";
			get_from_cache(fd, r,c, false);
		}else{
			//Get value from the node which sent the highest version number
			cout<<"In else\n";
			Packet response_packet = get_latest_data(maxsender, r, c);

			if(response_packet.status_code().compare("200") == 0){
				//Insert latest data to own cache
				int putret = put_to_cache(-1,r,c,response_packet.data(),maxvn, false);
				if(putret > 0){
					packet_response.set_status_code("200");
					packet_response.set_status("success");
					packet_response.set_data(response_packet.data());
					//cout<<"Found "<<val<<endl;
					if(!packet_response.SerializeToString(&response)){
						fprintf(stderr, "Could not serialize response\n");
					}
					response = generate_prefix(response.size()) + response;
					char tosend[response.size()];
					response.copy(tosend, response.size());
					do_send(&fd, tosend, sizeof(tosend));
				}
			}else{
				packet_response.set_status_code("500");
				packet_response.set_status("not found");
				cout<<"Not found\n";
				if(!packet_response.SerializeToString(&response)){
					fprintf(stderr, "Could not serialize response\n");
				}
				response = generate_prefix(response.size()) + response;
				char tosend[response.size()];
				response.copy(tosend, response.size());
				do_send(&fd, tosend, sizeof(tosend));
			}
			
		}
	}
}

int do_cput(int fd, string r, string c, string v1, string v2, int id){
	Packet packet_response;
	string response;
	int maxvn;

	vector<string> current_replicas; //Stores IP:Port of all replicas as key and a boolean (initially set to false) to indicate if we have got a response from that replica
	vector<pair<int*,bool>> replicas_connected_to; //Stores socket FD of all those replicas (R/W out of N) to whom the proposal request was sent
	vector<string> remaining_replicas;
	current_replicas = replicas;

	//Create and send packet requesting version number (to get the latest data) and proposed number (for ordering)
	int maxfd = 0;
	int connected = 0;
	bool over = false;
	for (int i=0;i<current_replicas.size();i++){
		pair<string, int> add = get_ip_port(current_replicas[i]);
		int *fd_rcvd;
		if(over){
			remaining_replicas.push_back(current_replicas[i]);
		}else{
			if((fd_rcvd = request_proposed_number(add.first, add.second,id,"CPUT", r, c, v2, v1)) == 0){
				//Could not connect to this replica
				remaining_replicas.push_back(current_replicas[i]);
			}else{
				replicas_connected_to.push_back(make_pair(fd_rcvd,false));
				connected++;
				if(*fd_rcvd > maxfd){
					maxfd = *fd_rcvd;
				}
			}
		} 
		if(connected == W){
			over = true;
		}
	}
	cout<<"connected to: "<<replicas_connected_to.size()<<endl;
	if(replicas_connected_to.size() < W){
		//Send error to user
		packet_response.set_status_code("500");
		packet_response.set_status("could not connect to replica");
		cout<<"could not connect to replica\n";
		if(!packet_response.SerializeToString(&response)){
			fprintf(stderr, "Could not serialize response\n");
		}
		response = generate_prefix(response.size()) + response;
		char tosend[response.size()];
		response.copy(tosend, response.size());
		do_send(&fd, tosend, sizeof(tosend));
		return -1;
	}else{
		//Add command to own holdback queue
		int mymaxP, mymaxA;
		if(ordering.find(r) != ordering.end()){
			mymaxP = ordering[r].first;
			mymaxA = ordering[r].second;
		}else{
			mymaxP = 0;
			mymaxA = 0;
			ordering.insert(pair<string, pair<int, int>>(r,make_pair(0,0)));
		}
		int mypn = max(mymaxA,mymaxP)+1;
		ordering[r].first = mypn;
		string queuedata;
		Queuepacket q;
		q.set_command("CPUT");
		q.set_row(r);
		q.set_column(c);
		q.set_data_to_compare(v1);
		q.set_data(v2);
		q.set_msgid(to_string(KVS_NO) + ":" + to_string(msgid));
		q.set_sender_socket(fd);
		if(!q.SerializeToString(&queuedata)){
			fprintf(stderr, "Could not serialize response\n");
		}
		write_lock(&queue_rw_mtx);
		if(holdbackqueue.find(r) != holdbackqueue.end()){
			//User already present in holdbackqueue
			holdbackqueue[r].insert(pair<pair<int, int>,pair<string, bool>>(make_pair(mypn,KVS_NO),make_pair(queuedata, false)));
		}else{
			map <pair<int, int>, pair<string, bool>> t;
			t.insert(pair<pair<int, int>,pair<string, bool>>(make_pair(mypn,KVS_NO),make_pair(queuedata, false)));
			holdbackqueue.insert(pair<string,map<pair<int, int>, pair<string, bool>>>(r,t));
		}
		write_unlock(&queue_rw_mtx);


		//Get version number from own cache/disk
		int myvn = get_version_from_cache(r,c);
		//Wait for proposed number from all replicas
		int count = 0;
		int maxpn = mypn;
		maxvn = myvn;
		cout<<"maxvn: "<<maxvn<<endl;
		int maxsender = -1; //IP + PORT of the node which sent the highest version number
		int maxpnsender = KVS_NO; //ID (KVS NO) of the node which sent the highest Proposed number (for ordering)
		fd_set readfd;
		string mid;
		
		while(count < replicas_connected_to.size()){
			FD_ZERO(&readfd);
			for(int i=0;i<replicas_connected_to.size();i++){
				FD_SET(*(replicas_connected_to[i].first),&readfd);
			}
			int activity = select(maxfd+1, &readfd, NULL, NULL, NULL);

			switch(activity){
				case -1:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					exit(1);
				case 0:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					exit(1);
				default:
				for(int i=0;i<replicas_connected_to.size();i++){
					if(FD_ISSET(*(replicas_connected_to[i].first),&readfd)){
						count++;
						char *rcvd = (char*)malloc(1024 * 1024 + 200);
						int rcvdb = do_recv(replicas_connected_to[i].first,&rcvd[0],-1);
						
						RepPacket rcvdpacket;
						rcvdpacket.ParseFromString(rcvd);
						mid = rcvdpacket.msgid();
						cout<<"mid rcvd: "<<rcvdpacket.msgid()<<endl;
						cout<<"VN RCVD: "<<rcvdpacket.version_number()<<endl;
						if(rcvdpacket.version_number() > maxvn){
							maxvn = rcvdpacket.version_number();
							maxsender = *(replicas_connected_to[i].first);
						}
						if(rcvdpacket.proposed_number() > maxpn){
							maxpn = rcvdpacket.proposed_number();
							maxpnsender = rcvdpacket.senderid();
						}
						cout<<"MAX VN: "<<maxvn<<endl;
						cout<<"PN: "<<rcvdpacket.proposed_number()<<endl;
					}
				}
			}
		}
		
		cout<<"MaxPN: "<<maxpn<<" MaxVN: "<<maxvn<<endl;
		ordering[r].second = maxpn;

		string maxval;
		if(maxsender < 0){
			//Node itself has the latest data
			maxval = get_from_cache(-1, r,c, true);
		}else{
			//Get value from the node which sent the highest version number
			Packet response_packet = get_latest_data(maxsender, r, c);
			if(response_packet.status_code().compare("500") == 0){
				//Send error
				maxval = "$$NOTFOUND$$";
			}else{
				maxval = response_packet.data();
			}
		}
		cout<<"Maxval: "<<maxval<<endl;
		if(maxval.compare(v1) == 0){
			//Send data with maximum proposed number and latest version number to all replicas
			for(int i=0;i<replicas_connected_to.size();i++){
				send_final_data(*(replicas_connected_to[i].first),mid,"CPUT", r, c, v2, maxvn+1, maxpn, maxpnsender);
			}


			//Wait for acknowledgement from all replicas and then update own holdback queue.
			count = 0;
			bool err = false;
			struct timeval tv,tv1;
			tv.tv_sec = 10;
			vector <int> ack_rcvd_from;
			while(count < replicas_connected_to.size()){
				memcpy(&tv1, &tv, sizeof(tv));
				FD_ZERO(&readfd);
				for(int i=0;i<replicas_connected_to.size();i++){
					FD_SET(*(replicas_connected_to[i].first),&readfd);
				}
				int activity = select(maxfd+1, &readfd, NULL, NULL, &tv1);

				switch(activity){
					case -1:
						fprintf(stderr, "Error in selecting stream. Terminating\n");
						err = true;
						break;
					case 0:
						//Terminating because of time out.
						fprintf(stderr, "Error in selecting stream because of time out. Terminating\n");
						err = true;
						break;
					default:
					for(int i=0;i<replicas_connected_to.size();i++){
						if(FD_ISSET(*(replicas_connected_to[i].first),&readfd)){
							char *rcvd = (char*)malloc(1024 * 1024 + 200);
							int rcvdb = do_recv(replicas_connected_to[i].first,&rcvd[0],-1);
							rcvd[rcvdb] = 0;

							Packet rcvdpacket;
							rcvdpacket.ParseFromString(rcvd);
							if(rcvdpacket.status_code().compare("200") == 0){
								count++;
								ack_rcvd_from.push_back(*(replicas_connected_to[i].first));
							}else{
								//Send error to user
								err = true;
								break;
							}
						}
					}
					if(err)
						break;
				}
				if(err)
					break;
			}
			if(err){
				//Send an undo to all those who acknowledged
				for(int i=0;i<ack_rcvd_from.size();i++){
					send_undo(ack_rcvd_from[i],r,c);
				}
			}
			for(int i=0;i<replicas_connected_to.size();i++){
				close(*(replicas_connected_to[i].first));
			}
			if(!err){
				write_lock(&queue_rw_mtx);
				//Update own holdback queue
				ordering[r].second = max(ordering[r].second, maxpn);
				for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
					Queuepacket tempq;
					tempq.ParseFromString(it->second.first);
					cout<<"mid: "<<mid<<" In queue: "<<tempq.msgid()<<endl;
					if(tempq.msgid().compare(mid) == 0){
						tempq.set_version_number(maxvn+1);
						string t;
						if(!tempq.SerializeToString(&t)){
							fprintf(stderr, "Could not serialize response\n");
						}
						holdbackqueue[r].erase(it);
						holdbackqueue[r].insert(pair<pair<int, int>, pair<string, bool>>(make_pair(maxpn, maxpnsender),make_pair(t,true)));
						break;
					}
				}
				//Check if request is deliverable
				cout<<"Holdbackqueue\n";
				for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
					cout<<it->first.first<<" "<<it->first.second<<" "<<it->second.first<<" "<<it->second.second<<endl;
				}
				while(!holdbackqueue[r].empty() && holdbackqueue[r].begin()->second.second){
					Queuepacket tempq;
					tempq.ParseFromString(holdbackqueue[r].begin()->second.first);
					int putret = put_to_cache(tempq.sender_socket(), tempq.row(), tempq.column(), tempq.data(), tempq.version_number(), false);
					
					if(putret > 0){
						//Add entry to log with a global sequence number
						//Log commands to log file (as JSON)
						pthread_mutex_lock(&logmtx);
						fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
						map <string, string> tempjsonmap;
						tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
						tempjsonmap.insert(pair<string, string>("command", "CPUT"));
						tempjsonmap.insert(pair<string, string>("row", tempq.row()));
						tempjsonmap.insert(pair<string, string>("col", tempq.column()));
						tempjsonmap.insert(pair<string, string>("val", tempq.data()));
						tempjsonmap.insert(pair<string, string>("version", to_string(maxvn+1)));
						log_seq++;
						tempjsonmap.insert(pair<string, string>("sequence_number", to_string(log_seq)));
						tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
						json logjson(tempjsonmap);
						mylog<<logjson<<"\n";
						tempjsonmap.clear();
						logsize += mylog.tellg();
						
						if(logsize > 1000){
							
							//Dump data to disk (file)
							put_to_disk(keyvaluecache);
							//Clear log file
							ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
							ofs.close();
							logsize = 0;
						}
						mylog.close();
						pthread_mutex_unlock(&logmtx);
						//Send an acknowledgement to the socket present in the queuepacket
						Queuepacket tempq;
						tempq.ParseFromString(holdbackqueue[r].begin()->second.first);
						Packet packet_response;
						packet_response.set_status_code("200");
						packet_response.set_status("success");
						string serialized;
						if(!packet_response.SerializeToString(&serialized)){
							fprintf(stderr, "Could not serialize response\n");
						}
						serialized = generate_prefix(serialized.size()) + serialized;
						char tosend[serialized.size()];
						serialized.copy(tosend, serialized.size());
						int tfd = tempq.sender_socket();
						do_send(&tfd,tosend, sizeof(tosend));
						forward_to_remaining(remaining_replicas, holdbackqueue[r].begin()->first, holdbackqueue[r].begin()->second.first);
					}
					holdbackqueue[r].erase(holdbackqueue[r].begin());
				}
				write_unlock(&queue_rw_mtx);
			}else{
				write_lock(&queue_rw_mtx);
				//Remove the packet from the holdbackqueue
				for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
					Queuepacket tempq;
					tempq.ParseFromString(it->second.first);
					cout<<"mid: "<<mid<<" In queue: "<<tempq.msgid()<<endl;
					if(tempq.msgid().compare(mid) == 0){
						holdbackqueue[r].erase(it);
						break;
					}
				}
				write_unlock(&queue_rw_mtx);
				return -1;
			}
		}else{
			//Send data with maximum proposed number and latest version number to all replicas
			for(int i=0;i<replicas_connected_to.size();i++){
				send_final_data(*(replicas_connected_to[i].first),mid,"CPUT", r, c, v2, -1, maxpn, maxpnsender);
			}


			//Wait for acknowledgement from all replicas and then update own holdback queue.
			count = 0;
			bool err = false;
			while(count < replicas_connected_to.size()){
				FD_ZERO(&readfd);
				for(int i=0;i<replicas_connected_to.size();i++){
					FD_SET(*(replicas_connected_to[i].first),&readfd);
				}
				int activity = select(maxfd+1, &readfd, NULL, NULL, NULL);

				switch(activity){
					case -1:
						fprintf(stderr, "Error in selecting stream. Terminating\n");
						exit(1);
					case 0:
						fprintf(stderr, "Error in selecting stream. Terminating\n");
						exit(1);
					default:
					for(int i=0;i<replicas_connected_to.size();i++){
						if(FD_ISSET(*(replicas_connected_to[i].first),&readfd)){
							char *rcvd = (char*)malloc(1024 * 1024 + 200);
							int rcvdb = do_recv(replicas_connected_to[i].first,&rcvd[0],-1);
							rcvd[rcvdb] = 0;
							Packet rcvdpacket;
							rcvdpacket.ParseFromString(rcvd);
							if(rcvdpacket.status_code().compare("200") == 0){
								count++;
							}else{
								//Send error to user
								err = true;
								break;
							}
						}
					}
					if(err)
						break;
				}
			}
			for(int i=0;i<replicas_connected_to.size();i++){
				close(*(replicas_connected_to[i].first));
			}
			if(!err){
				write_lock(&queue_rw_mtx);
				cout<<"Holdbackqueue\n";
				for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
					cout<<it->first.first<<" "<<it->first.second<<" "<<it->second.first<<" "<<it->second.second<<endl;
				}
				//Remove the packet from the holdbackqueue
				for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
					Queuepacket tempq;
					tempq.ParseFromString(it->second.first);
					cout<<"mid: "<<mid<<" In queue: "<<tempq.msgid()<<endl;
					if(tempq.msgid().compare(mid) == 0){
						//Add entry to log with a global sequence number
						//Log commands to log file (as JSON)
						pthread_mutex_lock(&logmtx);
						fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
						map <string, string> tempjsonmap;
						tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
						tempjsonmap.insert(pair<string, string>("command", "CPUT"));
						tempjsonmap.insert(pair<string, string>("row", tempq.row()));
						tempjsonmap.insert(pair<string, string>("col", tempq.column()));
						tempjsonmap.insert(pair<string, string>("val", tempq.data()));
						tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
						tempjsonmap.insert(pair<string, string>("version", "exit"));
						json logjson(tempjsonmap);
						mylog<<logjson<<"\n";
						tempjsonmap.clear();
						logsize += mylog.tellg();
						
						if(logsize > 1000){
							
							//Dump data to disk (file)
							put_to_disk(keyvaluecache);
							//Clear log file
							ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
							ofs.close();
							logsize = 0;
						}
						mylog.close();
						pthread_mutex_unlock(&logmtx);
						cout<<"REMOVING\n";
						holdbackqueue[r].erase(it);
						break;
					}
				}
				
				Packet packet_response;
				string res;
				packet_response.set_status_code("500");
				packet_response.set_status("error");
				cout<<"could not add data to kvstore\n";
				if(!packet_response.SerializeToString(&res)){
					fprintf(stderr, "Could not serialize response\n");
				}
				res = generate_prefix(res.size()) + res;
				char tosend[res.size()];
				res.copy(tosend, res.size());
				//l = response.length();
				do_send(&fd, tosend, sizeof(tosend));
				write_unlock(&queue_rw_mtx);
			}else{
				write_lock(&queue_rw_mtx);
				//Remove the packet from the holdbackqueue
				for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
					Queuepacket tempq;
					tempq.ParseFromString(it->second.first);
					cout<<"mid: "<<mid<<" In queue: "<<tempq.msgid()<<endl;
					if(tempq.msgid().compare(mid) == 0){
						//Add entry to log with a global sequence number
						//Log commands to log file (as JSON)
						pthread_mutex_lock(&logmtx);
						fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
						map <string, string> tempjsonmap;
						tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
						tempjsonmap.insert(pair<string, string>("command", "CPUT"));
						tempjsonmap.insert(pair<string, string>("row", tempq.row()));
						tempjsonmap.insert(pair<string, string>("col", tempq.column()));
						tempjsonmap.insert(pair<string, string>("val", tempq.data()));
						tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
						tempjsonmap.insert(pair<string, string>("version", "exit"));
						json logjson(tempjsonmap);
						mylog<<logjson<<"\n";
						tempjsonmap.clear();
						logsize += mylog.tellg();
						
						if(logsize > 1000){
							
							//Dump data to disk (file)
							put_to_disk(keyvaluecache);
							//Clear log file
							ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
							ofs.close();
							logsize = 0;
						}
						mylog.close();
						pthread_mutex_unlock(&logmtx);
						holdbackqueue[r].erase(it);
						break;
					}
				}
			}
			write_unlock(&queue_rw_mtx);
			return -1;
		}
	}
	return maxvn;
}

int do_delete(int fd, string r, string c, int id){
	Packet packet_response;
	string response;

	vector<string> current_replicas; //Stores IP:Port of all replicas as key and a boolean (initially set to false) to indicate if we have got a response from that replica
	vector<pair<int*,bool>> replicas_connected_to; //Stores socket FD of all those replicas (R/W out of N) to whom the proposal request was sent
	vector<string> remaining_replicas;
	current_replicas = replicas;

	//Create and send packet requesting version number (to get the latest data) and proposed number (for ordering)
	int maxfd = 0;
	int connected = 0;
	bool over = false;
	for (int i=0;i<current_replicas.size();i++){
		pair<string, int> add = get_ip_port(current_replicas[i]);
		int *fd_rcvd;
		if(over){
			remaining_replicas.push_back(current_replicas[i]);
		}else{
			if((fd_rcvd = request_proposed_number(add.first, add.second,id,"DEL", r, c, "", "")) == 0){
				//Could not connect to this replica
				remaining_replicas.push_back(current_replicas[i]);
			}else{
				replicas_connected_to.push_back(make_pair(fd_rcvd,false));
				connected++;
				if(*fd_rcvd > maxfd){
					maxfd = *fd_rcvd;
				}
			}
		} 
		if(connected == W){
			over = true;
		}
	}
	cout<<"connected to: "<<replicas_connected_to.size()<<endl;
	if(replicas_connected_to.size() < W){
		//Send error to user
		packet_response.set_status_code("500");
		packet_response.set_status("could not connect to replica");
		cout<<"could not connect to replica\n";
		if(!packet_response.SerializeToString(&response)){
			fprintf(stderr, "Could not serialize response\n");
		}
		response = generate_prefix(response.size()) + response;
		char tosend[response.size()];
		response.copy(tosend, response.size());
		do_send(&fd, tosend, sizeof(tosend));
		return -1;
	}else{
		//Add command to own holdback queue
		int mymaxP, mymaxA;
		if(ordering.find(r) != ordering.end()){
			mymaxP = ordering[r].first;
			mymaxA = ordering[r].second;
		}else{
			mymaxP = 0;
			mymaxA = 0;
			ordering.insert(pair<string, pair<int, int>>(r,make_pair(0,0)));
		}
		int mypn = max(mymaxA,mymaxP)+1;
		ordering[r].first = mypn;
		string queuedata;
		Queuepacket q;
		q.set_command("DEL");
		q.set_row(r);
		q.set_column(c);
		q.set_msgid(to_string(KVS_NO) + ":" + to_string(msgid));
		q.set_sender_socket(fd);
		if(!q.SerializeToString(&queuedata)){
			fprintf(stderr, "Could not serialize response\n");
		}
		write_lock(&queue_rw_mtx);
		if(holdbackqueue.find(r) != holdbackqueue.end()){
			//User already present in holdbackqueue
			holdbackqueue[r].insert(pair<pair<int, int>,pair<string, bool>>(make_pair(mypn,KVS_NO),make_pair(queuedata, false)));
		}else{
			map <pair<int, int>, pair<string, bool>> t;
			t.insert(pair<pair<int, int>,pair<string, bool>>(make_pair(mypn,KVS_NO),make_pair(queuedata, false)));
			holdbackqueue.insert(pair<string,map<pair<int, int>, pair<string, bool>>>(r,t));
		}
		write_unlock(&queue_rw_mtx);

		//Wait for proposed number from all replicas
		int count = 0;
		int maxpn = mypn;
		int maxpnsender = KVS_NO; //ID (KVS NO) of the node which sent the highest Proposed number (for ordering)
		fd_set readfd;
		string mid;
		
		while(count < replicas_connected_to.size()){
			FD_ZERO(&readfd);
			for(int i=0;i<replicas_connected_to.size();i++){
				FD_SET(*(replicas_connected_to[i].first),&readfd);
			}
			int activity = select(maxfd+1, &readfd, NULL, NULL, NULL);

			switch(activity){
				case -1:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					exit(1);
				case 0:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					exit(1);
				default:
				for(int i=0;i<replicas_connected_to.size();i++){
					if(FD_ISSET(*(replicas_connected_to[i].first),&readfd)){
						count++;
						char *rcvd = (char*)malloc(1024 * 1024 + 200);
						int rcvdb = do_recv(replicas_connected_to[i].first,&rcvd[0],-1);
						
						RepPacket rcvdpacket;
						rcvdpacket.ParseFromString(rcvd);
						mid = rcvdpacket.msgid();
						cout<<"mid rcvd: "<<rcvdpacket.msgid()<<endl;
						
						if(rcvdpacket.proposed_number() > maxpn){
							maxpn = rcvdpacket.proposed_number();
							maxpnsender = rcvdpacket.senderid();
						}
						
						cout<<"PN: "<<rcvdpacket.proposed_number()<<endl;
					}
				}
			}
		}

		ordering[r].second = maxpn;

		//Send data with maximum proposed number and latest version number to all replicas
		for(int i=0;i<replicas_connected_to.size();i++){
			send_final_data(*(replicas_connected_to[i].first),mid,"DEL", r, c, "", 0, maxpn, maxpnsender);
		}

		//Wait for acknowledgement from all replicas and then update own holdback queue.
		count = 0;
		bool err = false;
		struct timeval tv, tv1;
		tv.tv_sec = 10;
		vector <int> ack_rcvd_from; 
		while(count < replicas_connected_to.size()){
			memcpy(&tv1, &tv, sizeof(tv));
			FD_ZERO(&readfd);
			for(int i=0;i<replicas_connected_to.size();i++){
				FD_SET(*(replicas_connected_to[i].first),&readfd);
			}
			int activity = select(maxfd+1, &readfd, NULL, NULL, &tv1);

			switch(activity){
				case -1:
					fprintf(stderr, "Error in selecting stream. Terminating\n");
					err = true;
					break;
				case 0:
					//Terminating because of time out.
					fprintf(stderr, "Error in selecting stream because of time out. Terminating\n");
					err = true;
					break;
				default:
				for(int i=0;i<replicas_connected_to.size();i++){
					if(FD_ISSET(*(replicas_connected_to[i].first),&readfd)){
						char *rcvd = (char*)malloc(1024 * 1024 + 200);
						int rcvdb = do_recv(replicas_connected_to[i].first,&rcvd[0],-1);
						rcvd[rcvdb] = 0;

						Packet rcvdpacket;
						rcvdpacket.ParseFromString(rcvd);
						if(rcvdpacket.status_code().compare("200") == 0){
							count++;
							ack_rcvd_from.push_back(*(replicas_connected_to[i].first));
						}else{
							//Send error to user
							err = true;
							break;
						}
					}
				}
				if(err)
					break;
			}
			if(err)
				break;
		}
		if(err){
			//Send an undo to all those who acknowledged
			for(int i=0;i<ack_rcvd_from.size();i++){
				send_undo(ack_rcvd_from[i],r,c);
			}
		}
		cout<<"Out of loop\n";
		for(int i=0;i<replicas_connected_to.size();i++){
			cout<<"closing connection\n";
			close(*(replicas_connected_to[i].first));
		}
		if(!err){
			write_lock(&queue_rw_mtx);
			//Update own holdback queue
			ordering[r].second = max(ordering[r].second, maxpn);
			for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
				Queuepacket tempq;
				tempq.ParseFromString(it->second.first);
				cout<<"mid: "<<mid<<" In queue: "<<tempq.msgid()<<endl;
				if(tempq.msgid().compare(mid) == 0){
					string t = it->second.first;
					holdbackqueue[r].erase(it);
					holdbackqueue[r].insert(pair<pair<int, int>, pair<string, bool>>(make_pair(maxpn, maxpnsender),make_pair(t,true)));
					break;
				}
			}
			
			//Check if request is deliverable
			cout<<"Holdbackqueue\n";
			for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
				cout<<it->first.first<<" "<<it->first.second<<" "<<it->second.first<<" "<<it->second.second<<endl;
			}
			while(!holdbackqueue[r].empty() && holdbackqueue[r].begin()->second.second){
				Queuepacket tempq;
				tempq.ParseFromString(holdbackqueue[r].begin()->second.first);
				if(delete_from_cache(tempq.sender_socket(), tempq.row(), tempq.column(), false)){
					//Add entry to log with a global sequence number
					//Log commands to log file (as JSON)
					pthread_mutex_lock(&logmtx);
					fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
					map <string, string> tempjsonmap;
					tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
					tempjsonmap.insert(pair<string, string>("command", "DEL"));
					tempjsonmap.insert(pair<string, string>("row", r));
					tempjsonmap.insert(pair<string, string>("col", c));
					log_seq++;
					tempjsonmap.insert(pair<string, string>("sequence_number", to_string(log_seq)));
					tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
					json logjson(tempjsonmap);
					mylog<<logjson<<"\n";
					tempjsonmap.clear();
					logsize += mylog.tellg();
					
					if(logsize > 1000){
						
						//Dump data to disk (file)
						put_to_disk(keyvaluecache);
						//Clear log file
						ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
						ofs.close();
						logsize = 0;
					}
					mylog.close();
					pthread_mutex_unlock(&logmtx);
					
					//Send an acknowledgement to the socket present in the queuepacket
					Queuepacket tempq;
					tempq.ParseFromString(holdbackqueue[r].begin()->second.first);
					Packet packet_response;
					packet_response.set_status_code("200");
					packet_response.set_status("success");
					string serialized;
					if(!packet_response.SerializeToString(&serialized)){
						fprintf(stderr, "Could not serialize response\n");
					}
					serialized = generate_prefix(serialized.size()) + serialized;
					char tosend[serialized.size()];
					serialized.copy(tosend, serialized.size());
					int tfd = tempq.sender_socket();
					do_send(&tfd,tosend, sizeof(tosend));

					forward_to_remaining(remaining_replicas, holdbackqueue[r].begin()->first, holdbackqueue[r].begin()->second.first);
				}
				holdbackqueue[r].erase(holdbackqueue[r].begin());
			}
			write_unlock(&queue_rw_mtx);
		}else{
			write_lock(&queue_rw_mtx);
			//Remove the packet from the holdbackqueue
			for(auto it = holdbackqueue[r].begin();it!=holdbackqueue[r].end();++it){
				Queuepacket tempq;
				tempq.ParseFromString(it->second.first);
				cout<<"mid: "<<mid<<" In queue: "<<tempq.msgid()<<endl;
				if(tempq.msgid().compare(mid) == 0){
					//Send an acknowledgement to the socket present in the queuepacket
					Queuepacket tempq;
					tempq.ParseFromString(holdbackqueue[r].begin()->second.first);
					Packet packet_response;
					packet_response.set_status_code("500");
					packet_response.set_status("Not Found");
					string serialized;
					if(!packet_response.SerializeToString(&serialized)){
						fprintf(stderr, "Could not serialize response\n");
					}
					serialized = generate_prefix(serialized.size()) + serialized;
					char tosend[serialized.size()];
					serialized.copy(tosend, serialized.size());
					int tfd = tempq.sender_socket();
					do_send(&tfd,tosend, sizeof(tosend));

					holdbackqueue[r].erase(it);
					break;
				}
			}
			write_unlock(&queue_rw_mtx);
			return -1;
		}
	}
	return 1;
}

string get_all_keys(){
	set <pair<string, string>> all_keys;
	//Loop through cache and insert all key pairs
	for(auto kvit = keyvaluecache.begin();kvit!=keyvaluecache.end();++kvit){
		string curr_row_key = kvit->first;
		for(auto rowit = kvit->second.begin();rowit != kvit->second.end();++rowit){
			all_keys.insert(make_pair(curr_row_key,rowit->first));
		}
	}
	//Open file and read all keys from disk
	cout<<"Getting lock\n";
	pthread_mutex_lock(&storemtx);
	cout<<"Opening file\n";
	fstream f("kvstore" + to_string(KVS_NO), ios::in);
	if(f.is_open()){
		string line;
		while(getline(f,line)){ //Each line in the json contains a single JSON object corresponding to one row (user)
			if(line.length()!=0){
				json j = json::parse(line);
				json::iterator it = j.begin();
				string curr_row = it.key();
				for(json::iterator it1 = (*it).begin(); it1 != (*it).end(); ++it1){ //Loop through all columns of the row
					all_keys.insert(make_pair(curr_row,it1.key()));
				}
			}
		}
	}
	pthread_mutex_unlock(&storemtx);

	//Convert to a JSON serialized as string
	json all_json(all_keys);
	string all_keys_str = all_json.dump();

	//Send the string containing all keys to the user
	return all_keys_str;
}


void *replication_thread(void *arg){
	int fd = *(int *)arg;
	char *buffer = (char*)malloc(1024 * 1024 + 200);
	RepPacket rep_packet,response_packet;
	string command, serialized;

	while(true){
		cout<<"waiting to rcv\n";
		int rcvdbytes = do_recv(&fd, &buffer[0],-1);
		cout<<"rcvd bytes: "<<rcvdbytes<<endl;
		if(rcvdbytes < 0)
			break;
		buffer[rcvdbytes] = 0;
		rep_packet.ParseFromString(buffer);
		cout<<"in rep thread: "<<buffer<<endl;
		command = rep_packet.command();
		transform(command.begin(), command.end(), command.begin(), ::tolower);
		if(command.compare("propose") == 0){
			cout<<"In propose if\n";
			response_packet.set_version_number(get_version_from_cache(rep_packet.row(),rep_packet.column()));
			cout<<"MY VN: "<<response_packet.version_number()<<endl;
			response_packet.set_sender(self_ip + ":" + to_string(replica_port));
			int mymaxP, mymaxA;
			if(ordering.find(rep_packet.row()) != ordering.end()){
				mymaxP = ordering[rep_packet.row()].first;
				mymaxA = ordering[rep_packet.row()].second;
			}else{
				mymaxP = 0;
				mymaxA = 0;
				ordering.insert(pair<string, pair<int, int>>(rep_packet.row(),make_pair(0,0)));
			}
			int pn = max(mymaxA, mymaxP)+1;
			ordering[rep_packet.row()].first = pn;
			response_packet.set_proposed_number(pn);
			response_packet.set_senderid(KVS_NO);
			cout<<"msg id rcvd: "<<rep_packet.msgid()<<endl;
			response_packet.set_msgid(rep_packet.msgid());
			cout<<"MSGID SET TO: "<<response_packet.msgid()<<endl;

			if(rep_packet.request().compare("GET") != 0){
				//Log commands to log file (as JSON)
				pthread_mutex_lock(&logmtx);
				fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
				map <string, string> tempjsonmap;
				tempjsonmap.insert(pair<string, string>("state", "BEGIN"));
				tempjsonmap.insert(pair<string, string>("command", rep_packet.request()));
				tempjsonmap.insert(pair<string, string>("row", rep_packet.row()));
				tempjsonmap.insert(pair<string, string>("col", rep_packet.column()));
				if(rep_packet.request().compare("PUT") == 0 || rep_packet.request().compare("CPUT") == 0){
					tempjsonmap.insert(pair<string, string>("val", rep_packet.data()));
				}
				if(rep_packet.request().compare("CPUT") == 0){
					tempjsonmap.insert(pair<string, string>("val_to_compare", rep_packet.data_to_compare()));
				}
				tempjsonmap.insert(pair<string, string>("id", rep_packet.msgid()));
				json logjson(tempjsonmap);
				mylog<<logjson<<"\n";
				tempjsonmap.clear();
				logsize += mylog.tellg();
				
				mylog.close();
				pthread_mutex_unlock(&logmtx);

				//Serialize command to store in holdback queue
				string queuedata;
				Queuepacket q;
				q.set_command(rep_packet.request());
				q.set_row(rep_packet.row());
				q.set_column(rep_packet.column());
				q.set_data(rep_packet.data());
				q.set_data_to_compare(rep_packet.data_to_compare());
				q.set_msgid(rep_packet.msgid());
				q.set_sender_socket(fd);
				cout<<"MSG ID IN QUEUE SET TO: "<<q.msgid()<<endl;
				if(!q.SerializeToString(&queuedata)){
						fprintf(stderr, "Could not serialize response\n");
				}
				write_lock(&queue_rw_mtx);
				//Add packet to holdback queue
				if(holdbackqueue.find(rep_packet.row()) != holdbackqueue.end()){
					//User already present in holdbackqueue
					holdbackqueue[rep_packet.row()].insert(pair<pair<int, int>,pair<string,bool>>(make_pair(pn,KVS_NO),make_pair(queuedata,false)));
				}else{
					map <pair<int, int>, pair<string, bool>> t;
					t.insert(pair<pair<int, int>,pair<string, bool>>(make_pair(pn,KVS_NO),make_pair(queuedata,false)));
					holdbackqueue.insert(pair<string,map<pair<int, int>, pair<string, bool>>>(rep_packet.row(),t));
				}
				write_unlock(&queue_rw_mtx);
			}

			//Serialize and send packet back
			if(!response_packet.SerializeToString(&serialized)){
				fprintf(stderr, "Could not serialize response\n");
			}
			serialized = generate_prefix(serialized.size()) + serialized;
			char tosend[serialized.size()];
			serialized.copy(tosend, serialized.size());
			do_send(&fd, tosend, sizeof(tosend));
		}else if(command.compare("maxproposal") == 0){
			write_lock(&queue_rw_mtx);
			//Update the holdback queue
			ordering[rep_packet.row()].second = max(ordering[rep_packet.row()].second, rep_packet.proposed_number());
			for(auto it = holdbackqueue[rep_packet.row()].begin();it!=holdbackqueue[rep_packet.row()].end();++it){
				Queuepacket tempq;
				tempq.ParseFromString(it->second.first);
				cout<<"In message: "<<tempq.msgid()<<" rcvd: "<<rep_packet.msgid()<<endl;
				if(tempq.msgid().compare(rep_packet.msgid()) == 0){
					tempq.set_version_number(rep_packet.version_number());
					string t;
					if(!tempq.SerializeToString(&t)){
						fprintf(stderr, "Could not serialize response\n");
					}
					holdbackqueue[rep_packet.row()].erase(it);
					holdbackqueue[rep_packet.row()].insert(pair<pair<int, int>, pair<string, bool>>(make_pair(rep_packet.proposed_number(), rep_packet.senderid()),make_pair(t,true)));
					break;
				}
			}
			write_unlock(&queue_rw_mtx);
			//Check if requests in queue are deliverable
			
			write_lock(&queue_rw_mtx);
			cout<<"q size: "<<holdbackqueue[rep_packet.row()].size()<<endl;
			while(!holdbackqueue[rep_packet.row()].empty() && holdbackqueue[rep_packet.row()].begin()->second.second){
				Queuepacket tempq;
				cout<<"In update while\n";
				tempq.ParseFromString(holdbackqueue[rep_packet.row()].begin()->second.first);
				if(tempq.command().compare("PUT") == 0){
					cout<<"Adding to cache\n";
					int putret = put_to_cache(-1, tempq.row(), tempq.column(), tempq.data(), tempq.version_number(), true);
					//Add entry to log with a global sequence number
					//Log commands to log file (as JSON)
					pthread_mutex_lock(&logmtx);
					fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
					map <string, string> tempjsonmap;
					tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
					tempjsonmap.insert(pair<string, string>("command", "PUT"));
					tempjsonmap.insert(pair<string, string>("row", tempq.row()));
					tempjsonmap.insert(pair<string, string>("col", tempq.column()));
					tempjsonmap.insert(pair<string, string>("val", tempq.data()));
					tempjsonmap.insert(pair<string, string>("version", to_string(rep_packet.version_number())));
					tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
					log_seq++;
					tempjsonmap.insert(pair<string, string>("sequence_number", to_string(log_seq)));
					json logjson(tempjsonmap);
					mylog<<logjson<<"\n";
					tempjsonmap.clear();
					logsize += mylog.tellg();
					
					if(logsize > 1000){
						
						//Dump data to disk (file)
						put_to_disk(keyvaluecache);
						//Clear log file
						ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
						ofs.close();
						logsize = 0;
					}
					mylog.close();
					pthread_mutex_unlock(&logmtx);
					cout<<"putret: "<<putret<<endl;
					if(putret > 0){
						//Send an acknowledgement to the socket present in the queuepacket
						cout<<"Sending response\n";
						Packet packet_response_new;
						packet_response_new.set_status_code("200");
						packet_response_new.set_status("success");
						string serialized;
						if(!packet_response_new.SerializeToString(&serialized)){
							fprintf(stderr, "Could not serialize response\n");
						}
						serialized = generate_prefix(serialized.size()) + serialized;
						cout<<"serialized: "<<serialized<<endl;
						char tosend[serialized.size()];
						serialized.copy(tosend, serialized.size());
						int tfd = tempq.sender_socket();
						do_send(&tfd,tosend, sizeof(tosend));
					}
					//Check remaining memory and take a checkpoint if required
					double rem = (sysconf(_SC_AVPHYS_PAGES)*100)/(sysconf(_SC_PHYS_PAGES)); //Gives percentage of available memory
					if(rem < 2 || keyvaluecache.size() > 4000){
						//Dump data to disk (file)
						put_to_disk(keyvaluecache);
						//Clear log file
						ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
						ofs.close();
						logsize = 0;
						//Clear the cache
						keyvaluecache.clear();
						deleted.clear();
					}
					cout<<"done\n";
				}else if(tempq.command().compare("CPUT") == 0){
					if(rep_packet.version_number() != -1){
						cout<<"Adding to cache\n";
						int putret = put_to_cache(-1, tempq.row(), tempq.column(), tempq.data(),rep_packet.version_number(), true);
						if(putret > 0){
							//Log commands to log file (as JSON)
							pthread_mutex_lock(&logmtx);
							fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
							map <string, string> tempjsonmap;
							tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
							tempjsonmap.insert(pair<string, string>("command", "CPUT"));
							tempjsonmap.insert(pair<string, string>("row", tempq.row()));
							tempjsonmap.insert(pair<string, string>("col", tempq.column()));
							tempjsonmap.insert(pair<string, string>("val", tempq.data()));
							tempjsonmap.insert(pair<string, string>("version", to_string(rep_packet.version_number())));
							tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
							log_seq++;
							tempjsonmap.insert(pair<string, string>("sequence_number", to_string(log_seq)));
							json logjson(tempjsonmap);
							mylog<<logjson<<"\n";
							tempjsonmap.clear();
							logsize += mylog.tellg();
							
							if(logsize > 1000){
								
								//Dump data to disk (file)
								put_to_disk(keyvaluecache);
								//Clear log file
								ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
								ofs.close();
								logsize = 0;
							}
							mylog.close();
							pthread_mutex_unlock(&logmtx);
							//Send an acknowledgement to the socket present in the queuepacket
							Packet packet_response;
							packet_response.set_status_code("200");
							packet_response.set_status("success");
							string serialized;
							if(!packet_response.SerializeToString(&serialized)){
								fprintf(stderr, "Could not serialize response\n");
							}
							serialized = generate_prefix(serialized.size()) + serialized;
							char tosend[serialized.size()];
							serialized.copy(tosend, serialized.size());
							int tfd = tempq.sender_socket();
							do_send(&tfd,tosend, sizeof(tosend));
						}
						cout<<"done\n";
					}else{
						//Log commands to log file (as JSON)
						pthread_mutex_lock(&logmtx);
						fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
						map <string, string> tempjsonmap;
						tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
						tempjsonmap.insert(pair<string, string>("command", "CPUT"));
						tempjsonmap.insert(pair<string, string>("row", tempq.row()));
						tempjsonmap.insert(pair<string, string>("col", tempq.column()));
						tempjsonmap.insert(pair<string, string>("val", tempq.data()));
						tempjsonmap.insert(pair<string, string>("version", "exit"));
						tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
						json logjson(tempjsonmap);
						mylog<<logjson<<"\n";
						tempjsonmap.clear();
						logsize += mylog.tellg();
						
						if(logsize > 1000){
							
							//Dump data to disk (file)
							put_to_disk(keyvaluecache);
							//Clear log file
							ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
							ofs.close();
							logsize = 0;
						}
						mylog.close();
						pthread_mutex_unlock(&logmtx);
						//Send an acknowledgement to the socket present in the queuepacket
						Packet packet_response;
						packet_response.set_status_code("200");
						packet_response.set_status("success");
						string serialized;
						if(!packet_response.SerializeToString(&serialized)){
							fprintf(stderr, "Could not serialize response\n");
						}
						serialized = generate_prefix(serialized.size()) + serialized;
						char tosend[serialized.size()];
						serialized.copy(tosend, serialized.size());
						int tfd = tempq.sender_socket();
						do_send(&tfd,tosend, sizeof(tosend));
					}
					//Check remaining memory and take a checkpoint if required
					double rem = (sysconf(_SC_AVPHYS_PAGES)*100)/(sysconf(_SC_PHYS_PAGES)); //Gives percentage of available memory
					if((rem < 2 || keyvaluecache.size() > 4000)){
						//Dump data to disk (file)
						put_to_disk(keyvaluecache);
						//Clear log file
						ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
						ofs.close();
						logsize = 0;
						//Clear the cache
						keyvaluecache.clear();
						deleted.clear();
					}
				}else if(tempq.command().compare("DEL") == 0){
					cout<<"deleting from cache\n";
					if(delete_from_cache(-1, tempq.row(), tempq.column(), true)){
						//Log commands to log file (as JSON)
						pthread_mutex_lock(&logmtx);
						fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
						map <string, string> tempjsonmap;
						tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
						tempjsonmap.insert(pair<string, string>("command", "DEL"));
						tempjsonmap.insert(pair<string, string>("row", tempq.row()));
						tempjsonmap.insert(pair<string, string>("col", tempq.column()));
						tempjsonmap.insert(pair<string, string>("version", to_string(rep_packet.version_number())));
						tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
						log_seq++;
						tempjsonmap.insert(pair<string, string>("sequence_number", to_string(log_seq)));
						json logjson(tempjsonmap);
						mylog<<logjson<<"\n";
						tempjsonmap.clear();
						logsize += mylog.tellg();
						
						if(logsize > 1000){
							
							//Dump data to disk (file)
							put_to_disk(keyvaluecache);
							//Clear log file
							ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
							ofs.close();
							logsize = 0;
						}
						mylog.close();
						pthread_mutex_unlock(&logmtx);
						//Send an acknowledgement to the socket present in the queuepacket
						Packet packet_response;
						packet_response.set_status_code("200");
						packet_response.set_status("success");
						string serialized;
						if(!packet_response.SerializeToString(&serialized)){
							fprintf(stderr, "Could not serialize response\n");
						}
						serialized = generate_prefix(serialized.size()) + serialized;
						char tosend[serialized.size()];
						serialized.copy(tosend, serialized.size());
						int tfd = tempq.sender_socket();
						do_send(&tfd,tosend, sizeof(tosend));
					}else{
						//Log commands to log file (as JSON)
						pthread_mutex_lock(&logmtx);
						fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
						map <string, string> tempjsonmap;
						tempjsonmap.insert(pair<string, string>("state", "COMMIT"));
						tempjsonmap.insert(pair<string, string>("command", "DEL"));
						tempjsonmap.insert(pair<string, string>("row", tempq.row()));
						tempjsonmap.insert(pair<string, string>("col", tempq.column()));
						tempjsonmap.insert(pair<string, string>("version", "exit"));
						tempjsonmap.insert(pair<string, string>("id", tempq.msgid()));
						log_seq++;
						tempjsonmap.insert(pair<string, string>("sequence_number", to_string(log_seq)));
						json logjson(tempjsonmap);
						mylog<<logjson<<"\n";
						tempjsonmap.clear();
						logsize += mylog.tellg();
						
						if(logsize > 1000){
							
							//Dump data to disk (file)
							put_to_disk(keyvaluecache);
							//Clear log file
							ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
							ofs.close();
							logsize = 0;
						}
						mylog.close();
						pthread_mutex_unlock(&logmtx);
						//Send an acknowledgement to the socket present in the queuepacket
						Packet packet_response;
						packet_response.set_status_code("500");
						packet_response.set_status("Not found");
						string serialized;
						if(!packet_response.SerializeToString(&serialized)){
							fprintf(stderr, "Could not serialize response\n");
						}
						serialized = generate_prefix(serialized.size()) + serialized;
						char tosend[serialized.size()];
						serialized.copy(tosend, serialized.size());
						int tfd = tempq.sender_socket();
						do_send(&tfd,tosend, sizeof(tosend));
					}
					cout<<"done\n";
					//Check remaining memory and take a checkpoint if required
					double rem = (sysconf(_SC_AVPHYS_PAGES)*100)/(sysconf(_SC_PHYS_PAGES)); //Gives percentage of available memory
					if((rem < 2 || keyvaluecache.size() > 4000)){
						//Dump data to disk (file)
						put_to_disk(keyvaluecache);
						//Clear log file
						ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
						ofs.close();
						logsize = 0;
						//Clear the cache
						keyvaluecache.clear();
						deleted.clear();
					}
				}
				cout<<"removing from queue\n";
				holdbackqueue[rep_packet.row()].erase(holdbackqueue[rep_packet.row()].begin());
				cout<<"removed from queue\n";
			}
			printcache();
			write_unlock(&queue_rw_mtx);

		}else if(command.compare("getlatest") == 0){
			get_from_cache(fd, rep_packet.row(), rep_packet.column(), false);
		}else if(command.compare("forward") == 0){
			//Serialize command to store in holdback queue
			string queuedata;
			Queuepacket q;
			q.set_command(rep_packet.request());
			q.set_row(rep_packet.row());
			q.set_column(rep_packet.column());
			q.set_data(rep_packet.data());
			q.set_version_number(rep_packet.version_number());
			cout<<"MSG ID IN QUEUE SET TO: "<<q.msgid()<<endl;
			if(!q.SerializeToString(&queuedata)){
					fprintf(stderr, "Could not serialize response\n");
			}
			write_lock(&queue_rw_mtx);
			//Add packet to holdback queue
			if(holdbackqueue.find(rep_packet.row()) != holdbackqueue.end()){
				//User already present in holdbackqueue
				holdbackqueue[rep_packet.row()].insert(pair<pair<int, int>,pair<string,bool>>(make_pair(rep_packet.proposed_number(),rep_packet.senderid()),make_pair(queuedata,true)));
			}else{
				map <pair<int, int>, pair<string, bool>> t;
				t.insert(pair<pair<int, int>,pair<string, bool>>(make_pair(rep_packet.proposed_number(),rep_packet.senderid()),make_pair(queuedata,true)));
				holdbackqueue.insert(pair<string,map<pair<int, int>, pair<string, bool>>>(rep_packet.row(),t));
			}
			while(!holdbackqueue[rep_packet.row()].empty() && holdbackqueue[rep_packet.row()].begin()->second.second){
				Queuepacket tempq;
				cout<<"In update while\n";
				tempq.ParseFromString(holdbackqueue[rep_packet.row()].begin()->second.first);
				if(tempq.command().compare("PUT") == 0){
					put_to_cache(-1, tempq.row(), tempq.column(), tempq.data(), tempq.version_number(), true);
					printcache();
				}else if(tempq.command().compare("DEL") == 0){
					delete_from_cache(-1, tempq.row(), tempq.column(), true);
					printcache();
				}
				cout<<"removing from queue\n";
				holdbackqueue[rep_packet.row()].erase(holdbackqueue[rep_packet.row()].begin());
				cout<<"removed from queue\n";
			}
			write_unlock(&queue_rw_mtx);
			
		}else if(command.compare("undo") == 0){
			undo_operation(rep_packet.row(),rep_packet.column());
		}
	}
}



void *worker(void *arg){
	int fd = *(int *)arg;
	string request, response, command;
	char t;
	int l;
	locale loc;
	Packet packet;
	
	
	
	char *buffer = (char*)malloc(1024 * 1024 + 200);


	while(true){
		//cout<<"Outside While...\n";
		/*do{
			//cout<<"Inside while...\n";
			if(read(fd, &t, 1) < 0){
				fprintf(stderr, "Could not read command from client. Try again!\n");
			}
			if(t != '\n' && t != '\r')
				request += t;
			//cout<<request<<" ";
		}while(t != '\n');*/
		char *buffer = (char*)malloc(1024 * 1024 + 200);
		int rcvdbytes = do_recv(&fd, &buffer[0],-1);
		if(rcvdbytes < 0)
			break;
		cout<<"RCVD BYTES: "<<rcvdbytes<<endl;
		cout<<"RCVD MSG: "<<buffer<<endl;
		/*buffer[rcvdbytes -2] = '\0';
		buffer[rcvdbytes -1] = '\0';
		buffer[rcvdbytes ] = '\0';*/
		packet.ParseFromString(buffer);
		//cout<<endl;
		if(vflag){
			fprintf(stderr, "[%d] C: %s", fd, request.c_str());
		}
		command = packet.command();
		//command = request.substr(0,4);
		transform(command.begin(), command.end(), command.begin(), ::tolower);
		if(command.compare("restart") != 0 && killed){
			close(fd);
			pthread_exit(NULL);
		}
		if(command.compare("get") == 0 && !killed){ //Handle GET command from client (other servers in this case)
		//if(packet.command().compare("get")==0){
			//Parse the input request
			/*request = request.substr(4);
			string r = request.substr(0,request.find(","));
			string c = request.substr(request.find(",")+1);*/
			string r = packet.row();
			string c = packet.column();
			cout<<"Command: get "<<"row: "<<r<<" col: "<<c<<endl;
			msgid +=1;
			int myid = msgid;
			//Log commands to log file (as JSON)
			/*pthread_mutex_lock(&mtx);
			fstream mylog("kvlog", ios::in | ios::out | ios::app); //Open log file
			map <string, string> tempjsonmap;
			tempjsonmap.insert(pair<string, string>("command", "get"));
			tempjsonmap.insert(pair<string, string>("row", r));
			tempjsonmap.insert(pair<string, string>("col", c));
			tempjsonmap.insert(pair<string, string>("val1", ""));
			tempjsonmap.insert(pair<string, string>("val2", ""));
			json logjson(tempjsonmap);
			mylog<<logjson<<"\n";
			tempjsonmap.clear();
			logsize += mylog.tellg();
			
			if(logsize > 1000){
				//Dump data to disk (file)
				put_to_disk(keyvaluecache);
				//Clear log file
				ofstream ofs("kvlog", ios::trunc);
				ofs.close();
				logsize = 0;
			}
			mylog.close();
			pthread_mutex_unlock(&mtx);*/
			
			do_get(fd, r, c, myid);

			//CHECKPOINTING
			//Check remaining memory and take a checkpoint if required
			double rem_pages = (sysconf(_SC_AVPHYS_PAGES)*100)/(sysconf(_SC_PHYS_PAGES)); //Gives percentage of available memory
			if(rem_pages < 2 || keyvaluecache.size() > 4000){
				//Dump data to disk (file)
				put_to_disk(keyvaluecache);
				//Clear log file
				ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
				ofs.close();
				logsize = 0;
				//Clear the cache
				keyvaluecache.clear();
				deleted.clear();
			}
			request.clear();
			command.clear();
		}else if(command.compare("put") == 0 && !killed){ //Handle PUT command from client (other servers in this case)
			//Parse input request
			/*request = request.substr(4);
			string r = request.substr(0,request.find(","));
			string rem = request.substr(request.find(",")+1);
			string c = rem.substr(0,rem.find(","));
			string v = rem.substr(rem.find(",")+1);*/
			string r = packet.row();
			string c = packet.column();
			string v = packet.data();
			cout<<"Command: put "<<"row: "<<r<<" col: "<<c<<" val: "<<v<<endl;
			msgid +=1;
			int myid = msgid;
			//Log commands to log file (as JSON)
			pthread_mutex_lock(&logmtx);
			fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
			map <string, string> tempjsonmap;
			tempjsonmap.insert(pair<string, string>("state", "BEGIN"));
			tempjsonmap.insert(pair<string, string>("command", "PUT"));
			tempjsonmap.insert(pair<string, string>("row", r));
			tempjsonmap.insert(pair<string, string>("col", c));
			tempjsonmap.insert(pair<string, string>("val", v));
			tempjsonmap.insert(pair<string, string>("id", to_string(KVS_NO) + ":" + to_string(myid)));
			json logjson(tempjsonmap);
			mylog<<logjson<<"\n";
			tempjsonmap.clear();
			logsize += mylog.tellg();
			
			mylog.close();
			pthread_mutex_unlock(&logmtx);

			int vn = do_put(fd, r, c, v, myid);

			

			//v.pop_back();
			//Check remaining memory and take a checkpoint if required
			double rem_pages = (sysconf(_SC_AVPHYS_PAGES)*100)/(sysconf(_SC_PHYS_PAGES)); //Gives percentage of available memory
			if(rem_pages < 2 || keyvaluecache.size() > 4000){
				//Dump data to disk (file)
				put_to_disk(keyvaluecache);
				//Clear log file
				ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
				ofs.close();
				logsize = 0;
				//Clear the cache
				keyvaluecache.clear();
				deleted.clear();
			}
			
			request.clear();
			command.clear();
		}else if(command.compare("cput") == 0 && !killed){ //Handle CPUT command from client (other servers in this case)
			//request = request.substr(5);
			/*string r = request.substr(0,request.find(","));
			string rem = request.substr(request.find(",")+1);
			string c = rem.substr(0,rem.find(","));
			rem = rem.substr(rem.find(",")+1);
			string v1 = rem.substr(0,rem.find(","));
			rem = rem.substr(rem.find(",")+1);
			string v2 = rem.substr(0,rem.find(","));*/
			//v2.pop_back();
			string r = packet.row();
			string c = packet.column();
			string v1 = packet.arg(0);
			string v2 = packet.data();
			cout<<"Command: cput "<<"row: "<<r<<" col: "<<c<<" val: "<<v1<<" v2: "<<v2<<endl;
			msgid +=1;
			int myid = msgid;
			//Log commands to log file (as JSON)
			pthread_mutex_lock(&logmtx);
			fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
			map <string, string> tempjsonmap;
			tempjsonmap.insert(pair<string, string>("state", "BEGIN"));
			tempjsonmap.insert(pair<string, string>("command", "CPUT"));
			tempjsonmap.insert(pair<string, string>("row", r));
			tempjsonmap.insert(pair<string, string>("col", c));
			tempjsonmap.insert(pair<string, string>("val", v2));
			tempjsonmap.insert(pair<string, string>("val_to_compare", v1));
			tempjsonmap.insert(pair<string, string>("id", to_string(KVS_NO) + ":" + to_string(myid)));
			json logjson(tempjsonmap);
			mylog<<logjson<<"\n";
			tempjsonmap.clear();
			logsize += mylog.tellg();
			
			mylog.close();
			pthread_mutex_unlock(&logmtx);

			int vn = do_cput(fd, r, c, v1, v2, myid);

			//Check remaining memory and take a checkpoint if required
			double rem_pages = (sysconf(_SC_AVPHYS_PAGES)*100)/(sysconf(_SC_PHYS_PAGES)); //Gives percentage of available memory
			if(rem_pages < 2 || keyvaluecache.size() > 4000){
				//Dump data to disk (file)
				put_to_disk(keyvaluecache);
				//Clear log file
				ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
				ofs.close();
				logsize = 0;
				//Clear the cache
				keyvaluecache.clear();
				deleted.clear();
			}

			request.clear();
			command.clear();
		}else if(command.compare("del") == 0 && !killed){ //Handle DEL command from client (other servers in this case)
			/*request = request.substr(4);
			string r = request.substr(0,request.find(","));
			string c = request.substr(request.find(",")+1);*/
			string r = packet.row();
			string c = packet.column();
			cout<<"R: "<<r<<" C: "<<c.length()<<endl;
			//c.pop_back();
			cout<<"Command: del "<<"row: "<<r<<" col: "<<c<<endl;
			msgid +=1;
			int myid = msgid;
			//Log commands to log file (as JSON)
			pthread_mutex_lock(&logmtx);
			fstream mylog("kvlog" + to_string(KVS_NO), ios::in | ios::out | ios::app); //Open log file
			map <string, string> tempjsonmap;
			tempjsonmap.insert(pair<string, string>("state", "BEGIN"));
			tempjsonmap.insert(pair<string, string>("command", "DEL"));
			tempjsonmap.insert(pair<string, string>("row", r));
			tempjsonmap.insert(pair<string, string>("col", c));
			tempjsonmap.insert(pair<string, string>("id", to_string(KVS_NO) + ":" + to_string(myid)));
			json logjson(tempjsonmap);
			mylog<<logjson<<"\n";
			tempjsonmap.clear();
			logsize += mylog.tellg();
			
			mylog.close();
			pthread_mutex_unlock(&logmtx);

			int st = do_delete(fd, r, c, myid);
			
			//Check remaining memory and take a checkpoint if required
			double rem_pages = (sysconf(_SC_AVPHYS_PAGES)*100)/(sysconf(_SC_PHYS_PAGES)); //Gives percentage of available memory
			if(rem_pages < 2 || keyvaluecache.size() > 4000){
				//Dump data to disk (file)
				put_to_disk(keyvaluecache);
				//Clear log file
				ofstream ofs("kvlog" + to_string(KVS_NO), ios::trunc);
				ofs.close();
				logsize = 0;
				//Clear the cache
				keyvaluecache.clear();
				deleted.clear();
			}
			request.clear();
			command.clear();
		}else if(command.compare("getall") == 0 & !killed){
			string all_keys_str = get_all_keys();
			//Send all keys back to the client
			Packet packet_response;
			packet_response.set_status_code("200");
			packet_response.set_data(all_keys_str);

			//Serialize packet to string before sending
			string serialized;
			if(!packet_response.SerializeToString(&serialized)){
				fprintf(stderr, "Could not serialize response\n");
			}
			serialized = generate_prefix(serialized.size()) + serialized;
			char tosend[serialized.size()];
			serialized.copy(tosend, serialized.size());
			do_send(&fd,tosend, sizeof(tosend));

			request.clear();
			command.clear();
		}else if(command.compare("quit") == 0 && !killed){
			
			request.clear();
			command.clear();
			close(fd);
			vector <int>::iterator pos = find(open_conn.begin(), open_conn.end(),fd);
			if(pos != open_conn.end())
				open_conn.erase(pos);
			if(vflag) fprintf(stderr, "[%d] Connection Closed\n", fd);
			pthread_exit(NULL);
			break;
		}else if(command.compare("kill") == 0){
			killed = true;
			request.clear();
			command.clear();

			Packet packet_response;
			packet_response.set_command("OK");
			string serialized;
			if(!packet_response.SerializeToString(&serialized)){
				fprintf(stderr, "Could not serialize response\n");
			}
			serialized = generate_prefix(serialized.size()) + serialized;
			char tosend[serialized.size()];
			serialized.copy(tosend, serialized.size());
			do_send(&fd,tosend, sizeof(tosend));
			
			close(fd);
			vector <int>::iterator pos = find(open_conn.begin(), open_conn.end(),fd);
			if(pos != open_conn.end())
				open_conn.erase(pos);
			if(vflag) fprintf(stderr, "[%d] I am dead!\n", fd);
			pthread_exit(NULL);
			break;
		}else if(command.compare("restart") == 0){
			killed = false;
			//Recover
			//Read the log file, if it is empty, ignore. If it has anything in it, replay the commands. (For recovery)
			string line;
			fstream logf("kvlog" + to_string(KVS_NO), ios::in);
			logsize += logf.tellg();
			map <string, json> begin_commands;
			map <int, json> commit_commands;
			while(getline(logf, line)){
				if(line.length()!=0){
					cout<<"line: "<<line<<endl;
					json logj = json::parse(line);
					string st = logj["state"];
					cout<<"st: "<<st<<endl;
					if(st.compare("BEGIN") == 0){
						string tempid = logj["id"];
						cout<<"tempid: "<<tempid<<endl;
						begin_commands.insert(pair<string, json>(tempid,logj));
					}else if(st.compare("COMMIT") == 0){
						string tempid = logj["id"];
						cout<<"tempid: "<<tempid<<endl;
						if(begin_commands.find(tempid) != begin_commands.end()){
							string vers = logj["version"];
							if(vers.compare("exit") == 0){
								begin_commands.erase(tempid);
							}else{
								string seq = logj["sequence_number"];
								commit_commands.insert(pair<int, json>(stoi(seq),logj));
								begin_commands.erase(tempid);
							}
						}
					}
				}
			}
			//Loop through the commit map
			for(auto it = commit_commands.begin(); it != commit_commands.end(); ++it){
				json to_commit = it->second;
				//cout<<"aaaa\n";
				string com = to_commit["command"];
				transform(com.begin(), com.end(), com.begin(), ::tolower);
				//cout<<com<<endl;
				string r, c, v, ver;
				if(com.compare("put") == 0){
					r = to_commit["row"];
					c = to_commit["col"];
					v = to_commit["val"];
					ver = to_commit["version"];
					int vno = stoi(ver);
					put_to_cache(-1, r, c, v, vno, true);
				}else if(com.compare("cput") == 0){
					string vers = to_commit["version"];
					if(vers.compare("exit") == 0){
						//Do nothing
					}else{
						r = to_commit["row"];
						c = to_commit["col"];
						v = to_commit["val"];
						ver = to_commit["version"];
						int vno = stoi(ver);
						put_to_cache(-1, r, c, v, vno, true);
					}
				}else if(com.compare("del") == 0){
					string vers = to_commit["version"];
					if(vers.compare("exit") == 0){
						//Do nothing
					}else{
						r = to_commit["row"];
						c = to_commit["col"];
						delete_from_cache(-1, r, c, true);
					}
				}
			}
			Packet packet_response;
			packet_response.set_command("OK");
			string serialized;
			if(!packet_response.SerializeToString(&serialized)){
				fprintf(stderr, "Could not serialize response\n");
			}
			serialized = generate_prefix(serialized.size()) + serialized;
			char tosend[serialized.size()];
			serialized.copy(tosend, serialized.size());
			do_send(&fd,tosend, sizeof(tosend));
			request.clear();
			command.clear();
			if(vflag) fprintf(stderr, "[%d] I am back!\n", fd);
			pthread_exit(NULL);
			break;
		}else{
			Packet packet_response;
			//Send error to user
			packet_response.set_status_code("500");
			packet_response.set_status("Unknown Command");
			cout<<"Unknown Command\n";
			if(!packet_response.SerializeToString(&response)){
				fprintf(stderr, "Could not serialize response\n");
			}
			response = generate_prefix(response.size()) + response;
			char tosend[response.size()];
			response.copy(tosend, response.size());
			do_send(&fd, tosend, sizeof(tosend));
			if(vflag){
				fprintf(stderr, "[%d] S: %s", fd, "Unknown Command");
			}
			request.clear();
			command.clear();
		}

	}
}

int build_fd_sets(int *sock_tcp, int *sock_udp, fd_set *readfds){
	FD_ZERO(readfds);
	FD_SET(*sock_tcp, readfds);
	FD_SET(*sock_udp, readfds);
	return 0;
}


int main(int argc, char *argv[])
{
	int c;
	opterr = 0;
	cout<<"MAIN\n";

	void sigint_handler(int sig);
	struct sigaction sa;

	struct sockaddr_in serv_addr, serv_addr_udp;
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	while((c = getopt(argc, argv, "p:av")) != -1){
		switch(c){
		case 'p':
			replica_port = atoi(optarg);
			break;
		case 'a':
			fprintf(stderr, "*** Author: Saket Milind Karve (saketk)");
			exit(0);
			break;
		case 'v':
			vflag = true;
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

	string config_file = string(argv[optind]);
	KVS_NO = stoi(string(argv[optind+1]));

	map <char, vector<string> > addr_list = parse_config(config_file);

	// Get address of self
	string myaddr = addr_list['K'][KVS_NO - 1];
	self_ip = myaddr.substr(0, myaddr.find(":"));
	myaddr = myaddr.substr(myaddr.find(":")+1);
	self_port = stoi(myaddr.substr(0,myaddr.find(",")));
	myaddr = myaddr.substr(myaddr.find(",")+1);
	heartbeat_port = stoi(myaddr.substr(0,myaddr.find(",")));
	myaddr = myaddr.substr(myaddr.find(",")+1);
	replica_port = stoi(myaddr);
	cout<<"my port: "<<self_port<<" Heartbeat port: "<<heartbeat_port<<" replica_port: "<<replica_port<<endl;
	
	string master_addr_str = self_ip + ":" + to_string(heartbeat_port);

	//Heartbeat handler thread
	struct heartbeatArgs args;
	args.heartAddr = master_addr_str;
	args.killed = &killed;
	pthread_t heartbeat;
	pthread_create(&heartbeat, NULL, heartbeat_client, &args);
	pthread_detach(heartbeat);

	//replica_port = 10001; //Add this to config file later.

	//Get all replicas from the config file
	fstream repf("tempRepConfig", ios::in);
	int line_number = ((KVS_NO-1) / N) + 1;
	int cnt = 0;
	string line1;
	cout<<line_number<<endl;
	while(getline(repf,line1)){
		cnt++;
		//cout<<cnt<<endl;
		if(cnt == line_number){
			cout<<"In: "<<cnt<<endl;
			string n;
			while(line1.find(",") != string::npos){
				n = line1.substr(0,line1.find(","));
				string curr_ip = n.substr(0,n.find(":"));
				n = n.substr(n.find(":")+1);
				string curr_port = n.substr(0,n.find(";"));
				string curr_rep_port = n.substr(n.find(";")+1);
				string curr_add = curr_ip + ":" + curr_port;
				cout<<"IP: "<<curr_ip<<" PORT: "<<curr_port<<" REP: "<<curr_rep_port<<endl;
				if(curr_add.compare(self_ip + ":" + to_string(self_port)) != 0){
					replicas.push_back(curr_ip + ":" + curr_rep_port);
				}
				line1 = line1.substr(line1.find(",")+1);
			}
			n = line1.substr(0,line1.find(","));
			string curr_ip = n.substr(0,n.find(":"));
			n = n.substr(n.find(":")+1);
			string curr_port = n.substr(0,n.find(";"));
			string curr_rep_port = n.substr(n.find(";")+1);
			string curr_add = curr_ip + ":" + curr_port;
			cout<<"IP: "<<curr_ip<<" PORT: "<<curr_port<<" REP: "<<curr_rep_port<<endl;
			if(curr_add.compare(self_ip + ":" + to_string(self_port)) != 0){
				replicas.push_back(curr_ip + ":" + curr_rep_port);
			}
		}
	}
	for(int i=0;i<replicas.size();i++){
		cout<<i<<" "<<replicas[i]<<endl;
	}

	//Handle Ctrl + C signal
	sa.sa_handler = sigint_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	if(sigaction(SIGINT, &sa, NULL) == -1){
		fprintf(stderr, "Error in setting signal handler\n");
		exit(1);
	}

	//pthread_t forwarding_thread;
	//pthread_create(&forwarding_thread, NULL, forward, NULL);
	
	printcache();
	int iSetOption1 = 1;
	int iSetOption2 = 1;
	//Create a socket for listening and accepting connections from client
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption1, sizeof(iSetOption1));
	if(sockfd < 0)
		fprintf(stderr, "Error opening socket\n");

	bzero((char *) &serv_addr, sizeof(serv_addr));

	//Set up address structure
	serv_addr.sin_family = AF_INET;
	//serv_addr.sin_addr.s_addr = INADDR_ANY;
	inet_pton(AF_INET, self_ip.c_str(), &(serv_addr.sin_addr));
	serv_addr.sin_port = htons(self_port);

	//Bind the socket to a port
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		fprintf(stderr, "Failed to bind socket\n");

	//Bind another socket for communicating with replicas to a different port
	sockreplica = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(sockreplica, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption2, sizeof(iSetOption2));
	if(sockreplica < 0)
		fprintf(stderr, "Error opening socket\n");

	bzero((char *) &serv_addr_udp, sizeof(serv_addr_udp));

	//Set up address structure
	serv_addr_udp.sin_family = AF_INET;
	//serv_addr.sin_addr.s_addr = INADDR_ANY;
	inet_pton(AF_INET, self_ip.c_str(), &(serv_addr_udp.sin_addr));
	serv_addr_udp.sin_port = htons(replica_port);
	cout<<replica_port<<endl;
	//Bind the socket to a port
	if(bind(sockreplica, (struct sockaddr *) &serv_addr_udp, sizeof(serv_addr_udp)) < 0)
		fprintf(stderr, "Replica Failed to bind socket\n");

	fd_set readfds;
	int maxfd = max(sockfd, sockreplica);
	
	//Listen for connections
	listen(sockfd, SOMAXCONN);
	listen(sockreplica, SOMAXCONN);


	//Loop through accepting connections
	while(true){
		struct sockaddr_in cli_addr;
		socklen_t clilen = sizeof(cli_addr);
		build_fd_sets(&sockfd, &sockreplica, &readfds);
		int activity = select(maxfd+1, &readfds, NULL, NULL, NULL);

		switch(activity){
			case -1:
				fprintf(stderr, "Error in selecting stream. Terminating\n");
				exit(1);
			case 0:
				fprintf(stderr, "Error in selecting stream. Terminating\n");
				exit(1);
			default:
				if(FD_ISSET(sockfd, &readfds)){
					//TCP Connection
					int *commfd = (int *)malloc(sizeof(int));

					//Accept the connection
					*commfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
					if(*commfd < 0){
						fprintf(stderr, "Error accepting connection\n");
					}else if(vflag){
						fprintf(stderr, "[%d] New Connection\n",*commfd);
					}
					open_conn.push_back(*commfd);
					
					//Create a thread for the new connection
					pthread_t cli_thread;
					pthread_create(&cli_thread, NULL, worker, commfd);
				}else if(FD_ISSET(sockreplica, &readfds)){
					//TCP Connection
					int *commfdreplica = (int *)malloc(sizeof(int));
					if(!killed){
						//Accept the connection
						*commfdreplica = accept(sockreplica, (struct sockaddr *) &cli_addr, &clilen);
						if(*commfdreplica < 0){
							fprintf(stderr, "Error accepting connection\n");
						}else if(vflag){
							fprintf(stderr, "[%d] New Connection\n",*commfdreplica);
						}
						
						//Create a thread for the new connection
						pthread_t cli_thread_replica;
						pthread_create(&cli_thread_replica, NULL, replication_thread, commfdreplica);
					}
					
				}
			}
		
	}

  return 0;
}
