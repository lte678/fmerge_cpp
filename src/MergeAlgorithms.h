#pragma once

#include "FileTree.h"

#include <unordered_map>


namespace fmerge {

    typedef std::unordered_map<std::string, std::vector<Change>> SortedChangeSet;

    // Takes a list of changes and sorts them, so that each element of the unordered map
    // contains a list of changes only relevant to that specific file.
    SortedChangeSet sort_changes_by_file(std::vector<Change> changes);

    void print_sorted_changes(const SortedChangeSet &sorted_changes);

    SortedChangeSet merge_change_sets(const SortedChangeSet &a, const SortedChangeSet &b);
}