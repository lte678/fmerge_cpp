#include "FileTree.h"

#include "Terminal.h"

#include <cstring>
#include <valarray>
#include <sstream>
#include <fstream>
#include <iomanip>


namespace fmerge {

    MetadataNode::MetadataNode(std::string _name, FileType _ftype, long _mtime) : mtime(_mtime), ftype(_ftype) {
        size_t filename_len = _name.length();
        char* _cname = new char[filename_len + 1];
        strncpy(_cname, _name.c_str(), filename_len + 1);
        name = _cname;
    }


    void MetadataNode::serialize(std::ostream& stream) const {
        unsigned short name_len = static_cast<unsigned short>(strlen(name));
        stream.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        stream.write(name, name_len);
        stream.write(reinterpret_cast<const char*>(&mtime), sizeof(mtime));
        stream.write(reinterpret_cast<const char*>(&ftype), sizeof(ftype));
    }


    MetadataNode MetadataNode::deserialize(std::istream& stream) {
        unsigned short name_len{};
        stream.read(reinterpret_cast<char*>(&name_len), sizeof(unsigned short));
        char* name = new char[name_len + 1];
        stream.read(name, name_len);
        name[name_len] = '\0';
        long mtime;
        FileType ftype;
        stream.read(reinterpret_cast<char*>(&mtime), sizeof(mtime));
        stream.read(reinterpret_cast<char*>(&ftype), sizeof(ftype));

        return MetadataNode(name, ftype, mtime);
    }


    shared_ptr<MetadataNode> DirNode::get_child_file(std::string file_name) {
        auto results = std::find_if(this->files.begin(), this->files.end(),
            [&file_name](shared_ptr<MetadataNode> n) { return n->name == file_name; }
        );
        if(results == this->files.end()) {
            return shared_ptr<MetadataNode>();
        }
        return *results;
    }


    shared_ptr<MetadataNode> DirNode::get_child_file(std::vector<std::string> path_tokens) {
        if(path_tokens.size() == 0) {
            return shared_ptr<MetadataNode>();
        }

        auto current_dir = shared_from_this();
        for(auto folder = path_tokens.begin(); folder != (path_tokens.end() - 1); folder += 1) {
            current_dir = current_dir->get_child_dir(*folder);
            if(!current_dir) {
                return shared_ptr<MetadataNode>();
            }
        }
        return current_dir->get_child_file(path_tokens.back());
    }


    shared_ptr<DirNode> DirNode::get_child_dir(std::string dir_name) {
        auto results = std::find_if(this->subdirs.begin(), this->subdirs.end(),
            [&dir_name](shared_ptr<DirNode> n) { return n->metadata->name == dir_name; }
        );
        if(results == this->subdirs.end()) {
            return shared_ptr<DirNode>();
        }
        return *results;
    }


    shared_ptr<DirNode> DirNode::get_child_dir(std::vector<std::string> path_tokens) {
        auto current_dir = shared_from_this();
        for(auto folder : path_tokens) {
            current_dir = current_dir->get_child_dir(folder);
            if(!current_dir) {
                // Return null
                return current_dir;
            }
        }
        return current_dir;
    }


    void DirNode::for_node_in_tree(
        std::function<void(std::vector<std::string>, shared_ptr<MetadataNode>, bool)> f,
        std::vector<std::string> prefix) {

        for(const auto& subdir : subdirs) {
            auto dir_path = std::vector<std::string>(prefix);
            dir_path.push_back(subdir->metadata->name);
            f(dir_path, subdir->metadata, true);
            subdir->for_node_in_tree(f, dir_path);
        }
        for(const auto& file : files) {
            auto file_path = std::vector<std::string>(prefix);
            file_path.push_back(file->name);
            f(file_path, file, false);
        }
    }


    void DirNode::serialize(std::ostream& stream) const {
        metadata->serialize(stream);
        size_t subdirs_len = subdirs.size();
        size_t files_len = files.size();
        stream.write(reinterpret_cast<const char*>(&subdirs_len), sizeof(size_t));
        stream.write(reinterpret_cast<const char*>(&files_len), sizeof(size_t));
        for(auto subdir : subdirs) {
            subdir->serialize(stream);
        }
        for(auto file : files) {
            file->serialize(stream);
        }
    }


    shared_ptr<DirNode> DirNode::deserialize(std::istream& stream) {
        auto new_node = std::make_shared<DirNode>(MetadataNode::deserialize(stream));
        size_t num_subdirs{};
        size_t num_files{};
        stream.read(reinterpret_cast<char*>(&num_subdirs), sizeof(num_subdirs));
        stream.read(reinterpret_cast<char*>(&num_files), sizeof(num_files));
        for(size_t i = 0; i < num_subdirs; i++) {
            new_node->subdirs.push_back(DirNode::deserialize(stream));
        }
        for(size_t i = 0; i < num_files; i++) {
            new_node->files.push_back(std::make_shared<MetadataNode>(MetadataNode::deserialize(stream)));
        }
        return new_node;
    }


    std::ostream& operator<<(std::ostream& os, ChangeType change_type) {
        switch(change_type) {
        case ChangeType::Modification:
            os << "Modification";
            break;
        case ChangeType::Creation:
            os << "Creation";
            break;
        case ChangeType::Deletion:
            os << "Deletion";
            break;
        default:
            os << "Unknown Change";
        }
        return os;
    }


    bool operator==(const Change& lhs, const Change& rhs) {
        if(lhs.file.type != rhs.file.type) {
            return false;
        }
        if(lhs.earliest_change_time != rhs.earliest_change_time) {
            return false;
        }
        if(lhs.latest_change_time != rhs.latest_change_time) {
            return false;
        }
        if(lhs.type != rhs.type) {
            return false;
        }
        if(lhs.file.path != rhs.file.path) {
            return false;
        }
        return true;
    }


    std::ostream& operator<<(std::ostream& os, const Change& change) {
        os << std::setw(16) << std::left << change.type << change.file.type << " " << change.file.path;
        return os;
    }


    void Change::serialize(std::ostream& stream) const {
        stream << static_cast<int>(type) << ",";
        stream << earliest_change_time << ",";
        stream << latest_change_time << ",";
        stream << static_cast<int>(file.type) << ",";
        stream << file.path << std::endl; 
    }


    Change Change::deserialize(std::istream& stream) {
        Change ret{};
        std::stringbuf buffer{};

        stream.get(buffer, ',');
        stream.seekg(1, std::ios_base::cur); // Seek to after the delimiter
        ret.type = static_cast<ChangeType>(std::stoi(buffer.str()));
        buffer.str("");

        stream.get(buffer, ',');
        stream.seekg(1, std::ios_base::cur); // Seek to after the delimiter
        ret.earliest_change_time = std::stol(buffer.str());
        buffer.str("");

        stream.get(buffer, ',');
        stream.seekg(1, std::ios_base::cur); // Seek to after the delimiter
        ret.latest_change_time = std::stol(buffer.str());
        buffer.str("");

        stream.get(buffer, ',');
        stream.seekg(1, std::ios_base::cur); // Seek to after the delimiter
        ret.file.type = static_cast<FileType>(std::stoi(buffer.str()));
        buffer.str("");

        stream.get(buffer, '\n');
        stream.seekg(1, std::ios_base::cur); // Seek to after the delimiter
        ret.file.path = buffer.str();

        return ret;
    }


    void update_file_tree(shared_ptr<DirNode> base_node, std::string base_path, bool show_loading_bar) {        
        // Create file number total for loading bar
        unsigned int total_number_files{0};
        if(show_loading_bar) {
            for_file_in_dir(base_path,
                [&total_number_files](auto file, auto) {
                    if(file_ignored(file)) {
                        return;
                    }
                    total_number_files++;
                }
            );
        }


        unsigned int num_files_processed{0};
        for_file_in_dir(base_path,
            [=, &num_files_processed](auto file, const FileStats& stats) {
                if(file_ignored(file)) {
                    return;
                }

                if(show_loading_bar && (num_files_processed % 100) == 0) {
                    // Go back to previous line
                    term()->update_progress_bar(static_cast<float>(num_files_processed) / static_cast<float>(total_number_files), "Building File Tree");
                }
                num_files_processed++;

                auto path_tokens = split_path(file.path);

                shared_ptr<DirNode> parent_node;
                if(path_tokens.size() > 1) {
                    parent_node = base_node->get_child_dir(
                        std::vector<std::string>(path_tokens.begin(), path_tokens.end() - 1)
                    );
                } else {
                    parent_node = base_node;
                }

                if(!parent_node) {
                    std::cerr << "Parent node is missing for " << path_tokens.back() << std::endl;
                    return;
                }
                // termbuf() << "Added " << path_tokens.back() << std::endl;

                if(file.is_dir()) {
                    auto existing_file = parent_node->get_child_dir(path_tokens.back());
                    if(!existing_file) {
                        parent_node->subdirs.push_back(
                            std::make_shared<DirNode>(MetadataNode(path_tokens.back(), stats.type, stats.mtime))
                        );
                    } else {
                        // Update the metadata to match what is actually on disk
                        existing_file->metadata->mtime = stats.mtime;
                    }
                } else if(file.is_file() || file.is_link()) {
                    auto existing_file = parent_node->get_child_file(path_tokens.back());
                    if(!existing_file) {
                        parent_node->files.push_back(
                            std::make_shared<MetadataNode>(path_tokens.back(), stats.type, stats.mtime)
                        );
                    } else {
                        // Update the metadata to match what is actually on disk
                        existing_file->mtime = stats.mtime;
                        existing_file->ftype = stats.type;
                    }
                } else {
                    std::cerr << "[Error] " << file.path << ": Unknown file type (" << static_cast<int>(stats.type) << std::endl;
                }
            }
        );
        
        if(show_loading_bar) {
            term()->complete_progress_bar();
        }
    }


    std::vector<Change> compare_metadata(shared_ptr<MetadataNode> from_node, shared_ptr<MetadataNode> to_node, std::string path) {
        // Logic to determine what has changed.
        // This is one of the most critical parts of this application
        
        if(from_node && to_node) {
            // Ignore all directory changes other than creation and deletion
            if(from_node->ftype == FileType::Directory && to_node->ftype == FileType::Directory) {
                return {};
            }
            // The type of object differs. This means the previous was deleted, and the latter was created
            if(from_node->ftype != to_node->ftype) {
                return {
                    Change {
                        .type = ChangeType::Deletion,
                        .earliest_change_time = from_node->mtime,
                        .latest_change_time = to_node->mtime,
                        .file = File{.path=path, .type=from_node->ftype}
                    },
                    Change {
                        .type = ChangeType::Modification,
                        .earliest_change_time = to_node->mtime,
                        .latest_change_time = 0,
                        .file = File{.path=path, .type=to_node->ftype}
                    }
                };
            }
            // Both objects exist and are file-like
            // Compare modification times for files
            if(from_node->mtime < to_node->mtime) {
                return {Change {
                    .type = ChangeType::Modification,
                    .earliest_change_time = to_node->mtime,
                    .latest_change_time = 0,
                    .file = File{.path=path, .type=to_node->ftype}
                }};
            } else if(from_node->mtime > to_node->mtime) {
                termbuf() << "[Warning] Modification time of " << path << " lies " << 
                    from_node->mtime - to_node->mtime << "s in the future!" << std::endl;
                return {};
            } else {
                return {};
            }
        }

        if(from_node && !to_node) {
            return {Change {
                .type = ChangeType::Deletion,
                .earliest_change_time = from_node->mtime,
                .latest_change_time = get_timestamp_now(),
                .file = {.path=path, .type=from_node->ftype},
            }};
        }

        if(to_node && !from_node) {
            return {Change {
                .type = ChangeType::Modification,
                .earliest_change_time = to_node->mtime,
                .latest_change_time = 0,
                .file = {.path=path, .type=to_node->ftype},
            }};
        }

        std::cerr << "[Error] Change could not be properly identified!" << std::endl;
        return {Change{.type = ChangeType::Unknown}};
    }


    std::vector<Change> compare_trees(shared_ptr<DirNode> from_tree, shared_ptr<DirNode> to_tree) {
        std::vector<Change> changes;
        // Check for things that have changed that are present in the old tree
        from_tree->for_node_in_tree(
            [to_tree, &changes](std::vector<std::string> path, shared_ptr<MetadataNode> from_metadata, bool is_dir) { 
                shared_ptr<MetadataNode> to_metadata;
                if(is_dir) {
                    auto to_dir = to_tree->get_child_dir(path);
                    if(to_dir) {
                        to_metadata = to_dir->metadata;
                    } else {
                        to_metadata = shared_ptr<MetadataNode>{};
                    }
                } else {
                    to_metadata = to_tree->get_child_file(path);
                }
                auto new_changes = compare_metadata(from_metadata, to_metadata, path_to_str(path));
                changes.insert(changes.end(), new_changes.begin(), new_changes.end());
            }
        );

        // Check for things that are new that are only in the new tree
        to_tree->for_node_in_tree(
            [from_tree, &changes](std::vector<std::string> path, shared_ptr<MetadataNode> to_metadata, bool is_dir) { 
                shared_ptr<MetadataNode> from_metadata;
                if(is_dir) {
                    auto to_dir = from_tree->get_child_dir(path);
                    if(to_dir) {
                        from_metadata = to_dir->metadata;
                    } else {
                        from_metadata = shared_ptr<MetadataNode>{};
                    }
                } else {
                    from_metadata = from_tree->get_child_file(path);
                }
                
                // The only change we are looking for is file and folder creations
                if(!from_metadata && to_metadata) {
                    changes.push_back(Change{
                        .type = ChangeType::Creation,
                        .earliest_change_time = to_metadata->mtime,
                        .latest_change_time = 0,
                        .file = {.path=path_to_str(path), .type=to_metadata->ftype},
                    });
                }
            }
        );

        return changes;
    }


    std::vector<Change> deserialize_changes(std::istream& stream) {
        std::vector<Change> changes;

        Change current_change{.type = ChangeType::Unknown};
        while(true) {
            current_change = Change::deserialize(stream);
            if(current_change.type == ChangeType::TerminateList) {
                return changes;
            }
            changes.push_back(current_change);
        }
    }


    void serialize_changes(std::ostream& stream, std::vector<Change> changes, bool show_loading_bar) {
        size_t total_changes{changes.size()};
        size_t changes_count{0};

        for(const auto& change : changes) {
            if(show_loading_bar && (changes_count % 500) == 0) {
                term()->update_progress_bar(static_cast<float>(changes_count) / static_cast<float>(total_changes), "Write Changes");
            }
            changes_count++;
            change.serialize(stream);
        }
        auto terminator = Change {.type = ChangeType::TerminateList};
        terminator.serialize(stream);

        if(show_loading_bar) {
            term()->complete_progress_bar();
        }
    }


    bool append_changes(std::string path, std::vector<Change> new_changes) {
        auto all_changes = read_changes(path);
        // Append new changes
        all_changes.insert(all_changes.end(), new_changes.begin(), new_changes.end());
        // Write new change log
        write_changes(path, all_changes);
        return 0;
    }


    std::vector<Change> read_changes(std::string base_dir) {
        // Note: This function fails silently and returns an empty vector if the file is not found
        std::string changes_path = join_path(base_dir, ".fmerge/filechanges.db");
        std::vector<Change> changes{};
        if(exists(changes_path)) {
            std::ifstream changes_file(changes_path);
            changes = deserialize_changes(changes_file);
        }
        return changes;
    }


    void write_changes(std::string base_dir, std::vector<Change> changes) {
        std::string changes_path = join_path(base_dir, ".fmerge/filechanges.db");
        std::ofstream changes_file(changes_path, std::ios_base::trunc);
        serialize_changes(changes_file, changes, true);
    }


    std::vector<Change> get_new_tree_changes(std::string path) {
        std::string filetree_file = join_path(path, ".fmerge/filetree.db");

        auto root_stats = get_file_stats(path);
        auto root_node = std::make_shared<DirNode>(split_path(path).back(), root_stats->type, root_stats->mtime);
        update_file_tree(root_node, path); // This is where the current file tree is built in memory

        // Attempt to detect changes
        std::vector<Change> new_changes;
        if(!exists(filetree_file)) {
            termbuf() << "No historical filetree.db containing historical data found. Assuming new folder." << std::endl;
            
            auto blank_node = std::make_shared<DirNode>(split_path(path).back(), FileType::Directory, 0);
            new_changes = compare_trees(blank_node, root_node);
        } else {
            std::ifstream serialized_tree(filetree_file, std::ios_base::binary); 
            auto read_from_disk_node = DirNode::deserialize(serialized_tree);
            // Only check for changes if we were able to load historical data
            new_changes =  compare_trees(read_from_disk_node, root_node);
        }

        // Update the filetree.db with the latest state
        std::ofstream serialized_tree(filetree_file, std::ios_base::trunc | std::ios_base::binary); 
        root_node->serialize(serialized_tree);

        return new_changes;
    }

}