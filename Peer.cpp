#include "Peer.h"

#include <memory>

Peer::Peer() {
    id = 0;
    numPreferred = 0;
    unchokingInt = 0;
    optimisticInt = 0;
    fileName = "";
    fileSize = 0;
    pieceSize = 0;
    port = 0;
    optUnchoked = nullptr;
    pthread_mutex_init(&logLock, nullptr);
}

void Peer::setID(int ID) {
    id = ID;
    std::fstream temp("log_peer_" + std::to_string(ID) + ".log");
    log.open("log_peer_" + std::to_string(ID) + ".log", std::ofstream::out | std::ofstream::app);
    //std::string fileName = "log_peer_" + std::to_string(ID) + ".log";
    //log.open(fileName, std::ofstream::out | std::ofstream::app);
}

void Peer::initBitfield(int bit) {
    int chunks = ceil(1.0 * fileSize / pieceSize);
    bitField = "";
    for (int i = 0; i < chunks; i++) bitField += std::to_string(bit);
}

std::string Peer::intToBytes(int num) {
    std::string out(4, '\0');
    out[0] = (num >> 24) & 0xFF;
    out[1] = (num >> 16) & 0xFF;
    out[2] = (num >> 8) & 0xFF;
    out[3] = num & 0xFF;
    return out;
}

int Peer::bytesToInt(std::string num) {
    int out = ((unsigned char)num[0] << 24) |
              ((unsigned char)num[1] << 16) |
              ((unsigned char)num[2] << 8) |
              ((unsigned char)num[3]);
    return out;
}

std::string Peer::sendChoke() {
    return "0";
}

std::string Peer::sendUnchoke() {
    return "1";
}

std::string Peer::sendInterested() {
    return "2";
}

std::string Peer::sendNotInterested() {
    return "3";
}

std::string Peer::sendHave(int index) {
    if (bitField.at(index) == '1') {
        std::string message = "4" + static_cast<char>(index);
        return message;
    }
    return "";
}

std::string Peer::sendBitfield() {
    int pieces = ceil(1.0 * fileSize/pieceSize);
    std::string message = "5";
    message += bitField;
    return message;
}

std::string Peer::sendRequest(int index) {
    std::string message = "6" + static_cast<char>(index);
    return message;
}

std::string Peer::checkIfInterested(peerInfo* peer) {
    for (int i = 0; i < peer->bitfield.size(); i++) {
        if (peer->bitfield.at(i) == '1' && bitField.at(i) == '0') {
            return sendInterested();
        }
    }
    return sendNotInterested();
}

std::string Peer::sendPiece(int index) {
    std::string message = "7" + static_cast<char>(index);
    message += getPiece(index);
    return message;
}

std::string Peer::getPiece(int index) {
    return std::to_string(index);
}

int Peer::logConnectTo(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " makes a connection to Peer " << peer->id << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logConnectFrom(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " is connected from Peer " << peer->id << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logPreferred(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " has the preferred neighbors " << getPreferred() << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logOptUnchoke(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " has the optimistically unchoked neighbor " << peer->id << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logUnchoke(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " is unchoked by " << peer->id << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logChoke(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " is choked by " << peer->id << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logHave(peerInfo *peer, int index) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " received the 'have' message from " << peer->id
        << " for the piece " << index << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logInterested(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " received the 'interested' message from " << peer->id << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logNotInterested(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " received the 'not interested' message from " << peer->id << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logDownload(peerInfo *peer, int index) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " has downloaded the piece " << index
        << " from " << peer->id << ". Now the number of pieces it has is " << getNumPieces() << "." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logComplete(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " has downloaded the complete file." << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

std::string Peer::getPreferred() {
    std::string out = "";
    for (int i = 0; i < preferredPeers.size(); i++) {
        out += preferredPeers.at(i)->id + ", ";
    }
    return out.substr(0, out.length() - 2); // list of peer IDs separated by comma
}

int Peer::getNumPieces() {
    return 0;
}
