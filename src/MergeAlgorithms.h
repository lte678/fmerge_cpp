#pragma once

#include "FileTree.h"

#include <unordered_map>
#include <map>
#include <functional>


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

    typedef std::map<std::string, std::vector<FileOperation>, std::greater<std::string>> SortedOperationSet;


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

        friend bool operator<(const Conflict& l, const Conflict& r) {
            return l.conflict_key < r.conflict_key;
        }
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

    std::tuple<SortedChangeSet, std::vector<Conflict>>
        merge_change_sets(const SortedChangeSet &loc, const SortedChangeSet &rem, const std::unordered_map<std::string, ConflictResolution> &resolutions);

    // Merges two lists of changes into a single list containing both sets. 
    // Will fail if an obvious merge is not possible and user intervention is required.
    optional<vector<Change>> try_automatic_resolution(const vector<Change> &rem, const vector<Change> &loc);

    // Create an unordered map of file operations for each file-key.
    SortedOperationSet construct_operation_set(const SortedChangeSet &current, const SortedChangeSet& target);

    // Create a list of file operations that create the given changes that originate from the remote
    std::vector<FileOperation> construct_operations(const vector<Change> &current, const vector<Change> &target);

    // Simplify the list of file operations to be performed to a minimal set.
    SortedOperationSet squash_operations(const SortedOperationSet& ops);

    /// @brief Sorts the given list of conflicts alphabetically according to their path. Useful for outputting to the user
    /// and for calculating statistics of the dataset.
    /// @param conflicts List of conflicts which is sorted in-place
    void sort_conflicts_alphabetically(std::vector<Conflict>& conflicts);

    /// @brief Prints the conflicts in a more easily readable form
    /// @param conflicts Conflicts. Must be alphabetically sorted!
    void print_conflicts(const std::vector<Conflict>& conflicts);

    /// Simplify the list of changes to the final resulting file
    /// @returns Timestamp of latest modification if file exists, or 0 if it is deleted.
    /// The timestamp is used as a unique hash to identify a specific file revision in this case.
    long squash_changes(const vector<Change> &changes);

    bool is_change_equal(const Change& lhs, const Change& rhs);

    void print_sorted_changes(const SortedChangeSet &sorted_changes);
    void print_sorted_operations(const SortedOperationSet &sorted_ops);
}