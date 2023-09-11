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


    ConflictResolutionSet translate_peer_resolutions(ConflictResolutionSet local_set) {
        ConflictResolutionSet peer_set{};
        for(const auto &resolution : local_set) {
            if (resolution.second == ConflictResolution::KeepLocal) {
                peer_set.emplace(resolution.first, ConflictResolution::KeepRemote);
            } else if(resolution.second == ConflictResolution::KeepRemote) {
                peer_set.emplace(resolution.first, ConflictResolution::KeepLocal);
            } else {
                std::cerr << "[Error] translate_peer_resolutions" << std::endl;
            }
        }
        return peer_set;
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


    std::tuple<SortedChangeSet, std::vector<Conflict>>
        merge_change_sets(const SortedChangeSet& loc, const SortedChangeSet& rem, const std::unordered_map<std::string, ConflictResolution> &resolutions) {
        
        SortedChangeSet merged_set{};
        std::vector<Conflict> conflicts{};

        unsigned long progress_counter{0};
        float progress_counter_end{static_cast<float>(loc.size() + rem.size())};

        // termbuf() << "Merging " <<  loc.size() + rem.size() << " changes..." << std::endl;
        // termbuf() << "Using " << loc.bucket_count() << " buckets" << std::endl;

        term()->start_progress_bar("Merging");

        // Work starting with the 'loc' branch, but this process MUST be symmetric!
        for(const auto &file_changes : loc) {
            // TODO: Check if progress bar option enabled
            if(progress_counter % 100) {
                term()->update_progress_bar(static_cast<float>(progress_counter) / progress_counter_end);
            }
            progress_counter++;

            const auto &path = file_changes.first;
            auto& changes = file_changes.second;
            if(change_set_contains(rem, path)) {
                if(resolutions.find(path) != resolutions.end()) {
                    // A resolution has been specified
                    switch(resolutions.at(path)) {
                    case ConflictResolution::KeepLocal:
                        merged_set.emplace(path, changes);
                        break;
                    case ConflictResolution::KeepRemote:
                        merged_set.emplace(path, rem.at(path));
                        break;
                    default:
                        std::cerr << "[Error] Invalid resolution type " << static_cast<int>(resolutions.at(path)) << std::endl;
                    }
                } else {
                    // The user has not specified a resolution
                    auto rem_file_changes = rem.at(path);
                    auto file_merge_result = try_automatic_resolution(rem_file_changes, changes);
                    if(file_merge_result.has_value()) {
                        // Automatic resolution successful
                        merged_set.emplace(path, file_merge_result.value());
                    } else {
                        // No automatic resolution was possible
                        conflicts.emplace_back(path);
                    }
                }
                
            } else {
                // Trivial merge. The other branch never did anything with this file
                merged_set.emplace(path, changes);
            }
        }

        // Repeat for 'rem' branch, but ignore conflicts, since these must already all have been resolved
        for(const auto &file_changes : rem) {
            if(progress_counter % 100) {
                term()->update_progress_bar(static_cast<float>(progress_counter) / progress_counter_end);
            }
            progress_counter++;

            const auto &path = file_changes.first;
            auto& changes = file_changes.second;
            if(!change_set_contains(loc, path)) {
                // Trivial merge. The other branch never did anything with this file
                merged_set.emplace(path, changes);
            }
        }

        // Done merging
        term()->complete_progress_bar();
        if(conflicts.size() > 0) {
            return std::make_tuple(SortedChangeSet{}, conflicts);
        }
        return std::make_tuple(merged_set, conflicts);
    }


    std::optional<std::vector<Change>> try_automatic_resolution(const std::vector<Change> &rem, const std::vector<Change> &loc) {
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
            return loc;
        } else {
            return rem;
        }
    }


    SortedOperationSet construct_operation_set(const SortedChangeSet &current, const SortedChangeSet& target) {
        SortedOperationSet ops{};

        for(const auto &target_changes : target) {
            if(change_set_contains(current, target_changes.first)) {
                const auto &current_changes = current.at(target_changes.first);
                ops.emplace(target_changes.first, construct_operations(current_changes, target_changes.second));
            } else {
                ops.emplace(target_changes.first, construct_operations(std::vector<Change>{}, target_changes.second));
            }
        }
        return ops;
    }


    std::vector<FileOperation> construct_operations(const vector<Change> &current, const vector<Change> &target) {
        std::vector<FileOperation> ops;

        auto target_mtime = squash_changes(target);
        auto current_mtime = squash_changes(current);
        // NOTE: The modification time is used as a hash
        if(target_mtime == 0) {
            // Delete the file if it exists
            if(current_mtime != 0) {
                // The file currently exists on disk
                ops.push_back(FileOperation(FileOperationType::Delete, target.back().file.path));
            }
            // else
            //      The file does not exist locally. Do not delete it
        } else {
            // A version of the file exists in the target state
            if(target_mtime != current_mtime) {
                // The file versions are not identical
                ops.push_back(FileOperation(FileOperationType::Transfer, target.back().file.path));
            }
            // else
            //      The file versions are identical. Do not do anything
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


    long squash_changes(const vector<Change> &changes) {
        long mtime{0};
        if(!changes.empty()) {
            const auto& change = changes.back();
            switch(change.type) {
            case ChangeType::Creation:
            case ChangeType::Modification:
                mtime = change.earliest_change_time;
                break;
            case ChangeType::Deletion:
                mtime = 0;
                break;
            default:
                std::cerr << "[Error] squash_changes: Unhandled change type!" << std::endl;
            }
        }
        return mtime;
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
            termbuf() << "    " << std::setw(64) << std::left << change_set.first << ":";
            for(const auto &change : change_set.second) {
                termbuf() << " " << change.type;
            }
            termbuf() << std::endl;
        }
    }


    void print_sorted_operations(const SortedOperationSet &sorted_ops) {
        for(const auto &ops : sorted_ops) {
            for(const auto &op : ops.second) {
                termbuf() << "    " << op;
            }
        }
    }
}