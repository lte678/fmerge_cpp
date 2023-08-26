#include "MergeAlgorithms.h"

#include <vector>


namespace fmerge {
    std::unordered_map<std::string, ConflictResolution> ask_for_resolutions(const std::vector<Conflict>& conflicts, const SortedChangeSet& loc, const SortedChangeSet& rem);
}