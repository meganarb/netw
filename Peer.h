#ifndef PEER_H
#define PEER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <chrono>
#include <pthread.h>

class Peer {

public:

    struct peerInfo {
        Peer* me;
        int id;
        std::string host;
        int port;
        std::string bitfield;
        int sock;
    };

    std::fstream log;
    pthread_mutex_t logLock{};

    int id;
    int port;
    std::string bitField;
    std::vector<peerInfo*> preferredPeers;
    peerInfo* optUnchoked;

    int numPreferred;
    int unchokingInt;
    int optimisticInt;
    std::string fileName;
    int fileSize;
    int pieceSize;

    Peer();
    void initBitfield(int bit);
    void setID(int ID);

    static std::string sendChoke();
    static std::string sendUnchoke();
    std::string sendInterested();
    std::string sendNotInterested();
    std::string sendHave(int index);
    std::string sendBitfield();
    std::string sendRequest(int index);
    std::string sendPiece(int index);

    std::string checkIfInterested(peerInfo* peer);
    std::string getPiece(int piece);

    // logging functions - logs into file log_peer_[peerID].log
    int logConnectTo(peerInfo* peer);
    int logConnectFrom(peerInfo* peer);
    int logPreferred(peerInfo* peer);
    int logOptUnchoke(peerInfo* peer);
    int logUnchoke(peerInfo* peer);
    int logChoke(peerInfo* peer);
    int logHave(peerInfo* peer, int index);
    int logInterested(peerInfo* peer);
    int logNotInterested(peerInfo* peer);
    int logDownload(peerInfo* peer, int index);
    int logComplete(peerInfo* peer);

    std::string getPreferred();
    int getNumPieces();
};

#endif //PEER_H