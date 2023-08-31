#include "StateController.h"

#include "ConflictResolver.h"
#include "Errors.h"
#include "Version.h"
#include "Terminal.h"
#include "protocol/NetProtocolRegistry.h"

#include <memory>
#include <fstream>
#include <uuid/uuid.h>


using namespace fmerge::protocol;

namespace fmerge {

    StateController::~StateController() {
    }

    void StateController::run() {
        c->listen([this](auto msg) { handle_message(msg); });

        termbuf() << "Checking version" << std::endl;
        send_version();

        wait_for_state(State::SendTree);
        termbuf() << "Sending file tree" << std::endl;
        send_filetree();

        wait_for_state(State::ResolvingConflicts);
        do_merge();

        wait_for_state(State::SyncUserWait);
        if(!ask_proceed()) {
            return;
        }
        wait_for_state(State::SyncingFiles);
        termbuf() << "Performing file sync. This may take a while..." << std::endl;
        do_sync();

        termbuf() << "Waiting for peer to complete" << std::endl;
        wait_for_state(State::Finished);
    }


    void StateController::handle_message(std::shared_ptr<GenericMessage> msg) {
        if(msg->type() == MsgType::Version) {
            return handle_version_message(std::dynamic_pointer_cast<VersionMessage>(msg));
        } else if(msg->type() == MsgType::Changes) {
            return handle_changes_message(std::dynamic_pointer_cast<ChangesMessage>(msg));
        } else if(msg->type() == MsgType::FileTransfer) {
            return handle_file_transfer_message(std::dynamic_pointer_cast<FileTransferMessage>(msg));
        } else if(msg->type() == MsgType::FileRequest) {
            return handle_file_request_message(std::dynamic_pointer_cast<FileRequestMessage>(msg));
        } else if(msg->type() == MsgType::StartSync) {
            return handle_start_sync_message(std::dynamic_pointer_cast<StartSyncMessage>(msg));
        } else if(msg->type() == MsgType::ConflictResolutions) {
            return handle_resolutions_message(std::dynamic_pointer_cast<ConflictResolutionsMessage>(msg));
        } else {
            termbuf() << "[Error] Received invalid message with type " << msg->type() << std::endl;
        }
    }


    void StateController::send_version() {
        std::array<unsigned char, 16> uuid{};
        std::string our_uuid = config.value("uuid", "");
        if(uuid_parse(our_uuid.c_str(), uuid.data()) == -1) {
            std::cerr << "Error parsing our UUID!" << std::endl;
            return;
        }
        c->send_message(
            std::make_shared<VersionMessage>(VersionPayload{.major=MAJOR_VERSION, .minor=MINOR_VERSION, .uuid=uuid})
        );
    }


    void StateController::handle_version_message(std::shared_ptr<VersionMessage> msg) {
        auto& ver_payload = msg->get_payload();

        if (ver_payload.major != MAJOR_VERSION || ver_payload.minor != MINOR_VERSION) { 
            std::cerr << "Peer has invalid version!";
            std::cerr << " Peer : v" << ver_payload.major << "." << ver_payload.minor;
            std::cerr << " Local: v" << MAJOR_VERSION << "." << MINOR_VERSION;
            return;
        }

        if(state == State::AwaitingVersion) state = State::SendTree;            
    }


    void StateController::handle_changes_message(std::shared_ptr<ChangesMessage> msg) {
        if(state == State::SendTree) {
            state_lock.lock();
            peer_changes = msg->get_payload();
            termbuf() << "Received " << peer_changes.size() << " changes from peer" << std::endl;
            state_lock.unlock();

            state = State::ResolvingConflicts;
        }
    }


    void StateController::handle_file_request_message(std::shared_ptr<FileRequestMessage> msg) {
        auto& ft_payload = msg->get_payload();
        c->send_message(
            create_file_transfer_message(ft_payload)
        );
    }


    std::shared_ptr<FileTransferMessage> StateController::create_file_transfer_message(std::string ft_payload) {
        std::string file_fullpath = join_path(path, ft_payload);
        auto fstats = get_file_stats(file_fullpath);
        if(!fstats.has_value()) {
            std::cerr << "[Error] Peer requested a file that does not exist! (" << ft_payload << ")" << std::endl;
            return std::make_shared<FileTransferMessage>(ft_payload);
        }

        if(fstats->type == FileType::Directory) {
            // Return an empty response. The host does not require any extra data to create a folder.
            return std::make_shared<FileTransferMessage>(ft_payload, nullptr, *fstats);
        } else if(fstats->type == FileType::File) {
            std::ifstream filestream(file_fullpath, std::ifstream::binary);
            std::shared_ptr<unsigned char> file_buffer((unsigned char*)malloc(fstats->fsize), free);
            if(file_buffer == nullptr) {
                std::cerr << "[Error] Reached memory allocation limit for file " << ft_payload << std::endl;
                return std::make_shared<FileTransferMessage>();
            }
            filestream.read(reinterpret_cast<char*>(file_buffer.get()), fstats->fsize);
            if(!filestream) {
                std::cerr << "[Error] Failed to read data for " << ft_payload << std::endl;
            }
            return std::make_shared<FileTransferMessage>(file_buffer, fstats->fsize, *fstats);
        } else if(fstats->type == FileType::Link) {
            std::shared_ptr<unsigned char> link_buffer((unsigned char*)malloc(fstats->fsize + 1), free);
            if(readlink(file_fullpath.c_str(), reinterpret_cast<char*>(link_buffer.get()), fstats->fsize) == -1) {
                print_clib_error("readlink");
                return std::make_shared<FileTransferMessage>(ft_payload);
            }
            // Null terminate string
            link_buffer.get()[fstats->fsize] = 0;
            return std::make_shared<FileTransferMessage>(link_buffer, fstats->fsize + 1, *fstats);
        } else {
            std::cerr << "[Error] Failed to process unidentifiable item at path '" << file_fullpath << "'." << std::endl;
            return std::make_shared<FileTransferMessage>(ft_payload);
        }
    }


    void StateController::handle_start_sync_message(std::shared_ptr<StartSyncMessage>) {
        state_lock.lock();
        state = State::SyncingFiles;
        state_lock.unlock();
    }


    void StateController::handle_file_transfer_message(std::shared_ptr<FileTransferMessage> msg) {
        //termbuf() << "Received " << filepath << " from peer (" << ft_msg->payload_len << " bytes) " << std::endl;
        auto& ft_payload = msg->get_payload();

        std::string fullpath = join_path(path, ft_payload.path);

        // Create folder for file
        auto path_tokens = split_path(fullpath);
        auto file_folder = join_path("/", path_to_str(std::vector<std::string>(path_tokens.begin(), path_tokens.end() - 1)));
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
            if(symlink(reinterpret_cast<char*>(ft_payload.payload.get()), fullpath.c_str()) == -1) {
                print_clib_error("symlink");
                return;
            }
        } else {
            std::cerr << "[Error] Received unknown file type in FileTransfer response! (" << static_cast<int>(ft_payload.ftype) << ")" << std::endl;
            return;
        }

        // TODO: Return error codes
        set_timestamp(fullpath, ft_payload.mod_time, ft_payload.access_time);
    }


    void StateController::handle_resolutions_message(std::shared_ptr<ConflictResolutionsMessage> msg) {
        termbuf() << "Received conflict resolutions from peer:" << std::endl;
        auto& resolutions = msg->get_payload();
        for(const auto& resolution : resolutions) {
            termbuf() << "    " << resolution.first << ":" << resolution.second << std::endl;
        }
        termbuf() << "TODO: Interrupt ask for resolutions!" << std::endl;
    }


    void StateController::send_filetree() {
        c->send_message(std::make_shared<ChangesMessage>(read_changes(path)));
    }


    void StateController::do_merge() {
        state = ResolvingConflicts;
        state_lock.lock();
        auto sorted_peer_changes = sort_changes_by_file(peer_changes);

        // print_sorted_changes(sorted_peer_changes);

        // termbuf() << "Merging..." << std::endl;
        sorted_local_changes = sort_changes_by_file(read_changes(path));
        state_lock.unlock();
        
        while(true) {
            auto [merged_sorted_changes, operations, conflicts] = merge_change_sets(sorted_local_changes, sorted_peer_changes, resolutions);

            if(conflicts.size() > 0) {
                std::cerr << "!!! Merge conflicts occured for the following paths:" << std::endl;
                for(const auto &conflict : conflicts) { 
                    termbuf() << "    " << "CONFLICT  " << conflict.conflict_key << std::endl;
                }
            
                resolutions = ask_for_resolutions(conflicts, sorted_local_changes, sorted_peer_changes);
            } else {
                state_lock.lock();
                pending_operations = operations;
                pending_changes = merged_sorted_changes;
                state_lock.unlock();

                // Forward the now valid list of resolutions to the remote
                if(resolutions.size() > 0) {
                    c->send_message(
                        std::make_shared<ConflictResolutionsMessage>(resolutions)
                    );
                }

                //print_sorted_changes(merged_sorted_changes);
                termbuf() << "Pending operations:" << std::endl;
                print_sorted_operations(operations);
                state = SyncUserWait;
                return;
            }
        }
    }


    void StateController::do_sync() {
        // Contains the changes that have been committed to disk
        //std::vector<Change> processed_changes{};
        unsigned long processed_change_count{0};
        // This is probably too strict of a lock
        for(const auto& file_ops : pending_operations) {
            if(processed_change_count % 250 == 0) {
                term()->update_progress_bar(static_cast<float>(processed_change_count) / static_cast<float>(pending_operations.size()), "Syncing");
            }
            
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
                    c->send_message(
                        std::make_shared<FileRequestMessage>(filepath)
                    );
                } else {
                    std::cerr << "[Error] Could not perform unknown file operation " << op.type << std::endl;
                    all_successfull = false;
                }
            }
            if(all_successfull) {
                // file_ops.first = path
                sorted_local_changes.insert_or_assign(file_ops.first, pending_changes.at(file_ops.first));
            } else {
                std::cerr << "[Error] File " << file_ops.first << " is in a conflicted state!" << std::endl;
            }
            processed_change_count++;
        }
        state_lock.unlock();

        term()->complete_progress_bar();

        // This creates duplicates once we also update the filetree on-disk structure at the next execution
        //write_changes(path, recombine_changes_by_file(sorted_local_changes));
        termbuf() << "Saved changes to disk" << std::endl;
    }


    bool StateController::ask_proceed() {
        char response = term()->prompt_choice("yn");
        if(response == 'y') {
            c->send_message(std::make_shared<StartSyncMessage>());
            return true;
        }
        termbuf() << "Exiting." << std::endl;
        return false;
    }


    void StateController::wait_for_state(State target_state) {
        while(target_state != state) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}