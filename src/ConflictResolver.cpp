// Author: Leon Teichröb
// Date:   26.08.2023
//
// Description:  UI elements for user-resolution of conflicts. 


#include "ConflictResolver.h"

#include "Terminal.h"
#include "Errors.h"

#include <iomanip>
#include <ctime>


namespace fmerge {
    constexpr int HEADER_WIDTH = 80;
    constexpr char HEADER_CHAR = '=';
    constexpr int CHANGE_TYPE_WIDTH = 12;
    constexpr int CHANGE_TIME_WIDTH = 28;
    constexpr int CHANGE_WIDTH = CHANGE_TYPE_WIDTH + CHANGE_TIME_WIDTH;


    static std::string make_centered(const std::string& contents, int width, char padding_char = ' ') {
        auto inner_width = static_cast<int>(contents.length());

        // Amount of padding to put on left and right
        int padding_l = 0;
        int padding_r = 0;
        if(inner_width < width - 3) {
            padding_l = (width - inner_width) / 2 - 1;
            padding_r = width - inner_width - padding_l - 2;
        }

        return std::string(padding_l, padding_char) + " " + contents + " " + std::string(padding_r, padding_char);
    }


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
        termbuf() << make_centered("~~~ LOCAL ~~~", CHANGE_WIDTH) << make_centered("~~~ REMOTE ~~~", CHANGE_WIDTH) << std::endl;

        size_t list_length = std::max(loc.size(), rem.size());
        for(size_t i = 0; i < list_length; i++) {
            // Print left column
            if(i < loc.size()) {
                const auto& change = loc[i];
                termbuf() << std::setw(CHANGE_TYPE_WIDTH) << change.type;
                termbuf() << std::setw(CHANGE_TIME_WIDTH) << timetostr(change.earliest_change_time);
            } else {
                termbuf() << std::string(CHANGE_WIDTH, ' ');
            }
    
            // Print right column
            if(i < rem.size()) {
                const auto& change = rem[i];
                termbuf() << std::setw(CHANGE_TYPE_WIDTH) << change.type;
                termbuf() << std::setw(CHANGE_TIME_WIDTH) << timetostr(change.earliest_change_time);
            } else {
                termbuf() << std::string(CHANGE_WIDTH, ' ');
            }
            termbuf() << std::endl;
        }
    }


    std::unordered_map<std::string, ConflictResolution> ask_for_resolutions(const std::vector<Conflict>& conflicts, const SortedChangeSet& loc, const SortedChangeSet& rem) {
        std::unordered_map<std::string, ConflictResolution> resolutions{};
        
        termbuf() << std::string(HEADER_WIDTH, HEADER_CHAR) << std::endl;
        termbuf() << make_centered("RESOLVING CONFLICTS", HEADER_WIDTH, HEADER_CHAR) << std::endl;
        termbuf() << std::string(HEADER_WIDTH, HEADER_CHAR) << std::endl;
        termbuf() << std::endl;

        for(const auto& conflict : conflicts) {
            auto key = conflict.conflict_key;
            termbuf() << make_centered("CONFLICT: " + key, HEADER_WIDTH, HEADER_CHAR) << std::endl;
            print_change_comparison(loc.at(key), rem.at(key));

            auto choice = term()->prompt_choice("lr");
            if(choice == 'l') {
                resolutions.emplace(key, ConflictResolution::KeepLocal);
            } else {
                resolutions.emplace(key, ConflictResolution::KeepRemote);
            }
        }

        return resolutions;
    }
}