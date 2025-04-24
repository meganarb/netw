#include "Peer.h"

#include <memory>

Peer::Peer() {
    complete = false;
    unchoke = false;
    optUnchoke = false;
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
    pthread_mutex_init(&unchokeLock, nullptr);
    pthread_mutex_init(&optUnchokeLock, nullptr);
}

void Peer::setID(int ID) {
    id = ID;
    log.open("log_peer_" + std::to_string(ID) + ".log", std::ofstream::out);
    if (log.is_open()) std::cout << "open file" << std::endl;
    else std::cout << "file not open\n";
}

void Peer::initBitfield(int bit) {
    if (bit == 1) fullBitfield = true;
    else fullBitfield = false;
    int chunks = ceil(1.0 * fileSize / pieceSize);
    bitField = "";
    for (int i = 0; i < chunks; i++) bitField += std::to_string(bit);
}

int Peer::splitFile() {
    std::ifstream file(fileName, std::ios::binary);
    
    int i = 0;

    std::string buffer(pieceSize, '\0');
    while (!file.eof()) {
        file.read(buffer.data(), pieceSize);
        std::streamsize bytesRead = file.gcount();
        
        std::string fp = "peer_" + std::to_string(id) + "/" + std::to_string(i);
        std::ofstream piece(fp, std::ios::binary);
        piece.write(buffer.data(), bytesRead);
        piece.close();
        
        i++;
        if (i > 1500) break; // just in case it somehow goes crazy again, keep my vm from crashing :,(
    }
    file.close();
    return 0;
}

void Peer::changeUnchoked() {
    // determine which are interested first
    std::vector<peerInfo*> options;  
    for (auto* peer : allPeers) if (peer->interested) options.push_back(peer);
    if (options.size() <= numPreferred) {
        // all of them get unchoked
        preferredPeers.clear();
        for (auto* p : options) preferredPeers.push_back(p);
    } else if (fullBitfield) {
        // choose randomly
        preferredPeers.clear();
        for (int i = 0; i < numPreferred; i++) {
            std::random_device rand;
            std::mt19937 gen(rand());
            std::uniform_int_distribution<> distrib(0, options.size()-1);
            int ind = distrib(gen);
            peerInfo* p = options[ind];
            options.erase(find(options.begin(), options.end(), p));
            preferredPeers.push_back(p);
        }    
    } else {
        // go through the data transfer rates in the last round (reset them at the end of this)
        // if there are ties, choose randomly
    }
}

void Peer::changeOptimistic() {
    std::vector<peerInfo*> options; // fill this with interested + choked 
    for (auto* peer : allPeers) if (peer->interested && !peer->unchoked) options.push_back(peer);
    if (options.size() < 1) return;
    
    std::random_device rand;
    std::mt19937 gen(rand());
    std::uniform_int_distribution<> distrib(0, options.size()-1);
    int ind = distrib(gen);
    peerInfo* p = options[ind];
    
    optUnchoked = p;
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
    std::string num = intToBytes(index);
    char message = 6 & 0xFF;
    num = message + num;
    for (unsigned char c : num) std::cout << (int)c << " ";
    std::cout << std::endl;
    return num;
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
    std::string message = "7" + intToBytes(index);
    message += getPiece(index);
    return message;
}

std::string Peer::getPiece(int index) {
    std::ifstream file(fileName, std::ios::binary);
    std::string buffer(pieceSize, '\0');
    
    file.read(buffer.data(), pieceSize);
    std::streamsize bytesRead = file.gcount();
    
    file.close();
    return buffer;
}

int Peer::savePiece(int pieceNum, std::string buffer) {
    std::string fp = "peer_" + std::to_string(id) + "/" + std::to_string(pieceNum);
    std::ofstream piece(fp, std::ios::binary);
    piece.write(buffer.data(), buffer.size());
    piece.close();
    return 0;
}

int Peer::logCommonCFG() {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " reads the following from Common.cfg:\nNumberOfPreferredNeighbors: " << numPreferred << 
        "\nUnchokingInterval: " << unchokingInt << "\nOptimisticUnchokingInterval: " << optimisticInt <<
        "\nFileName: " << fileName << "\nFileSize: " << fileSize << "\nPieceSize: " << pieceSize << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logPeerInfoCFG() {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " reads the following info about itself from PeerInfo.cfg:\nHost: " << host << 
        "\nPort: " << port << "\nBitfield: " << bitField << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logConnectTo(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " makes a connection to Peer " << peer->id << " and receives its handshake." << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logConnectFrom(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " is connected from Peer " << peer->id << " and receives its handshake." << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logReceiveBitfield(peerInfo* peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " receives the following bitfield from " << peer->id << ":\n" << peer->bitfield << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logPreferred() {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " has the preferred neighbors " << getPreferred() << "." << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logOptUnchoke() {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " has the optimistically unchoked neighbor " << optUnchoked->id << "." << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logUnchoke(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " is unchoked by " << peer->id << "." << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logChoke(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " is choked by " << peer->id << "." << std::endl << std::endl;
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
        << " for the piece " << index << "." << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logInterested(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " received the 'interested' message from " << peer->id << "." << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logNotInterested(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " received the 'not interested' message from " << peer->id << "." << std::endl << std::endl;
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
        << " from " << peer->id << ". Now the number of pieces it has is " << getNumPieces() << "." << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

int Peer::logComplete(peerInfo *peer) {
    if (!log.is_open()) return -1;
    auto now = std::chrono::system_clock::now();
    std::time_t cur = std::chrono::system_clock::to_time_t(now);
    pthread_mutex_lock(&logLock);
    log << std::ctime(&cur) << "Peer " << id
        << " has downloaded the complete file." << std::endl << std::endl;
    pthread_mutex_unlock(&logLock);
    return 0;
}

std::string Peer::getPreferred() {
    if (preferredPeers.size() == 0) return "[none]";
    std::string out = "";
    for (auto* peer : preferredPeers) out += std::to_string(peer->id) + ", ";
    return out.substr(0, out.length() - 2); // list of peer IDs separated by comma
}

int Peer::getNumPieces() {
    return std::count(bitField.begin(), bitField.end(), '1');
}
