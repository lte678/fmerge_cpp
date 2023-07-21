#pragma once

#include "FileTree.h"

#include <sstream>
#include <array>
#include <vector>


namespace fmerge::protocol {

    // Warning: last bit is reserved for the request/response flag
    typedef unsigned short MessageType;
    
    constexpr unsigned short MsgIgnore = 1;
    constexpr unsigned short MsgVersion = 2;
    constexpr unsigned short MsgChanges = 3;
    constexpr unsigned short MsgUnknown = 4;

    std::string msg_type_to_string(const MessageType msg);


    struct Message {
        virtual void serialize(int fd) const = 0;

        virtual MessageType type() const = 0;
        virtual unsigned long length() const = 0;
    };


    typedef std::function<void(void*,size_t)> ReadFunc;


    struct VersionMessage : public Message {
        VersionMessage(int _major, int _minor, std::array<unsigned char, 16> _uuid) : major(_major), minor(_minor), uuid(_uuid) {};

        int major{};
        int minor{};
        std::array<unsigned char, 16> uuid{};

        void serialize(int fd) const override;
        static VersionMessage deserialize(ReadFunc receive);

        MessageType type() const override { return MsgVersion; };
        unsigned long length() const override { return sizeof(major) + sizeof(minor) + 16; };
    };


    struct ChangesMessage  : public Message {
        // Transmit all the changes that the connection partner has not yet received.
        // The server decides which information is missing, since this index must be
        // maintained locally, in accordance with any changes that happen.
        ChangesMessage(std::vector<Change> _changes);

        std::vector<Change> changes;
        std::string serialized_changes;

        void serialize(int fd) const override;
        static ChangesMessage deserialize(ReadFunc receive, unsigned long length);

        inline MessageType type() const { return MsgChanges; };
        inline unsigned long length() const { return serialized_changes.length(); };
    };


    std::shared_ptr<Message> deserialize_packet(MessageType type, unsigned long length, ReadFunc receive);
}