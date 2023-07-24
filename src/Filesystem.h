#pragma once

#include "FileTree.h"

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

    enum class FileType {
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

    optional<FileStats> get_file_stats(std::string filepath);
    bool set_timestamp(std::string filepath, long mod_time, long access_time);
    bool exists(std::string filepath);
    bool remove_path(std::string path);
    bool ensure_dir(std::string path);
    long get_timestamp_now();

    optional<std::string> abs_path(std::string basepath);
    std::vector<std::string> split_path(std::string path);
    std::string join_path(std::string p1, std::string p2);
    std::string path_to_str(std::vector<std::string> tokens);

    void _for_file_in_dir(std::string basepath, std::string prefix, std::function<void(std::string, const FileStats&)> f);
    inline void for_file_in_dir(std::string basepath, std::function<void(std::string, const FileStats&)> f) {
        _for_file_in_dir(basepath, "", f);
    }

    // Check if path should be ignored
    bool path_ignored(const std::string &path, bool is_dir);
}