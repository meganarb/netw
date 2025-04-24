#ifndef PEER_H
#define PEER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <chrono>
#include <pthread.h>
#include <algorithm>
#include <cstdint>
#include <random>
#include <bits/stdc++.h>

class Peer {

public:

    struct peerInfo {
        Peer* me;
        int id;
        std::string host;
        int port;
        bool interested;
        bool unchoked;
        bool meUnchoked;
        int piecesProvided;
        std::string bitfield;
        int sock;
    };
    
    bool complete;
    bool unchoke;
    bool optUnchoke;
    pthread_mutex_t unchokeLock;
    pthread_mutex_t optUnchokeLock;

    std::ofstream log;
    pthread_mutex_t logLock{};
    std::string pieceFolder;

    int id;
    int host;
    int port;
    std::string bitField;
    bool fullBitfield;
    
    std::vector<peerInfo*> allPeers;
    std::vector<peerInfo*> preferredPeers;
    peerInfo* optUnchoked;
    
    std::string intToBytes(int num);
    int bytesToInt(std::string num);

    int numPreferred;
    int unchokingInt;
    int optimisticInt;
    std::string fileName;
    int fileSize;
    int pieceSize;

    Peer();
    void initBitfield(int bit);
    void setID(int ID);
    
    int splitFile();
    
    void changeUnchoked();
    void changeOptimistic();

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
    int savePiece(int pieceNums, std::string buffer);

    // logging functions - logs into file log_peer_[peerID].log
    int logCommonCFG();
    int logPeerInfoCFG();
    int logConnectTo(peerInfo* peer);
    int logConnectFrom(peerInfo* peer);
    int logReceiveBitfield(peerInfo* peer);
    int logPreferred();
    int logOptUnchoke();
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
