#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <strings.h>
#include <cstring>
#include <pthread.h> 
#include <algorithm>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <vector>
#include <sys/types.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include "utility.h"
#include "smtp_forward_client.h"


using namespace std;

// Debug flag
static int debug = 1;

// Get host name from email
string extract_domain_name(string address)
{
	string name = "";
	int i = 0;
	while(address[i] != '@')
		i = i + 1;
	i = i + 1;
	while(address[i] != '>')
	{
		name = name + address[i];
		i = i + 1;
	}
	return name;
}

// Send mail to non-local host
string get_domain_server(string address)
{
	string domain = extract_domain_name(address);

	// Perform DNS lookup
	unsigned char response[4096];
	char buff[4096];
	
	int query_status = res_query(domain.c_str(), C_IN, T_MX, response, 4096);

	if(query_status == -1)
	{
        cerr << h_errno << " " << hstrerror(h_errno) << "\n";
        return "ERROR";
    }

    // Check the contents of the reply
    HEADER *head = reinterpret_cast<HEADER*>(response);

    //Check for errors
    if (head->rcode != NOERROR)
    {

        cerr << "Error: ";
        switch (head->rcode)
        {
	        case HOST_NOT_FOUND:
	            cerr << "Unable to identify the given domain name.\n";
	            break;
	        case NO_DATA:
	            cerr << "No data received.\n";
	            break;
	        case NO_RECOVERY:
	            std::cerr << "No recovery error.\n";
	            break;
	        case TRY_AGAIN:
	            cerr << "Error occurred. Try again.\n";
	            break;
	        default:
	            cerr << "Unknown error.\n";
        }
        return "ERROR";
    }

    // Obtain the smtp server name
    ns_msg message;
    ns_rr rr;

    // Get number of mx records
    ns_initparse(response, query_status, &message);
    int num_of_servers = ns_msg_count(message, ns_s_an);

    // Check for server with lowest priority number
    string smtp_server;
    int priority = INT_MAX;
    for(int i = 0; i < num_of_servers; i++)
    {
    	int prr = ns_parserr(&message, ns_s_an, i, &rr);
		ns_sprintrr(&message, &rr, NULL, NULL, reinterpret_cast<char*>(buff), sizeof(buff));

		char server_name_buff[NS_MAXDNAME];
		const u_char *rdata = ns_rr_rdata(rr);

		const uint16_t pri = ns_get16(rdata);
		int len = dn_expand(response, response + query_status, rdata + 2, server_name_buff, sizeof(server_name_buff));
		string servername(server_name_buff);

		// Check if server has higher priority(lower priority number)
		if(priority > pri)
		{
			priority = pri;
			smtp_server = servername;
		}
    }
    return smtp_server;
}

// Write data stream to server
void write_stream(int sockfd, const char *str, const char *arg)
{
	char buff[4096];
	if (arg != NULL)
    	snprintf(buff, sizeof(buff), str, arg);        
    else
        snprintf(buff, sizeof(buff), str, NULL);
    send(sockfd, buff, strlen(buff), 0);
}

// Write undeliverable msg to tmp file
void write_to_tmp(vector<string> data)
{
	string tmp_file = "tmp";
	ofstream my_new_file;
	my_new_file.open(tmp_file, ios::app);
	for(int i = 3; i < data.size(); i++)
	{
		my_new_file << data[i];
	}
	my_new_file.close();
}

// Get email id from header
string get_mail_id(string header)
{
	string id = "";
	int i = 0;
	while(header[i] != '>')
	{
		id = id + header[i];
		i++;
	}
	id = id + header[i];
	return id;
}

string extract_user_name(string email_id)
{
	string name = "";
	int i = 1;
	while(email_id[i] != '@')
	{
		name = name + email_id[i];
		i = i + 1;
	}
	return name;
}

// Open transmission channel to server
int send_data(vector<string> data)
{
	// Get domain name
	string domain = data[0];
	cout << "Domain: " << domain << endl;

	// Create socket to communicate with mailserver
	int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd < 0)
	{
		cerr << "Cannot open socket\n";
		//write_to_tmp(data);
		return 0;
	}
	else
	{	
		cout << "Socket created\n";
		// Initialize server data
		struct sockaddr_in server_addr;
		bzero(&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(25);
		struct hostent *host = gethostbyname(domain.c_str());
		if (host == NULL)
		{
	       printf("gethostbyname() failed\n");
	       close(sockfd);
	       //write_to_tmp(data);
	       return 0;
	    }
	    else
	    {
	    	cout << "Host name obtained.\n";
	    	cout << "Host name: " << host->h_name << endl;
		    // Store IP of mail server
		    struct in_addr** addr_list = (struct in_addr **)host->h_addr_list;
			memcpy((char*)&(server_addr.sin_addr), host->h_addr, host->h_length);

			// Buffer to store server response
			char response[1024];

			// Connect to mail server
			int status = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
			if(status != -1)
			{
				cout << "Connected to MX: " << host->h_name << endl;
				string local_domain = "localhost.upenn.edu";
				write_stream(sockfd, "HELO %s\r\n", local_domain.c_str());
				pretty_print("C", "HELO " + local_domain + "\n", debug);
				recv(sockfd, response, sizeof(response), 0);
				pretty_print("S", string(response), debug);

				// Check for validity of response
				if(string(response).substr(0, 3) != "220")
				{
					//write_to_tmp(data);
					close(sockfd);
					return 0;
				}
				string name = extract_user_name(data[1]);
				cout << "Name: " << name << endl;
				string s = "<" + name + "@localhost.upenn.edu" + ">";
				cout << "New: " + s << endl;
				memset(response, 0, sizeof(response));
			    //write_stream(sockfd, "MAIL FROM:%s\r\n", data[1].c_str());
			    write_stream(sockfd, "MAIL FROM:%s\r\n", s.c_str());
			    //pretty_print("C", "MAIL FROM:" + data[1] + "\n", debug);
			    pretty_print("C", "MAIL FROM:" + s + "\n", debug);
			    recv(sockfd, response, sizeof(response), 0);
				pretty_print("S", string(response), debug);
				// Check for validity of response
				if(string(response).substr(0, 3) != "250")
				{
					//write_to_tmp(data);
					cout << "Failed in Mail From.\n";
					close(sockfd);
					return 0;
				}
				memset(response, 0, sizeof(response));

			    write_stream(sockfd, "RCPT TO:%s\r\n", data[2].c_str());
			    pretty_print("C", "RCPT TO:" + data[2], debug);
			    recv(sockfd, response, sizeof(response), 0);
				pretty_print("S", string(response), debug);
				// Check for validity of response
				if(string(response).substr(0, 3) != "250")
				{
					//write_to_tmp(data);
					cout << "Failed in RCPT TO.\n";
					close(sockfd);
					return 0;
				}
				memset(response, 0, sizeof(response));

			    write_stream(sockfd, "DATA\r\n", NULL);
			    pretty_print("C", "DATA\r\n", debug);
			    recv(sockfd, response, sizeof(response), 0);
				pretty_print("S", string(response), debug);
				// Check for validity of response
				if(string(response).substr(0, 1) == "5")
				{
					//write_to_tmp(data);
					cout << "Failed in DATA.\n";
					close(sockfd);
					return 0;
				}
				memset(response, 0, sizeof(response));

			    for(int i = 3; i < data.size(); i++)
			    {
			    	write_stream(sockfd, "%s", data[i].c_str());
			    	pretty_print("C", data[i], debug);
			    }
			    write_stream(sockfd, ".\r\n", NULL); 
			    pretty_print("C", ".\r\n", debug);
			    recv(sockfd, response, sizeof(response), 0);
				pretty_print("S", string(response), debug);
				
				// Check for validity of response
				if(string(response).substr(0, 1) == "5")
				{
					cout << "Failed in DATA TRANSFER.\n";
					close(sockfd);
					return 0;
				}
				memset(response, 0, sizeof(response));

			    write_stream(sockfd, "QUIT\r\n", NULL); 
			    pretty_print("C","QUIT\r\n", debug);
			    recv(sockfd, response, sizeof(response), 0);
				pretty_print("S", string(response), debug);
				memset(response, 0, sizeof(response));
			    close(sockfd);
			    return 1;
			}
			else
			{
				cerr << "Unable to connect to server...\n";
				close(sockfd);
				return 0;
			}
		}
	}
	return 0;
}

int send_mail(string tmp_name, string recipient, string sender, vector<string> header)
{
	string line;
	string address;
	ifstream myfile;
	myfile.open(tmp_name, ios::in);
	vector<string> data;

	// Store the entire mail including headers
	vector<string> mail;

	for(int i = 0; i < header.size(); i++)
	{
		mail.push_back(header[i]);
	}

	// Push the mail data in mail
	while(getline(myfile, line))
	{
		if(line.substr(0,8) != "Subject:")
			mail.push_back(line + "\n");
	}

	myfile.close();

	// Get the domain name for receivers mbox
	string domain = get_domain_server(recipient);
	cout << "Recipient Domain: " << domain << endl;

	// Check if mail server name obtained
	if(domain == "ERROR")
	{
		cerr << "[C]: Error retrieving MX records.\n";
		cerr << "[C]: Moving on to next mail(if exists)";
		return 0;
	}

	mail.insert(mail.begin(), domain);
	mail.insert(mail.begin() + 1, "<" + sender + ">");
	mail.insert(mail.begin() + 2, "<" + recipient + ">");

	int flag = send_data(mail);

	return flag;
}

// Reorder headers
vector<string> reorder_header(vector<string> header)
{
	vector<string> res;

	string to = header[0];
	string from = header[1];
	string sub = header[4];
	string mid = header[3];
	string date = header[2];

	res.push_back(to);
	res.push_back(from);
	res.push_back(sub);
	res.push_back(mid);
	res.push_back(date);

	return res;

}

vector<int> forward_mail(string tmp_name, vector<string> header, vector<string> non_local_recipients, string sender, int flag)
{
	// Vector maintains the status of sent mails
	vector<int> status;

	cout << "In forward client\n";

	for(int i = 0; i < non_local_recipients.size(); i++)
	{
		if(flag == 0)
		{
			header.insert(header.begin(), "To: " + non_local_recipients[i] + "\r\n");
			vector<string> reordered_headers = reorder_header(header);
			int stat = send_mail(tmp_name, non_local_recipients[i], sender, reordered_headers);
			status.push_back(stat);
			header.erase(header.begin());
		}
		else
		{
			int stat = send_mail(tmp_name, non_local_recipients[i], sender, header);
			status.push_back(stat);
		}

	}
	return status;
}