#include "Connection.h"

#include "Errors.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>


namespace fmerge {

    void MessageHeader::serialize(int fd) const {
        unsigned short nettype = htole16(static_cast<unsigned short>(raw_type));
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

        unsigned short netindex{};
        receive(&netindex, sizeof(netindex));
        unsigned short index = le16toh(netindex);

        unsigned long netlength{};
        receive(&netlength, sizeof(netlength));
        unsigned long length = le64toh(netlength);

        return MessageHeader(type, length, index);
    }


    void Connection::send_request(std::shared_ptr<protocol::Message> req , ResponseCallback response_callback) {
        // This function is thread safe
        if(!req->is_request()) {
            std::cerr << "[Error] send_request received a non-request message!" << std::endl;
            return;
        }

        transmission_idx current_index = message_index.fetch_add(1); 
        // Note down our message index and the callback we should call once we receive a 
        // response

        while(resp_handler_worker_count >= MAX_RESPONCE_WORKERS) {
            // We will be spending a lot of time here. We should not be using a busy lock, since it will limit us to 100 ops per second.
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::lock(transmit_lock, response_lock);
        pending_responses.push_back({.index = current_index, .type = req->type(), .callback = response_callback});
        resp_handler_worker_count++;
        // Now send header-only request message
        MessageHeader(req, current_index).serialize(fd);
        req->serialize(fd);
        transmit_lock.unlock();
        response_lock.unlock();
        if(debug_protocol) {
            std::cout << "[Peer <- Local] Request " << protocol::msg_type_to_string(req->raw_type()) << std::endl;
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


    void Connection::listen(RequestCallback request_callback) {
        listener_thread_handle = std::thread([=]() { listener_thread(request_callback); });
    }

    void Connection::listener_thread(RequestCallback request_callback) {
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
                auto received_packet = protocol::deserialize_packet(received_header.raw_type, received_header.length, receive_func);

                if(received_packet->is_request()) {
                    if(debug_protocol) {
                        std::cout << "[Peer -> Local] Request " << protocol::msg_type_to_string(received_header.raw_type) << std::endl;
                    }
                    // Request message from peer
                    auto response = request_callback(received_packet);
                    if(response) {
                        // Send back the response
                        if(debug_protocol) {
                            std::cout << "[Peer <- Local] Response " << protocol::msg_type_to_string(response->raw_type()) << std::endl;
                        }
                        transmit_lock.lock();
                        MessageHeader(response, received_header.index).serialize(fd);
                        response->serialize(fd);
                        transmit_lock.unlock();
                    } else {
                        std::cerr << "Error: Cannot send response for " << protocol::msg_type_to_string(received_header.raw_type) << " request" << std::endl;
                    }
                } else {
                    if(debug_protocol) {
                        std::cout << "[Peer -> Local] Response " << protocol::msg_type_to_string(received_header.raw_type) << std::endl;
                    }
                    // Response message from peer
                    // Find the callback within the pending responses.
                    response_lock.lock();
                    ResponseCallback resp_callback;
                    for(auto pending_it = pending_responses.begin(); pending_it != pending_responses.end(); pending_it++) {
                        if(pending_it->index == received_header.index) {
                            // We found the request this response fulfills
                            if(received_header.raw_type == pending_it->type) {
                                // Copy callback
                                resp_callback = pending_it->callback;
                            } else {
                                // Delete the pending response object
                                std::cerr << "Receive message type " << protocol::msg_type_to_string(received_header.raw_type) << " is invalid with request (" << protocol::msg_type_to_string(pending_it->type) << ")" << std::endl;
                            }
                            pending_responses.erase(pending_it);
                            break;
                        }
                    }
                    response_lock.unlock();

                    // Call the callback outside of response_lock, since it may invoke another transmission.
                    join_finished_workers(); // Try joining any finished processes
                    resp_handler_workers.push_back(std::thread{ [this, resp_callback, received_packet]() { resp_callback(received_packet); resp_handler_worker_count--; } });
                }   
            }
        } catch(const connection_terminated_exception& e) {
            std::cout << "Exited: " << e.what() << std::endl;
            exit(0);
        }
    }

    void Connection::receive(void *buffer, size_t len) {
        size_t read{0};
        
        while(read < len) {
            ssize_t received = recv(fd, reinterpret_cast<unsigned char*>(buffer) + read, len - read, 0);
            read += received;

            if(received == 0) {
                throw connection_terminated_exception();
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