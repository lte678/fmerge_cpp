#pragma once

#include "Errors.h"
#include "FileTree.h"

#include <string>
#include <functional>


enum MessageType {
    MsgIgnore,
    MsgVersion,
    MsgSendChanges,
    MsgUnknown,
};

std::ostream& operator<<(std::ostream &out, const MessageType msg);


struct MessageHeader {
    MessageHeader() {};
    MessageHeader(MessageType _type, unsigned long _length) : type(_type), length(_length) {};
    
    MessageType type{};
    unsigned long length{};

    void serialize(int fd) const;
    static MessageHeader deserialize(int fd);
};


struct VersionMessage {
    VersionMessage(int _major, int _minor, std::array<unsigned char, 16> _uuid) : major(_major), minor(_minor), uuid(_uuid) {};

    int major{};
    int minor{};
    std::array<unsigned char, 16> uuid{};

    void serialize(int fd) const;
    static VersionMessage deserialize(int fd);
    inline unsigned int length() const { return sizeof(major) + sizeof(minor) + 16; };
};


struct ChangesMessage {
    // Transmit all the changes that the connection partner has not yet received.
    // The server decides which information is missing, since this index must be
    // maintained locally, in accordance with any changes that happen.
    ChangesMessage(std::vector<Change> _changes) : changes(_changes) {};

    std::vector<Change> changes;

    void serialize(int fd) const;
    static ChangesMessage deserialize(int fd, unsigned long length);
};


class Peer {
public:
    Peer() = delete;
    Peer(int _fd, std::array<unsigned char, 16> _uuid) : fd(_fd), uuid(_uuid) {}
private:
    int fd;
    std::array<unsigned char, 16> uuid;
public:
    std::array<unsigned char, 16> get_uuid() { return uuid; };
    int get_fd() { return fd; };
};


void listen_for_peers(int port, std::string our_uuid, std::function<void(Peer)> peer_handler);
void server_handshake(int client_sock, std::string our_uuid, std::function<void(Peer)> peer_handler);

void connect_to_server(int port, std::string server_addr, std::string our_uuid, std::function<void(Peer)> peer_handler);
void client_handshake(int server_sock, std::string our_uuid, std::function<void(Peer)> peer_handler);