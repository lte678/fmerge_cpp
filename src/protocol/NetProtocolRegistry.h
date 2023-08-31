#pragma once

#include "GenericMessage.h"
#include "NetProtocol.h"

#include <exception>


namespace fmerge::protocol {

    struct MsgEntry {
        MsgType message_type;
        std::string_view string_representation;
        std::shared_ptr<GenericMessage> (*deserialize_function)(ReadFunc, unsigned long);
    };


    // NOTE: Must contain an MsgType::Unknown field for lookup_protocol_registry to return
    constexpr MsgEntry protocol_registry[] = {
        {MsgType::Unknown,             "UNKNOWN"            , IgnoreMessage::deserialize             },
        {MsgType::Ignore,              "IGNORE"             , IgnoreMessage::deserialize             },
        {MsgType::Version,             "VERSION"            , VersionMessage::deserialize            },
        {MsgType::Changes,             "CHANGES"            , ChangesMessage::deserialize            },
        {MsgType::FileTransfer,        "FILE_TRANSFER"      , FileTransferMessage::deserialize       },
        {MsgType::FileRequest,         "FILE_REQUEST"       , FileRequestMessage::deserialize        },
        {MsgType::StartSync,           "START_SYNC"         , StartSyncMessage::deserialize          },
        {MsgType::ConflictResolutions, "CONFLICT_RESOLUTION", ConflictResolutionsMessage::deserialize},
    };

    
    inline const MsgEntry& lookup_protocol_registry(const MsgType& msg_type) {
        for(const auto& msg_entry : protocol_registry) {
            if(msg_entry.message_type == msg_type) {
                return msg_entry;
            }
        }
        return lookup_protocol_registry(MsgType::Unknown);
    }

    inline std::shared_ptr<GenericMessage> deserialize_packet(MsgType type, unsigned long length, ReadFunc receive) {
        auto msg_entry = lookup_protocol_registry(type);
        if(msg_entry.message_type == MsgType::Unknown) {
            throw std::invalid_argument("attempted to deserialize packet with invalid type value");
        }

        return msg_entry.deserialize_function(receive, length);
    }


    inline std::ostream& operator<<(std::ostream& os, const MsgType& msg) {
        return os << lookup_protocol_registry(msg).string_representation;
    }
}