#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <optional>
#include <functional>
#include <algorithm>
#include <ranges>


namespace fmerge {

    using std::optional;
    typedef struct stat Stat;

    enum class FileType : unsigned char {
        Unknown,
        Directory,
        File,
        Link
    };


    struct FileStats {
        long mtime; // Modification time
        long ctime; // Creation time
        long atime; // Access time
        FileType type;
        unsigned long fsize;
    };

    optional<std::string> abs_path(std::string basepath);
    std::vector<std::string> split_path(std::string path);
    std::string join_path(std::string p1, std::string p2);
    std::string path_to_str(std::vector<std::string> tokens);

    struct File {
        std::string path{};
        FileType type{FileType::Unknown};

        inline bool is_dir() const { return type == FileType::Directory; };
        inline bool is_file() const { return type == FileType::File; };
        inline bool is_link() const { return type == FileType::Link; };

        inline std::string name() const {
            return split_path(path).back();
        }
    };
    
    optional<FileStats> get_file_stats(std::string filepath);
    bool set_timestamp(std::string filepath, long mod_time, long access_time);
    bool exists(std::string filepath);
    bool remove_path(std::string path);
    bool ensure_dir(std::string path);
    long get_timestamp_now();

    void _for_file_in_dir(std::string basepath, std::string prefix, std::function<void(File, const FileStats&)> f);
    inline void for_file_in_dir(std::string basepath, std::function<void(File, const FileStats&)> f) {
        _for_file_in_dir(basepath, "", f);
    }

    // Check if file should be ignored
    bool file_ignored(const File &file);
    std::ostream& operator<<(std::ostream& os, const FileType& filetype);
}