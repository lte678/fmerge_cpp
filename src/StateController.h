#pragma once

#include "Config.h"
#include "Connection.h"
#include "protocol/NetProtocol.h"
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
            SyncUserWait,
            SyncingFiles,
            Finished
        };
    public:
        StateController(std::unique_ptr<Connection> conn, std::string _path, json _config) : c(std::move(conn)), config(_config), path(_path), state(State::AwaitingVersion) {};
        ~StateController();

        void run();
    private:
        // Protocol message handling functions
        void handle_message(std::shared_ptr<protocol::GenericMessage> msg);

        void handle_version_message(std::shared_ptr<protocol::VersionMessage> msg);
        void handle_changes_message(std::shared_ptr<protocol::ChangesMessage> msg);
        void handle_start_sync_message(std::shared_ptr<protocol::StartSyncMessage> msg);
        void handle_file_transfer_message(std::shared_ptr<protocol::FileTransferMessage> msg);
        void handle_file_request_message(std::shared_ptr<protocol::FileRequestMessage> msg);
        void handle_resolutions_message(std::shared_ptr<protocol::ConflictResolutionsMessage> msg);

        // Message handling helper functions
        std::shared_ptr<protocol::FileTransferMessage> create_file_transfer_message(std::string path);

        // State machine steps
        void send_version();
        void send_filetree();
        void do_merge();
        void do_sync();
        void ask_proceed();

        // Wait for the next state to be activated asynchronously, usually by completion of a thread or peer message
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
        SortedChangeSet sorted_local_changes;
        // Each operation also has changes associated with it
        SortedOperationSet pending_operations;
        SortedChangeSet pending_changes;

        // These start out empty for the first pass
        std::unordered_map<std::string, ConflictResolution> resolutions;

        std::thread listener_thread_handle;
    };

}