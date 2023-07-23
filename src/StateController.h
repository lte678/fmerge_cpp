#pragma once

#include "Config.h"
#include "Connection.h"
#include "NetProtocol.h"
#include "MergeAlgorithms.h"

#include <thread>
#include <atomic>
#include <mutex>


namespace fmerge {

    class StateController {
        // This is the main class that handles connections to peers and initiates
        // the necessary operations to make the file sync happen.
    private:
        enum State {
            AwaitingVersion,
            SendTree,
            ResolvingConflicts,
            SyncingFiles,
            Finished
        };
    public:
        StateController(std::unique_ptr<Connection> conn, std::string _path, json _config) : c(std::move(conn)), config(_config), path(_path), state(State::AwaitingVersion) {};

        void run();
    private:
        std::shared_ptr<protocol::Message> handle_request(std::shared_ptr<protocol::Message> msg);
        std::shared_ptr<protocol::Message> handle_version_request(std::shared_ptr<protocol::Message>);
        std::shared_ptr<protocol::Message> handle_changes_request(std::shared_ptr<protocol::Message>);
        std::shared_ptr<protocol::Message> handle_file_transfer_request(std::shared_ptr<protocol::Message> msg);
        void handle_version_response(std::shared_ptr<protocol::Message> msg);
        void handle_changes_response(std::shared_ptr<protocol::Message> msg);
        void handle_file_transfer_response(std::shared_ptr<protocol::Message> msg, std::string filepath);

        void do_merge();
        void do_sync();

        void wait_for_state(State target_state);

        // Cross-thread state
        std::mutex state_lock; // Used for all non-constant members.

        std::unique_ptr<Connection> c;
        // Read-only. Otherwise we will need a lock for this
        json config;
        // Read-only
        std::string path;
        std::atomic<State> state;
        std::vector<Change> peer_changes;
        // Each operation also has changes associated with it
        SortedOperationSet pending_operations;
        SortedChangeSet pending_changes;

        std::thread listener_thread_handle;
    };

}