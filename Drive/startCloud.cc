#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/signal.h>
#include <string.h>
#include <fcntl.h>

using namespace std;

map<char, vector<string>> serverArguments;
vector<pid_t> pids;

map<char, vector<string>> parse_config(string file_name){
	map < char, vector < string >  > server_nodes;
	server_nodes.insert({'M', vector < string >()});
	server_nodes.insert({'D', vector < string >()});
	server_nodes.insert({'F', vector < string >()});
	server_nodes.insert({'S', vector < string >()});
	server_nodes.insert({'P', vector < string >()});
	server_nodes.insert({'K', vector < string >()});
	server_nodes.insert({'R', vector < string >()});

	ifstream config;
	config.open(file_name.c_str());

	string line;
	while (getline(config,line)){
		char node_type = line.at(0);
		string node_address = line.substr(1);
		if(node_address.length() == 0){
			cerr <<" Bad config\n";
			exit(1);
		}
		server_nodes[node_type].push_back(node_address);
	}

	return server_nodes;
}
 
void ctrlc_handler(int signum) {
  // iterate through all pids and send kill
  for (auto it = pids.begin(); it != pids.end(); ++it) {
    cout << "Killing: " << *it << endl;
    kill(*it, SIGINT);
  }
  exit(0);
}

int main(int argc, char * argv[]) {
  if (argc != 2) {
    cout << "please input config filename" << endl;
    exit(1);
  }
  string configFilename(argv[1]);
  map<char, vector<string>> serverConfig = parse_config(configFilename);

  // register ctrl-c handler to kill all processes
  signal(SIGINT, ctrlc_handler);
  
  // set up the arguments for each server type
  serverArguments['C'] = { "./admin", "-v", configFilename, "NUM" };
  serverArguments['A'] = { "./auth", configFilename, "NUM" };
  serverArguments['R'] = { "./drive", configFilename, "NUM" };
  serverArguments['D'] = { "./dummy", "-v", configFilename, "NUM" };
  serverArguments['K'] = { "./kvs", configFilename, "NUM" };
  serverArguments['M'] = { "./master", "-v", "-r", "2", "-q", "1", "-t",
    "3", "-h", "20", configFilename };
  serverArguments['L'] = { "./middleLoad", "-v", configFilename, "-n", "NUM" };
  serverArguments['P'] = { "./pop3_server", configFilename, "NUM" };
  serverArguments['S'] = { "./smtp_server", configFilename, "NUM" };
  serverArguments['F'] = { "./fs/frontend", configFilename, "NUM" };

  // loop through each of the servers and fork a process that exec's each server
  // with the given arguments
  for (auto type = serverConfig.begin(); type != serverConfig.end(); ++type) {
    if (type->first == 'F')
      continue; // wait until the end to boot up any frontends
    auto servers = type->second;
    int serversSize = servers.size();
    for (int i = 0; i < serversSize; i++) {
      // create the argument list for this type
      vector<string> args = serverArguments[type->first];
      char * progArgs[args.size()+1];
      for (int j = 0; j < args.size(); j++) {
        if (args[j] == "NUM")
          args[j] = to_string(i+1);
        char *c_string = strndup(args[j].c_str(), args[j].size());
        progArgs[j] = c_string;
      }
      progArgs[args.size()] = NULL;
      // fork a process
      pid_t c_pid = fork();

      if (c_pid == 0) {
        // child, exec a new process with progArgs
        cout << "Running: " << progArgs[0] << endl;
        execvp(progArgs[0], progArgs);
        perror("execvp failed");
      } else if (c_pid > 0) {
        // parent, save the pid_t in pids
        pids.push_back(c_pid);
      } else {
        perror("fork failed");
        exit(1);
      }
    }
  } 

  // sleep for a bit before booting up the frontends
  cout << "sleeping..." << endl;
  sleep(4);
  cout << "back" << endl;

  // boot up frontends
  int frontends = serverConfig['F'].size();
  for (int i = 0; i < frontends; i++) {
    vector<string> args = serverArguments['F'];
    char *progArgs[args.size()+1];
    for (int j = 0; j < args.size(); j++) {
      if (args[j] == "NUM")
        args[j] = to_string(i+1);
      char *c_string = strndup(args[j].c_str(), args[j].size());
      progArgs[j] = c_string;
    }
    progArgs[args.size()] = NULL;
    pid_t c_pid = fork();

    if (c_pid == 0) {
      // open dev null
      int devNull = open("/dev/null", O_WRONLY);
      dup2(devNull, STDOUT_FILENO);
      dup2(devNull, STDERR_FILENO);
      close(devNull);
      cout << "Running: " << progArgs[0] << endl;
      execvp(progArgs[0], progArgs);
      perror("execvp failed");
    } else if (c_pid > 0) {
      pids.push_back(c_pid);
    } else {
      perror("fork failed");
      exit(1);
    }
  }

  // wait on all of the child processes
  wait(NULL);

  // if we get here, someone died, so raise sigint to kill everyone
  raise(SIGINT);

  return 0;
}
