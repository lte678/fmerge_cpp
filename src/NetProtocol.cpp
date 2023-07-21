#include "NetProtocol.h"

#include <unistd.h>


namespace fmerge::protocol {

    std::string msg_type_to_string(const MessageType msg) {
        if(msg == MsgIgnore) {
            return "IGNORE";
        } else if (msg == MsgVersion) {
            return "VERSION";
        } else if (msg == MsgChanges) {
            return "CHANGES";
        } else if (msg == MsgUnknown) {
            return "UNKNOWN";
        }
        return "INVALID";
    }


    void VersionMessage::serialize(int fd) const {
        // Note:: Serialize adds the header, while deserialize does not expect it!
        unsigned int netmajor = htole32(major);
        write(fd, &netmajor, sizeof(netmajor));
        unsigned int netminor = htole32(minor);
        write(fd, &netminor, sizeof(netminor));

        write(fd, uuid.data(), uuid.size());
    }


    VersionMessage VersionMessage::deserialize(ReadFunc receive) {
        unsigned int major{};
        receive(&major, sizeof(major));
        major = le32toh(major);
        unsigned int minor{};
        receive(&minor, sizeof(minor));
        minor = le32toh(minor);

        std::array<unsigned char, 16> uuid;
        receive(uuid.data(), uuid.size());

        return VersionMessage(major, minor, uuid);
    }


    ChangesMessage::ChangesMessage(std::vector<Change> changes) : serialized_changes("") {
        std::stringstream ser_stream;
        write_changes(ser_stream, changes);
        serialized_changes = ser_stream.str();
    }


    void ChangesMessage::serialize(int fd) const {
        write(fd, serialized_changes.c_str(), length());
    }


    ChangesMessage ChangesMessage::deserialize(ReadFunc receive, unsigned long length) {
        char *change_buffer = new char[length];
        receive(change_buffer, length);

        std::stringstream change_stream(change_buffer);
        auto changes = read_changes(change_stream);
        return ChangesMessage(changes);
    }


    std::shared_ptr<Message> deserialize_packet(MessageType type, unsigned long length, ReadFunc receive) {
        if(type == MsgVersion) {
            return std::make_shared<VersionMessage>(VersionMessage::deserialize(receive));
        } else if(type == MsgChanges) {
            return std::make_shared<ChangesMessage>(ChangesMessage::deserialize(receive, length));
        } else {
            std::cerr << "Cannot deserialize message type " << msg_type_to_string(type);
            return nullptr;
        }
    }
}