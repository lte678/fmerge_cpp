#include "Filesystem.h"

#include "Errors.h"

#include <fcntl.h>


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
        stats.ctime = clib_stats.st_ctime;
        stats.atime = clib_stats.st_atime;
        if(S_ISDIR(clib_stats.st_mode)) {
            stats.type = FileType::Directory;
        } else if(S_ISREG(clib_stats.st_mode)) {
            stats.type = FileType::File;
        } else if(S_ISLNK(clib_stats.st_mode)) {
            stats.type = FileType::Link;
        } else {
            stats.type = FileType::Unknown;
        }
        stats.fsize = clib_stats.st_size;

        return optional<FileStats>{stats};
    }


    bool set_timestamp(std::string filepath, long mod_time, long access_time) {
        timespec times[] = {
            {.tv_sec = access_time, .tv_nsec = 0 },
            {.tv_sec = mod_time, .tv_nsec = 0}
        };
        if(utimensat(AT_FDCWD, filepath.c_str(), times, AT_SYMLINK_NOFOLLOW) == -1) {
            print_clib_error("utimensat");
            return false;
        }
        return true;
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


    bool ensure_dir(std::string path, bool allow_exists) {
        // Create dir if it does not exist
        if(!get_file_stats(path).has_value()) {
            // Make sure parent directory exists
            auto tokens = split_path(path);
            std::vector<std::string> parent_dir(tokens.begin(), tokens.end() - 1);
            if(!ensure_dir(path_to_str(parent_dir), allow_exists)) {
                return false;
            }

            if(mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
                if(errno != EEXIST || !allow_exists) {
                    print_clib_error("mkdir");
                    return false;   
                }
            }
            //LOG("Created " << split_path(path).back() << " directory" << std::endl);
        }
        return true;
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
            } else if((pos - last) == 0 && (last == 0)) {
                ret.push_back("/");
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
            if(*(p1.end()) == '/' && *(p2.begin()) == '/') {
                return p1 + std::string(p2.begin() + 1, p2.end());
            } else if(*(p1.end()) == '/' || *(p2.begin()) == '/') {
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
        std::string res{};
        for(auto token : tokens) {
            res = join_path(res, token);
        }
        return res;
    }


    void _for_file_in_dir(std::string basepath, std::string prefix, std::function<void(File, const FileStats&)> f) {
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
                    File file{path:relative_path, type:file_stats->type};
                    f(file, *file_stats);
                    if(file.is_dir()) _for_file_in_dir(subdirpath, relative_path, f);
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


    bool file_ignored(const File& file) {
        std::string compare_path = file.path;
        if(file.is_dir()) {
            compare_path.append("/");
        }

        // Ignore .fmerge directory
        if(str_starts_with(compare_path, ".fmerge/")) {
            return true;
        }
        return false;
    }


    std::ostream& operator<<(std::ostream& os, const FileType& filetype) {
        if(filetype == FileType::File) {
            os << "F";
        } else if(filetype == FileType::Directory) {
            os << "D";
        } else if(filetype == FileType::Link) {
            os << "L";
        } else {
            os << "?";
        }
        return os;
    }

}
