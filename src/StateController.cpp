#include "StateController.h"

#include "ConflictResolver.h"
#include "Errors.h"
#include "Terminal.h"
#include "Globals.h"
#include "Version.h"
#include "protocol/NetProtocolRegistry.h"

#include <memory>
#include <fstream>
#include <uuid/uuid.h>


using namespace fmerge::protocol;

namespace fmerge {

    StateController::~StateController() {
    }

    void StateController::run() {
        c->listen(
            [this](auto msg) { handle_message(msg); },
            [this]() { handle_peer_disconnect(); }
        );


        while(true) {
            auto old_state = state.load();
            switch(old_state) {
            case State::AwaitingVersion:
                LOG("Checking version" << std::endl);
                send_version();
                break;
            case State::SendTree:
                break;
            case State::ResolvingConflicts:
                do_merge();
                break;
            case State::SyncUserWait:
                if(g_ask_confirmation) {
                    ask_proceed();
                } else {
                    state_lock.lock();
                    state = State::SyncingFiles;
                    state_lock.unlock();
                }
                break;
            case State::SyncingFiles:
                LOG("Performing file sync. This may take a while..." << std::endl);
                do_sync();
                LOG("Waiting for peer to complete" << std::endl);
                break;
            case State::Finished:
                break;
            case State::Exiting:
                return;
            }
            wait_for_state_change(old_state);
        }    
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
        } else if(msg->type() == MsgType::ExitingState) {
            return handle_exiting_state_message(std::dynamic_pointer_cast<ExitingStateMessage>(msg));
        } else if(msg->type() == MsgType::ConflictResolutions) {
            return handle_resolutions_message(std::dynamic_pointer_cast<ConflictResolutionsMessage>(msg));
        } else {
            LOG("[Error] Received invalid message with type " << msg->type() << std::endl);
        }
    }


    void StateController::handle_peer_disconnect() {
        DEBUG("Peer disconnected." << std::endl);

        auto s = state.load();
        if(s == State::SyncUserWait) {
            LOG("Operation cancelled by peer" << std::endl);
            state_lock.lock();
            state = State::Exiting;
            state_lock.unlock();
            return;
        }
        if(s != State::Finished && s != State::Exiting) {
            std::cerr << "[Error] Peer disconnected unexpectedly!" << std::endl;
            exit(1);
        }
        // Otherwise, there is nothing to handle.
    }


    void StateController::send_version() {
        std::string our_uuid = config.value("uuid", "");
        std::string version_payload = std::string(g_fmerge_version) + ";" + our_uuid;
        c->send_message(
            std::make_shared<VersionMessage>(std::make_unique<StringPayload>(version_payload))
        );
    }


    void StateController::handle_version_message(std::shared_ptr<VersionMessage> msg) {
        auto& ver_payload = msg->get_payload();
        auto peer_version = ver_payload.substr(0, ver_payload.find(';'));

        auto version_ok = check_peer_version(g_fmerge_version, peer_version);
        if (version_ok != NoError) { 
            LOG("Version mismatch (code " << version_ok << "):" << std::endl);
            LOG(" Peer : " << peer_version << std::endl);
            LOG(" Local: " << g_fmerge_version << std::endl);
            LOG("Continue? ");
            auto user_choice = term()->prompt_choice("yn");
            if(user_choice == 'n') {
                state = State::Exiting;
                return;
            }
        }

        state = State::SendTree;  
        c->send_message(std::make_shared<ExitingStateMessage>(State::AwaitingVersion));
    }


    void StateController::handle_changes_message(std::shared_ptr<ChangesMessage> msg) {
        if(state == State::SendTree) {
            state_lock.lock();
            peer_changes = msg->get_payload();
            LOG("Received " << peer_changes.size() << " changes from peer" << std::endl);
            state_lock.unlock();

            state = State::ResolvingConflicts;
        } else {
            LOG("[Warning] Received unexpected 'Changes' message from peer" << std::endl);
        }
    }


    void StateController::handle_file_request_message(std::shared_ptr<FileRequestMessage> msg) {
        auto& ft_payload = msg->get_payload();
        DEBUG("Peer requested file " << ft_payload << std::endl);
        c->send_message(
            create_file_transfer_message(ft_payload)
        );
    }


    std::shared_ptr<FileTransferMessage> StateController::create_file_transfer_message(std::string ft_payload) {
        std::string file_fullpath = join_path(path, ft_payload);
        auto fstats = get_file_stats(file_fullpath);
        if(!fstats.has_value()) {
            std::cerr << "[Error] Peer requested a file that does not exist! (" << ft_payload << ")" << std::endl;
            return std::make_shared<FileTransferMessage>(std::make_unique<FileTransferPayload>(ft_payload));
        }

        if(fstats->type == FileType::Directory) {
            // Return an empty response. The host does not require any extra data to create a folder.
            DEBUG("Sending folder placeholder for " << ft_payload << std::endl);
            return std::make_shared<FileTransferMessage>(std::make_unique<FileTransferPayload>(ft_payload, nullptr, *fstats));
        } else if(fstats->type == FileType::File) {
            DEBUG("Sending file transfer for " << ft_payload << std::endl);
            std::ifstream filestream(file_fullpath, std::ifstream::binary);
            std::shared_ptr<unsigned char> file_buffer((unsigned char*)malloc(fstats->fsize), free);
            if(file_buffer == nullptr) {
                std::cerr << "[Error] Reached memory allocation limit for file " << ft_payload << std::endl;
                return std::make_shared<FileTransferMessage>(std::make_unique<FileTransferPayload>(ft_payload));
            }
            filestream.read(reinterpret_cast<char*>(file_buffer.get()), fstats->fsize);
            if(!filestream) {
                std::cerr << "[Error] Failed to read data for " << ft_payload << std::endl;
            }
            return std::make_shared<FileTransferMessage>(std::make_unique<FileTransferPayload>(ft_payload, file_buffer, *fstats));
        } else if(fstats->type == FileType::Link) {
            DEBUG("Sending link transfer for " << ft_payload << std::endl);
            std::shared_ptr<unsigned char> link_buffer((unsigned char*)malloc(fstats->fsize), free);
            if(readlink(file_fullpath.c_str(), reinterpret_cast<char*>(link_buffer.get()), fstats->fsize) == -1) {
                print_clib_error("readlink");
                return std::make_shared<FileTransferMessage>(std::make_unique<FileTransferPayload>(ft_payload));
            }
            return std::make_shared<FileTransferMessage>(std::make_unique<FileTransferPayload>(ft_payload, link_buffer, *fstats));
        } else {
            std::cerr << "[Error] Failed to process unidentifiable item at path '" << file_fullpath << "'." << std::endl;
            return std::make_shared<FileTransferMessage>(std::make_unique<FileTransferPayload>(ft_payload));
        }
    }


    void StateController::handle_exiting_state_message(std::shared_ptr<ExitingStateMessage> msg) {
        if(msg->get_payload().state == State::SyncUserWait) {
            term()->cancel_prompt();
            LOG("Continuing (triggered by peer)..." << std::endl);
            state_lock.lock();
            state = State::SyncingFiles;
            state_lock.unlock();
        } else if(msg->get_payload().state == State::SyncingFiles) {
            state_lock.lock();
            peer_finished.store(true);
            if(state.load() == State::Finished) {
                state = State::Exiting;
            }
            state_lock.unlock();
        } else if(msg->get_payload().state == State::AwaitingVersion) {
            // The user accepted the version difference at the peer
            term()->cancel_prompt();
            LOG("Sending file tree" << std::endl);
            send_filetree();
        } else {
            std::cerr << "Error: Received unknown exit state message from peer" << std::endl;
        }
    }


    void StateController::handle_file_transfer_message(std::shared_ptr<FileTransferMessage> msg) {
        //LOG("Received " << filepath << " from peer (" << ft_msg->payload_len << " bytes) " << std::endl);

        state_lock.lock();
        auto s = state.load();
        state_lock.unlock();
        if(s == State::SyncingFiles) {
            if(syncer) {
                syncer->submit_file_transfer(msg->get_payload());
            } else {
                std::cerr << "[Error] Invalid 'syncer' object" << std::endl;
            }
        } else {
            std::cerr << "[Error] Invalid file transfer message before we have entered the SyncingFiles state." << std::endl;
        }
    }


    void StateController::handle_resolutions_message(std::shared_ptr<ConflictResolutionsMessage> msg) {
        LOG("Received conflict resolutions from peer:" << std::endl);
        resolutions = msg->get_payload();
        for(const auto& resolution : resolutions) {
            LOG("    " << std::setw(64) << std::left << resolution.first << ": " << resolution.second << std::endl);
        }
        // Cancel asking for resolutions locally, since it has already been resolved on the remote
        term()->cancel_prompt();
        // This triggers the file sync
    }


    void StateController::send_filetree() {
        c->send_message(std::make_shared<ChangesMessage>(read_changes(path)));
    }


    void StateController::do_merge() {
        state = State::ResolvingConflicts;
        state_lock.lock();
        auto sorted_peer_changes = sort_changes_by_file(peer_changes);

        // print_sorted_changes(sorted_peer_changes);

        // LOG("Merging..." << std::endl);
        sorted_local_changes = sort_changes_by_file(read_changes(path));
        state_lock.unlock();
        
        std::vector<Conflict> conflicts;
        while((conflicts = attempt_merge(sorted_local_changes, sorted_peer_changes, resolutions)).empty() == false) {
            std::cerr << "!!! Merge conflicts occured for the following paths:" << std::endl;
            sort_conflicts_alphabetically(conflicts);
            print_conflicts(conflicts);
            LOG(std::endl);

            auto user_resolutions = ask_for_resolutions(conflicts, sorted_local_changes, sorted_peer_changes);
            // This may be empty, then keep the existing resolutions
            if(!user_resolutions.empty()) {
                resolutions = user_resolutions;

                auto peer_resolutions = translate_peer_resolutions(resolutions);
                c->send_message(
                    std::make_shared<ConflictResolutionsMessage>(peer_resolutions)
                );
            }
        }

        state = State::SyncUserWait;
    }


    std::vector<Conflict> StateController::attempt_merge(const SortedChangeSet& loc, const SortedChangeSet& rem, const std::unordered_map<std::string, ConflictResolution> &resolutions) {
        auto [merged_sorted_changes, conflicts] = merge_change_sets(loc, rem, resolutions);
        if(conflicts.size() > 0) {
            // Indicate failure
            return conflicts;
        }
        // Success
        state_lock.lock();
        pending_operations = construct_operation_set(loc, merged_sorted_changes);
        pending_changes = merged_sorted_changes;

        LOG("Pending operations:" << std::endl);
        print_sorted_operations(pending_operations);
        state_lock.unlock();
        return {};
    }


    void StateController::do_sync() {
        // Contains the changes that have been committed to disk
        //std::vector<Change> processed_changes{};
        unsigned long processed_change_count{0};
        unsigned long total_changes{pending_operations.size()};
        term()->start_progress_bar("Syncing");        

        syncer = std::make_unique<Syncer>(pending_operations, path, *c, [this, &processed_change_count, total_changes](std::string file, bool successful) {
            // Update file change log
            if(successful) {
                sorted_local_changes.insert_or_assign(file, pending_changes.at(file));
            }

            // Update status bar
            processed_change_count++;
            if(processed_change_count % 250 == 0) {
                term()->update_progress_bar(static_cast<float>(processed_change_count) / static_cast<float>(total_changes));
            }
        });
        // This is where the file sync is performed
        syncer->perform_sync();

        term()->complete_progress_bar();

        write_changes(path, recombine_changes_by_file(sorted_local_changes));
        LOG("Saved changes to disk" << std::endl);

        if(syncer->get_error_count() > 0) {
            // Set the global exit code to 1
            g_exit_code = 1;
            std::stringstream ss{};
            ss << "WARNING: " << syncer->get_error_count() << " errors encountered while syncing!";
            LOG("================================================================================" << std::endl);
            LOG(make_centered(ss.str(), 80, '=') << std::endl);
            LOG("================================================================================" << std::endl);
        }

        state_lock.lock();
        if(peer_finished.load()) {
            state = State::Exiting;
        } else {
            state = State::Finished;
        }
        state_lock.unlock();

        // Notify our peer that we are done
        c->send_message(std::make_shared<ExitingStateMessage>(State::SyncingFiles));
    }


    void StateController::ask_proceed() {
        term()->prompt_choice_async("yn", [this](char choice) {
            if(choice == 'y') {
                c->send_message(std::make_shared<ExitingStateMessage>(state.load()));
                state_lock.lock();
                state = State::SyncingFiles;
                state_lock.unlock();
            } else {
                state_lock.lock();
                state = State::Exiting;
                state_lock.unlock();
            }
        });
    }


    void StateController::wait_for_state_change(State current_state) {
        while(current_state == state) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}