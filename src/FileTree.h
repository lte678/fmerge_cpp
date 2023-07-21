#pragma once

#include "Filesystem.h"

#include <optional>
#include <functional>
#include <iostream>
#include <vector>
#include <memory>
#include <string>


using std::shared_ptr;
using std::optional;


namespace fmerge {

    class MetadataNode {
    public:
        MetadataNode() = delete;
        MetadataNode(const char* _name, long _mtime) : name(_name), mtime(_mtime) {}
        MetadataNode(std::string _name, long _mtime);
        // Metadata
        const char* name;
        long mtime;
    public:
        void serialize(std::ostream& stream) const;
        static MetadataNode deserialize(std::istream& stream);
    };


    class DirNode : public std::enable_shared_from_this<DirNode> {
    public:
        DirNode() = delete;
        DirNode(shared_ptr<MetadataNode> _metadata) : metadata(_metadata) {}
        DirNode(MetadataNode _metadata) : metadata(std::make_shared<MetadataNode>(_metadata)) {}
        DirNode(std::string _name, long _mtime) : metadata(std::make_shared<MetadataNode>(_name, _mtime)) {}

        std::vector<shared_ptr<DirNode>> subdirs;
        std::vector<shared_ptr<MetadataNode>> files;
        // Metadata
        shared_ptr<MetadataNode> metadata;
    public:
        shared_ptr<MetadataNode> get_child_file(std::string file_name);
        shared_ptr<MetadataNode> get_child_file(std::vector<std::string> path_tokens);
        shared_ptr<DirNode> get_child_dir(std::string dir_name);
        shared_ptr<DirNode> get_child_dir(std::vector<std::string> path_tokens);

        void for_node_in_tree(std::function<void(std::vector<std::string>, shared_ptr<MetadataNode>, bool)> f, std::vector<std::string> prefix = {});

        void serialize(std::ostream& stream) const;
        static shared_ptr<DirNode> deserialize(std::istream& stream);
    };


    enum class ChangeType {
        Unknown,
        Modification,
        Creation,
        Deletion,
        FileType,
        TerminateList,
    };


    class Change {
    public:
        ChangeType type;
        // NOTE: Some changes have a precise time associated with them,
        // while others, such as deletion, are associated with an entire range.
        // This is due to the fact that deletion events, for example, can only be
        // inferred based on differences between one timestamp and another.
        long earliest_change_time{}; // This is the default field
        long latest_change_time{}; // Only used if range is necessary
        std::string path{};
    public:
        friend std::ostream& operator<<(std::ostream& os, const Change& change);

        void serialize(std::ostream& stream) const;
        static Change deserialize(std::istream& stream);
    };

    void update_file_tree(shared_ptr<DirNode> base_node, std::string base_path);
    optional<Change> compare_metadata(shared_ptr<MetadataNode> from_node, shared_ptr<MetadataNode> to_node, std::string path);
    std::vector<Change> compare_trees(shared_ptr<DirNode> from_tree, shared_ptr<DirNode> to_tree);

    std::vector<Change> read_changes(std::istream& stream);
    void write_changes(std::ostream& stream, std::vector<Change> changes);
    bool append_changes(std::string path, std::vector<Change> new_changes);

    std::vector<Change> get_new_tree_changes(std::string path);

}