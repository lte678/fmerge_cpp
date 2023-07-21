#include "Terminal.h"

#include <iostream>
#include <cmath>


namespace fmerge {
    constexpr int PROGRESS_BAR_WIDTH = 45; // Does not include trailing percentage

    void print_progress_bar(float progress, std::string trailing) {
        // Call repeatedly without printing anything else.
        // Once finished, insert a newline.
        int steps = PROGRESS_BAR_WIDTH - 2;
        int i_progress = std::round(progress * steps);
        std::cout << "[";
        for(int i = 1; i < (steps + 1); i++) {
            if(i <= i_progress) {
                std::cout << "#";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "] " << trailing << " " << std::round(progress * 100.0f) << "%\r";
    }

    char prompt_choice(const std::string &options) {
        std::string response;
        while(true) {
            std::cout << "[";
            for(size_t i = 0; i < options.length(); i++) {
                if(i != 0) {
                    std::cout << "/";
                }
                std::cout << options[i];
            }
            std::cout << "] ";
            std::cin >> response;
            if(response.length() > 1 || (options.find(response[0]) == std::string::npos)) {
                std::cout << "Invalid option." << std::endl;
            } else {
                return response[0];
            }
        }
    }
}