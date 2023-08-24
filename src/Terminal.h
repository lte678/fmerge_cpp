#pragma once

#include <string>
#include <functional>
#include <iostream>
#include <sstream>


namespace fmerge {
    using std::ostream, std::istream, std::string;

    class Terminal : public std::stringbuf {
    public:
        Terminal(ostream& _os, istream& _is);

        void update_progress_bar(float progress, string trailing = "");
        void complete_progress_bar();
        
        char prompt_choice(const string &options);
        void prompt_choice_async(const string &options, std::function<char(void)> callback);

        // Overridden from std::streambuf
        int sync();
    private:
        ostream& os;
        istream& is;

        void print(string printable);

        // Width of terminal in characters. Resize not supported.
        int terminal_width;

        /// Set to the contents of the last line if it is still in progress and 
        /// was not completed with a newline.
        string last_line{};
        /// Moves the cursor to the position of the last line in the console.
        void cursor_to_last_line();

        string persistent_footer{};

        string progress_last_suffix{};
    };

    extern std::ostream* _stream;
    extern Terminal* _stream_term;
    Terminal* term();
    std::ostream& termbuf();
}