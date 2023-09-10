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

    // A tree consisting of Metadata nodes is constructed to represent the file 
    // system on disk. It includes the minimal set of metadata required by the change
    // detection algorithms.
    class MetadataNode {
    public:
        MetadataNode() = delete;
        MetadataNode(const char* _name, FileType _ftype, long _mtime) : name(_name), mtime(_mtime), ftype(_ftype) {}
        MetadataNode(std::string _name, FileType _ftype, long _mtime);
        // Metadata
        const char* name;
        long mtime;
        FileType ftype;
    public:
        void serialize(std::ostream& stream) const;
        static MetadataNode deserialize(std::istream& stream);
    };


    class DirNode : public std::enable_shared_from_this<DirNode> {
    public:
        DirNode() = delete;
        DirNode(shared_ptr<MetadataNode> _metadata) : metadata(_metadata) {}
        DirNode(MetadataNode _metadata) : metadata(std::make_shared<MetadataNode>(_metadata)) {}
        DirNode(std::string _name, FileType _ftype, long _mtime) : metadata(std::make_shared<MetadataNode>(_name, _ftype, _mtime)) {}

        std::vector<shared_ptr<DirNode>> subdirs{};
        std::vector<shared_ptr<MetadataNode>> files{};
        // Metadata
        shared_ptr<MetadataNode> metadata;
    public:
        shared_ptr<MetadataNode> get_child_file(std::string file_name);
        shared_ptr<MetadataNode> get_child_file(std::vector<std::string> path_tokens);
        shared_ptr<DirNode> get_child_dir(std::string dir_name);
        shared_ptr<DirNode> get_child_dir(std::vector<std::string> path_tokens);

        bool insert_node(std::vector<std::string> path_tokens, std::shared_ptr<DirNode> dir);
        bool insert_node(std::vector<std::string> path_tokens, std::shared_ptr<MetadataNode> file);
        bool remove_node(std::vector<std::string> path_tokens);

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


    std::ostream& operator<<(std::ostream& os, ChangeType change_type);

    class Change {
    public:
        ChangeType type;
        // NOTE: Some changes have a precise time associated with them,
        // while others, such as deletion, are associated with an entire range.
        // This is due to the fact that deletion events, for example, can only be
        // inferred based on differences between one timestamp and another.
        long earliest_change_time{}; // This is the default field
        long latest_change_time{}; // Only used if range is necessary
        File file{}; // Aggregate of path and dir/file/link indentification
    public:
        friend std::ostream& operator<<(std::ostream& os, const Change& change);
        friend bool operator==(const Change& lhs, const Change& rhs);

        void serialize(std::ostream& stream) const;
        static Change deserialize(std::istream& stream);
    };

    void update_file_tree(shared_ptr<DirNode> base_node, std::string base_path, bool show_loading_bar = true);
    std::vector<Change> compare_metadata(shared_ptr<MetadataNode> from_node, shared_ptr<MetadataNode> to_node, std::string path);
    std::vector<Change> compare_trees(shared_ptr<DirNode> from_tree, shared_ptr<DirNode> to_tree);

    std::vector<Change> deserialize_changes(std::istream& stream);
    void serialize_changes(std::ostream& stream, std::vector<Change> changes, bool show_loading_bar = false);
    bool append_changes(std::string path, std::vector<Change> new_changes);
    std::vector<Change> read_changes(std::string base_dir);
    void write_changes(std::string base_dir, std::vector<Change> changes);

    std::shared_ptr<DirNode> construct_tree_from_changes(std::vector<Change> changes);
    void insert_file_into_tree(std::shared_ptr<DirNode> root_node, const File& file, long mtime);
    void remove_file_from_tree(std::shared_ptr<DirNode> root_node, const File& file);

    std::vector<Change> get_new_tree_changes(std::string path);

}