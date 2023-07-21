#pragma once

#include "FileTree.h"

#include <unordered_map>


namespace fmerge {

    typedef std::unordered_map<std::string, std::vector<Change>> SortedChangeSet;


    enum class FileOperationType {
        Transfer,
        Delete,
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


    struct Conflict {
        Conflict(std::string _conflict_key) : conflict_key(_conflict_key) {};

        // Key for conflicting entry in the associated SortedChangeSet
        std::string conflict_key;
    };


    // Takes a list of changes and sorts them, so that each element of the unordered map
    // contains a list of changes only relevant to that specific file.
    SortedChangeSet sort_changes_by_file(std::vector<Change> changes);

    std::tuple<SortedChangeSet, SortedOperationSet, std::vector<Conflict>>
        merge_change_sets(const SortedChangeSet &loc, const SortedChangeSet &rem, const std::unordered_map<std::string, ConflictResolution> &resolutions);

    // Create a list of file operations that create the given changes that originate from the remote
    std::vector<FileOperation> construct_changes(std::vector<Change> changes);

    // Create a list of file operations that revert the given changes on the local machine
    std::vector<FileOperation> revert_changes(std::vector<Change> changes);

    // Simplify the list of file operations to be performed to a minimal set.
    // Is critical to avoid unnecessary and also impossible operations that are generated during the merge.
    SortedOperationSet squash_operations(const SortedOperationSet& ops);

    void print_sorted_changes(const SortedChangeSet &sorted_changes);
    void print_sorted_operations(const SortedOperationSet &sorted_ops);
}