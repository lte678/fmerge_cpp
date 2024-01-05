// Author: Leon Teichröb
// Date:   26.08.2023
//
// Description:  UI elements for user-resolution of conflicts. 


#include "ConflictResolver.h"

#include "Terminal.h"
#include "Errors.h"
#include "Util.h"

#include <iomanip>
#include <ctime>


namespace fmerge {
    constexpr int HEADER_WIDTH = 80;
    constexpr char HEADER_CHAR = '=';
    constexpr int CHANGE_TYPE_WIDTH = 14;
    constexpr int CHANGE_TIME_WIDTH = 26;
    constexpr int CHANGE_WIDTH = CHANGE_TYPE_WIDTH + CHANGE_TIME_WIDTH;


    static std::string timetostr(time_t unix_time) {
        char timestamp_str[128];
        struct tm *local_time = localtime(&unix_time);
        if(local_time == nullptr) {
            print_clib_error("localtime");
        }
        if(strftime(timestamp_str, sizeof(timestamp_str), "%b %e %H:%M:%S %Y", local_time) == 0) {
            print_clib_error("strftime");
        }

        return std::string(timestamp_str);
    }


    static void print_change_comparison(const std::vector<Change>& loc, const std::vector<Change>& rem) {
        LOG(make_centered("~~~ LOCAL ~~~", CHANGE_WIDTH) << make_centered("~~~ REMOTE ~~~", CHANGE_WIDTH) << std::endl);

        size_t list_length = std::max(loc.size(), rem.size());
        for(size_t i = 0; i < list_length; i++) {
            // Print left column
            if(i < loc.size()) {
                const auto& change = loc[i];
                LOG(std::setw(CHANGE_TYPE_WIDTH) << change.type);
                LOG(std::setw(CHANGE_TIME_WIDTH) << timetostr(change.earliest_change_time));
            } else {
                LOG(std::string(CHANGE_WIDTH, ' '));
            }
    
            // Print right column
            if(i < rem.size()) {
                const auto& change = rem[i];
                LOG(std::setw(CHANGE_TYPE_WIDTH) << change.type);
                LOG(std::setw(CHANGE_TIME_WIDTH) << timetostr(change.earliest_change_time));
            } else {
                LOG(std::string(CHANGE_WIDTH, ' '));
            }
            LOG(std::endl);
        }
    }


    /// @brief Asks for a single resolution while providing more advanced selection options.
    /// For example, here the user might select to resolve an entire folder instead of just the file
    /// @return Returns the set of resolutions
    static std::unordered_map<std::string, ConflictResolution> ask_for_adv_resolution(const std::vector<Conflict>& conflicts, const Conflict& conflict) {
        auto key = conflict.conflict_key;
        auto path_tokens = split_path(key);

        LOG("Advanced resolution options:" << std::endl);
        std::vector<std::pair<std::string, std::string>> options{};
        options.emplace_back("l", "Keep Local");
        options.emplace_back("r", "Keep Remote");
        for(size_t i = 0; i < path_tokens.size() - 1; i++) {
            auto path_str = path_to_str(std::vector(path_tokens.begin(), path_tokens.begin() + i + 1));
            options.emplace_back("l" + std::to_string(i), "Keep Local  Directory " + path_str);
            options.emplace_back("r" + std::to_string(i), "Keep Remote Directory " + path_str);
        }
        auto resp = term()->prompt_list_choice(options);

        std::unordered_map<std::string, ConflictResolution> resolutions{};

        if(resp == "l") {
            resolutions.emplace(key, ConflictResolution::KeepLocal);
        } else if(resp == "r") { 
            resolutions.emplace(key, ConflictResolution::KeepRemote);
        } else if(resp.size() > 1) {
            ConflictResolution res;
            if(resp[0] == 'l') {
                res = ConflictResolution::KeepLocal;
            } else {
                res = ConflictResolution::KeepRemote;
            }
            int depth = std::stoi(resp.substr(1));
            auto path_str = path_to_str(std::vector(path_tokens.begin(), path_tokens.begin() + depth + 1));
            
            // Find the applicable conflicts we resolve with this large scale resolution
            for(const auto& c : conflicts) {
                // See if the path starts with this path
                if(c.conflict_key.rfind(path_str, 0) == 0) {
                    // If it does, then add the resolution to it
                    resolutions.emplace(c.conflict_key, res);
                }
            }
        } else {
            std::cerr << "ask_for_adv_resolution:error invalid option " << resp << std::endl;
        }

        return resolutions;
    }


    std::unordered_map<std::string, ConflictResolution> ask_for_resolutions(const std::vector<Conflict>& conflicts, const SortedChangeSet& loc, const SortedChangeSet& rem) {
        std::unordered_map<std::string, ConflictResolution> resolutions{};
        
        size_t nr_conflicts = conflicts.size();
        std::string header_str = "RESOLVING " + std::to_string(nr_conflicts) + " CONFLICTS";

        // Print the header
        LOG(std::string(HEADER_WIDTH, HEADER_CHAR) << std::endl);
        LOG(make_centered(header_str, HEADER_WIDTH, HEADER_CHAR) << std::endl);
        LOG(std::string(HEADER_WIDTH, HEADER_CHAR) << std::endl);
        LOG(std::endl);

        for(const auto& conflict : conflicts) {
            auto key = conflict.conflict_key;
            if(resolutions.find(key) != resolutions.end()) {
                // We already added a resolution somehow while processing!
                // This is probably due to a directory-wide resolution which will also resolve
                // some of the conflicts that otherwise would be queried later.
                continue;
            }

            termbuf() << make_centered(
                "[ " + std::to_string(resolutions.size()) + " / " + std::to_string(conflicts.size()) + " ]", 
                HEADER_WIDTH, HEADER_CHAR) << std::endl;
            LOG(make_centered("CONFLICT: " + key, HEADER_WIDTH, HEADER_CHAR) << std::endl);

            print_change_comparison(loc.at(key), rem.at(key));

            LOG("(Local, Remote, Other) ");
            auto choice = term()->prompt_choice("lro");
            if(choice == 'l') {
                resolutions.emplace(key, ConflictResolution::KeepLocal);
            } else if (choice == 'r') {
                resolutions.emplace(key, ConflictResolution::KeepRemote);
            } else if (choice == 'o') {
                auto adv_resolutions = ask_for_adv_resolution(conflicts, conflict);
                // Merge old resolutions into new ones,
                adv_resolutions.merge(resolutions);
                resolutions = adv_resolutions;
            } else {
                // Returned 0, meaning that input was interrupted
                return {};
            }
        }

        return resolutions;
    }
}