#include "protocol/NetProtocol.h"
#include "Globals.h"
#include "Errors.h"
#include "Terminal.h"

#include <fstream>

#include "Syncer.h"


namespace fmerge {
    Syncer::Syncer(SortedOperationSet &operations, std::string _base_path, Connection &_peer_conn) : Syncer(operations, _base_path, _peer_conn, nullptr) {}

    Syncer::Syncer(SortedOperationSet &operations, std::string _base_path, Connection &_peer_conn, CompletionCallback _status_callback) : queued_operations(operations), peer_conn(_peer_conn) {
        queued_operations = operations;
        completion_callback = _status_callback;
        base_path = _base_path;
    }

    
    void Syncer::perform_sync() {
        for(int i = 0; i < MAX_SYNC_WORKERS; i++) {
            worker_threads.push_back(
                std::thread{[this, i](){worker_function(i);}}
            );
        }
        // Wait for threads to finish processing everything
        for(auto &t: worker_threads) {
            t.join();
        }
        // Quick sanity check
        if(file_transfer_flags.size() != 0) {
            std::cerr << "[Error] Not all file transfer flags processed after sync! Contact the developers." << std::endl;
        }
    }

    void Syncer::worker_function(int tid) {
        while(true) {
            // Fetch a new task
            std::unique_lock<std::mutex> op_lock(operations_mtx);
            // We are done syncing.
            if(queued_operations.size() == 0) return;
            // List of operations to apply to the file
            auto op_set = queued_operations.begin();
            auto filepath = op_set->first;
            auto op_list = op_set->second;
            queued_operations.erase(op_set);
            op_lock.unlock();

            if(g_debug_protocol) {
                termbuf() << "[DEBUG] [tid:" << tid << "] Processing file " << filepath << std::endl;
            }

            // Process that task

            // For us to accurately reproduce the new file history, all operations
            // have to be executed successfully. If this fails, we will have to resolve it
            // or leave the file history in a dirty state, which will be corrected at
            // the next database rebuild and merge.

            bool successful = process_file(op_list);
            
            if(!successful) {
                std::cerr << "[Error] File " << filepath << " is in a conflicted state!" << std::endl;
            }
            std::unique_lock<std::mutex> cb_lock(callback_mtx);
            if(completion_callback) completion_callback(filepath, successful);
        }
    }

    bool Syncer::process_file(const std::vector<FileOperation> &ops) {
        for(const auto& op : ops) {
            std::string filepath{op.path};
            if(op.type == FileOperationType::Delete) {
                if(!remove_path(join_path(base_path, filepath))) {
                    // Operation failed
                    return false;
                }
            } else if(op.type == FileOperationType::Transfer) {
                peer_conn.send_message(
                    std::make_shared<protocol::FileRequestMessage>(filepath)
                );
                // Sleep until the file has been transferred
                file_transfer_flags.emplace(filepath, 5);
                int attempts{FILE_TRANSFER_TIMEOUT / 5};
                int i{0};
                while(file_transfer_flags.at(filepath).wait()) {
                    if(i == attempts) {
                        file_transfer_flags.erase(file_transfer_flags.find(filepath));
                        std::cerr << "[Error] File transfer timed out for " << filepath << std::endl;
                        return false; 
                    }
                    i++;
                    termbuf() << "Waited " << 5*i << "s for " << filepath << std::endl;
                }
                file_transfer_flags.erase(file_transfer_flags.find(filepath));
                // File has arrived
            } else {
                std::cerr << "[Error] Could not perform unknown file operation " << op.type << std::endl;
                return false;
            }
        }
        return true;
    }


    void Syncer::submit_file_transfer(const protocol::FileTransferPayload &ft_payload) {
        std::string fullpath = join_path(base_path, ft_payload.path);

        if(g_debug_protocol) {
            termbuf() << "[DEBUG] Received data for " << fullpath << std::endl;
        }

        // Create folder for file
        auto path_tokens = split_path(fullpath);
        auto file_folder = path_to_str(std::vector<std::string>(path_tokens.begin(), path_tokens.end() - 1));
        termbuf() << file_folder << std::endl;
        if(!exists(file_folder)) {
            //termbuf() << "[Warning] Out of order file transfer. Creating folder for file that should already exist." << std::endl;
            if(!ensure_dir(file_folder)) {
                std::cerr << "[Error] Failed to create directory " << file_folder << std::endl;
                return;
            }
        }

        if(ft_payload.ftype == FileType::Directory) {
            // Create folder
            if(!ensure_dir(fullpath)) {
                return;
            }
        } else if(ft_payload.ftype == FileType::File) {
            // Create file
            std::ofstream out_file(fullpath, std::ofstream::binary);
            if(out_file) {
                out_file.write(reinterpret_cast<char*>(ft_payload.payload.get()), ft_payload.payload_len);
            } else {
                std::cerr << "[Error] Could not open file " << fullpath << " for writing." << std::endl;
                return;
            }
        } else if(ft_payload.ftype == FileType::Link) {
            // Create symlink
            // Warning: Payload is not null-terminated
            char symlink_contents[ft_payload.payload_len + 1];
            memcpy(symlink_contents, ft_payload.payload.get(), ft_payload.payload_len);
            symlink_contents[ft_payload.payload_len] = '\0';
            // Delete if exists
            if(exists(fullpath)) {
                if(unlink(fullpath.c_str()) == -1) {
                    print_clib_error("unlink");
                    std::cerr << "^^^ " << fullpath << std::endl;
                    return;
                }
            }
            // Create link
            if(symlink(symlink_contents, fullpath.c_str()) == -1) {
                print_clib_error("symlink");
                std::cerr << "^^^ " << fullpath << std::endl;
                return;
            }
        } else {
            std::cerr << "[Error] Received unknown file type in FileTransfer response! (" << static_cast<int>(ft_payload.ftype) << ")" << std::endl;
            return;
        }

        // TODO: Return error codes
        set_timestamp(fullpath, ft_payload.mod_time, ft_payload.access_time);

        // Notify completion
        file_transfer_flags.at(ft_payload.path).notify();
    }
}