#include "StateController.h"

#include "NetProtocol.h"
#include "Errors.h"
#include "Version.h"
#include "Terminal.h"

#include <memory>
#include <fstream>
#include <uuid/uuid.h>


namespace fmerge {
    void StateController::run() {
        c->listen([this](auto msg_type) { return handle_request(msg_type); });

        c->send_request(std::make_shared<protocol::VersionRequest>(), [this](auto msg) { handle_version_response(msg); });

        wait_for_state(State::SendTree);
        std::cout << "Requesting file tree" << std::endl;
        c->send_request(std::make_shared<protocol::ChangesRequest>(), [this](auto msg) { handle_changes_response(msg); });

        wait_for_state(State::ResolvingConflicts);
        do_merge();

        wait_for_state(State::SyncingFiles);
        do_sync();

        std::cout << "Waiting for peer to complete" << std::endl;
        wait_for_state(State::Finished);
    }


    std::shared_ptr<protocol::Message> StateController::handle_request(std::shared_ptr<protocol::Message> msg) {
        if(msg->type() == protocol::MsgVersion) {
            return handle_version_request(msg);
        } else if(msg->type() == protocol::MsgChanges) {
            return handle_changes_request(msg);
        } else if(msg->type() == protocol::MsgFileTransfer) {
            return handle_file_transfer_request(msg);
        }
        // Error message is handled caller function
        return nullptr;
    }


    std::shared_ptr<protocol::Message> StateController::handle_version_request(std::shared_ptr<protocol::Message>) {
        std::array<unsigned char, 16> uuid{};
        std::string our_uuid = config.value("uuid", "");
        if(uuid_parse(our_uuid.c_str(), uuid.data()) == -1) {
            std::cerr << "Error parsing our UUID!" << std::endl;
            return nullptr;
        }
        return std::make_shared<protocol::VersionResponse>(MAJOR_VERSION, MINOR_VERSION, uuid);
    }


    void StateController::handle_version_response(std::shared_ptr<protocol::Message> msg) {
        auto ver_msg = std::dynamic_pointer_cast<protocol::VersionResponse>(msg);
        
        if (ver_msg->major != MAJOR_VERSION || ver_msg->minor != MINOR_VERSION) { 
            std::cerr << "Peer has invalid version!";
            std::cerr << " Peer : v" << ver_msg->major << "." << ver_msg->minor;
            std::cerr << " Local: v" << MAJOR_VERSION << "." << MINOR_VERSION;
            return;
        }

        if(state == State::AwaitingVersion) state = State::SendTree;            
    }


    std::shared_ptr<protocol::Message> StateController::handle_changes_request(std::shared_ptr<protocol::Message>) {
        return std::make_shared<protocol::ChangesResponse>(read_changes(path));
    }

    void StateController::handle_changes_response(std::shared_ptr<protocol::Message> msg) {
        auto changes_msg = std::dynamic_pointer_cast<protocol::ChangesResponse>(msg);

        if(state == State::SendTree) {
            state_lock.lock();
            peer_changes = changes_msg->changes;
            std::cout << "Received " << peer_changes.size() << " changes from peer" << std::endl;
            state_lock.unlock();

            state = State::ResolvingConflicts;
        }
    }


    std::shared_ptr<protocol::Message> StateController::handle_file_transfer_request(std::shared_ptr<protocol::Message> msg) {
        auto ft_msg = std::static_pointer_cast<protocol::FileTransferRequest>(msg);
        
        std::string file_fullpath = join_path(path, ft_msg->filepath);

        auto fstats = get_file_stats(file_fullpath);
        if(!fstats.has_value()) {
            std::cerr << "[Error] Peer requested a file that does not exist! (" << ft_msg->filepath << ")" << std::endl;
            return std::make_shared<protocol::FileTransferResponse>();
        }

        if(fstats->type == FileType::Directory) {
            // Return an empty response. The host does not require any extra data to create a folder.
            return std::make_shared<protocol::FileTransferResponse>(nullptr, 0, *fstats);
        } else if(fstats->type == FileType::File) {
            std::ifstream filestream(file_fullpath, std::ifstream::binary);
            std::shared_ptr<unsigned char> file_buffer((unsigned char*)malloc(fstats->fsize), free);
            if(file_buffer == nullptr) {
                std::cerr << "[Error] Reached memory allocation limit for file " << ft_msg->filepath << std::endl;
                return std::make_shared<protocol::FileTransferResponse>();
            }
            filestream.read(reinterpret_cast<char*>(file_buffer.get()), fstats->fsize);
            if(!filestream) {
                std::cerr << "[Error] Failed to read data for " << ft_msg->filepath << std::endl;
            }
            return std::make_shared<protocol::FileTransferResponse>(file_buffer, fstats->fsize, *fstats);
        } else if(fstats->type == FileType::Link) {
            std::shared_ptr<unsigned char> link_buffer((unsigned char*)malloc(fstats->fsize + 1), free);
            if(readlink(file_fullpath.c_str(), reinterpret_cast<char*>(link_buffer.get()), fstats->fsize) == -1) {
                print_clib_error("readlink");
                return std::make_shared<protocol::FileTransferResponse>();
            }
            // Null terminate string
            link_buffer.get()[fstats->fsize] = 0;
            return std::make_shared<protocol::FileTransferResponse>(link_buffer, fstats->fsize + 1, *fstats);
        } else {
            std::cerr << "[Error] Failed to process unidentifiable item at path '" << file_fullpath << "'." << std::endl;
            return std::make_shared<protocol::FileTransferResponse>();
        }
    }


    void StateController::handle_file_transfer_response(std::shared_ptr<protocol::Message> msg, std::string filepath) {
        auto ft_msg = std::static_pointer_cast<protocol::FileTransferResponse>(msg);
        //std::cout << "Received " << filepath << " from peer (" << ft_msg->payload_len << " bytes) " << std::endl;

        std::string fullpath = join_path(path, filepath);

        // Create folder for file
        auto path_tokens = split_path(fullpath);
        auto file_folder = join_path("/", path_to_str(std::vector<std::string>(path_tokens.begin(), path_tokens.end() - 1)));
        if(!exists(file_folder)) {
            //std::cout << "[Warning] Out of order file transfer. Creating folder for file that should already exist." << std::endl;
            if(!ensure_dir(file_folder)) {
                std::cerr << "[Error] Failed to create directory " << file_folder << std::endl;
                return;
            }
        }

        if(ft_msg->ftype == FileType::Directory) {
            // Create folder
            if(!ensure_dir(fullpath)) {
                return;
            }
        } else if(ft_msg->ftype == FileType::File) {
            // Create file
            std::ofstream out_file(fullpath, std::ofstream::binary);
            if(out_file) {
                out_file.write(reinterpret_cast<char*>(ft_msg->payload.get()), ft_msg->payload_len);
            } else {
                std::cerr << "[Error] Could not open file " << fullpath << " for writing." << std::endl;
                return;
            }
        } else if(ft_msg->ftype == FileType::Link) {
            // Create symlink
            if(symlink(reinterpret_cast<char*>(ft_msg->payload.get()), fullpath.c_str()) == -1) {
                print_clib_error("symlink");
                return;
            }
        } else {
            std::cerr << "[Error] Received unknown file type in FileTransfer response! (" << static_cast<int>(ft_msg->ftype) << ")" << std::endl;
            return;
        }

        // TODO: Return error codes
        set_timestamp(fullpath, ft_msg->modification_time, ft_msg->access_time);
    }


    void StateController::do_merge() {
        state = ResolvingConflicts;
        state_lock.lock();
        auto sorted_peer_changes = sort_changes_by_file(peer_changes);
        state_lock.unlock();

        print_sorted_changes(sorted_peer_changes);

        std::cout << "Merging..." << std::endl;
        // These start out empty by default
        std::unordered_map<std::string, ConflictResolution> resolutions{};
        auto [merged_sorted_changes, operations, conflicts] = merge_change_sets(sort_changes_by_file(read_changes(path)), sorted_peer_changes, resolutions);
        if(conflicts.size() > 0) {
            std::cerr << "!!! Merge conflicts occured for the following paths:" << std::endl;
            for(const auto &conflict : conflicts) { 
                std::cout << "    " << conflict.conflict_key << std::endl;
            }
            // TODO: Create user interface for conflict resolutions
        } else {
            state_lock.lock();
            pending_operations = operations;
            pending_changes = merged_sorted_changes;
            state_lock.unlock();
            //print_sorted_changes(merged_sorted_changes);
            std::cout << "Pending operations:" << std::endl;
            print_sorted_operations(operations);
            state = SyncingFiles;
        }
    }

    void StateController::do_sync() {
        // Contains the changes that have been committed to disk
        SortedChangeSet processed_changes{};
        // This is probably too strict of a lock
        state_lock.lock();
        for(const auto& file_ops : pending_operations) {
            // For us to accurately reproduce the new file history, all operations
            // have to be executed successfully. If this fails, we will have to resolve it
            // or leave the file history in a dirty state, which will be corrected at
            // the next database rebuild and merge. 
            bool all_successfull{true};
            for(const auto& op : file_ops.second) {
                std::string filepath{op.path};
                if(op.type == FileOperationType::Delete) {
                    if(remove_path(join_path(path, filepath))) {
                        // Removal succeeded
                    } else {
                        all_successfull = false;
                    }
                } else if(op.type == FileOperationType::Transfer) {
                    c->send_request(
                        std::make_shared<protocol::FileTransferRequest>(filepath),
                        [this, filepath](auto msg) { handle_file_transfer_response(msg, filepath); }
                    );
                } else {
                    std::cerr << "[Error] Could not perform unknown file operation " << op.type << std::endl;
                    all_successfull = false;
                }
            }
            if(all_successfull) {
                // file_ops.first = path
                processed_changes.emplace(file_ops.first, pending_changes.at(file_ops.first));
            }
        }
        state_lock.unlock();
    }

    void StateController::wait_for_state(State target_state) {
        while(target_state != state) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}