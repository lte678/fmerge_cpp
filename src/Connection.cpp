#include "Connection.h"

#include "Terminal.h"
#include "Errors.h"
#include "Globals.h"
#include "protocol/NetProtocolRegistry.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>


namespace fmerge {

    void MessageHeader::serialize(protocol::WriteFunc write) const {
        unsigned short nettype = htole16(static_cast<unsigned short>(type));
        write(&nettype, sizeof(nettype));

        unsigned long netlength = htole64(static_cast<unsigned int>(length));
        write(&netlength, sizeof(netlength));
    }


    MessageHeader MessageHeader::deserialize(protocol::ReadFunc receive) {
        unsigned short nettype{};
        receive(&nettype, sizeof(nettype));
        protocol::MsgType type = static_cast<protocol::MsgType>(le16toh(nettype));

        unsigned long netlength{};
        receive(&netlength, sizeof(netlength));
        unsigned long length = le64toh(netlength);

        return MessageHeader(type, length);
    }


    Connection::~Connection() {
        disconnect = true;
        if(listener_thread_handle.joinable()) {
            listener_thread_handle.join();
        }
    }


    void Connection::send_message(std::shared_ptr<protocol::GenericMessage> msg) {
        // This function is thread safe

        transmit_lock.lock();
        // Now send header-only request message
        auto send_func = [this](auto buf, auto len) { send(buf, len); };
        MessageHeader(msg).serialize(send_func);
        msg->serialize(send_func);
        transmit_lock.unlock();
        if(g_debug_protocol) {
            termbuf() << "[Peer <- Local] Sending " << msg->type() << std::endl;
        }
    }


    void Connection::join_finished_workers() {
        auto worker_it = resp_handler_workers.begin();
        while(worker_it != resp_handler_workers.end()) {
            if(worker_it->joinable()) {
                worker_it->join();
                worker_it = resp_handler_workers.erase(worker_it);
            } else {
                worker_it++;
            }
        }
    }


    void Connection::listen(ReceiveCallback callback) {
        listener_thread_handle = std::thread([=]() { listener_thread(callback); });
    }


    void Connection::listener_thread(ReceiveCallback callback) {
        // To prevent the application from locking up, this thread must always return back to a state of listening
        // without any blocking writes. Otherwise the two clients can deadlock.
        //
        // This would imply creating infinite threads, if we just put each write into a new thread.
        // Thus, we should block on creating new requests, so we can also limit the max number of responses that we can
        // possibly receive.

        try {
            while(true) {
                auto receive_func = [this](auto buf, auto len) { receive(buf, len); };
                auto received_header = MessageHeader::deserialize(receive_func);
                auto received_packet = protocol::deserialize_packet(received_header.type, received_header.length, receive_func);

                if(g_debug_protocol) {
                    termbuf() << "[Peer -> Local] Received " << received_header.type << std::endl;
                }
                // Received message from peer
                join_finished_workers(); // Try joining any finished processes
                // Wait until worker thread is available
                while(resp_handler_worker_count >= MAX_WORKERS) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                // Create worker thread
                resp_handler_workers.push_back(std::thread{ 
                    [this, callback, received_packet]()
                        { callback(received_packet); resp_handler_worker_count--; } 
                });
            }
        } catch(const connection_terminated_exception& e) {
            termbuf() << "Exited: " << e.what() << std::endl;
            exit(0);
        }
    }

    void Connection::receive(void *buffer, size_t len) {
        size_t read{0};
        
        //TODO: This function is utter trash. We need to somehow make this use callbacks, ie. linux event system :S

        while(read < len) {
            ssize_t received = recv(fd, reinterpret_cast<unsigned char*>(buffer) + read, len - read, 0);
            if(received == 0 || disconnect) {
                throw connection_terminated_exception();
            } else if(received == -1) {
                if(errno != EAGAIN && errno != EWOULDBLOCK) {
                    print_clib_error("recv");
                    throw std::runtime_error("Connection failed");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                read += received;
            }
        }
    }

    void Connection::send(const void* buffer, size_t len) {
        size_t written{0};
        
        while(written < len) {
            ssize_t n = write(fd, reinterpret_cast<const unsigned char*>(buffer) + written, len - written);
            if(n == 0 || disconnect) {
                throw connection_terminated_exception();
            } else if(n == -1) {
                if(errno != EAGAIN && errno != EWOULDBLOCK) {
                    print_clib_error("write");
                    throw std::runtime_error("Connection failed");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                written += n;
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

        fcntl(client_sock, F_SETFL, O_NONBLOCK);

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