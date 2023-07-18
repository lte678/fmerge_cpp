#include "Peer.h"

#include "Version.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <array>
#include <sstream>


std::ostream& operator<<(std::ostream &out, const MessageType msg) {
    if(msg == MsgIgnore) {
        out << "Ignore";
    } else if (msg == MsgVersion) {
        out << "Version";
    } else if (msg == MsgSendChanges) {
        out << "SendChanges";
    } else if (msg == MsgUnknown) {
        out << "Unknown";
    } else {
        out << "Invalid";
    }

    return out;
}


void MessageHeader::serialize(int fd) const {
    unsigned int nettype = htole32(static_cast<unsigned int>(type));
    write(fd, &nettype, sizeof(nettype));

    unsigned long netlength = htole64(static_cast<unsigned int>(length));
    write(fd, &netlength, sizeof(netlength));
}


MessageHeader MessageHeader::deserialize(int fd) {
    MessageHeader new_header{};

    unsigned int nettype{};
    read(fd, &nettype, sizeof(nettype));
    new_header.type = static_cast<MessageType>(le32toh(nettype));

    unsigned long netlength{};
    read(fd, &netlength, sizeof(netlength));
    new_header.length = le64toh(netlength);

    return new_header;
}


void VersionMessage::serialize(int fd) const {
    // Note:: Serialize adds the header, while deserialize does not expect it!
    MessageHeader header(MsgVersion, length());
    header.serialize(fd);

    unsigned int netmajor = htole32(major);
    write(fd, &netmajor, sizeof(netmajor));
    unsigned int netminor = htole32(minor);
    write(fd, &netminor, sizeof(netminor));

    write(fd, uuid.data(), uuid.size());
}


VersionMessage VersionMessage::deserialize(int fd) {
    unsigned int major{};
    read(fd, &major, sizeof(major));
    major = le32toh(major);
    unsigned int minor{};
    read(fd, &minor, sizeof(minor));
    minor = le32toh(minor);

    std::array<unsigned char, 16> uuid;
    read(fd, uuid.data(), uuid.size());

    return VersionMessage(major, minor, uuid);
}


void ChangesMessage::serialize(int fd) const {
    std::stringstream serialized_changes;
    write_changes(serialized_changes, changes);
    std::string serialized_changes2 = serialized_changes.str();

    MessageHeader header(MsgSendChanges, serialized_changes2.length());
    header.serialize(fd);
    write(fd, serialized_changes2.c_str(), serialized_changes2.length());
}


ChangesMessage ChangesMessage::deserialize(int fd, unsigned long length) {
    char *change_buffer = new char[length];
    read(fd, change_buffer, length); // TODO: Implement blocking read that always returns all bytes of message

    std::stringstream change_stream(change_buffer);
    auto changes = read_changes(change_stream);
    return ChangesMessage(changes);
}


void listen_for_peers(int port, std::string our_uuid, std::function<void(Peer)> peer_handler) {
    // Prepare listening socket
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_sock == -1) {
        print_clib_error("socket");
    }

    // Allow port reuse
    int reuseaddr{1};
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));

    sockaddr_in listen_addr{};
    listen_addr.sin_family = AF_INET,
    listen_addr.sin_port = htons(port),
    inet_aton("0.0.0.0", &listen_addr.sin_addr);

    bind(listen_sock, reinterpret_cast<sockaddr*>(&listen_addr), sizeof(listen_addr));

    if(listen(listen_sock, 1) == -1) {
        print_clib_error("listen");
    }

    // Listen for connection
    sockaddr_in client_addr{};
    socklen_t client_addr_size{};
    int client_sock = accept(listen_sock, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_size);
    if(client_sock == -1) {
        print_clib_error("accept");
    }

    // We have received a valid client connection
    server_handshake(client_sock, our_uuid, peer_handler);

    if(close(client_sock) == -1) {
        print_clib_error("close");
    }
}


void server_handshake(int client_sock, std::string our_uuid, std::function<void(Peer)> peer_handler) {
    // Create a new peer object
    auto msg_header = MessageHeader::deserialize(client_sock);
    if(msg_header.type != MsgVersion) {
        std::cerr << "Invalid message received during handshake (Received: " << msg_header.type << ")" << std::endl;
    }

    auto client_ver = VersionMessage::deserialize(client_sock);
    if (client_ver.major != MAJOR_VERSION || client_ver.minor != MINOR_VERSION) { 
        std::cerr << "Client has invalid version!";
        std::cerr << " Client: v" << client_ver.major << "." << client_ver.minor;
        std::cerr << " Server: v" << MAJOR_VERSION << "." << MINOR_VERSION;
        return;
    }

    VersionMessage server_ver(MAJOR_VERSION, MINOR_VERSION, {0});
    if(uuid_parse(our_uuid.c_str(), server_ver.uuid.data()) == -1) {
        std::cerr << "Error parsing our UUID!" << std::endl;
        return;
    }
    server_ver.serialize(client_sock);

    peer_handler(Peer(client_sock, client_ver.uuid));

}


void connect_to_server(int port, std::string server_addr, std::string our_uuid, std::function<void(Peer)> peer_handler) {
    // Prepare listening socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        print_clib_error("socket");
        return;
    }

    // Resolve the hostname
    addrinfo hints{};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    addrinfo *result;
    if(getaddrinfo(server_addr.c_str(), std::to_string(port).c_str(), &hints, &result) == -1) {
        print_clib_error("getaddrinfo");
        return;
    }

    if(result == nullptr) {
        std::cerr << "Failed to lookup host " << server_addr << std::endl;
        return;
    }

    if(connect(sock, result->ai_addr, result->ai_addrlen) == -1) {
        print_clib_error("connect");
        return;
    }

    // We have received a valid client connection
    client_handshake(sock, our_uuid, peer_handler);

    // Close connection
    if(close(sock) == -1) {
        print_clib_error("close");
    }
}


void client_handshake(int server_sock, std::string our_uuid, std::function<void(Peer)> peer_handler) {
    // TRANSACTION 1.1 ----> Transmit version
    VersionMessage client_ver(MAJOR_VERSION, MINOR_VERSION, {0});
    if(uuid_parse(our_uuid.c_str(), client_ver.uuid.data()) == -1) {
        std::cerr << "Error parsing our UUID!" << std::endl;
        return;
    }
    client_ver.serialize(server_sock);

    // TRANSACTION 1.2 <---- Receive server version
    auto msg_header = MessageHeader::deserialize(server_sock);
    if(msg_header.type != MsgVersion) {
        std::cerr << "Invalid message received during handshake (Received: " << msg_header.type << ")" << std::endl;
        return;
    }

    auto server_ver = VersionMessage::deserialize(server_sock);
    if (server_ver.major != MAJOR_VERSION || server_ver.minor != MINOR_VERSION) { 
        std::cerr << "Server has invalid version!";
        std::cerr << " Client: v" << MAJOR_VERSION << "." << MINOR_VERSION;
        std::cerr << " Server: v" << server_ver.major << "." << server_ver.minor;
        return;
    }

    // HANDSHAKE COMPLETE
    peer_handler(Peer(server_sock, server_ver.uuid));
}