#pragma once

#include "GenericMessage.h"
#include "../FileTree.h"
#include "../MergeAlgorithms.h"
#include "../ApplicationState.h"

#include <sstream>
#include <array>
#include <vector>


namespace fmerge::protocol {
    
    // --------------------------------------------------------------------------------
    // ---------------------------- Payload Definitions -------------------------------
    // --------------------------------------------------------------------------------

    struct VersionPayload {
        VersionPayload(int _major, int _minor, std::array<unsigned char, 16> _uuid) :
            major(_major), minor(_minor), uuid(_uuid) {}

        int major{};
        int minor{};
        std::array<unsigned char, 16> uuid{};

        void serialize(WriteFunc write) const;
        static std::unique_ptr<VersionPayload> deserialize(ReadFunc receive, unsigned long length);
    };


    struct ChangesPayload : public std::vector<Change> {
        using std::vector<Change>::vector;
        ChangesPayload(std::vector<Change> _other) : std::vector<Change>(_other) {}

        void serialize(WriteFunc write) const;
        static std::unique_ptr<ChangesPayload> deserialize(ReadFunc receive, unsigned long length);
    };


    struct FileTransferPayload {
        FileTransferPayload(std::string _path, std::shared_ptr<unsigned char> file_contents, unsigned long file_len, FileType type, long mtime, long atime)
            : path(_path), payload(file_contents), payload_len(file_len), ftype(type), mod_time(mtime), access_time(atime) {}
        // Constructor used to create empty packets indicating an error.
        FileTransferPayload(std::string _path) :
            path(_path), payload(nullptr), payload_len(0), ftype(FileType::Unknown), mod_time(0), access_time(0) {}
        // Constructor for fstat object 
        FileTransferPayload(std::string _path, std::shared_ptr<unsigned char> file_contents, const FileStats& stats)
            : path(_path), payload(file_contents), payload_len(0), ftype(stats.type), mod_time(stats.mtime), access_time(stats.atime) {
            if(stats.type != FileType::Directory) {
                payload_len = stats.fsize;
            }
        }

        std::string path;
        std::shared_ptr<unsigned char> payload;
        unsigned long payload_len;
        FileType ftype;
        long mod_time;
        long access_time;

        void serialize(WriteFunc write) const;
        static std::unique_ptr<FileTransferPayload> deserialize(ReadFunc receive, unsigned long length);
    };


    struct StringPayload : public std::string {
        using std::string::string;
        StringPayload(std::string _other) : std::string(_other) {}

        void serialize(WriteFunc write) const;
        static std::unique_ptr<StringPayload> deserialize(ReadFunc receive, unsigned long length);
    };


    struct ConflictResolutionPayload : public std::unordered_map<std::string, ConflictResolution> {
        using std::unordered_map<std::string, ConflictResolution>::unordered_map;
        ConflictResolutionPayload(std::unordered_map<std::string, ConflictResolution> _other) :
            std::unordered_map<std::string, ConflictResolution>(_other) {}

        void serialize(WriteFunc write) const;
        static std::unique_ptr<ConflictResolutionPayload> deserialize(ReadFunc receive, unsigned long length);
    };


    struct StatePayload {
        StatePayload(State _state) : state(_state) {}

        State state;

        void serialize(WriteFunc write) const;
        static std::unique_ptr<StatePayload> deserialize(ReadFunc receive, unsigned long length);
    };


    // --------------------------------------------------------------------------------
    // ---------------------------- Message Definitions -------------------------------
    // --------------------------------------------------------------------------------

    class IgnoreMessage : public EmptyMessage {
    public:
        using EmptyMessage::EmptyMessage;
        MsgType type() const override { return MsgType::Ignore; }
    };


    class VersionMessage : public Message<VersionPayload> {
    public:
        using Message<VersionPayload>::Message;
        VersionMessage() = delete;
        MsgType type() const override { return MsgType::Version; }
    };


    class ChangesMessage : public Message<ChangesPayload> {
    public:
        using Message<ChangesPayload>::Message;
        ChangesMessage() = delete;
        MsgType type() const override { return MsgType::Changes; }
    };


    class FileTransferMessage : public Message<FileTransferPayload> {
    public:
        using Message<FileTransferPayload>::Message;
        FileTransferMessage() = delete;
        MsgType type() const override { return MsgType::FileTransfer; }
    };

    class FileRequestMessage : public Message<StringPayload> {
    public:
        using Message<StringPayload>::Message;
        FileRequestMessage() = delete;
        MsgType type() const override { return MsgType::FileRequest; }
    };


    class ExitingStateMessage : public Message<StatePayload> {
    public:
        using Message<StatePayload>::Message;
        ExitingStateMessage() = delete;
        MsgType type() const override { return MsgType::ExitingState; }
    };


    class ConflictResolutionsMessage : public Message<ConflictResolutionPayload> {
    public:
        using Message<ConflictResolutionPayload>::Message;
        ConflictResolutionsMessage() = delete;
        MsgType type() const override { return MsgType::ConflictResolutions; }
    };

}