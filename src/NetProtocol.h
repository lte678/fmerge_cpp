#pragma once

#include "FileTree.h"

#include <sstream>
#include <array>
#include <vector>


namespace fmerge::protocol {

    // Warning: last bit is reserved for the request/response flag
    typedef unsigned short MessageType;
    
    constexpr MessageType MsgIgnore = 1;
    constexpr MessageType MsgVersion = 2;
    constexpr MessageType MsgChanges = 3;
    constexpr MessageType MsgUnknown = 5;
    constexpr MessageType MsgRequestFlag = 0x8000;

    std::string msg_type_to_string(const MessageType msg);


    struct Message {
        virtual void serialize(int fd) const = 0;

        virtual MessageType raw_type() const = 0;
        virtual unsigned long length() const = 0;

        bool is_request() const { return raw_type() & MsgRequestFlag; };
        MessageType type() const { return raw_type() & ~MsgRequestFlag; };
    };


    typedef std::function<void(void*,size_t)> ReadFunc;


    struct EmptyRequest : public Message{
        // A request that does not require any extra parameters, such as a VersionRequest or ChangesRequest.
        void serialize(int) const override {};
        unsigned long length() const override { return 0; };
    };

    
    struct VersionRequest : public EmptyRequest {
        MessageType raw_type() const override { return MsgRequestFlag | MsgVersion; };
    };


    struct ChangesRequest : public EmptyRequest {
        MessageType raw_type() const override { return MsgRequestFlag | MsgChanges; };
    };


    struct VersionResponse : public Message {
        VersionResponse(int _major, int _minor, std::array<unsigned char, 16> _uuid) : major(_major), minor(_minor), uuid(_uuid) {};

        int major{};
        int minor{};
        std::array<unsigned char, 16> uuid{};

        void serialize(int fd) const override;
        static VersionResponse deserialize(ReadFunc receive);

        MessageType raw_type() const override { return MsgVersion; };
        unsigned long length() const override { return sizeof(major) + sizeof(minor) + 16; };
    };


    struct ChangesResponse  : public Message {
        // Transmit all the changes that the connection partner has not yet received.
        // The server decides which information is missing, since this index must be
        // maintained locally, in accordance with any changes that happen.
        ChangesResponse(std::vector<Change> _changes);

        std::vector<Change> changes;
        std::string serialized_changes;

        void serialize(int fd) const override;
        static ChangesResponse deserialize(ReadFunc receive, unsigned long length);

        inline MessageType raw_type() const { return MsgChanges; };
        inline unsigned long length() const { return serialized_changes.length(); };
    };


    std::shared_ptr<Message> deserialize_packet(MessageType raw_type, unsigned long length, ReadFunc receive);
}