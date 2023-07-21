#include "StateController.h"

#include "NetProtocol.h"
#include "Version.h"

#include <memory>
#include <fstream>
#include <uuid/uuid.h>


namespace fmerge {
    void StateController::run() {
        c->listen([this](auto msg_type) { return handle_request(msg_type); });

        c->send_request(protocol::MsgVersion, [this](auto msg) { handle_version_response(msg); });

        while(true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }


    std::shared_ptr<protocol::Message> StateController::handle_request(protocol::MessageType msg_type) {
        if(msg_type == protocol::MsgVersion) {
            return handle_version_request();
        } else if(msg_type == protocol::MsgChanges) {
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
        return std::make_shared<protocol::VersionMessage>(MAJOR_VERSION, MINOR_VERSION, uuid);
    }


    void StateController::handle_version_response(std::shared_ptr<protocol::Message> msg) {
        auto ver_msg = std::dynamic_pointer_cast<protocol::VersionMessage>(msg);
        
        if (ver_msg->major != MAJOR_VERSION || ver_msg->minor != MINOR_VERSION) { 
            std::cerr << "Peer has invalid version!";
            std::cerr << " Peer : v" << ver_msg->major << "." << ver_msg->minor;
            std::cerr << " Local: v" << MAJOR_VERSION << "." << MINOR_VERSION;
            return;
        }

        if(state == State::AwaitingVersion) {
            state = State::SendTree;

            std::cout << "Requesting file tree" << std::endl;
            c->send_request(protocol::MsgChanges, [this](auto msg) { handle_changes_response(msg); });
        }
    }


    std::shared_ptr<protocol::Message> StateController::handle_changes_request() {
        std::string changes_path = join_path(path, ".fmerge/filechanges.db");
        std::vector<Change> changes{};
        if(exists(changes_path)) {
            std::ifstream changes_file(changes_path);
            changes = read_changes(changes_file);
        }
        return std::make_shared<protocol::ChangesMessage>(changes);
    }

    void StateController::handle_changes_response(std::shared_ptr<protocol::Message> msg) {
        auto changes_msg = std::dynamic_pointer_cast<protocol::ChangesMessage>(msg);

        state_lock.lock();
        peer_changes = changes_msg->changes;

        std::cout << "Received " << peer_changes.size() << " changes from peer" << std::endl;
        for(const auto &change : peer_changes) {
            std::cout << "    " << change << std::endl;
        }
        state_lock.unlock();
    }
}