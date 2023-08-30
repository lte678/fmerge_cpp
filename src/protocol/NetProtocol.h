#pragma once

#include "GenericMessage.h"
#include "../FileTree.h"
#include "../MergeAlgorithms.h"

#include <sstream>
#include <array>
#include <vector>


namespace fmerge::protocol {
    
    struct VersionPayload {
        int major{};
        int minor{};
        std::array<unsigned char, 16> uuid{};
    };


    struct FileTransferPayload {
        std::shared_ptr<unsigned char> payload;
         unsigned long payload_len;
         FileType ftype;
         long modification_time;
         long access_time;
    };


    // --------------------------------------------------------------------------------
    // ---------------------------- Message Definitions -------------------------------
    // --------------------------------------------------------------------------------

    class IgnoreMessage : public EmptyMessage {
    public:
        MsgType type() const override { return MsgType::Ignore; }

        static std::shared_ptr<GenericMessage> deserialize(ReadFunc, unsigned long) {
            return std::make_shared<IgnoreMessage>();
        }
    };


    class VersionMessage : public Message<VersionPayload> {
    public:
        MsgType type() const override { return MsgType::Version; }
    };


    class ChangesMessage : public Message<std::vector<Change>> {
    public:
        MsgType type() const override { return MsgType::Changes; }
    };


    class FileTransferMessage : public Message<FileTransferPayload> {
    public:
        MsgType type() const override { return MsgType::FileTransfer; }
    };


    class StartSyncMessage : public EmptyMessage {
    public:
        MsgType type() const override { return MsgType::StartSync; }

        static std::shared_ptr<GenericMessage> deserialize(ReadFunc, unsigned long) {
            return std::make_shared<StartSyncMessage>();
        }
    };


    class ConflictResolutionsMessage : public Message<std::unordered_map<std::string, ConflictResolution>> {
    public:
        MsgType type() const override { return MsgType::ConflictResolutions; }
    };

}