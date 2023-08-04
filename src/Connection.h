#pragma once

#include "NetProtocol.h"

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <list>
#include <thread>


namespace fmerge {

    typedef unsigned short transmission_idx;

    constexpr int MAX_RESPONCE_WORKERS{16};
    constexpr int MAX_REQUEST_WORKERS{32};

    class connection_terminated_exception : public std::exception {
    public:
        char const* what() const noexcept { return "Connection terminated"; }
    };

    class Connection {
    public:
        Connection() = delete;
        Connection(int _fd, std::string _address) : fd(_fd), address(_address) {}
        ~Connection();
    
        typedef std::function<void(std::shared_ptr<protocol::Message>)> ResponseCallback;
        typedef std::function<std::shared_ptr<protocol::Message>(std::shared_ptr<protocol::Message>)> RequestCallback;
    private:
        int fd;
        std::mutex transmit_lock;
        std::string address;
        std::atomic<transmission_idx> message_index{0};
        std::thread listener_thread_handle;

        std::atomic<int> resp_handler_worker_count{0};
        std::vector<std::thread> resp_handler_workers;
        std::atomic<int> req_handler_worker_count{0};
        std::vector<std::thread> req_handler_workers;

        void join_finished_workers();

        std::atomic<bool> disconnect{false};
        void listener_thread(RequestCallback request_callback);

        struct PendingResponse { transmission_idx index; protocol::MessageType type; ResponseCallback callback; };

        std::mutex response_lock;
        std::list<PendingResponse> pending_responses;
    public:
        void send_request(std::shared_ptr<protocol::Message> req , ResponseCallback response_callback);
        void listen(RequestCallback request_callback);

        // Blocking receive that is guaranteed to return the requested number of bytes
        // May throw an exception if the peer disconnects.
        void receive(void *buffer, size_t len);
        // Blocking write that is guaranteed to write the requested number of bytes
        void send(const void *buffer, size_t len);

        std::string get_address() { return address; };
        int get_fd() { return fd; };
    };


    struct MessageHeader {
        MessageHeader() = delete;
        MessageHeader(protocol::MessageType _raw_type, unsigned long _length, transmission_idx _index)
            : raw_type(_raw_type), length(_length), index(_index) {};
        MessageHeader(std::shared_ptr<protocol::Message> msg, transmission_idx _index)
            : raw_type(msg->raw_type()), length(msg->length()), index(_index) {};

        protocol::MessageType raw_type{};
        unsigned long length{};
        transmission_idx index{};

        void serialize(protocol::WriteFunc send) const;
        static MessageHeader deserialize(protocol::ReadFunc receive);
    };


    void listen_for_peers(int port, std::function<void(std::unique_ptr<Connection>)> conn_handler);
    void connect_to_server(int port, std::string server_addr, std::function<void(std::unique_ptr<Connection>)> conn_handler);

}