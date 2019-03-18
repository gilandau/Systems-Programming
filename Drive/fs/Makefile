TARGETS = frontend

all: $(TARGETS)

frontend: frontend.cc ../utility.cc  middleServerComms.cc ../DATA_PACKET.pb.cc 
	g++ $^ -lpthread -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto  -lprotobuf -g -o $@

pack:
	rm -f submit-fs.zip
	zip -r submit-fs.zip *.cc Makefile

clean::
	rm -fv $(TARGETS) *~

realclean:: clean
	rm -fv cis505-hw2.zip
