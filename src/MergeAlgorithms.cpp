#include "MergeAlgorithms.h"

#include <iomanip>


namespace fmerge {
    
    inline bool change_set_contains(SortedChangeSet set, std::string key) { return set.find(key) != set.end(); };


    std::ostream& operator<<(std::ostream& os, FileOperationType fop_type) {
        if(fop_type == FileOperationType::Delete) {
            os << "DELETE";
        } else if(fop_type == FileOperationType::PlaceholderRevert) {
            os << "PLACEHOLDER_REVERT";
        } else if(fop_type == FileOperationType::Transfer) {
            os << "TRANSFER";
        } else {
            os << "UNKNOWN";
        }
        return os;
    }


    std::ostream& operator<<(std::ostream& os, const FileOperation& fop) {
        os << std::setw(64) << std::left << fop.path << ": " << fop.type << std::endl;
        return os;
    }


    SortedChangeSet sort_changes_by_file(std::vector<Change> changes) {
        SortedChangeSet sorted_set{};
        for(const auto &change : changes) {
            if(sorted_set.find(change.path) == sorted_set.end()) {
                // The key is not yet present in the unordered map
                sorted_set.emplace(change.path, std::vector<Change>{change});
            } else {
                // The key is present
                sorted_set.at(change.path).push_back(change);
            }
        }
        return sorted_set;
    }


    std::vector<Change> recombine_changes_by_file(SortedChangeSet changes) {
        std::vector<Change> unsorted_set{};
        for(const auto &change : changes) {
            unsorted_set.insert(unsorted_set.end(), change.second.begin(), change.second.end());
        }
        return unsorted_set;
    }


    std::tuple<SortedChangeSet, SortedOperationSet, std::vector<Conflict>>
        merge_change_sets(const SortedChangeSet& loc, const SortedChangeSet& rem, const std::unordered_map<std::string, ConflictResolution> &resolutions) {
        
        SortedChangeSet merged_set{};
        SortedOperationSet operations{}; // The file operations required to achieve the merged file tree on the local device
        std::vector<Conflict> conflicts{};

        // Work starting with the 'loc' branch, but this process MUST be symmetric!
        for(const auto &file_changes : loc) {
            const auto &path = file_changes.first;
            if(change_set_contains(rem, path)) {
                // TODO: Add some smarter merge conflict resolutions
                if(resolutions.find(path) != resolutions.end()) {
                    // A resolution has been specified
                    switch(resolutions.at(path)) {
                    case ConflictResolution::KeepLocal:
                        merged_set.emplace(path, file_changes.second);
                        // No changes to file tree required
                        break;
                    case ConflictResolution::KeepRemote:
                        merged_set.emplace(path, rem.at(path));
                        // File tree must be modified.
                        // Revert our discarded changes
                        operations.emplace(path, revert_changes(file_changes.second));
                        // Add the selected remote changes
                        operations.emplace(path, construct_changes(rem.at(path)));
                        break;
                    default:
                        std::cerr << "[Error] Invalid resolution type " << static_cast<int>(resolutions.at(path)) << std::endl;
                    }
                } else {
                    // The user has not specified a resolution
                    conflicts.emplace_back(path);
                }
                
            } else {
                // Trivial merge. The other branch never did anything with this file
                merged_set.emplace(path, file_changes.second);
            }
        }

        // Repeat for 'rem' branch, but ignore conflicts, since these must already all have been resolved
        for(const auto &file_changes : rem) {
            const auto &path = file_changes.first;
            if(!change_set_contains(loc, path)) {
                // Trivial merge. The other branch never did anything with this file
                merged_set.emplace(path, file_changes.second);
                // These changes will have to be constructed locally
                operations.emplace(path, construct_changes(file_changes.second));
            }
        }

        // Done merging
        if(conflicts.size() > 0) {
            return std::make_tuple(SortedChangeSet{}, SortedOperationSet{}, conflicts);
        }

        operations = squash_operations(operations);
        return std::make_tuple(merged_set, operations, conflicts);
    }


    std::vector<FileOperation> construct_changes(std::vector<Change> changes) {
        // Make remote change local
        std::vector<FileOperation> ops;
        for(const auto &change: changes) {
            switch(change.type) {
            case ChangeType::Creation:
                // Transfer created file to our machine to also "create" it.
                ops.emplace_back(FileOperation{FileOperationType::Transfer, change.path});
                break;
            case ChangeType::Deletion:
                // Also delete the file on our machine
                ops.emplace_back(FileOperation{FileOperationType::Delete, change.path});
                break;
            case ChangeType::Modification:
                // Transfer modified file to our machine to also "modify" it.
                ops.emplace_back(FileOperation{FileOperationType::Transfer, change.path});
                break;
            default:
                std::cout << "[Error] Unknown change type " << change.type << std::endl;
            }
        }
        return ops;
    }


    std::vector<FileOperation> revert_changes(std::vector<Change> changes) {
        // Destroy local changes
        std::vector<FileOperation> ops;
        for(const auto &change: changes) {
            switch(change.type) {
            case ChangeType::Creation:
                // To revert the file tree we have to delete the file again
                ops.emplace_back(FileOperation{FileOperationType::Delete, change.path});
                break;
            case ChangeType::Deletion:
                // To revert deletion, we have to recreate it. This is impossible.
                // Thus we use a placeholder revert, which is optimized away, since this always indirectly
                // implies that another remote file will be copied over this one.
                ops.emplace_back(FileOperation{FileOperationType::PlaceholderRevert, change.path});
                break;
            case ChangeType::Modification:
                // Reverting is impossible.
                // Thus we use a placeholder revert, which is optimized away, since this always indirectly
                // implies that another remote file will be copied over this one.
                ops.emplace_back(FileOperation{FileOperationType::PlaceholderRevert, change.path});
                break;
            default:
                std::cout << "[Error] Unknown change type " << change.type << std::endl;
            }
        }
        return ops;
    }


    SortedOperationSet squash_operations(const SortedOperationSet& ops) {
        // Since there is no rename, only the last operation in the chain is relevant.
        // This is currently a trivial case, but it could become more complicated.
        SortedOperationSet sqaushed_set{};
        for(const auto &op: ops) {
            if(op.second.size() != 0) {
                sqaushed_set.emplace(op.first, std::vector<FileOperation>({op.second.back()}));   
            }
        }
        return sqaushed_set;
    }


    void print_sorted_changes(const SortedChangeSet &sorted_changes) {
        for(const auto &change_set : sorted_changes) {
            std::cout << "    " << std::setw(64) << std::left << change_set.first << ":";
            for(const auto &change : change_set.second) {
                std::cout << " " << change.type;
            }
            std::cout << std::endl;
        }
    }


    void print_sorted_operations(const SortedOperationSet &sorted_ops) {
        for(const auto &ops : sorted_ops) {
            for(const auto &op : ops.second) {
                std::cout << "    " << op;
            }
        }
    }
}