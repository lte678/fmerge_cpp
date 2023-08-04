#pragma once

#include "FileTree.h"

#include <sstream>
#include <array>
#include <vector>


namespace fmerge::protocol {

    typedef std::function<void(void*,size_t)> ReadFunc;
    typedef std::function<void(const void*,size_t)> WriteFunc;

    // Warning: last bit is reserved for the request/response flag
    typedef unsigned short MessageType;
    
    constexpr MessageType MsgIgnore       = 1;
    constexpr MessageType MsgVersion      = 2;
    constexpr MessageType MsgChanges      = 3;
    constexpr MessageType MsgFileTransfer = 4;
    constexpr MessageType MsgStartSync    = 5;
    constexpr MessageType MsgUnknown      = 0x4000;
    constexpr MessageType MsgRequestFlag  = 0x8000;

    std::string msg_type_to_string(const MessageType msg);


    struct Message {
        virtual void serialize(WriteFunc write) const = 0;

        virtual MessageType raw_type() const = 0;
        virtual unsigned long length() const = 0;

        bool is_request() const { return raw_type() & MsgRequestFlag; };
        MessageType type() const { return raw_type() & ~MsgRequestFlag; };
    };


    struct EmptyRequest : public Message {
        // A request that does not require any extra parameters, such as a VersionRequest or ChangesRequest.
        void serialize(WriteFunc) const override {};
        unsigned long length() const override { return 0; };
    };

    
    struct VersionRequest : public EmptyRequest {
        MessageType raw_type() const override { return MsgRequestFlag | MsgVersion; };
    };


    struct ChangesRequest : public EmptyRequest {
        MessageType raw_type() const override { return MsgRequestFlag | MsgChanges; };
    };

    struct StartSyncRequest : public EmptyRequest {
        MessageType raw_type() const override { return MsgRequestFlag | MsgStartSync; };
    };


    struct FileTransferRequest : public Message {
        FileTransferRequest(std::string _filepath) : filepath(_filepath) {};

        std::string filepath;

        void serialize(WriteFunc write) const override;
        static FileTransferRequest deserialize(ReadFunc receive, unsigned long length);

        MessageType raw_type() const override { return MsgRequestFlag | MsgFileTransfer; };
        unsigned long length() const override { return filepath.length(); };
    };

    struct VersionResponse : public Message {
        VersionResponse(int _major, int _minor, std::array<unsigned char, 16> _uuid) : major(_major), minor(_minor), uuid(_uuid) {};

        int major{};
        int minor{};
        std::array<unsigned char, 16> uuid{};

        void serialize(WriteFunc write) const override;
        static VersionResponse deserialize(ReadFunc receive);

        MessageType raw_type() const override { return MsgVersion; };
        unsigned long length() const override { return sizeof(major) + sizeof(minor) + 16; };
    };

    struct StartSyncResponse : public Message {
        void serialize(WriteFunc) const override {};
        static StartSyncResponse deserialize() { return StartSyncResponse(); };
        MessageType raw_type() const override { return MsgStartSync; };
        unsigned long length() const override { return 0; };
    };

    struct ChangesResponse : public Message {
        // Transmit all the changes that the connection partner has not yet received.
        // The server decides which information is missing, since this index must be
        // maintained locally, in accordance with any changes that happen.
        ChangesResponse(std::vector<Change> _changes);

        std::vector<Change> changes;
        std::string serialized_changes;

        void serialize(WriteFunc write) const override;
        static ChangesResponse deserialize(ReadFunc receive, unsigned long length);

        inline MessageType raw_type() const { return MsgChanges; };
        inline unsigned long length() const { return serialized_changes.length(); };
    };


    struct FileTransferResponse : public Message {
        FileTransferResponse()
            : payload(nullptr), payload_len(0), ftype(FileType::Unknown),
            modification_time(0), access_time(0) {};
        FileTransferResponse(std::shared_ptr<unsigned char> _payload, unsigned long _payload_len, FileType _ftype, long _mtime, long _ctime) 
            : payload(_payload), payload_len(_payload_len), ftype(_ftype),
            modification_time(_mtime), access_time(_ctime) {};
        FileTransferResponse(std::shared_ptr<unsigned char> _payload, unsigned long _payload_len, FileStats _stats) 
            : payload(_payload), payload_len(_payload_len), ftype(_stats.type),
            modification_time(_stats.mtime), access_time(_stats.atime) {};

        std::shared_ptr<unsigned char> payload;
        unsigned long payload_len;
        FileType ftype;
        long modification_time;
        long access_time;

        void serialize(WriteFunc write) const override;
        static FileTransferResponse deserialize(ReadFunc receive, unsigned long length);

        inline MessageType raw_type() const { return MsgFileTransfer; };
        // One extra byte for the is_folder flag
        inline unsigned long length() const { 
            return payload_len + sizeof(unsigned char) + sizeof(modification_time) + sizeof(access_time); 
        };
    };


    std::shared_ptr<Message> deserialize_packet(MessageType raw_type, unsigned long length, ReadFunc receive);
}