#include "NetProtocol.h"

#include "NetProtocolRegistry.h"

#include <unistd.h>


namespace fmerge::protocol {

    void VersionPayload::serialize(WriteFunc write) const {
        unsigned int netmajor = htole32(major);
        write(&netmajor, sizeof(netmajor));
        unsigned int netminor = htole32(minor);
        write(&netminor, sizeof(netminor));

        write(uuid.data(), uuid.size());
    }


    std::unique_ptr<VersionPayload> VersionPayload::deserialize(ReadFunc receive, unsigned long) {
        unsigned int major{};
        receive(&major, sizeof(major));
        major = le32toh(major);
        unsigned int minor{};
        receive(&minor, sizeof(minor));
        minor = le32toh(minor);

        std::array<unsigned char, 16> uuid;
        receive(uuid.data(), uuid.size());

        return std::make_unique<VersionPayload>(major, minor, uuid);
    }

    void ChangesPayload::serialize(WriteFunc write) const {
        std::stringstream ser_stream;
        serialize_changes(ser_stream, *this);
        auto serialized_changes = ser_stream.str();
        write(serialized_changes.c_str(), serialized_changes.length());
    }


    std::unique_ptr<ChangesPayload> ChangesPayload::deserialize(ReadFunc receive, unsigned long length) {
        char *change_buffer = new char[length];
        receive(change_buffer, length);

        std::stringstream change_stream(change_buffer);
        auto changes = deserialize_changes(change_stream);
        return std::make_unique<ChangesPayload>(changes.begin(), changes.end());
    }


    void FileTransferPayload::serialize(WriteFunc write) const {
        long mtime_le = htole64(mod_time);
        write(&mtime_le, sizeof(mtime_le));

        long atime_le = htole64(access_time);
        write(&atime_le, sizeof(atime_le));

        unsigned char ftype_char = static_cast<unsigned char>(ftype);
        write(&ftype_char, sizeof(ftype_char));

        unsigned short path_length = htole16(static_cast<unsigned short>(path.length()));
        write(&path_length, sizeof(path_length));
        write(path.c_str(), path.length());

        write(payload.get(), payload_len);
    }


    std::unique_ptr<FileTransferPayload> FileTransferPayload::deserialize(ReadFunc receive, unsigned long length) {
        long mtime{};
        receive(&mtime, sizeof(mtime));
        mtime = le64toh(mtime);

        long atime{};
        receive(&atime, sizeof(atime));
        atime = le64toh(atime);

        unsigned char ftype_char{};
        receive(&ftype_char, sizeof(ftype_char));

        unsigned short path_length{};
        receive(&path_length, sizeof(path_length));
        path_length = le16toh(path_length);

        char cpath[path_length + 1];
        receive(cpath, path_length);
        cpath[path_length] = '\0';
        std::string path(cpath);

        unsigned long payload_len = length - sizeof(mtime) - sizeof(atime) - sizeof(ftype_char) - sizeof(path_length) - path_length;
        std::shared_ptr<unsigned char> resp_buffer{(unsigned char*)malloc(payload_len), free};
        receive(resp_buffer.get(), payload_len);
        return std::make_unique<FileTransferPayload>(path, resp_buffer, payload_len, static_cast<FileType>(ftype_char), mtime, atime);
    }


    void StringPayload::serialize(WriteFunc write) const {
        write(c_str(), length());
    }


    std::unique_ptr<StringPayload> StringPayload::deserialize(ReadFunc receive, unsigned long length) {
        char cstr[length + 1];
        receive(cstr, length);
        cstr[length] = '\0';
        return std::unique_ptr<StringPayload>(new StringPayload(cstr));
    }


    void ConflictResolutionPayload::serialize(WriteFunc write) const {
        std::stringstream res_stream{};
        for(const auto& resolution : *this) {
            serialize_conflict_resolution(res_stream, resolution);
        }
        write(res_stream.str().c_str(), res_stream.str().length());
    }


    std::unique_ptr<ConflictResolutionPayload> ConflictResolutionPayload::deserialize(ReadFunc receive, unsigned long length) {
        auto resolutions = new ConflictResolutionPayload;
        unsigned long bytes_read{0};
        while(bytes_read < length) {
            unsigned short string_len_le;
            receive(&string_len_le, sizeof(string_len_le));
            char resolution_key[le16toh(string_len_le) + 1];
            receive(resolution_key, le16toh(string_len_le));
            resolution_key[le16toh(string_len_le)] = '\0';
            int resolution_choice_le;
            receive(&resolution_choice_le, sizeof(resolution_choice_le));

            resolutions->emplace(std::string(resolution_key), static_cast<ConflictResolution>(le32toh(resolution_choice_le)));
        }
        return std::unique_ptr<ConflictResolutionPayload>(resolutions);
    }

}