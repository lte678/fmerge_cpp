#include "MergeAlgorithms.h"

#include "Terminal.h"

#include <iomanip>


namespace fmerge {
    
    inline bool change_set_contains(const SortedChangeSet &set, std::string key) { return set.find(key) != set.end(); };


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
            if(sorted_set.find(change.file.path) == sorted_set.end()) {
                // The key is not yet present in the unordered map
                sorted_set.emplace(change.file.path, std::vector<Change>{change});
            } else {
                // The key is present
                sorted_set.at(change.file.path).push_back(change);
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

        unsigned long progress_counter{0};
        float progress_counter_end{static_cast<float>(loc.size() + rem.size())};

        // std::cout << "Merging " <<  loc.size() + rem.size() << " changes..." << std::endl;
        // std::cout << "Using " << loc.bucket_count() << " buckets" << std::endl;

        // Work starting with the 'loc' branch, but this process MUST be symmetric!
        for(const auto &file_changes : loc) {
            // TODO: Check if progress bar option enabled
            if(progress_counter % 100) {
                print_progress_bar(static_cast<float>(progress_counter) / progress_counter_end, "Merging");
            }
            progress_counter++;

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
                    auto rem_file_changes = rem.at(path);
                    auto file_merge_result = try_automatic_resolution(rem_file_changes, file_changes.second);
                    if(file_merge_result.has_value()) {
                        // Automatic resolution successful
                        merged_set.emplace(path, file_merge_result->first);
                        // Revert old changes and add merged changes.
                        // This will lead to the set of operations required
                        // to achieve the desired final merged state
                        operations.emplace(path, file_merge_result->second);
                    } else {
                        // No automatic resolution was possible
                        conflicts.emplace_back(path);
                    }
                }
                
            } else {
                // Trivial merge. The other branch never did anything with this file
                merged_set.emplace(path, file_changes.second);
            }
        }

        // Repeat for 'rem' branch, but ignore conflicts, since these must already all have been resolved
        for(const auto &file_changes : rem) {
            if(progress_counter % 100) {
                print_progress_bar(static_cast<float>(progress_counter) / progress_counter_end, "Merging");
            }
            progress_counter++;

            const auto &path = file_changes.first;
            if(!change_set_contains(loc, path)) {
                // Trivial merge. The other branch never did anything with this file
                merged_set.emplace(path, file_changes.second);
                // These changes will have to be constructed locally
                operations.emplace(path, construct_changes(file_changes.second));
            }
        }

        // Done merging
        print_progress_bar(1.0f, "Merging");
        std::cout << std::endl;
        if(conflicts.size() > 0) {
            return std::make_tuple(SortedChangeSet{}, SortedOperationSet{}, conflicts);
        }

        operations = squash_operations(operations);
        return std::make_tuple(merged_set, operations, conflicts);
    }


    std::optional<std::pair<std::vector<Change>, std::vector<FileOperation>>> try_automatic_resolution(const std::vector<Change> &rem, const std::vector<Change> &loc) {
        // Algorithms used to merge the change lists:
        // Equal stems: Check if one branch is ahead of the other. The just fast-forward it a la git.
        for(size_t i = 0; i < loc.size(); i++) {
            if(i >= rem.size()) {
                // The change lists match up to the latest common additions.
                break;
            } else if(!is_change_equal(rem[i], loc[i])) {
                return std::nullopt;
            }
        }
        // The change list stems match. Now take the longer change list
        if(loc.size() >= rem.size()) {
            // Use local changes. No local operations required
            return std::pair<std::vector<Change>, std::vector<FileOperation>>{loc, std::vector<FileOperation>{}};
        } else {
            std::vector<Change> new_local_changes(rem.begin() + loc.size(), rem.end());
            return std::pair<std::vector<Change>, std::vector<FileOperation>>{rem, construct_changes(new_local_changes)};
        }
    }


    std::vector<FileOperation> construct_changes(std::vector<Change> changes) {
        // Make remote change local
        std::vector<FileOperation> ops;
        for(const auto &change: changes) {
            switch(change.type) {
            case ChangeType::Creation:
                // Transfer created file to our machine to also "create" it.
                ops.emplace_back(FileOperation{FileOperationType::Transfer, change.file.path});
                break;
            case ChangeType::Deletion:
                // Also delete the file on our machine
                ops.emplace_back(FileOperation{FileOperationType::Delete, change.file.path});
                break;
            case ChangeType::Modification:
                // Transfer modified file to our machine to also "modify" it.
                ops.emplace_back(FileOperation{FileOperationType::Transfer, change.file.path});
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
                ops.emplace_back(FileOperation{FileOperationType::Delete, change.file.path});
                break;
            case ChangeType::Deletion:
                // To revert deletion, we have to recreate it. This is impossible.
                // Thus we use a placeholder revert, which is optimized away, since this always indirectly
                // implies that another remote file will be copied over this one.
                ops.emplace_back(FileOperation{FileOperationType::PlaceholderRevert, change.file.path});
                break;
            case ChangeType::Modification:
                // Reverting is impossible.
                // Thus we use a placeholder revert, which is optimized away, since this always indirectly
                // implies that another remote file will be copied over this one.
                ops.emplace_back(FileOperation{FileOperationType::PlaceholderRevert, change.file.path});
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


    bool is_change_equal(const Change& lhs, const Change& rhs) {
        if(lhs.file.type != rhs.file.type) {
            return false;
        }
        if(!lhs.file.is_dir() && (lhs.earliest_change_time != rhs.earliest_change_time)) {
            return false;
        }
        if(!lhs.file.is_dir() && (lhs.latest_change_time != rhs.latest_change_time)) {
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