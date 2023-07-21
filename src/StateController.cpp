#include "StateController.h"

#include "NetProtocol.h"
#include "Version.h"

#include <memory>
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
        if(msg->type() != protocol::MsgVersion) {
            std::cerr << "Received invalid response to version request!" << std::endl;
            return;
        }
        std::shared_ptr<protocol::VersionMessage> ver_msg = std::dynamic_pointer_cast<protocol::VersionMessage>(msg);
        
        if (ver_msg->major != MAJOR_VERSION || ver_msg->minor != MINOR_VERSION) { 
            std::cerr << "Peer has invalid version!";
            std::cerr << " Peer : v" << ver_msg->major << "." << ver_msg->minor;
            std::cerr << " Local: v" << MAJOR_VERSION << "." << MINOR_VERSION;
            return;
        }

        if(state == State::AwaitingVersion) {
            state = State::SendTree;

            std::cout << "Requesting file tree" << std::endl;
        }
    }

}