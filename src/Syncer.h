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

        std::pair<SortedOperationSet, SortedOperationSet> split_operations(SortedOperationSet &operations);

        void perform_sync();
        void submit_file_transfer(const protocol::FileTransferPayload &ft_payload);
        bool _submit_file_transfer(const protocol::FileTransferPayload &ft_payload);

        int get_error_count() { return error_count.load(); }
    private:
        SortedOperationSet queued_parallel_operations{};
        SortedOperationSet queued_sequential_operations{};
        std::mutex operations_mtx;
        // Status callback that is called after every processed file with the (completed, total) number of files
        CompletionCallback completion_callback;
        std::mutex callback_mtx; // Locks execution of the completion_callback (so the function does not need to be thread safe)

        std::vector<std::thread> worker_threads;
        // This map of flags is used to signify to the waiting worker thread that the file has been transferred
        std::unordered_map<std::string, SyncBarrier<bool>> file_transfer_flags;
        std::mutex ft_flag_mtx;

        std::string base_path;
        Connection &peer_conn;

        std::atomic_int error_count{0};

        void worker_function(int tid);
        void sequential_function();
        // Returns true if file was processed successfully
        bool process_file(const std::vector<FileOperation> &ops);
    };

}