peerProgram: main.o Peer.o
	g++ -std=c++2a -o peerProgram main.o Peer.o -lpthread

%.o: %.cpp
	g++ -std=c++2a -c $< -o $@ -lpthread
