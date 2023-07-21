#include "MergeAlgorithms.h"

#include <iomanip>


namespace fmerge {
    
    inline bool change_set_contains(SortedChangeSet set, std::string key) { return set.find(key) != set.end(); };

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


    void print_sorted_changes(const SortedChangeSet &sorted_changes) {
        for(const auto &change_set : sorted_changes) {
            std::cout << std::setw(64) << std::left << change_set.first << ":";
            for(const auto &change : change_set.second) {
                std::cout << " " << change.type;
            }
            std::cout << std::endl;
        }
    }

    SortedChangeSet merge_change_sets(const SortedChangeSet& a, const SortedChangeSet& b) {
        SortedChangeSet merged_set{};

        bool merge_conflict_occurred{false};

        // Work starting with the 'a' branch, but this process MUST be symmetric!
        for(const auto &file_changes : a) {
            if(change_set_contains(b, file_changes.first)) {
                // TODO: Add some smarter merge conflict resolutions
                std::cerr << "[Error] Unresolvable merge conflict for '" << file_changes.first << "'!" << std::endl;
                merge_conflict_occurred = true;
            } else {
                // Trivial merge. The other branch never did anything with this file
                merged_set.emplace(file_changes.first, file_changes.second);
            }
        }

        // Repeat for 'b' branch, but ignore conflicts, since these must already all have been resolved
        for(const auto &file_changes : b) {
            if(!change_set_contains(a, file_changes.first)) {
                // Trivial merge. The other branch never did anything with this file
                merged_set.emplace(file_changes.first, file_changes.second);
            }
        }

        if(merge_conflict_occurred) {
            return {};
        }

        // Done!
        return merged_set;
    }
}