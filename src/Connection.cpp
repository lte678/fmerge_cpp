#include "Connection.h"

#include "Errors.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>


namespace fmerge {

    void MessageHeader::serialize(int fd) const {
        unsigned short type_field = static_cast<unsigned short>(type);
        if(is_response) {
            type_field |= 1<<15;
        }

        unsigned short nettype = htole16(type_field);
        write(fd, &nettype, sizeof(nettype));

        unsigned short netindex = htole16(static_cast<unsigned short>(index));
        write(fd, &netindex, sizeof(netindex));

        unsigned long netlength = htole64(static_cast<unsigned int>(length));
        write(fd, &netlength, sizeof(netlength));
    }


    MessageHeader MessageHeader::deserialize(protocol::ReadFunc receive) {
        unsigned short nettype{};
        receive(&nettype, sizeof(nettype));
        unsigned short type = static_cast<protocol::MessageType>(le16toh(nettype));
        bool is_response = (type & 0x8000) != 0;

        unsigned long netlength{};
        receive(&netlength, sizeof(netlength));
        unsigned long length = le64toh(netlength);

        unsigned short netindex{};
        receive(&netindex, sizeof(netindex));
        unsigned short index = le16toh(netindex);

        return MessageHeader(type & 0x7FFF, length, index, is_response);
    }


    void Connection::send_request(protocol::MessageType request_type, ResponseCallback response_callback) {
        // This function is thread safe

        transmission_idx current_index = message_index.fetch_add(1); 
        // Note down our message index and the callback we should call once we receive a 
        // response
        transmit_lock.lock();
        pending_responses.push_back({.index = current_index, .callback = response_callback});
        // Now send header-only request message
        MessageHeader::make_request_header(request_type, current_index).serialize(fd);
        transmit_lock.unlock();
    }


    void Connection::listen(RequestCallback request_callback) {
        listener_thread_handle = std::thread([=]() { listener_thread(request_callback); });
    }

    void Connection::listener_thread(RequestCallback request_callback) {
        while(true) {
            auto receive_func = [this](auto buf, auto len) { receive(buf, len); };
            auto received_header = MessageHeader::deserialize(receive_func);

            if(received_header.is_response) {
                std::cout << "Peer sent " << protocol::msg_type_to_string(received_header.type) << " response" << std::endl;
                // Response message from peer
                auto pending_it = pending_responses.begin();
                for(; pending_it != pending_responses.end(); pending_it++) {
                    if(pending_it->index == received_header.index) {
                        pending_it->callback(protocol::deserialize_packet(received_header.type, received_header.length, receive_func));
                    }
                }
                // Delete the pending response object
                pending_responses.erase(pending_it);
            } else {
                std::cout << "Peer sent request for " << protocol::msg_type_to_string(received_header.type) << std::endl;
                // Request message from peer
                auto response = request_callback(received_header.type);
                if(response) {
                    // Send back the response
                    transmit_lock.lock();
                    MessageHeader(response, received_header.index).serialize(fd);
                    response->serialize(fd);
                    transmit_lock.unlock();
                } else {
                    std::cerr << "Error: Cannot send response for " << protocol::msg_type_to_string(received_header.type) << " request" << std::endl;
                }
            }   
        }
    }

    void Connection::receive(void *buffer, size_t len) {
        size_t read{0};
        
        while(read < len) {
            ssize_t received = recv(fd, reinterpret_cast<unsigned char*>(buffer) + read, len - read, 0);
            read += received;

            if(received == 0) {
                throw std::runtime_error("Connection terminated");
            } else if(received == -1) {
                print_clib_error("recv");
                throw std::runtime_error("Connection failed");
            }
        }
    }


    void listen_for_peers(int port, std::function<void(std::unique_ptr<Connection>)> conn_handler) {
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
        socklen_t client_addr_size{sizeof(sockaddr_in)};
        int client_sock = accept(listen_sock, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_size);
        if(client_sock == -1) {
            print_clib_error("accept");
        }

        char addr_string[INET6_ADDRSTRLEN];
        if(getnameinfo(reinterpret_cast<sockaddr*>(&client_addr), client_addr_size, addr_string, sizeof(addr_string), nullptr, 0, NI_NUMERICHOST) != 0) {
            std::cerr << "getnameinfo: error" << std::endl;
        } else {
            // We have received a valid client connection
            conn_handler(std::make_unique<Connection>(client_sock, addr_string));
        }

        if(close(client_sock) == -1) {
            print_clib_error("close");
        }
    }


    void connect_to_server(int port, std::string server_addr, std::function<void(std::unique_ptr<Connection>)> conn_handler) {
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
        conn_handler(std::make_unique<Connection>(sock, server_addr));

        // Close connection
        if(close(sock) == -1) {
            print_clib_error("close");
        }
    }

}