#include "middleServerComms.h"
#include "../utility.h"

using namespace std;

string serializedStringWithCommandArgs(string command, vector<string> args) {
  // returns a serialized (from Packet) string of the command and all args
  Packet p;
  p.set_command(command);
  for (auto it = args.begin(); it != args.end(); ++it) {
    // add it to the packet
    p.add_arg(*it);
  }
  // serialize the packet into a string
  string out;
  if (!p.SerializeToString(&out)) {
    cerr << "Could not serialize packet." << endl;
    exit(1);
  }
  string pre = generate_prefix(out.size());
  out = pre + out;
  return out;
}

Packet receivePacket(int sock) {
  int n = 0;
  char buf[(1024*1024) + 200];
  string rcvd;
  
  Packet p;
  n = do_recv(&sock, buf, 10);
  buf[n] = 0;
  cout<<"Buf: "<<buf<<endl;
  if (n < 0) {
    cerr << "Could not receive" << endl;
    p.set_status_code("601");
    p.set_status("Server down");
    return p;
  }
  //buf[n-2] = 0;

  if (!p.ParseFromString(buf)) {
    cerr << "Could not parse received string" << endl;
    exit(1);
  }
  cout << "Status code: " << p.status_code() << endl;
  cout << "Status: " << p.status() << endl;
  return p;
}

vector<string> genHeaderStr(map<string, string> headerInfo) {
  vector<string> headerStr;
  for(int i = 0; i < 5; i ++)
  {
    headerStr.push_back("");
  }
  for (auto it = headerInfo.begin(); it != headerInfo.end(); ++it)
  {
    if(it->first == "User-Agent:")
    {
      headerStr[0] = it->first + " " + it->second;
    }
    else if(it->first == "Content-Type:")
    {
      headerStr[2] = it->first + " " + "text/plain; charset=utf-8; format=flowed;";
    }
  }
  map<string, string>::iterator ait = headerInfo.find("MIME-Version:");
  if (ait == headerInfo.end()){
	  headerStr[1] = "MIME-Version: 1.0";
  }
   map<string, string>::iterator bit = headerInfo.find("Content-Transfer-Encoding:");
  if (bit == headerInfo.end()){
    headerStr[3] = "Content-Transfer-Encoding: 7bit";
  }
  map<string, string>::iterator cit = headerInfo.find("Content-Language:");
  if (cit == headerInfo.end()){
	   headerStr[4] = "Content-Language: en-US";
  }
  
  return headerStr;
}


// returns status_code
// int sendmail(int sock, map<string, string> headerInfo,
//     string from, vector<string> to, string subject, string msg) {
// 	cout << "\n\nHEADER INFO\n\n:";

// 	map<string, string>::iterator it;

// 	for ( it = headerInfo.begin(); it != headerInfo.end(); it++ )
// 	{
// 	    std::cout << it->first  // string (key)
// 	              << ':'
// 	              << it->second   // string's value
// 	              << std::endl ;
// 	}
//   // we are already connected to the mail server, we simply have to execute the
//   // commands in the order that the SMTP server is expecting them
//   // we send the HELO command first
//   string heloStr = serializedStringWithCommandArgs("HELO", 
//       vector<string>(1, "penncloud"));
//   // send the helo command
//   char heloMsg[heloStr.size()];
//   heloStr.copy(heloMsg, heloStr.size());
//   do_send(&sock, heloMsg, sizeof(heloMsg));
  
//   // receive the response from the server
//   Packet packet_rcvd = receivePacket(sock);

//   string respCode = packet_rcvd.status_code();
//   if (respCode != "250") {
//     cerr << "Error from SMTP server" << endl;
//     return stoi(respCode);
//   }

//   // get ready to send the next message
//   packet_rcvd.Clear();
//   respCode.erase(respCode.begin(), respCode.end());

//   string mailFromStr = serializedStringWithCommandArgs("MAIL FROM",
//       vector<string>(1, from));
//   char mailMsg[mailFromStr.size()];
//   mailFromStr.copy(mailMsg, mailFromStr.size());
//   do_send(&sock, mailMsg, sizeof(mailMsg));

//   // receive next response
//   packet_rcvd = receivePacket(sock);

//   respCode = packet_rcvd.status_code();
//   if (respCode != "250"){
//     cerr << "Error from SMTP server" << endl;
//     return stoi(respCode);
//   }

//   packet_rcvd.Clear();
//   respCode.clear(); 
//   for(auto it = to.begin(); it != to.end(); it++)
//   {
//     string rcptTo = serializedStringWithCommandArgs("RCPT TO", vector<string>(1, *it));
//     char rcptMsg[rcptTo.size()];
//     rcptTo.copy(rcptMsg, rcptTo.size());
//     do_send(&sock, rcptMsg, sizeof(rcptMsg));
//     // receive next response
//     packet_rcvd = receivePacket(sock);

//     respCode = packet_rcvd.status_code();
//     if (respCode != "250" && respCode != "251") {
//       cerr << "Error from SMTP server" << endl;
//       return stoi(respCode);
//     }
//     packet_rcvd.Clear();
//   }
  

//   // get ready to send next message
//   packet_rcvd.Clear();
//   respCode.clear();
//   Packet headerPacket;
//   headerPacket.set_command("DATA");
//   string headerStr = genHeaderStr(headerInfo);
//   headerPacket.set_data(headerStr);
//   string out;
//   if (!headerPacket.SerializeToString(&out)) {
//     cerr << "Could not serialize packet." << endl;
//     exit(1);
//   }
//   string pre = generate_prefix(out.size());
//   out = pre + out;
//   char response[out.size()];
//   out.copy(response, out.size());

//   do_send(&sock, response, sizeof(response));

//   // receive response from server
//   packet_rcvd = receivePacket(sock);

//   respCode = packet_rcvd.status_code();
//   //cout << respCode << endl;
//   if (respCode != "354") {
//     cerr << "Error from SMTP server" << endl;
//     return stoi(respCode);
//   }

//   // get ready to send the actual message
//   packet_rcvd.Clear();
//   respCode.clear();

//   Packet finalData;
//   finalData.add_arg(subject);
//   finalData.set_data(msg);
//   string dataOut;
//   if (!finalData.SerializeToString(&dataOut)) {
//     cerr << "Could not serialize packet." << endl;
//     exit(1);
//   }
//   dataOut = generate_prefix(dataOut.size()) + dataOut;
//   char resp[dataOut.size()];
//   dataOut.copy(resp, dataOut.size());

//   do_send(&sock, resp, sizeof(resp));

//   // wait for the final 250 OK
//   packet_rcvd = receivePacket(sock);
//   respCode = packet_rcvd.status_code();
//   if (respCode != "250") {
//     cerr << "Error from SMTP server" << endl;
//     return stoi(respCode);
//   }

//   // now send the QUIT command
//   string quitStr = serializedStringWithCommandArgs("QUIT",
//       vector<string>());
//   char quitMsg[quitStr.size()];
//   quitStr.copy(quitMsg, quitStr.size());
//   do_send(&sock, quitMsg, sizeof(quitMsg));

//   // receive the last message and close the socket
//   packet_rcvd.Clear();
//   respCode.clear();
//   packet_rcvd = receivePacket(sock);
//   respCode = packet_rcvd.status_code();
//   if (respCode != "221") {
//     cerr << "Error from SMTP server" << endl;
//     return stoi(respCode);
//   }
//   // close the socket, we're done
//   return stoi(respCode);
// }

// SMTP PROTOCOL SEND
// void smtp_send(string command, int sock)
// {
//   char buff[command.size()];
//   command.copy(buff, command.size());
//   send_t(&sock, buff, sizeof(buff));
// }

void smtp_send(string cmd, vector<string> arg, int sock)
{
  Packet pack;
  pack.set_command(cmd);
  for(int i = 0; i < arg.size(); i++)
  {
    pack.add_arg(arg[i]);
  }
  string out;
  if(!pack.SerializeToString(&out))
  {
    cerr << "Could not serialize packet to send.\n";
  }
  string pre = generate_prefix(out.size());
  out = pre + out + "\r\n";
  char response[out.size()];
  out.copy(response, out.size());
  do_send(&sock, response, sizeof(response));
}

// Split the message based on \r\n
vector<string> split_msg(string msg)
{
  vector<string> res;
  string tmp = "";
  for(int i = 0; i < msg.size(); i++)
  {
    tmp = tmp + msg[i];
    if(tmp.size() >= 0 && msg[i-1] == '\r' && msg[i] == '\n')
    {
      res.push_back(tmp.substr(0, tmp.size()-2));
      tmp = "";
    }
  }
  if(tmp.size() > 0)
    res.push_back(tmp + "\r\n");
  return res;
}


int sendmail(int sock, map<string, string> headerInfo,
    string from, vector<string> to, string subject, string msg) {
  cout << "\n\nHEADER INFO\n\n:";

  map<string, string>::iterator it;

  for ( it = headerInfo.begin(); it != headerInfo.end(); it++ )
  {
      std::cout << it->first  // string (key)
                << ':'
                << it->second   // string's value
                << std::endl ;
  }
  // we are already connected to the mail server, we simply have to execute the
  // commands in the order that the SMTP server is expecting them
  // we send the HELO command first
  vector<string> arg;
  arg.push_back("Penncloud");
  smtp_send("HELO", arg, sock);
  arg.clear();
  // string tmp = "HELO Penncloud\r\n";
  // smtp_send(tmp, sock);
  
  // receive the response from the server
  Packet packet_rcvd = receivePacket(sock);

  string respCode = packet_rcvd.status_code();
  if (respCode != "250") {
    cerr << "Error from SMTP server" << endl;
    return stoi(respCode);
  }

  // get ready to send the next message
  packet_rcvd.Clear();
  respCode.erase(respCode.begin(), respCode.end());

  arg.push_back("<" + from + ">");
  smtp_send("MAIL FROM", arg, sock);
  arg.clear();
  // tmp = "MAIL FROM:<" + from + ">\r\n";
  // smtp_send(tmp, sock); 

  // receive next response
  packet_rcvd = receivePacket(sock);

  respCode = packet_rcvd.status_code();
  if (respCode != "250"){
    cerr << "Error from SMTP server" << endl;
    return stoi(respCode);
  }

  packet_rcvd.Clear();
  respCode.clear(); 
  arg.clear();
  for(int i = 0; i < to.size(); i++)
  {
    arg.push_back("<" + to[i] + ">");
    smtp_send("RCPT TO", arg, sock);
    arg.clear();
    // tmp = "RCPT TO:<" + to[i] + ">\r\n";
    // smtp_send(tmp, sock);
    // receive next response
    packet_rcvd = receivePacket(sock);

    respCode = packet_rcvd.status_code();
    if (respCode != "250" && respCode != "251") {
      cerr << "Error from SMTP server" << endl;
      return stoi(respCode);
    }
    packet_rcvd.Clear();
  }
  

  // get ready to send next message
  // packet_rcvd.Clear();
  // respCode.clear();
  // Packet headerPacket;
  smtp_send("DATA", arg, sock);
  //smtp_send("DATA\r\n", sock);
  vector<string> headerStr = genHeaderStr(headerInfo);
 
  // receive response from server
  packet_rcvd = receivePacket(sock);

  respCode = packet_rcvd.status_code();
  //cout << respCode << endl;
  if (respCode != "354") {
    cerr << "Error from SMTP server" << endl;
    return stoi(respCode);
  }

  // get ready to send the actual message
  packet_rcvd.Clear();
  respCode.clear();

  vector<string> msg_bdy = split_msg(msg);

  // Send data to smtp
  string sub = "Subject: " + subject + "\r\n";
  char buff_sub[sub.size()];
  sub.copy(buff_sub, sub.size());
  send_t(&sock, buff_sub, sizeof(buff_sub));

  for(int i = 0; i < headerStr.size(); i++)
  {
     string tmp = headerStr[i] + "\r\n";
     char buff[tmp.size()];
     tmp.copy(buff, tmp.size());
     send_t(&sock, buff, sizeof(buff));
  }

  char n_line[] = "\r\n";
  send_t(&sock, n_line, sizeof(n_line)-1);

  for(int i = 0; i < msg_bdy.size(); i++)
  {
    string tmp = msg_bdy[i] + "\r\n";
    char buff[tmp.size()];
    tmp.copy(buff, tmp.size());
    send_t(&sock, buff, sizeof(buff));
  }
  char buff[] = "\r\n.\r\n";
  send_t(&sock, buff, sizeof(buff)-1);


  // wait for the final 250 OK
  packet_rcvd = receivePacket(sock);
  respCode = packet_rcvd.status_code();
  if (respCode != "250") {
    cerr << "Error from SMTP server" << endl;
    return stoi(respCode);
  }

  // now send the QUIT command
  smtp_send("QUIT", arg, sock);

  // receive the last message and close the socket
  packet_rcvd.Clear();
  respCode.clear();
  packet_rcvd = receivePacket(sock);
  respCode = packet_rcvd.status_code();
  if (respCode != "221") {
    cerr << "Error from SMTP server" << endl;
    return stoi(respCode);
  }
  // close the socket, we're done
  return stoi(respCode);
}


vector<string> receiveMail(int sock, string username, string password) {
  // simply will create packets and get all messages to pass back as a vector to
  // the HTTP server
	cout << "RECEIVE MAIL USER: " << username << "\n";
	cout << "RECEIVE MAIL PASS: " << password << "\n";

  vector<string> errVec;
  errVec.push_back("ERROR");
  vector<string> toRet;
  toRet.push_back("SUCCESS");

  string userStr = serializedStringWithCommandArgs("USER", 
      vector<string>(1, username));
  cout << userStr;
  char userMsg[userStr.size()];
  userStr.copy(userMsg, userStr.size());
  do_send(&sock, userMsg, sizeof(userMsg));

  // wait for response
  Packet rcvd = receivePacket(sock);
  string respCode = rcvd.status_code();
  // check to make sure we have +OK
  if (respCode != "+OK") {
    cerr << "Error from POP3 server (USER)" << endl;
    return errVec;
  }

  // get ready to send PASS
  rcvd.Clear();
  respCode.clear();

  string passStr = serializedStringWithCommandArgs("PASS",
      vector<string>(1, password));
  char passMsg[passStr.size()];
  passStr.copy(passMsg, passStr.size());
  do_send(&sock, passMsg, sizeof(passMsg));

  // wait for response
  rcvd = receivePacket(sock);
  respCode = rcvd.status_code();
  // make sure we get +OK
  if (respCode != "+OK") {
    cerr << "Error from POP3 server (PASS)" << endl;
    return errVec;
  }
  
  // get ready to send STAT
  rcvd.Clear();
  respCode.clear();

  string statStr = serializedStringWithCommandArgs("STAT", vector<string>());
  char statMsg[statStr.size()];
  statStr.copy(statMsg, statStr.size());
  do_send(&sock, statMsg, sizeof(statMsg));

  // wait for response
  rcvd = receivePacket(sock);
  respCode = rcvd.status_code();
  if (respCode != "+OK") {
    cerr << "Error from POP3 server (STAT)" << endl;
    return errVec;
  }
  // get the number of messages
  string status = rcvd.status();
  int numMsgs = stoi(status.substr(0, status.find(" ")));

  // get ready to RETR all of the messages
  rcvd.Clear();
  respCode.clear();

  for (int i = 1; i <= numMsgs; i++) {
    // create a packet to send to the server
    string retrStr = serializedStringWithCommandArgs("RETR", 
        vector<string>(1, to_string(i)));
    char retrMsg[retrStr.size()];
    retrStr.copy(retrMsg, retrStr.size());
    do_send(&sock, retrMsg, sizeof(retrMsg));
    // wait for response
    rcvd = receivePacket(sock);
    respCode = rcvd.status_code();
    if (respCode != "+OK") {
      cerr << "Error from POP3 server (RETR)" << endl;
      return errVec;
    }
    // add data to vector
    toRet.push_back(rcvd.data());
    // clear received
    rcvd.Clear();
    respCode.clear();
  }

  // clear again to be sure
  rcvd.Clear();
  respCode.clear();
  
  // send the QUIT command to the server
  string quitStr = serializedStringWithCommandArgs("QUIT", vector<string>());
  char quitMsg[quitStr.size()];
  quitStr.copy(quitMsg, quitStr.size());
  do_send(&sock, quitMsg, sizeof(quitMsg));

  // wait for final response
  rcvd = receivePacket(sock);
  respCode = rcvd.status_code();
  if (respCode != "+OK") {
    cerr << "Error from POP3 server (QUIT)" << endl;
    return errVec;
  }

  // return vector of messages
  return toRet;
}

string deleteMail(int sock, string username, string password,
    vector<int> msgIndexToDelete) {
  string errStr("ERROR");
  string succStr("SUCCESS");

  string userStr = serializedStringWithCommandArgs("USER",
      vector<string>(1, username));
  char userMsg[userStr.size()];
  userStr.copy(userMsg, userStr.size());
  do_send(&sock, userMsg, sizeof(userMsg));

  // wait for response
  Packet rcvd = receivePacket(sock);
  // check response code
  if (rcvd.status_code() != "+OK") {
    cerr << "Error from POP3 server (USER)" << endl;
    return errStr;
  }

  // get ready to send pass
  rcvd.Clear();

  string passStr = serializedStringWithCommandArgs("PASS",
      vector<string>(1, password));
  char passMsg[passStr.size()];
  passStr.copy(passMsg, passStr.size());
  do_send(&sock, passMsg, sizeof(passMsg));

  // wait for resp
  rcvd = receivePacket(sock);
  // check code
  if (rcvd.status_code() != "+OK") {
    cerr << "Error from POP3 server (USER)" << endl;
    return errStr;
  }

  rcvd.Clear();

  // iterate over each integer that we want to delete and send a DELE command
  for (auto it = msgIndexToDelete.begin(); it != msgIndexToDelete.end();
      ++it) {
   // create a packet to send to the server
   string deleStr = serializedStringWithCommandArgs("DELE",
      vector<string>(1, to_string(*it)));
  char deleMsg[deleStr.size()];
  deleStr.copy(deleMsg, deleStr.size());
  do_send(&sock, deleMsg, sizeof(deleMsg));

  // wait for response
  rcvd = receivePacket(sock);
  if (rcvd.status_code() != "+OK") {
    cerr << "Error from POP3 server (DELE)" << endl;
    return errStr;
  }
  // clear received
  rcvd.Clear();
  }

  // clear again to be sure
  rcvd.Clear();

  // send QUIT command to the server
  string quitStr = serializedStringWithCommandArgs("QUIT", vector<string>());
  char quitMsg[quitStr.size()];
  quitStr.copy(quitMsg, quitStr.size());
  do_send(&sock, quitMsg, sizeof(quitMsg));

  rcvd = receivePacket(sock);
  if (rcvd.status_code() != "+OK") {
    cerr << "Error from POP3 server (QUIT)" << endl;
    return errStr;
  }

  return succStr;
}

string driveTransaction(int sock, string command, map<string, string> driveArgs, int data_sz, unsigned char* file_packet_data )
{
  // based on the type of transaction, we will create a packet to send to the
  // drive server
  Packet to_send, rcvd;
  string errStr("ERROR");
  string retStr;
  
  // check to make sure username is an argument
  auto it = driveArgs.find("username");
  if (it == driveArgs.end())
  {
    cerr << "Missing username in drive arguments" << endl;
    return errStr;
  }

  // create packArgs (for commands that need arguments in addition to the
  // username)
  vector<string> packArgs;
  packArgs.push_back(driveArgs["username"]);

  transform(command.begin(), command.end(), command.begin(), ::tolower);

  if (command == "display")
  {
    // construct the packet to send to the drive server
    string dispStr = serializedStringWithCommandArgs("DISPLAY",
        vector<string>{driveArgs["username"], driveArgs["filename"], driveArgs["type"]});
    char dispMsg[dispStr.size()];
    dispStr.copy(dispMsg, dispStr.size());
    do_send(&sock, dispMsg, sizeof(dispMsg));

    // wait for the response (json string that we'll return)
    rcvd = receivePacket(sock);
    // check to make sure +OK status code
    string status = rcvd.status_code();
    if (status != "+OK")
    {
      cerr << "Error receiving DISPLAY response from drive" << endl;
      return errStr;
    }
    retStr.append(rcvd.data());
  } 
  else if(command == "download")
  {
    // downloading a file, make sure filename is in driveArgs
    auto it = driveArgs.find("filename");
    if (it == driveArgs.end())
    {
      cerr << "Missing filename in drive argument to download" << endl;
      return errStr;
    }
    // construct the packet to send
    packArgs.push_back(driveArgs["filename"]);
    string downStr = serializedStringWithCommandArgs("DOWNLOAD", packArgs);
    char downMsg[downStr.size()];
    downStr.copy(downMsg, downStr.size());
    do_send(&sock, downMsg, sizeof(downMsg));

    // wait for response
    rcvd = receivePacket(sock);
    // check to make sure +OK status code
    if (rcvd.status_code() != "+OK") {
      cerr << "Error receiving DOWNLOAD response from drive" << endl;
      return errStr;
    }
    int num_packets = stoi(rcvd.data());
    for(int i = 0; i < num_packets; i++){
    // add data in the packet
    	// wait for response
    	rcvd = receivePacket(sock);
    	// check to make sure +OK status code
    	if (rcvd.status_code() != "+OK") {
    		cerr << "Error receiving DOWNLOAD response from drive" << endl;
    	    return errStr;
    	}
    	retStr.append(rcvd.data());
    }
  } else if (command == "new") {
    // check to make sure directory is in driveArgs
    auto it = driveArgs.find("filename");
    if (it == driveArgs.end()) {
      cerr << "Missing directory in drive argument to new" << endl;
      return errStr;
    }
    // construct the packet to send to the drive
    packArgs.push_back(driveArgs["filename"]);
    packArgs.push_back(driveArgs["type"]);

    string newStr = serializedStringWithCommandArgs("NEW", packArgs);
    char newMsg[newStr.size()];
    newStr.copy(newMsg, newStr.size());
    do_send(&sock, newMsg, sizeof(newMsg));

    // wait for response
    rcvd = receivePacket(sock);
    // check to make sure +OK status code
    if (rcvd.status_code() != "+OK") {
      cerr << "Error receiving NEW response from drive" << endl;
      return errStr;
    }
    // add the name of the new file to retStr
    retStr.append(driveArgs["filename"]);
  } else if (command == "rename" || command == "move") {
    // check to make sure we have oldName and newName in drive args
    auto it = driveArgs.find("oldname");
    if (it == driveArgs.end()) {
      cerr << "Missing oldName in drive argument to rename/move" << endl;
      return errStr;
    }
    it = driveArgs.find("newname");
    if (it == driveArgs.end()) {
      cerr << "Missing newName in drive argument to rename/move" << endl;
      return errStr;
    }

    // construct the packet to send to the drive
    packArgs.push_back(driveArgs["oldname"]);
    packArgs.push_back(driveArgs["newname"]);
    packArgs.push_back(driveArgs["type"]);

    string rnameStr = serializedStringWithCommandArgs("RENAME", packArgs);
    char rnameMsg[rnameStr.size()];
    rnameStr.copy(rnameMsg, rnameStr.size());
    do_send(&sock, rnameMsg, sizeof(rnameMsg));

    // wait for response
    rcvd = receivePacket(sock);
    // check to make sure +OK status code
    if (rcvd.status_code() != "+OK") {
      cerr << "Error receiving RENAME/MOVE response from drive" << endl;
      return errStr;
    }
    // add name of the renamed file
    retStr.append(driveArgs["newname"]);
  } else if (command == "upload")
  {
    // check to make sure filename and fileData is in the drive args
    auto it = driveArgs.find("filename");
    if (it == driveArgs.end())
    {
      cerr << "Missing filename in drive argument to upload" << endl;
      return errStr;
    }
    // it = driveArgs.find("fileData");
    // if (it == driveArgs.end()) {
    //   cerr << "Missing fileData in drive argument to upload" << endl;
    //   return errStr;
    // }
    to_send.set_data("");
    // for(int i = 0; i < data_sz; i++)
    // {
    //   to_send.set_data();
    // }
    
    // construct the packet to send to the drive
    to_send.set_command("UPLOAD");
    to_send.add_arg(driveArgs["username"]);
    to_send.add_arg(driveArgs["filename"]);
    to_send.add_arg(driveArgs["type"]);
    to_send.add_arg(driveArgs["num_packets"]);
    cout << "MIDDLE SERVER COMMS" << "\n";

    cout << driveArgs["username"] << "\n";
    cout << driveArgs["filename"] << "\n";
    cout << driveArgs["type"] << "\n";
    cout << driveArgs["num_packets"] << "\n";


    to_send.set_data((char*)file_packet_data);
    //string str(reinterpret_cast<const char *> (file_packet_data), sizeof (file_packet_data) / sizeof (file_packet_data[0]));
    //cout << "STR: " << str << endl;
//    to_send.set_drivedata(0, (char*)file_packet_data, data_sz);
//    cout << "DATA: " << to_send.data();
    //cout << str << "\n";
    string out;
//    to_send.Serialize
    if (!to_send.SerializeToString(&out)) {
      cerr << "Could not serialize packet." << endl;
      exit(1);
    }
    string pre = generate_prefix(out.size());
    out = pre + out;
    cout << "Out:  "<< out << endl;
    cout << "Out size: " << out.size() << endl;
    char response[out.size()];
    out.copy(response, out.size());
    do_send(&sock, response, sizeof(response));

    //if(driveArgs["curr_n"].compare(driveArgs["num_packets"]) == 0){

		// wait for response
		rcvd = receivePacket(sock);
		// check to make sure +OK
		if (rcvd.status_code() != "+OK") {
		  cerr << "Error receiving UPLOAD response from drive" << endl;
		  return errStr;
		}
		// add the name of the uploaded file
		retStr.append(driveArgs["filename"]);
    //}
    //else{
//    	retStr.append(driveArgs["filename"] + " " + driveArgs["curr_n"]);
   // }
  } else if (command == "delete") {
    // check to make sure filename is in drive args
    auto it = driveArgs.find("filename");

    if (it == driveArgs.end()) {
      cerr << "Missing filename in drive argument to delete" << endl;
      cout <<"A";
      return errStr;
    }
    cout <<"B";

    // construct the packet to send
    packArgs.push_back(driveArgs["filename"]);
    packArgs.push_back(driveArgs["type"]);
    string delStr = serializedStringWithCommandArgs("DELETE", packArgs);
    char delMsg[delStr.size()];
    delStr.copy(delMsg, delStr.size());
    do_send(&sock, delMsg, sizeof(delMsg));
    cout <<"C";

    // wait for response
    rcvd = receivePacket(sock);
    if (rcvd.status_code() != "+OK") {
      cerr << "Error receiving DELETE response from drive" << endl;
      cout <<"D";

      return errStr;
    }
    // return the name of the deleted file
    retStr.append(driveArgs["filename"]);
  } else {
    // unknown command, return the error string
      cout <<"E";

    cerr << "Unknown command to drive" << endl;
    return errStr;
  }

  // send the QUIT command to the server, then return the retStr we've built
  string quitStr = serializedStringWithCommandArgs("QUIT", vector<string>());
  char quitMsg[quitStr.size()];
  quitStr.copy(quitMsg, quitStr.size());
  do_send(&sock, quitMsg, sizeof(quitMsg));
  cout <<"F";

  // receive the last OK
  rcvd = receivePacket(sock);
  if (rcvd.status_code() != "+OK") {
    cerr << "Error receiving QUIT response from drive" << endl;
    cout <<"G";

    return errStr;
  }
  // return the retStr
  cout <<"H";

  return retStr;
}

vector<string> getMiddleServerAddresses(int sock) {
  /* Input is an open (and connected) socket to a random middle load balancer.
   * Will return a vector of strings in IP:Port format that represent the
   * address to use for POP3, SMTP, Drive, and Authentication.
   * Returned vector will have the addresses in this order:
   * [0]: POP3
   * [1]: SMTP
   * [2]: Drive
   * [3]: Admin
   * In the event of ANY error, an empty vector is returned.
   * If a group of servers is all down it will have *DOWN in its place:
   * IF POP or SMTP are down the returned vector will have POPDOWN and SMTPDOWN
   * in 0,1 respectively. If Drive is down DRIVEDOWN will be in 2. Finally, if
   * Admin is down ADMINDOWN will be in 3 (Obviously,
   * if all services are down the vector will be:
   * "POPDOWN","SMTPDOWN","DRIVEDOWN","ADMINDOWN")
   */
  // create empty vector to return in the event of an error
  vector<string> empty;

  // generate the packet to send
  Packet middle;
  middle.set_command("MIDDLE");
  string out;
  if (!middle.SerializeToString(&out)) {
    cerr << "Could not serialize packet" << endl;
    return empty;
  }
  string pre = generate_prefix(out.size());
  out = pre + out;
  char msg[out.size()];
  out.copy(msg, out.size());
  // send out of the socket
  do_send(&sock, msg, out.size());

  // wait for response from middle load balancer
  int n = 0;
  char buf[BUF_SIZE];

  n = do_recv(&sock, buf, 10);
  if (n < 0) {
    cerr << "Could not receive" << endl;
    return empty;
  }

  Packet p;
  if (!p.ParseFromString(buf)) {
    cerr << "Could not parse received string" << endl;
    return empty;
  }

  // check that command is MIDSRV
  string command = p.command();
  transform(command.begin(), command.end(), command.begin(), ::tolower);
  if (command != "midsrv") {
    cerr << "Unknown command " << command << endl;
    return empty;
  }
  // add addresses and return
  vector<string> addresses;
  for (int i = 0; i < p.arg_size(); i++)
   addresses.push_back(p.arg(i));

  return addresses;
}

map<char, map<string, bool>> parseDataField(string data) {
  data = data.substr(1, data.length()-2);
  map<char, map<string, bool>> status;
  size_t lastComma = 0;
  size_t nextComma = data.find_first_of(",", lastComma);

  while (nextComma != string::npos) {
    string entry = data.substr(lastComma, nextComma-lastComma);
    string addrType(entry.substr(0, entry.find("=")));
    char type = addrType.at(0);
    string addr(addrType.substr(1));
    string statusS(entry.substr(entry.find("=")+1));
    status[type][addr] = (statusS == "true");
    lastComma = nextComma + 1;
    nextComma = data.find_first_of(",", lastComma);
  }
  string entry = data.substr(lastComma);
  string addrType(entry.substr(0, entry.find("=")));
  char type = addrType.at(0);
  string addr(addrType.substr(1));
  string statusS(entry.substr(entry.find("=")+1));
  status[type][addr] = (statusS == "true");

  return status;
}

map<char, map<string, bool>> getServerStatus(int sock) {
  /* Input is open and connected socket to the primary master address (TCP).
   * Output will be a map from server address (IP:Port form) to bool. True for
   * up, false for down
   */
  map<char, map<string, bool>> status;
  Packet admin;
  admin.set_command("ADMINUP");
  string out;
  if (!admin.SerializeToString(&out)) {
    cerr << "Could not serialize packet" << endl;
    return status;
  }
  string pre = generate_prefix(out.size());
  out = pre + out;
  char msg[out.size()];
  out.copy(msg, out.size());
  do_send(&sock, msg, out.size());

  // wait for response from master
  char buf[BUF_SIZE];
  int n = do_recv(&sock, buf, 10);

  if (n < 0) {
    cerr << "Could not receive" << endl;
    return status;
  }
  buf[n] = 0;
  cout<<"buf: "<<buf<<endl;
  Packet r;
  if (!r.ParseFromString(buf)) {
    cerr << "Could not parse received string" << endl;
    return status;
  }
  // check that command is SERVSTAT 
  string command = r.command();
  cout<<"command: "<<command<<endl;
  cout<<"status code: "<<r.status_code()<<endl;
  transform(command.begin(), command.end(), command.begin(), ::tolower);
  if (command != "servstat" || r.status_code() == "-ERR") {
    cerr << "Unknown command " << command << endl;
    return status;
  }

  // now, we parse the data field in the packet, which has the encoded map
  status = parseDataField(r.data());
  // return status
  return status;
}

map<char, map<string, bool>> sendAdminKillRestartMessage(int sock, Packet p) {
  map<char, map<string, bool>> status;
  string out;
  if (!p.SerializeToString(&out)) {
    cerr << "couldn't serialize to string" << endl;
    return status;
  }
  string pre = generate_prefix(out.size());
  out = pre + out;
  char msg[out.size()];
  out.copy(msg, out.size());
  do_send(&sock, msg, out.size());

  // wait for response from admin server
  char buf[BUF_SIZE];
  int n = do_recv(&sock, buf, -1);
  if (n < 0) {
    cerr << "admin server read error" << endl;
    return status;
  }
  buf[n] = 0;

  Packet r;
  if (!r.ParseFromString(buf)) {
    cerr << "could not parse string" << endl;
    return status;
  }

  string command = r.command();
  if (command != "SERVSTAT" || r.status_code() == "-ERR") {
    // bad command response, send back empty status
    return status;
  }

  status = parseDataField(r.data());
  return status;
}

/* Input is an open and connected socket to admin and string of the address to
 * kill. Output is the current server status following the killing of addrToKill
 * An empty map<char, map<string, bool>> is returned in any error
 */
map<char, map<string, bool>> killServer(int sock, string addrToKill) {
  Packet p;
  p.set_command("KILLSERV");
  p.add_arg(addrToKill);
  return sendAdminKillRestartMessage(sock, p);
}

/* Input is an open and connected socket to admin and string of the address to
 * restart. Output is the current server status following the killing of
 * addrToRestart. An empty map<char, map<string, bool>> is returned in any error
 */
map<char, map<string, bool>> restartServer(int sock, string addrToRestart) {
  Packet p;
  p.set_command("RESTARTSERV");
  p.add_arg(addrToRestart);
  return sendAdminKillRestartMessage(sock, p);
}

/* Input is the address (IP:Port) of one key-value store (this function needs 
 * to be called for each kv-store you want to display to the admin user).
 * Additionally, the open and connected socket to the admin server to
 * communicate with.
 * Returns a json string of all row/col pairs from the specific key-value store.
 * In the event of any error, an empty string is returned.
 */
string getRowColsofKV(int sock, string kvAddr) {
  // create blank string in case of errors
  string rowCols;
  // create a packet to send to the KV address
  Packet to_send;
  to_send.set_command("GETALL");
  to_send.add_arg(kvAddr);
  // send the packet
  string out;
  if (!to_send.SerializeToString(&out)) {
    cerr << "couldn't serialize to string" << endl;
    return rowCols;
  }
  string pre = generate_prefix(out.size());
  out = pre + out;
  char msg[out.size()];
  out.copy(msg, out.size());
  do_send(&sock, msg, out.size());

  // wait for response from admin server
  char buf[BUF_SIZE];
  int n = do_recv(&sock, buf, -1);
  if (n < 0) {
    cerr << "admin server read error" << endl;
    return rowCols;
  }
  buf[n] = 0;

  Packet r;
  if (!r.ParseFromString(buf)) {
    cerr << "could not parse string" << endl;
    return rowCols;
  }

  if (r.status_code() != "200") {
    // bad command response, send back empty string 
    return rowCols;
  }

  // simply return the json string that is in the data field of the packet
  return r.data();
}

/* Input is an open and connected socket to the admin server. The row and column
 * to get the data for, and the address (IP:Port) of the key-value store that
 * this row and column is stored on.
 * Returns the data that is stored at this row,col in the kv-store. In the event
 * of any error, a blank string is returned.
 */
string getRowColVal(int sock, string row, string column, string kvAddr) {
  // create blank string in case of errors
  string data;
  // create a packet to send to the KV address
  Packet to_send;
  to_send.set_command("VIEW");
  to_send.add_arg(row);
  to_send.add_arg(column);
  to_send.set_data(kvAddr);
  // send the packet
  string out;
  if (!to_send.SerializeToString(&out)) {
    cerr << "couldn't serialize to string" << endl;
    return data;
  }
  string pre = generate_prefix(out.size());
  out = pre + out;
  char msg[out.size()];
  out.copy(msg, out.size());
  do_send(&sock, msg, out.size());

  // wait for response from admin server
  char buf[BUF_SIZE];
  int n = do_recv(&sock, buf, -1);
  if (n < 0) {
    cerr << "admin server read error" << endl;
    return data;
  }
  buf[n] = 0;

  Packet r;
  if (!r.ParseFromString(buf)) {
    cerr << "could not parse string" << endl;
    return data;
  }

  if (r.status_code() != "200") {
    // bad command response, send back empty string 
    return data;
  }

  // simply return the string that is in the data field of the packet
  return r.data();
}


/*int main(int argc, char *argv[]) {
  int sock;
  if ( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("sock");
    exit(1);
  }

  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(6001);
  inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));

  if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    perror("connect");
    exit(1);
  }
  // receive initial welcome message
  Packet r = receivePacket(sock);

  string command("DISPLAY");
  string username("carter");
  map<string, string> driveArgs;
  driveArgs["username"] = username;

  string response = driveTransaction(sock, command, driveArgs);

  return 0;
}*/

/*
 * Testing code, not needed for the actual code to be compiled with the http
 * (frontend server)
int main(int argc, char *argv[]) {
  int sock;
  if ( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("sock");
    exit(1);
  }

  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8000);
  inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));

  if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    perror("connect");
    exit(1);
  }
  Packet r = receivePacket(sock);

  map<string, string> header;
  header["User-Agent"] = "Mozilla";
  header["MIME-Version"] = "1.0";
  header["Content-Type"] = "text/mail";
  header["Content-Transfer-Encoding"] = "i don't know what this is";
  header["Content-Language"] = "C++";

  string from("carter@penncloud");
  vector<string> toVec;
  toVec.push_back("varad@penncloud");
  toVec.push_back("alice@penncloud");
  string subj("project");
  string msg("i hope we're done soon");

  sendmail(sock, header, from, toVec, subj, msg);

  return 0;
}
*/
