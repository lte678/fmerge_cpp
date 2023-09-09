#pragma once

#include "FileTree.h"

#include <unordered_map>


namespace fmerge {

    using std::vector;
    using std::optional;
    using std::pair;

    typedef std::unordered_map<std::string, std::vector<Change>> SortedChangeSet;


    enum class FileOperationType {
        Transfer,
        Delete,
        CreateFolder,
        // We cannot revert files, since their historical variants don't exist.
        // This is still required as the anti-operation that is applied to modications that will
        // be overwritten. In theory it should always become optimized away after we simplify the
        // unification operations. If not, it will cause an error, since it is an impossible 
        // operation :) 
        PlaceholderRevert,
    };

    std::ostream& operator<<(std::ostream& os, FileOperationType fop_type);

    // The final product of the merge is a list of file operations to perform to achieve
    // the new, unified file structure
    struct FileOperation {
        FileOperation(FileOperationType _type, std::string _path) : type(_type), path(_path) {};

        FileOperationType type;
        std::string path;

        friend std::ostream& operator<<(std::ostream& os, const FileOperation& fop);
    };

    typedef std::unordered_map<std::string, std::vector<FileOperation>> SortedOperationSet;


    enum class ConflictResolution {
        // Defines how a conflict between two file histories should be resolved
        KeepLocal,
        KeepRemote,
    };


    inline std::ostream& operator<<(std::ostream& os, const ConflictResolution& res) { 
        if(res == ConflictResolution::KeepLocal) {
            os << "KeepLocal";
        } else if(res == ConflictResolution::KeepRemote) {
            os << "KeepRemote";
        } else {
            os << "ERROR";
        }
        return os;
    }


    struct Conflict {
        Conflict(std::string _conflict_key) : conflict_key(_conflict_key) {};

        // Key for conflicting entry in the associated SortedChangeSet
        std::string conflict_key;
    };

    typedef std::unordered_map<std::string, ConflictResolution> ConflictResolutionSet;
    
    // Translates the local set of conflict resolutions into the inverse resolutions for the
    // peer.
    ConflictResolutionSet translate_peer_resolutions(ConflictResolutionSet local_set);

    // Takes a list of changes and sorts them, so that each element of the unordered map
    // contains a list of changes only relevant to that specific file.
    SortedChangeSet sort_changes_by_file(std::vector<Change> changes);
    // Takes a map containing changes for each file as the key and returns all the sublists
    // as one merged change list. Preserves per-file change order. Does not preserve global
    // order.
    std::vector<Change> recombine_changes_by_file(SortedChangeSet changes);

    std::tuple<SortedChangeSet, SortedOperationSet, std::vector<Conflict>>
        merge_change_sets(const SortedChangeSet &loc, const SortedChangeSet &rem, const std::unordered_map<std::string, ConflictResolution> &resolutions);

    // Merges two lists of changes into a single list containing both sets. 
    // Will fail if an obvious merge is not possible and user intervention is required.
    optional<pair<vector<Change>, vector<FileOperation>>> try_automatic_resolution(const vector<Change> &rem, const vector<Change> &loc);

    // Create a list of file operations that create the given changes that originate from the remote
    std::vector<FileOperation> construct_operations(std::vector<Change> changes);

    // Create a list of file operations that revert the given changes on the local machine
    std::vector<FileOperation> revert_operations(std::vector<Change> changes);

    // Simplify the list of file operations to be performed to a minimal set.
    // Is critical to avoid unnecessary and also impossible operations that are generated during the merge.
    SortedOperationSet squash_operations(const SortedOperationSet& ops);

    bool is_change_equal(const Change& lhs, const Change& rhs);

    void print_sorted_changes(const SortedChangeSet &sorted_changes);
    void print_sorted_operations(const SortedOperationSet &sorted_ops);
}