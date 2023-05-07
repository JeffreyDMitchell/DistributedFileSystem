CXX = gcc
CXXFLAGS = -g -Wall -Wno-deprecated-declarations

all: clean dfs dfc

dfc: netutils.o com.o dfc.c
	$(CXX) $(CXXFLAGS) netutils.o com.o dfc.c -o dfc -lcrypto

dfs: netutils.o com.o dfs.c
	$(CXX) $(CXXFLAGS) netutils.o com.o dfs.c -o dfs -lcrypto

test: netutils.o com.o test.c
	$(CXX) $(CXXFLAGS) netutils.o com.o test.c -o test -lcrypto

netutils.o: netutils.c
	$(CXX) $(CXXFLAGS) -c netutils.c -lcrypto

com.o: com.c
	$(CXX) $(CXXFLAGS) -c com.c 

clean:
	rm -f dfs dfc test *.o