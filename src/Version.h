#pragma once
#include <string>


namespace fmerge {

    extern const char *g_fmerge_version;

    struct Version {
        int major;
        int minor;
    };


    enum VersionError {
        NoError,
        MalformedLocalVersion,
        MalformedRemoteVersion,
        RemoteOutdatedMajor,
        RemoteOutdatedMinor,
        LocalOutdatedMajor,
        LocalOutdatedMinor,
        DevVersionMismatch
    };


    static Version parse_version_number(std::string ver) {
        auto delimiter = ver.find('.');
        auto terminator = ver.find('~');
        if(delimiter == std::string::npos || terminator == std::string::npos) {
            return {-1, -1};
        }
        auto v_major{ver.substr(0, delimiter)};
        auto v_minor{ver.substr(delimiter + 1, terminator - delimiter - 1)};

        return {std::atoi(v_major.c_str()), std::atoi(v_minor.c_str())};
    }


    static VersionError check_version_numbers(Version local, Version remote) {
        if(local.major == -1 || local.minor == -1) {
            return MalformedLocalVersion;
        }
        if(remote.major == -1 || remote.minor == -1) {
            return MalformedRemoteVersion;
        }

        if(local.major > remote.major) {
            return RemoteOutdatedMajor;
        } else if(local.major < remote.major) {
            return LocalOutdatedMajor;
        }

        if(local.minor > remote.minor) {
            return RemoteOutdatedMinor;
        } else if(local.minor < remote.minor) {
            return LocalOutdatedMinor;
        }

        return NoError;
    }


    static bool is_dev_version(std::string ver) {
        if(ver.size() >= 3) {
            if(ver.substr(0, 3) == "dev") {
                return true;
            }
        }
        return false;
    }


    static std::string parse_dev_hash(std::string ver) {
        auto hash_start = ver.find('~') + 1;
        if(ver.size() < hash_start + 1) {
            return "";
        } else {
            return ver.substr(hash_start);
        }
    }


    inline VersionError check_peer_version(std::string local_version, std::string remote_version) {
        bool is_dev_local{is_dev_version(local_version)};
        bool is_dev_remote{is_dev_version(remote_version)};

        if(!is_dev_local && !is_dev_remote) {
            // Release versioning
            return check_version_numbers(
                parse_version_number(local_version),
                parse_version_number(remote_version)
            );
        } else {
            // Development versioning
            auto local_hash = parse_dev_hash(local_version);
            auto remote_hash = parse_dev_hash(remote_version);

            if(local_hash.empty()) return MalformedLocalVersion;
            if(remote_hash.empty()) return MalformedRemoteVersion;
            if(local_hash != remote_hash) return DevVersionMismatch;
            return NoError;
        }
    }

}