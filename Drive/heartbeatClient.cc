#include <iostream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <stdlib.h>

#include "DATA_PACKET.pb.h"
#include "utility.h"

using namespace std;

int sock;
int servNum;

void * heartbeat_handler(void * args) {
  string addrStr = *( (string *) args);

  if ( (sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  string ip(addrStr.substr(0, addrStr.find(":")));
  string port(addrStr.substr(addrStr.find(":")+1));
  int portNum = stoi(port);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  inet_aton(ip.c_str(), &(addr.sin_addr));
  addr.sin_port = htons(portNum);

  if ( bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("bind");
    exit(1);
  }

  int numResp = 0;

  // listen for packets forever
  while (true) {
    char buf[4096];
    int n = 0;
    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    n = recvfrom(sock, buf, 4095, 0, (struct sockaddr *) &src, &srclen);
    if (n < 0) {
      cerr << "Error receiving packet from front-end" << endl;
      close(sock);
      exit(1);
    }
    buf[n-2] = 0;

    Packet packet_rcvd;
    if (!packet_rcvd.ParseFromString(buf)) {
      cerr << "Unable to parse received message: " << buf << endl;
      close(sock);
      exit(1);
    }

    string command = packet_rcvd.command();
    transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (command == "heart") {
      // send response to the received server
      Packet toSend;
      toSend.set_command("UP");
      string out;
      if (!toSend.SerializeToString(&out)) {
        cerr << "Could not serialize packet" << endl;
        exit(1);
      }
      out = out + "\r\n";
      char msg[out.size()];
      out.copy(msg, out.size());

      numResp++;

      if (sendto(sock, msg, strlen(msg), 0,
            (struct sockaddr *) &src, sizeof(src)) < 0) {
        perror("sendto");
        exit(1);
      }
    }
  }
}

void handler(int signal) {
  close(sock);
  exit(0);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, handler);

  if (argc != 2) {
    cerr << "enter port number argument" << endl;
    return 1;
  }

  string addr("127.0.0.1:");
  addr.append(argv[1]);

  pthread_t thread;
  pthread_create(&thread, NULL, heartbeat_handler, &addr);
  pthread_join(thread, NULL);

  return 0;
}
