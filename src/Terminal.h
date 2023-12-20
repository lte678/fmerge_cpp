#pragma once

#include "Globals.h"

#include <string>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <signal.h>


#define LOG(output)                           \
    do                                        \
    {                                         \
        std::unique_lock l(term()->print_mtx);\
        termbuf() << output;                  \
    } while (0)

#define DEBUG(output)                         \
    do                                        \
    {                                         \
        std::unique_lock l(term()->print_mtx);\
        if(g_debug_protocol)                  \
            termbuf() << "[DEBUG] " << output;\
    } while (0)


namespace fmerge {

    constexpr int DEFAULT_TERMINAL_WIDTH = 80;

    class Terminal : public std::stringbuf {
        // NOTE: Make sure to call kill_thread() at program exit (otherwise execution will hang)
    public:
        Terminal(std::ostream& _os, std::istream& _is);

        /// @brief Kill the cin listener thread. Must be called to terminate process if Terminal was used.
        inline void kill_thread() {
            sync();
            if(!istream_listener_thread_created) return;
            while(!istream_listener_thread.joinable()) {};
            pthread_kill(istream_listener_thread.native_handle(), SIGINT);
            istream_listener_thread.join();
        }

        void start_progress_bar(std::string trailing = "");
        void update_progress_bar(float progress);
        void complete_progress_bar();
        
        char prompt_choice(const std::string &options);
        void prompt_choice_async(
            const std::string &options, 
            std::function<void(char)> callback,
            std::function<void(void)> _cancel_callback = {});
        std::string prompt_list_choice(const std::vector<std::pair<std::string, std::string>> options);
        void prompt_list_choice_async(
            const std::vector<std::pair<std::string, std::string>> options,
            std::function<void(std::string)> callback,
            std::function<void(void)> _cancel_callback = {});
        void cancel_prompt();

        // Overridden from std::streambuf
        int sync();

        // This mutex must be acquired before the Terminal ostream is allowed to be used.
        std::mutex print_mtx;
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
    
        std::atomic_bool istream_listener_thread_created{false};
        std::thread istream_listener_thread{};
        // @returns true if the callback should be destroyed after being called.
        std::function<bool(std::string)> istream_callback{};
        std::function<void(void)> cancel_callback{};
        std::mutex istream_callback_lock{};

    };

    extern std::ostream* _stream;
    extern Terminal* _stream_term;
    void kill_term();
    Terminal* term();
    std::ostream& termbuf();
}