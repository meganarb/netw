#include <fstream>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include "Peer.h"


void* unchokingTimer(void* arg) {
    auto* p = (Peer*)arg;
    int interval = p->unchokingInt;
    while (!p->complete) {
        sleep(interval);
        std::cout << "unchoking" << std::endl;
        pthread_mutex_lock(&p->unchokeLock);
        p->unchoke = true;
        p->changeUnchoked();
        p->logPreferred();
        pthread_mutex_unlock(&p->unchokeLock);
    }
    return nullptr;
}

void* optimisticTimer(void* arg) {
    sleep(3);
    auto* p = (Peer*)arg;
    p->optUnchoke = true;
    p->changeOptimistic();
    p->logOptUnchoke();
    int interval = p->optimisticInt;
    while (!p->complete) {
        sleep(interval);
        std::cout << "OPTunchoking" << std::endl;
        pthread_mutex_lock(&p->optUnchokeLock);
        p->optUnchoke = true;
        p->changeOptimistic();
        p->logOptUnchoke();
        pthread_mutex_unlock(&p->optUnchokeLock);
    }
    return nullptr;
}

void* connectTo(void* arg) {
    auto* p = (Peer::peerInfo*)arg;
    p->meUnchoked, p->unchoked = false;
    
    // sending handshake
    std::string temp = "P2PFILESHARINGPROJ0000000000" + std::to_string(p->me->id);
    send(p->sock, temp.c_str(), temp.size(), 0);

    char buffer[1024] = { 0 };
    
    // receive handshake
    recv(p->sock, buffer, 32, 0);
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
    send(p->sock, p->me->intToBytes(msg.size()).data(), 4, 0);
    send(p->sock, msg.c_str(), msg.size(), 0);
    
    bool cont = true;
    while (cont) {
        memset(buffer, '\0', sizeof(buffer));
        recv(p->sock, buffer, 4, 0);
        // get size of message
        int size = p->me->bytesToInt(buffer);
        memset(buffer, '\0', sizeof(buffer));
        std::cout << "size of message: " << size << std::endl;
        recv(p->sock, buffer, size, 0); // get the rest of the message
        std::cout << "message: " << buffer << std::endl;
        
        switch ((unsigned char)buffer[0]) {
        case '0':
        {
            std::cout << "choke" << std::endl;
            p->meUnchoked = false;
            p->me->logChoke(p);
            break;
        }
        case '1':
        {
            std::cout << "unchoke" << std::endl;
            p->meUnchoked = true;
            p->me->logUnchoke(p);
            break;
        }
        case '2':
        {
            std::cout << "interested" << std::endl;
            p->interested = true;
            p->me->logInterested(p);
            break;
        }
        case '3':
        {
            std::cout << "not interested" << std::endl;
            p->interested = false;
            p->me->logNotInterested(p);
            break;
        }
        case '4':
        {
            std::cout << "have" << std::endl;
            std::string piece = std::string(buffer + 1, 4);
            int num = p->me->bytesToInt(piece);
            p->bitfield[num] = 1;
            p->me->logHave(p, num);
            std::string message = p->me->checkIfInterested(p).c_str();
            send(p->sock, p->me->intToBytes(message.size()).data(), 4, 0);
            send(p->sock, message.c_str(), message.size(), 0);
            break;
        }
        case '5':
        {std::cout << "bitfield" << std::endl;
            std::string bfield = buffer;
            bfield = bfield.substr(1, bfield.size()-1);
            p->bitfield = bfield;
            p->me->logReceiveBitfield(p);
            std::string message = p->me->checkIfInterested(p);
            send(p->sock, p->me->intToBytes(message.size()).data(), 4, 0);
            send(p->sock, message.data(), message.size(), 0);
            break;
        }
        case '6':
        {
            std::cout << "request" << std::endl;
            // check if the requester is unchoked
            if (p->unchoked) {
                // send piece message containing the actual piece
                std::string piece = std::string(buffer + 1, 4);
                std::string message = p->me->sendPiece(p->me->bytesToInt(piece));
                send(p->sock, p->me->intToBytes(message.size()).data(), 4, 0);
                send(p->sock, message.data(), message.size(), 0);
            }
            break;
        }
        case '7':
        {
            std::cout << "piece" << std::endl;
            std::string piece = buffer;
            std::string pieceNum = piece.substr(0, 4);
            piece = piece.substr(4, piece.size()-4);
            p->me->savePiece(p->me->bytesToInt(pieceNum), piece);
            p->piecesProvided++;
            break;
        }
        default:
        {
            std::cout << "not a valid message type" << std::endl;
            sleep(10);
        }
        }
        
        pthread_mutex_lock(&p->me->unchokeLock);
        if (p->me->unchoke) {
            std::cout << 1 <<std::endl;
            p->me->unchoke = false;
            // change unchoked peers
        } pthread_mutex_unlock(&p->me->unchokeLock);
        
        pthread_mutex_lock(&p->me->optUnchokeLock);
        if (p->me->optUnchoke) {
        std::cout << 2<<std::endl;
            p->me->optUnchoke = false;
            // check if they were already unchoked
            if (p->me->optUnchoked != nullptr && !p->me->optUnchoked->unchoked) {
            std::cout << 3<<std::endl;
                p->me->optUnchoked->unchoked = true;
                std::string msg = p->me->sendUnchoke();
                // send unchoke
                send(p->sock, p->me->intToBytes(msg.size()).data(), 4, 0);
                send(p->sock, msg.data(), msg.size(), 0);
            }
        } pthread_mutex_unlock(&p->me->optUnchokeLock);
        std::cout << 4<<std::endl;
        if (p->meUnchoked) {
            // send request message
            std::cout << "  " << std::endl;
        }
      
    }

    return nullptr;
}

void* connectFrom(void* arg) {
    auto* p = (Peer::peerInfo*)arg;
    p->meUnchoked, p->unchoked = false;
    
    char buffer[1024] = { 0 };
    // receive handshake
    recv(p->sock, buffer, 32, 0);
    std::cout << "Message from client: " << buffer << std::endl;
    std::string handshake = buffer;
    if (handshake != "P2PFILESHARINGPROJ0000000000" + std::to_string(p->id)) {
        std::cout << "incorrect connection";
        return nullptr;
    } 
    memset(buffer, '\0', sizeof(buffer));

    // send handshake
    std::string temp = "P2PFILESHARINGPROJ0000000000" + std::to_string(p->me->id);
    send(p->sock, temp.c_str(), temp.size(), 0);
    p->me->logConnectFrom(p);
    
    // sending bitfield
    std::string msg = p->me->sendBitfield();
    std::cout << "sending: " << msg << std::endl;
    send(p->sock, p->me->intToBytes(msg.size()).data(), 4, 0);
    send(p->sock, msg.c_str(), msg.size(), 0);
    
    bool cont = true;
    while (cont) {
        memset(buffer, '\0', sizeof(buffer));
        recv(p->sock, buffer, 4, 0);
        // get size of message
        int size = p->me->bytesToInt(buffer);
        memset(buffer, '\0', sizeof(buffer));
        std::cout << "size of message: " << size << std::endl;
        recv(p->sock, buffer, size, 0); // get the rest of the message
        std::cout << "message: " << buffer << " from " << p->id << std::endl;
        
        switch (buffer[0]) {
        case '0':
        {
            std::cout << "choke" << std::endl;
            p->meUnchoked = false;
            break;
        }
        case '1':
        {
            std::cout << "unchoke" << std::endl;
            p->meUnchoked = true;
            break;
            // send request message for a piece that i do not have, and i have not already requested
        }
        case '2':
        {
            p->interested = true;
            p->me->logInterested(p);
            cont = false;
            break;
        }
        case '3':
        {
            p->interested = false;
            p->me->logNotInterested(p);            
            cont = false;
            break;
        }
        case '4':
        {
            std::cout << "have" << std::endl;
            std::string piece = std::string(buffer + 1, 4);
            int num = p->me->bytesToInt(piece);
            p->bitfield[num] = 1;
            p->me->logHave(p, num);
            std::string message = p->me->checkIfInterested(p).c_str();
            send(p->sock, p->me->intToBytes(message.size()).data(), 4, 0);
            send(p->sock, message.c_str(), message.size(), 0);
            break;
            // update bitfield, send interested/not interested
        }
        case '5':
        {
            std::string bfield = buffer;
            bfield = bfield.substr(1, bfield.size()-1);
            p->bitfield = bfield;
            p->me->logReceiveBitfield(p);
            std::string message = p->me->checkIfInterested(p).c_str();
            send(p->sock, p->me->intToBytes(message.size()).data(), 4, 0);
            send(p->sock, message.c_str(), message.size(), 0);
            break;
        }
        case '6':
        {
            std::cout << "request" << std::endl;
            // check if the requester is unchoked
            if (p->unchoked) {
                // send piece message containing the actual piece
                std::string piece = std::string(buffer + 1, 4);
                std::string message = p->me->sendPiece(p->me->bytesToInt(piece));
                send(p->sock, p->me->intToBytes(message.size()).data(), 4, 0);
                send(p->sock, message.data(), message.size(), 0);
            }
            break;
        }
        case '7':
        {
            std::cout << "piece" << std::endl;
            std::string piece = buffer;
            std::string pieceNum = piece.substr(0, 4);
            piece = piece.substr(4, piece.size()-4);
            p->me->savePiece(p->me->bytesToInt(pieceNum), piece);
            break;
            // download the piece, send another request message
            // check to see if i should send not interested now that i have this piece
        }
        default:
        {
            std::cout << "not a valid message type" << std::endl;
            cont = false;
        }
        }
        
        pthread_mutex_lock(&p->me->unchokeLock);
        if (p->me->unchoke) {
            p->me->unchoke = false;
            pthread_mutex_unlock(&p->me->unchokeLock);
            // change unchoked peers
        } else pthread_mutex_unlock(&p->me->unchokeLock);
        
        pthread_mutex_lock(&p->me->optUnchokeLock);
        if (p->me->optUnchoke) {
            p->me->optUnchoke = false;
            pthread_mutex_unlock(&p->me->optUnchokeLock);
            // change optimal unchoked peer
        } else pthread_mutex_unlock(&p->me->optUnchokeLock);
        
        if (p->meUnchoked) {
            // send request message
        }
        
    }

    return nullptr;
}




int main(int argc, char *argv[]) {

    Peer p;
    std::vector<pthread_t> threads;
    pthread_t unchokeThr;
    pthread_t optThr;

    if (argc != 2) return -1;
    try {
        p.setID(std::stoi(argv[1]));
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
        std::cout << p.fileSize << " " << p.pieceSize << std::endl;
        config.close();
    } else {std::cout << "file not open\n"; return -3;}
    p.logCommonCFG();

    config = std::ifstream("PeerInfo.cfg");
    if (config.is_open()) {
        while (!config.eof()) {
            std::string buf;
            std::getline(config, buf, ' '); // peer id

            if (stoi(buf) == p.id) {
                std::getline(config, buf, ' '); // host
                p.host = std::stoi(buf);
                std::getline(config, buf, ' '); // port
                p.port = std::stoi(buf);
                std::getline(config, buf); // owns file (1 = yes, 0 = no)
                if (std::stoi(buf) == 1) {
                    p.initBitfield(1);
                    p.splitFile();
                }
                else p.initBitfield(0);
                break;
            }
            int id = std::stoi(buf);
            std::getline(config, buf, ' '); // host
            std::string host = buf;
            std::getline(config, buf, ' '); // port
            int port = std::stoi(buf);
            std::getline(config, buf); // owns file (1 = yes, 0 = no)

            auto* temp = new Peer::peerInfo{&p, id, host, port, false, false, false, 0};
            p.allPeers.push_back(temp);
        }
        p.logPeerInfoCFG();
        
        
        for (auto* temp : p.allPeers) {
            pthread_t thr;
            int sock = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in peerAddress;
            peerAddress.sin_family = AF_INET;
            peerAddress.sin_port = htons(temp->port);
            inet_pton(AF_INET, temp->host.c_str(), &peerAddress.sin_addr);

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
        
        auto* temp = new Peer::peerInfo{&p, id, host, port, false, false, false, 0};
        p.allPeers.push_back(temp);
    }
    
    pthread_create(&unchokeThr, nullptr, unchokingTimer, &p);
    pthread_create(&optThr, nullptr, optimisticTimer, &p);

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
    
    for (int i = p.allPeers.size() - count; i < p.allPeers.size(); i++) {
        clientSocket = accept(sock, nullptr, nullptr);
        pthread_t thr;
        p.allPeers.at(i)->sock = clientSocket;

        pthread_create(&thr, nullptr, connectFrom, p.allPeers.at(i));
        threads.push_back(thr);
    }

    for (auto thread : threads) {
        pthread_join(thread, nullptr);
    }
    
    p.complete = true;
    pthread_join(unchokeThr, nullptr);
    pthread_join(optThr, nullptr);
    
    close(sock);
    for (auto peer : p.allPeers) close(peer->sock);

    for (auto* peer : p.allPeers) {
        delete peer;
    }

    return 0;
}

