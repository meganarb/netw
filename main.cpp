#include <fstream>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include "Peer.h"

void* connectTo(void* arg) {

    auto* p = (Peer::peerInfo*)arg;
    int sock = p->sock;

    // sending handshake
    std::string temp = "P2PFILESHARINGPROJ0000000000" + std::to_string(p->me->id);
    send(sock, temp.c_str(), temp.size(), 0);

    char buffer[1024] = { 0 };
    
    // receive handshake
    recv(sock, buffer, 32, 0);
    std::cout << "message from client: " << buffer << std::endl;
    // right here, check for correct handshake message
    std::string handshake = buffer;
    if (handshake != "P2PFILESHARINGPROJ0000000000" + std::to_string(p->id)) {
        std::cout << "incorrect connection";
        return nullptr;
    } 
    
    memset(buffer, '\0', sizeof(buffer));
    
    p->me->logConnectTo(p);

    // sending bitfield
    std::string msg = p->me->sendBitfield();
    std::cout << "sending: " << msg << std::endl;
    send(sock, p->me->intToBytes(msg.size()).data(), 4, 0);
    send(sock, msg.c_str(), msg.size(), 0);
    
    bool cont = true;
    std::string message;
    while (cont) {
        recv(sock, buffer, 4, 0);
        // get size of message
        std::cout << "size " << buffer << std::endl;
        std::string b = buffer;
        int size = p->me->bytesToInt(b);
        std::cout << "size of message: " << size << std::endl;
        recv(sock, buffer, size, 0); // get the rest of the message
        std::cout << "message: " << buffer << std::endl;
        
        switch (buffer[0]) {
        case '0':
            std::cout << "choke" << std::endl;
            break;
        case '1':
            std::cout << "unchoke" << std::endl;
            break;
        case '2':
        cont = false;
            std::cout << "interested" << std::endl;
            p->interested = true;
            break;
        case '3':
        cont = false;
            std::cout << "not interested" << std::endl;
            p->interested = false;
            break;
        case '4':
            std::cout << "have" << std::endl;
            break;
        case '5':
        {
            std::cout << "bitfield" << std::endl;
            std::string bfield = buffer;
            bfield = bfield.substr(1, bfield.size()-1);
            p->bitfield = bfield;
            std::cout << p->id << "'s bitfield is: " << p->bitfield << std::endl;
            message = p->me->checkIfInterested(p);
            send(sock, p->me->intToBytes(message.size()).data(), 4, 0);
            send(sock, message.c_str(), message.size(), 0);
            break;
        }
        case '6':
            std::cout << "request" << std::endl;
            break;
        case '7':
            std::cout << "piece" << std::endl;
            break;
        default:
            std::cout << "not a valid message type" << std::endl;
        }
      
        memset(buffer, '\0', sizeof(buffer));
    }

    return nullptr;
}

void* connectFrom(void* arg) {
    auto* p = (Peer::peerInfo*)arg;
    int clientSocket = p->sock;

    char buffer[1024] = { 0 };
    // receive handshake
    recv(clientSocket, buffer, 32, 0);
    std::cout << "Message from client: " << buffer << std::endl;
    std::string handshake = buffer;
    if (handshake != "P2PFILESHARINGPROJ0000000000" + std::to_string(p->id)) {
        std::cout << "incorrect connection";
        return nullptr;
    } 
    memset(buffer, '\0', sizeof(buffer));

    // send handshake
    std::string temp = "P2PFILESHARINGPROJ0000000000" + std::to_string(p->me->id);
    send(clientSocket, temp.c_str(), temp.size(), 0);
    p->me->logConnectFrom(p);
    
    // sending bitfield
    std::string msg = p->me->sendBitfield();
    std::cout << "sending: " << msg << std::endl;
    send(clientSocket, p->me->intToBytes(msg.size()).data(), 4, 0);
    send(clientSocket, msg.c_str(), msg.size(), 0);
    
    bool cont = true;
    std::string message;
    while (cont) {
        recv(clientSocket, buffer, 4, 0);
        // get size of message
        std::cout << "size " << buffer << std::endl;
        std::string b = buffer;
        int size = p->me->bytesToInt(b);
        std::cout << "size of message: " << size << std::endl;
        recv(clientSocket, buffer, size, 0); // get the rest of the message
        std::cout << "message: " << buffer << std::endl;
        
        switch (buffer[0]) {
        case '0':
            std::cout << "choke" << std::endl;
            break;
        case '1':
            std::cout << "unchoke" << std::endl;
            break;
            // send request message for a piece that i do not have, and i have not already requested
        case '2':
            std::cout << "interested" << std::endl;
            cont = false;
            p->interested = true;
            break;
            // record that they are interested, so they are an option for peer
            break;
        case '3':
            std::cout << "not interested" << std::endl;
            cont = false;
            p->interested = false;
            // record that they are not interested
            break;
        case '4':
            std::cout << "have" << std::endl;
            // update bitfield, send interested/not interested
        case '5':
        {
            std::cout << "bitfield" << std::endl;
            std::string bfield = buffer;
            bfield = bfield.substr(1, bfield.size()-1);
            p->bitfield = bfield;
            std::cout << p->id << "'s bitfield is: " << p->bitfield << std::endl;
            message = p->me->checkIfInterested(p).c_str();
            send(clientSocket, p->me->intToBytes(message.size()).data(), 4, 0);
            send(clientSocket, message.c_str(), message.size(), 0);
            break;
        }
        case '6':
            std::cout << "request" << std::endl;
            break;
            // send piece message containing the actual piece
        case '7':
            std::cout << "piece" << std::endl;
            break;
            // download the piece, send another request message
            // check to see if i should send not interested now that i have this piece
        default:
            std::cout << "not a valid message type" << std::endl;
        }
        
        memset(buffer, '\0', sizeof(buffer));
    }

    return nullptr;
}

int main(int argc, char *argv[]) {

    Peer p;
    std::vector<pthread_t> threads;
    std::vector<Peer::peerInfo*> peers;

    if (argc != 2) return -1;
    try {
        p.id = std::stoi(argv[1]);
    } catch (...) {return -2;}

    std::ifstream config = std::ifstream("Common.cfg");
    if (config.is_open()) {

        std::string buf;

        std::getline(config, buf, ' ');
        std::getline(config, buf);
        p.numPreferred = std::stoi(buf);

        std::getline(config, buf, ' ');
        std::getline(config, buf);
	p.unchokingInt = std::stoi(buf);

        std::getline(config, buf, ' ');
        std::getline(config, buf);
	p.optimisticInt = std::stoi(buf);

        std::getline(config, buf, ' ');
        std::getline(config, buf);
        p.fileName = buf;

        std::getline(config, buf, ' ');
        std::getline(config, buf);
        p.fileSize = std::stoi(buf);

        std::getline(config, buf, ' ');
        std::getline(config, buf);
        p.pieceSize = std::stoi(buf);

        config.close();
    } else return -3;

    config = std::ifstream("PeerInfo.cfg");
    if (config.is_open()) {
        while (!config.eof()) {
            std::string buf;
            std::getline(config, buf, ' '); // peer id

            if (stoi(buf) == p.id) {
                std::getline(config, buf, ' '); // host
                std::getline(config, buf, ' '); // port
                p.port = std::stoi(buf);
                std::getline(config, buf); // owns file (1 = yes, 0 = no)
                if (std::stoi(buf) == 1) p.initBitfield(1);
                else p.initBitfield(0);
                break;
            }
            int id = std::stoi(buf);
            std::getline(config, buf, ' '); // host
            std::string host = buf;
            std::getline(config, buf, ' '); // port
            int port = std::stoi(buf);
            std::getline(config, buf); // owns file (1 = yes, 0 = no)

            pthread_t thr;
            auto* temp = new Peer::peerInfo{&p, id, host, port};
            peers.push_back(temp);
            
            int sock = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in peerAddress;
            peerAddress.sin_family = AF_INET;
            peerAddress.sin_port = htons(port);
            inet_pton(AF_INET, host.c_str(), &peerAddress.sin_addr);

            // sending connection request
            connect(sock, (struct sockaddr*)&peerAddress, sizeof(peerAddress));
            temp->sock = sock;
            
            pthread_create(&thr, nullptr, connectTo, temp);
            threads.push_back(thr);
        }
    } else return -4;

    int count = 0;
    while (!config.eof()) {
        std::string buf = "";
        std::getline(config, buf, ' '); // peer id
	if (buf.empty()) break;
	count++;
	int id;
        id = std::stoi(buf);
        std::getline(config, buf, ' '); // host
        std::string host = buf;
        std::getline(config, buf, ' '); // port
	int port;
        port = std::stoi(buf);
        std::getline(config, buf); // owns file (1 = yes, 0 = no)
        
        auto* temp = new Peer::peerInfo{&p, id, host, port};
        peers.push_back(temp);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in peerAddress;
    peerAddress.sin_family = AF_INET;
    peerAddress.sin_port = htons(p.port);
    peerAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    bind(sock, (struct sockaddr*)&peerAddress, sizeof(peerAddress));

    int clientSocket;
    // listening to the assigned socket
    listen(sock, count);
    
    for (int i = peers.size() - count; i < peers.size(); i++) {
        clientSocket = accept(sock, nullptr, nullptr);
        pthread_t thr;
        peers.at(i)->sock = clientSocket;
        pthread_create(&thr, nullptr, connectFrom, peers.at(i));
        threads.push_back(thr);
    }

    for (auto thread : threads) {
        pthread_join(thread, nullptr);
    }
    
    close(sock);
    for (auto peer : peers) close(peer->sock);

    for (auto* peer : peers) {
        delete peer;
    }

    return 0;
}

