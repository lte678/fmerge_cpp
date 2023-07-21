#pragma once

#include "NetProtocol.h"

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>


namespace fmerge {

    typedef unsigned short transmission_idx;


    class Connection {
    public:
        Connection() = delete;
        Connection(int _fd, std::string _address) : fd(_fd), address(_address) {}
    
        typedef std::function<void(std::shared_ptr<protocol::Message>)> ResponseCallback;
        typedef std::function<std::shared_ptr<protocol::Message>(protocol::MessageType)> RequestCallback;
    private:
        int fd;
        std::mutex transmit_lock;
        std::string address;
        std::atomic<transmission_idx> message_index{0};
        std::thread listener_thread_handle;

        void listener_thread(RequestCallback request_callback);

        struct PendingResponse { transmission_idx index; protocol::MessageType type; ResponseCallback callback; };

        std::mutex response_lock;
        std::vector<PendingResponse> pending_responses;
    public:
        void send_request(protocol::MessageType request_type, ResponseCallback response_callback);
        void listen(RequestCallback request_callback);

        // Blocking receive that is guaranteed to return the requested number of bytes
        // May throw an exception if the peer disconnects.
        void receive(void *buffer, size_t len);

        std::string get_address() { return address; };
        int get_fd() { return fd; };
    };


    struct MessageHeader {
        MessageHeader() = delete;
        MessageHeader(protocol::MessageType _type, unsigned long _length, transmission_idx _index, bool _is_response)
            : type(_type), length(_length), index(_index), is_response(_is_response) {};
        MessageHeader(std::shared_ptr<protocol::Message> msg, transmission_idx _index)
            : type(msg->type()), length(msg->length()), index(_index), is_response(true) {};
        
        static MessageHeader make_request_header(protocol::MessageType _type, transmission_idx _index) {
            return MessageHeader(_type, 0, _index, false);
        }

        protocol::MessageType type{};
        unsigned long length{};
        transmission_idx index{};
        bool is_response{};

        void serialize(int fd) const;
        static MessageHeader deserialize(protocol::ReadFunc receive);
    };


    void listen_for_peers(int port, std::function<void(std::unique_ptr<Connection>)> conn_handler);
    void connect_to_server(int port, std::string server_addr, std::function<void(std::unique_ptr<Connection>)> conn_handler);

}