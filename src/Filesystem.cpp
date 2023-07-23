#include "Filesystem.h"

#include "Errors.h"


namespace fmerge {

    optional<FileStats> get_file_stats(std::string filepath) {
        Stat clib_stats;
        FileStats stats;
        int ret = lstat(filepath.c_str(), &clib_stats);
        if(ret != 0) {
            //print_clib_error("lstat");
            //std::cerr << "^^^ occurred for " << filepath << std::endl;
            return std::nullopt;
        }
        // Populate with the read metadata
        stats.mtime = clib_stats.st_mtime;
        if(S_ISDIR(clib_stats.st_mode)) {
            stats.type = FileType::Directory;
        } else if(S_ISREG(clib_stats.st_mode)) {
            stats.type = FileType::File;
        } else if(S_ISLNK(clib_stats.st_mode)) {
            stats.type = FileType::Link;
        } else {
            stats.type = FileType::Unknown;
        }

        return optional<FileStats>{stats};
    }


    bool exists(std::string filepath) {
        Stat clib_stats;
        return lstat(filepath.c_str(), &clib_stats) == 0;
    }


    bool remove_path(std::string path) {
        if(remove(path.c_str()) == -1) {
            print_clib_error("remove");
            return false;
        }
        return true;
    }


    int ensure_dir(std::string path) {
        // Create dir if it does not exist
        if(!get_file_stats(path).has_value()) {
            if(mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
                print_clib_error("mkdir");
                return 1;
            }
            std::cout << "Created " << split_path(path).back() << " directory" << std::endl;
        }
        return 0;
    }


    long get_timestamp_now() {
        return time(nullptr);
    }


    optional<std::string> abs_path(std::string basepath) {
        char fullpath[PATH_MAX];
        if(realpath(basepath.c_str(), fullpath) == nullptr) {
            print_clib_error("realpath");
            return std::nullopt;
        }
        return std::string(fullpath);
    }


    std::vector<std::string> split_path(std::string path)
    {
        auto ret = std::vector<std::string>();
        size_t pos = 0;
        size_t last = 0;
        std::string token;
        while ((pos = path.find("/", last)) != std::string::npos) {
            if(pos - last > 0) {
                ret.push_back(path.substr(last, pos - last));
            }
            last = pos + 1;
        }
        if(path.length() - last > 0) {
            ret.push_back(path.substr(last));
        }
        return ret;
    }


    std::string join_path(std::string p1, std::string p2) {
        if(!p1.empty()) {
            if(*(p1.end()) == '/') {
                return p1 + p2;
            } else {
                return p1 + "/" + p2;
            }
        } else {
            return p2;
        }
        return "";
    }


    std::string path_to_str(std::vector<std::string> tokens) {
        if(tokens.size() == 0) {
            return "";
        }

        std::string res = tokens.front();
        for(auto token = tokens.begin() + 1;
            token != tokens.end();
            token++) {
            res += "/" + *token;
        }
        return res;
    }


    void _for_file_in_dir(std::string basepath, std::string prefix, std::function<void(std::string, const FileStats&)> f) {
        // basepath must already be normalized with realpath/abs_path
        // The child files and directories are returned with prefix + / + filename

        // Open and check dir
        auto* dir = opendir(basepath.c_str());
        if(dir == nullptr) {
            print_clib_error("opendir");
            std::cerr << "^^^ occurred for " << basepath << std::endl;
            
            // We have reached the limit on the maximum number of files, globally or for this process.
            if(errno == ENFILE) {
                std::cerr << "[Error] System-wide file limit hit." << std::endl;
                exit(1);
            } else if(errno == EMFILE) {
                std::cerr << "[Error] Process file limit hit." << std::endl;
                exit(1);
            }
            return;
        }

        struct dirent* subdir;
        // "subdir" is not necessarily a directory
        while((subdir = readdir(dir)) != nullptr) {
            auto subdirname = std::string(subdir->d_name); 
            if(subdirname != "." && subdirname != "..") {
                auto subdirpath = std::string(basepath) + "/" + subdirname;
                auto relative_path = join_path(prefix, subdirname);
                auto file_stats = get_file_stats(subdirpath);
                if(file_stats.has_value()) {
                    f(relative_path, *file_stats);
                    if(file_stats->type == FileType::Directory) _for_file_in_dir(subdirpath, relative_path, f);
                }
            }
        }
        closedir(dir);
    }
    

    static bool str_starts_with(std::string s1, std::string starts_with) {
        // Returns true if the strings are equal, up to the length of the second string
        if(starts_with.length() > s1.length()) {
            return false;
        }

        for(size_t i = 0; i < starts_with.length(); i++) {
            if(s1[i] != starts_with[i]) {
                return false;
            }
        }
        return true;
    }


    bool path_ignored(const std::string &path, bool is_dir) {
        std::string compare_path = path;
        if(is_dir) {
            compare_path.append("/");
        }

        // Ignore .fmerge directory
        if(str_starts_with(compare_path, ".fmerge/")) {
            return true;
        }
        return false;
    }
}
