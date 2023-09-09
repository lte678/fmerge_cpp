#pragma once

#include <string>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>


namespace fmerge {

    constexpr int DEFAULT_TERMINAL_WIDTH = 80;

    class Terminal : public std::stringbuf {
    public:
        Terminal(std::ostream& _os, std::istream& _is);

        void update_progress_bar(float progress, std::string trailing = "");
        void complete_progress_bar();
        
        char prompt_choice(const std::string &options);
        void prompt_choice_async(
            const std::string &options, 
            std::function<void(char)> callback,
            std::function<void(void)> _cancel_callback = {});
        void cancel_prompt();

        // Overridden from std::streambuf
        int sync();
    private:
        std::ostream& os;
        std::istream& is;

        // Predically reads the input stream to enable non-blocking user input
        void istream_listener();
        // Periodically prints internal stringbuf contents
        void print(std::string printable);

        // Width of terminal in characters. Resize not supported.
        int terminal_width{DEFAULT_TERMINAL_WIDTH};

        /// Set to the contents of the last line if it is still in progress and 
        /// was not completed with a newline.
        std::string last_line{};
        /// Moves the cursor to the position of the last line in the console.
        void cursor_to_last_line();

        std::string persistent_footer{};

        std::string progress_last_suffix{};
    
        std::thread istream_listener_thread{};
        // @returns true if the callback should be destroyed after being called.
        std::function<bool(std::string)> istream_callback{};
        std::function<void(void)> cancel_callback{};
        std::mutex istream_callback_lock{};

    };

    extern std::ostream* _stream;
    extern Terminal* _stream_term;
    Terminal* term();
    std::ostream& termbuf();
}