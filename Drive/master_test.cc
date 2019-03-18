#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "DATA_PACKET.pb.h"
#include "utility.h"

#define BUF_SIZE 4096

using namespace std;

int main(int argc, char *argv[]) {
  struct sockaddr_in saddr_in;

  saddr_in.sin_family = AF_INET;
  inet_aton("127.0.0.1", &(saddr_in.sin_addr));
  saddr_in.sin_port = htons(5000);

  // open socket
  int sock;
  if ( (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("server_sock");
    exit(1);
  }

  connect(sock, (struct sockaddr *) &saddr_in, sizeof(saddr_in));

  Packet p;
  p.set_command("WHERIS");
  p.add_arg("carter");

  string msg;
  if (!p.SerializeToString(&msg))
    cerr << "Can't serialize" << endl;

  msg = msg + "\r\n";
  char response[msg.size()];
  msg.copy(response, msg.size());

  do_send(&sock, response, sizeof(response));
  
  // wait for response
  char buf[4096];
  int n = 0;
  n = do_recv(&sock, &buf[0]);
  if (n == -1) {
    cerr << "error receiving" << endl;
    close(sock);
    exit(1);
  }
  buf[n-2] = 0;
  buf[n-1] = 0;
  buf[n] = 0;
  
  Packet r;
  if (!r.ParseFromString(buf))
    cerr << "couldn't parse!" << endl;
  string command = r.command();
  cout << command << endl;

  if (command == "KVADDR")
    cout << r.arg(0) << endl;

  close(sock);
  sock = socket(AF_INET, SOCK_STREAM, 0);
  connect(sock, (struct sockaddr *) &saddr_in, sizeof(saddr_in));

  Packet q;
  q.set_command("WHERIS");
  q.add_arg("gil");
  string msg1;
  if (!q.SerializeToString(&msg1))
    cerr << "Can't serialize" << endl;
  msg1 = msg1+"\r\n";
  char response1[msg1.size()];
  msg1.copy(response1,msg1.size());

  do_send(&sock, response1, sizeof(response1));

  // wait
  memset(buf, 0, 4096);
  n = 0;
  cout << "receiving message 2" << endl;
  n = do_recv(&sock, &buf[0]);
  if (n == -1) {
    cerr << "error rcv" << endl;
    close(sock);
    exit(1);
  }
  buf[n-2] = 0;
  buf[n-1] = 0;
  buf[n] = 0;

  Packet r1;
  if (!r1.ParseFromString(buf))
    cerr << "couldn't parse!" << endl;
  string com = r1.command();
  cout << "command rcvd: " << com << endl;

  close(sock);

  sock = socket(AF_INET, SOCK_STREAM, 0);
  connect(sock, (struct sockaddr *) &saddr_in, sizeof(saddr_in));

  Packet s;
  s.set_command("NEWCL");
  s.add_arg("newguy");
  string msg2;
  if (!s.SerializeToString(&msg2))
    cerr << "Can't serialize" << endl;
  msg2 = msg2+"\r\n";
  char response2[msg2.size()];
  msg2.copy(response2,msg2.size());

  do_send(&sock, response2, sizeof(response2));

  // wait
  memset(buf, 0, 4096);
  n = 0;
  cout << "receiving message 3" << endl;
  n = do_recv(&sock, &buf[0]);
  if (n == -1) {
    cerr << "error rcv" << endl;
    close(sock);
    exit(1);
  }
  buf[n-2] = 0;
  buf[n-1] = 0;
  buf[n] = 0;

  Packet r2;
  if (!r2.ParseFromString(buf))
    cerr << "couldn't parse!" << endl;
  string com2 = r2.command();
  cout << "command rcvd: " << com2 << endl;
  if (com2 == "CLADDED")
    cout << "addr: " << r2.arg(0) << endl;
  if (com2 == "DUPCL")
    cout << "addr: " << r2.arg(0) << endl;

  close(sock);

  return 0;
}
