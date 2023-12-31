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
        {MsgType::Unknown,             "UNKNOWN"            , deserialize<IgnoreMessage>             },
        {MsgType::Ignore,              "IGNORE"             , deserialize<IgnoreMessage>             },
        {MsgType::Version,             "VERSION"            , deserialize<VersionMessage>            },
        {MsgType::Changes,             "CHANGES"            , deserialize<ChangesMessage>            },
        {MsgType::FileTransfer,        "FILE_TRANSFER"      , deserialize<FileTransferMessage>       },
        {MsgType::FileRequest,         "FILE_REQUEST"       , deserialize<FileRequestMessage>        },
        {MsgType::ExitingState,        "EXITING_STATE"      , deserialize<ExitingStateMessage>       },
        {MsgType::ConflictResolutions, "CONFLICT_RESOLUTION", deserialize<ConflictResolutionsMessage>},
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