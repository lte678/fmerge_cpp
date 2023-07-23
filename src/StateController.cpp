#include "StateController.h"

#include "NetProtocol.h"
#include "Version.h"
#include "Terminal.h"

#include <memory>
#include <fstream>
#include <uuid/uuid.h>


namespace fmerge {
    void StateController::run() {
        c->listen([this](auto msg_type) { return handle_request(msg_type); });

        c->send_request(std::make_shared<protocol::VersionRequest>(), [this](auto msg) { handle_version_response(msg); });

        while(true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }


    std::shared_ptr<protocol::Message> StateController::handle_request(std::shared_ptr<protocol::Message> msg) {
        if(msg->type() == protocol::MsgVersion) {
            return handle_version_request();
        } else if(msg->type() == protocol::MsgChanges) {
            return handle_changes_request();
        }
        // Error message is handled caller function
        return nullptr;
    }


    std::shared_ptr<protocol::Message> StateController::handle_version_request() {
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

        if(state == State::AwaitingVersion) {
            state = State::SendTree;

            std::cout << "Requesting file tree" << std::endl;
            c->send_request(std::make_shared<protocol::ChangesRequest>(), [this](auto msg) { handle_changes_response(msg); });
        }
    }


    std::shared_ptr<protocol::Message> StateController::handle_changes_request() {
        return std::make_shared<protocol::ChangesResponse>(read_changes(path));
    }

    void StateController::handle_changes_response(std::shared_ptr<protocol::Message> msg) {
        auto changes_msg = std::dynamic_pointer_cast<protocol::ChangesResponse>(msg);

        if(state == State::SendTree) {
            state_lock.lock();
            peer_changes = changes_msg->changes;
            std::cout << "Received " << peer_changes.size() << " changes from peer" << std::endl;
            state_lock.unlock();

            do_merge();

            exit(0);
        }
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
            pending_operations = operations;
            //print_sorted_changes(merged_sorted_changes);
            std::cout << "Pending operations:" << std::endl;
            print_sorted_operations(pending_operations);
            state = SyncingFiles;

            do_sync(merged_sorted_changes);
        }
    }

    void StateController::do_sync(const SortedChangeSet &target_changes) {
        // Contains the changes that have been committed to disk
        SortedChangeSet processed_changes{};
        for(const auto& file_ops : pending_operations) {
            // For us to accurately reproduce the new file history, all operations
            // have to be executed successfully. If this fails, we will have to resolve it
            // or leave the file history in a dirty state, which will be corrected at
            // the next database rebuild and merge. 
            bool all_successfull{true};
            for(const auto& op : file_ops.second) {
                if(op.type == FileOperationType::Delete) {
                    if(remove_path(join_path(path, op.path))) {
                        // Removal succeeded
                    } else {
                        all_successfull = false;
                    }
                } else if(op.type == FileOperationType::Transfer) {
                    //c->send_request(protocol::MsgFileTransfer, [this](auto msg) { handle_version_response(msg); });
                    std::cout << "TODO: Transfer" << std::endl;
                } else {
                    std::cerr << "[Error] Could not perform unknown file operation " << op.type << std::endl;
                    all_successfull = false;
                }
            }
            if(all_successfull) {
                // file_ops.first = path
                processed_changes.emplace(file_ops.first, target_changes.at(file_ops.first));
            }
        }
    }
}