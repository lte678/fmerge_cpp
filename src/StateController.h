#pragma once

#include "Config.h"
#include "Connection.h"
#include "NetProtocol.h"

#include <thread>
#include <atomic>


namespace fmerge {

    class StateController {
        // This is the main class that handles connections to peers and initiates
        // the necessary operations to make the file sync happen.
    private:
        enum State {
            AwaitingVersion,
            SendTree,
            Finished
        };
    public:
        StateController(std::unique_ptr<Connection> conn, json _config) : c(std::move(conn)), config(_config), state(State::AwaitingVersion) {};

        void run();
    private:
        std::shared_ptr<protocol::Message> handle_request(protocol::MessageType msg_type);

        std::shared_ptr<protocol::Message> handle_version_request();
        void handle_version_response(std::shared_ptr<protocol::Message> msg);

        // Cross-thread state
        std::unique_ptr<Connection> c;
        // Read-only. Otherwise we will need a lock for this
        json config;
        std::atomic<State> state;

        std::thread listener_thread_handle;
    };

}