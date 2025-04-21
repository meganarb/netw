#include <fstream>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include "Peer.h"


void* connectTo(void* arg) {
    auto* p = (Peer::peerInfo*)arg;
    std::cout << p->id << std::endl;

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in peerAddress;
    peerAddress.sin_family = AF_INET;
    peerAddress.sin_port = htons(p->port);
    peerAddress.sin_addr.s_addr = INADDR_ANY;

    // sending connection request
    connect(sock, (struct sockaddr*)&peerAddress, sizeof(peerAddress));

    // sending data
    std::string temp = "P2PFILESHARINGPROJ0000000000" + std::to_string(p->me->id);
    const char* message = temp.c_str();
    send(sock, message, strlen(message), 0);

    char buffer[1024] = { 0 };
    recv(sock, buffer, sizeof(buffer), 0);
    std::cout << "message from client: " << buffer << std::endl;
    memset(buffer, '\0', sizeof(buffer));
    p->me->logConnectTo(p);

    p->sock = sock;

    message = p->me->sendBitfield().c_str();
    if (strlen(message) > 1) send(sock, message, strlen(message), 0);
    else send(sock, "00000", strlen("00000"), 0);
    recv(sock, buffer, sizeof(buffer), 0);

    std::cout << "message from peer: " << buffer << std::endl;
    memset(buffer, '\0', sizeof(buffer));

    if (buffer[5] == '5') {
        std::string bfield = buffer;
        bfield = bfield.substr(6, bfield.size()-5);
        p->bitfield = bfield;
        message = p->me->checkIfInterested(p).c_str();
    } else {
        p->bitfield = "";
        for (int i = 0; i < ceil(1.0 * p->me->fileSize/p->me->pieceSize); i++) p->bitfield += '0';
        message = p->me->sendNotInterested(sock).c_str();
    }

    send(sock, message, strlen(message), 0);

    std::cout << p->id << "'s bitfield is: " << p->bitfield << std::endl;

    close(sock);

    return nullptr;
}

void* connectFrom(void* arg) {
    auto* p = (Peer::peerInfo*)arg;

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in peerAddress;
    peerAddress.sin_family = AF_INET;
    peerAddress.sin_port = htons(p->port);
    peerAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    bind(sock, (struct sockaddr*)&peerAddress, sizeof(peerAddress));

    // listening to the assigned socket

    int clientSocket;
    // accepting connection request
    listen(sock, 1);
    clientSocket = accept(sock, nullptr, nullptr);

    // recieving data
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << "Message from client: " << buffer << std::endl;
    memset(buffer, '\0', sizeof(buffer));

    std::string temp = "P2PFILESHARINGPROJ0000000000" + std::to_string(p->me->id);
    const char* message = temp.c_str();
    send(clientSocket, message, strlen(message), 0);
    p->me->logConnectFrom(p);

    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::string str = p->me->sendBitfield();
    if (strlen(message) > 1) send(clientSocket, str.c_str(), strlen(str.c_str()), 0);
    else send(clientSocket, "00000", strlen("00000"), 0);

    std::cout << "message from peer: " << buffer << std::endl;
    memset(buffer, '\0', sizeof(buffer));

    if (buffer[5] == '5') {
        std::string bfield = buffer;
        bfield = bfield.substr(6, bfield.size()-5);
        p->bitfield = bfield;
        message = p->me->checkIfInterested(p).c_str();
    } else {
        p->bitfield = "";
        for (int i = 0; i < ceil(1.0 * p->me->fileSize/p->me->pieceSize); i++) p->bitfield += '0';
        message = p->me->sendNotInterested(sock).c_str();
    }

    std::cout << p->id << "'s bitfield is: " << p->bitfield << std::endl;

    // closing the socket.
    close(sock);

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
            pthread_create(&thr, nullptr, connectTo, temp);
            threads.push_back(thr);
        }
    } else return -4;

    std::cout << "my bitfield is " << p.bitField << std::endl;

    while (!config.eof()) {
        std::string buf = "";
        std::getline(config, buf, ' '); // peer id
	    if (buf.empty()) break;
	    int id;
        id = std::stoi(buf);
        std::getline(config, buf, ' '); // host
        std::string host = buf;
        std::getline(config, buf, ' '); // port
	    int port;
        port = std::stoi(buf);
        std::getline(config, buf); // owns file (1 = yes, 0 = no)

        pthread_t thr;
        auto* temp = new Peer::peerInfo{&p, id, host, port};
        peers.push_back(temp);
	    pthread_create(&thr, nullptr, connectFrom, temp);
        threads.push_back(thr);
    }

    for (auto thread : threads) {
        pthread_join(thread, nullptr);
    }

    for (auto* peer : peers) {
        delete peer;
    }

    return 0;
}