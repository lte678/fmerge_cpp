#pragma once

#include <string>


namespace fmerge {

    void print_progress_bar(float progress, std::string trailing = "");
    char prompt_choice(const std::string &options);

}