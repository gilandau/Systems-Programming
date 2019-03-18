TARGETS = smtp_server pop3_server dummy master drive middleLoad kvs drive_test admin auth

all: $(TARGETS)

util: utility.cc utility.h
	g++ $^ -lpthread -lprotobuf -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -g -o $@

# smtp_test: smtp_test.cc utility.cc DATA_PACKET.pb.cc
# 	g++ $^ -lpthread -lresolv -lprotobuf -g -o $@
drive_test: drive_test.cc DATA_PACKET.pb.cc utility.cc
	g++ $^ -lpthread -lresolv -lprotobuf -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -g -o $@

smtp_server: smtp_server.cc DATA_PACKET.pb.cc utility.cc smtp_forward_client.cc
	g++ $^ -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -lpthread -lprotobuf -lresolv -g -o $@

smtp_client: smtp_forward_client.cc smtp_forward_client.h utility.cc
	g++ $^ -lresolv -g -o $@

pop3_server: pop3_server.cc utility.cc DATA_PACKET.pb.cc
	g++ $^ -lprotobuf -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -lpthread -g -o $@

dummy: dummy.cc DATA_PACKET.pb.cc utility.cc
	g++ $^ -lprotobuf -lpthread -Wall -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -g -o $@

master: master.cc heartbeat.cc masterComms.cc DATA_PACKET.pb.cc utility.cc
	g++ $^ -lprotobuf -lpthread -Wall -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -g -o $@

middleLoad: middleLoad.cc DATA_PACKET.pb.cc utility.cc
	g++ $^ -lprotobuf -lpthread -Wall -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -g -o $@
	
auth: authentication_server.cc utility.cc DATA_PACKET.pb.cc
	g++ $^ -lresolv -lpthread -lprotobuf -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -g -o $@
	
kvs: keyvalueserver.cc utility.cc DATA_PACKET.pb.cc replication_packet.pb.cc ordering_packet.pb.cc
	g++ $^ -lcrypto -lresolv -lpthread -lprotobuf -g -o $@

	g++ $^ -lresolv -lpthread -lprotobuf -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -g -o $@

drive: drive_server.cc utility.cc DATA_PACKET.pb.cc
	g++ $^ -lpthread -lresolv -lprotobuf -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -g -o $@

admin: admin.cc utility.cc DATA_PACKET.pb.cc
	g++ $^ -lpthread -lprotobuf -lpthread -lresolv -lprotobuf -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -g -o $@

pack:
	rm -f submit-hw2.zip
	zip -r submit-hw2.zip *.cc README Makefile

clean::
	rm -fv $(TARGETS) *~

realclean:: clean
	rm -fv cis505-hw2.zip
