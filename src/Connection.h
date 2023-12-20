#pragma once

#include "protocol/NetProtocol.h"

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <list>
#include <thread>


namespace fmerge {

    constexpr int MAX_WORKERS{32};

    class connection_terminated_exception : public std::exception {
    public:
        char const* what() const noexcept { return "Connection terminated"; }
    };

    class Connection {
    public:
        Connection() = delete;
        Connection(int _fd, std::string _address) : fd(_fd), address(_address) {}
        ~Connection();
    
        typedef std::function<void(std::shared_ptr<protocol::GenericMessage>)> ReceiveCallback;
    private:
        int fd;
        std::mutex transmit_lock;
        std::string address;
        std::thread listener_thread_handle;

        std::atomic<int> resp_handler_worker_count{0};
        std::unordered_map<std::thread::id, std::thread> resp_handler_workers;
        std::vector<std::thread::id> finished_workers;
        std::mutex finished_workers_mtx;
        void join_finished_workers();

        std::atomic<bool> disconnect{false};
        void listener_thread(ReceiveCallback callback, std::function<void(void)> terminate_callback);

        // Blocking receive that is guaranteed to return the requested number of bytes
        // May throw an exception if the peer disconnects.
        void receive(void *buffer, size_t len);
        // Blocking write that is guaranteed to write the requested number of bytes
        void send(const void *buffer, size_t len);

        int get_fd() { return fd; };
    public:
        void send_message(std::shared_ptr<protocol::GenericMessage> msg);
        void listen(ReceiveCallback callback, std::function<void(void)> terminate_callback);

        std::string get_address() { return address; };
    };


    struct MessageHeader {
        MessageHeader() = delete;
        MessageHeader(protocol::MsgType _type, unsigned long _length)
            : type(_type), length(_length) {};
        MessageHeader(std::shared_ptr<protocol::GenericMessage> msg)
            : type(msg->type()), length(msg->length()) {};

        protocol::MsgType type{};
        unsigned long length{};
        void serialize(protocol::WriteFunc send) const;
        static MessageHeader deserialize(protocol::ReadFunc receive);
    };


    void listen_for_peers(int port, std::function<void(std::unique_ptr<Connection>)> conn_handler);
    void connect_to_server(int port, std::string server_addr, std::function<void(std::unique_ptr<Connection>)> conn_handler);

}