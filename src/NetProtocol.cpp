#include "NetProtocol.h"

#include <unistd.h>


namespace fmerge::protocol {

    std::string msg_type_to_string(const MessageType msg) {
        std::string res{};
        MessageType underlying_type = msg & ~MsgRequestFlag;
        if(underlying_type == MsgIgnore) {
            res =  "IGNORE";
        } else if (underlying_type == MsgVersion) {
            res =  "VERSION";
        } else if (underlying_type == MsgChanges) {
            res =  "CHANGES";
        } else if (underlying_type == MsgFileTransfer) {
            res =  "FILE_TRANSFER";
        } else if (underlying_type == MsgUnknown) {
            res =  "UNKNOWN";
        } else {
            res =  "INVALID";
        }
        if(msg & MsgRequestFlag) {
            res += "_REQ";
        }
        return res;
    }


    void FileTransferRequest::serialize(int fd) const {
        write(fd, filepath.c_str(), filepath.length());
    }


    FileTransferRequest FileTransferRequest::deserialize(ReadFunc receive, unsigned long length) {
        char* c_filename = new char[length + 1];
        receive(c_filename, length);
        c_filename[length] = 0;
        std::string filename{c_filename};
        delete[] c_filename;

        return FileTransferRequest(filename);
    }
    

    void VersionResponse::serialize(int fd) const {
        // Note:: Serialize adds the header, while deserialize does not expect it!
        unsigned int netmajor = htole32(major);
        write(fd, &netmajor, sizeof(netmajor));
        unsigned int netminor = htole32(minor);
        write(fd, &netminor, sizeof(netminor));

        write(fd, uuid.data(), uuid.size());
    }


    VersionResponse VersionResponse::deserialize(ReadFunc receive) {
        unsigned int major{};
        receive(&major, sizeof(major));
        major = le32toh(major);
        unsigned int minor{};
        receive(&minor, sizeof(minor));
        minor = le32toh(minor);

        std::array<unsigned char, 16> uuid;
        receive(uuid.data(), uuid.size());

        return VersionResponse(major, minor, uuid);
    }


    ChangesResponse::ChangesResponse(std::vector<Change> _changes) : changes(_changes), serialized_changes("") {
        std::stringstream ser_stream;
        serialize_changes(ser_stream, changes);
        serialized_changes = ser_stream.str();
    }


    void ChangesResponse::serialize(int fd) const {
        write(fd, serialized_changes.c_str(), length());
    }


    ChangesResponse ChangesResponse::deserialize(ReadFunc receive, unsigned long length) {
        char *change_buffer = new char[length];
        receive(change_buffer, length);

        std::stringstream change_stream(change_buffer);
        auto changes = deserialize_changes(change_stream);
        return ChangesResponse(changes);
    }


    void FileTransferResponse::serialize(int fd) const {
        write(fd, &is_folder, 1);
        if(write(fd, payload.get(), payload_len) != static_cast<long int>(payload_len)) {
            std::cerr << "[Error] Failed to transfer all bytes of file transfer payload!" << std::endl;
        }
    }


    FileTransferResponse FileTransferResponse::deserialize(ReadFunc receive, unsigned long length) {
        char is_folder{};
        receive(&is_folder, 1);

        std::shared_ptr<unsigned char> resp_buffer{(unsigned char*)malloc(length), free};
        receive(resp_buffer.get(), length - 1);
        return FileTransferResponse(resp_buffer, length - 1, is_folder);
    }


    std::shared_ptr<Message> deserialize_packet(MessageType raw_type, unsigned long length, ReadFunc receive) {
        if(raw_type == MsgVersion) {
            return std::make_shared<VersionResponse>(VersionResponse::deserialize(receive));
        } else if(raw_type == MsgChanges) {
            return std::make_shared<ChangesResponse>(ChangesResponse::deserialize(receive, length));
        } else if(raw_type == MsgFileTransfer) {
            return std::make_shared<FileTransferResponse>(FileTransferResponse::deserialize(receive, length));
        } else if(raw_type == (MsgVersion | MsgRequestFlag)) {
            return std::make_shared<VersionRequest>(VersionRequest());
        } else if(raw_type == (MsgChanges | MsgRequestFlag)) {
            return std::make_shared<ChangesRequest>(ChangesRequest());
        } else if(raw_type == (MsgFileTransfer | MsgRequestFlag)) {
            return std::make_shared<FileTransferRequest>(FileTransferRequest::deserialize(receive, length));
        }else {
            std::cerr << "Cannot deserialize message type " << msg_type_to_string(raw_type);
            return nullptr;
        }
    }
}