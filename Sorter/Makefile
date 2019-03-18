TARGETS = makeinput mysort

all: $(TARGETS)

makeinput: makeinput.cc
	g++ $< -o $@

makeinput_100: makeinput
	./makeinput 100 > test.txt

mysort: mysort.cc
	g++ -pthread -std=c++11  $< -o $@
	
pack:
	rm -f submit-hw1.zip
	zip -r submit-hw1.zip *.cc README Makefile graph*.pdf graph*.jpg description*.txt

clean::
	rm -fv $(TARGETS)
