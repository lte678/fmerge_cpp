#pragma once

#include "MergeAlgorithms.h"
#include "Connection.h"
#include "Util.h"

#include <thread>
#include <mutex>
#include <functional>


namespace fmerge {

    constexpr int MAX_SYNC_WORKERS{8};
    constexpr int FILE_TRANSFER_TIMEOUT{300};

    class Syncer {
    public:
        // Returns the path of the processed file, and whether it was successfull or not
        typedef std::function<void(std::string, bool)> CompletionCallback;

        Syncer(SortedOperationSet &operations, std::string _base_path, Connection &_peer_conn);
        Syncer(SortedOperationSet &operations, std::string _base_path, Connection &_peer_conn, CompletionCallback _status_callback);

        void perform_sync();
        void submit_file_transfer(const protocol::FileTransferPayload &ft_payload);
    private:
        SortedOperationSet &queued_operations;
        std::mutex operations_mtx;
        // Status callback that is called after every processed file with the (completed, total) number of files
        CompletionCallback completion_callback;
        std::mutex callback_mtx; // Locks execution of the completion_callback (so the function does not need to be thread safe)

        std::vector<std::thread> worker_threads;
        // This map of flags is used to signify to the waiting worker thread that the file has been transferred
        std::unordered_map<std::string, SyncBarrier> file_transfer_flags;

        std::string base_path;
        Connection &peer_conn;

        void worker_function(int tid);
        // Returns true if file was processed successfully
        bool process_file(const std::vector<FileOperation> &ops);
    };

}