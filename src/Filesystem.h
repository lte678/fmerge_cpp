#pragma once

#include "FileTree.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <dirent.h>
#include <cstdlib>
#include <string>
#include <optional>
#include <functional>
#include <algorithm>
#include <ranges>

using std::optional;
typedef struct stat Stat;

enum class FileType {
    Unknown,
    Directory,
    File,
    Link
};


struct FileStats {
    long mtime;
    FileType type;
};

optional<FileStats> get_file_stats(std::string filepath);
bool exists(std::string filepath);
int ensure_dir(std::string path);
long get_timestamp_now();

optional<std::string> abs_path(std::string basepath);
std::vector<std::string> split_path(std::string path);
std::string join_path(std::string p1, std::string p2);
std::string path_to_str(std::vector<std::string> tokens);

void for_file_in_dir(std::string basepath, std::string prefix, std::function<void(std::string, const FileStats&)> f);