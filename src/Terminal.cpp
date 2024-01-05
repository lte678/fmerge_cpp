#include "Errors.h"

#include "Util.h"
#include "Terminal.h"

#include <cmath>
#include <cstdlib>
#include <condition_variable>
#include <sys/ioctl.h>

namespace fmerge {
    using std::ostream, std::istream, std::string;

    ostream* _stream;
    Terminal* _stream_term;
    Terminal* term() {
        if(_stream_term == nullptr) {
            _stream_term = new Terminal(std::cout, std::cin);
        }
        return _stream_term;
    }
    ostream& termbuf() {
        if(_stream == nullptr) {
            _stream = new std::ostream(term());
        }
        return *_stream;
    }
    void kill_term() {
        if(_stream_term != nullptr) {
            _stream_term->kill_thread();
        }
    }

    constexpr int PROGRESS_BAR_WIDTH = 45; // Does not include trailing percentage

    Terminal::Terminal(ostream& _os, istream& _is) :
        os(_os), is(_is) {
        struct winsize w;
        if(ioctl(0, TIOCGWINSZ, &w) == -1) {
            std::cerr << "[Error] Could not fetch terminal width" << std::endl;
        }
        terminal_width = w.ws_col;

        // Create input listener thread
        istream_listener_thread_created = true;
        istream_listener_thread = std::thread([this]() { istream_listener(); });
    }

    void Terminal::start_progress_bar(string trailing) {
        progress_last_suffix = trailing;
        update_progress_bar(0.0f);
    }

    void Terminal::update_progress_bar(float progress) {
        // Call repeatedly without printing anything else.
        // Once finished, insert a newline.
        int steps = PROGRESS_BAR_WIDTH - 2;
        int i_progress = std::round(progress * steps);

        std::stringstream footer_str{};
        footer_str << "[";
        for(int i = 1; i < (steps + 1); i++) {
            if(i <= i_progress) {
                footer_str << "#";
            } else {
                footer_str << " ";
            }
        }
        footer_str << "] " << progress_last_suffix << " " << std::round(progress * 100.0f) << "%";
        os << footer_str.str() << "\r" << std::flush;
        persistent_footer = footer_str.str();
    }


    void Terminal::complete_progress_bar() {
        update_progress_bar(1.0f);
        os << std::endl;
        progress_last_suffix.clear();
        persistent_footer.clear();
    }


    char Terminal::prompt_choice(const string &options) {
        std::condition_variable flag;
        std::mutex flag_m;
        std::unique_lock<std::mutex> flag_lock(flag_m);

        char response{0};

        prompt_choice_async(options,
        [&response, &flag](char _response) {
            response = _response;
            flag.notify_all();
        },
        [&response, &flag]() {
            response = '\0';
            flag.notify_all();
        });

        flag.wait(flag_lock);
        return response;
    }


    void Terminal::prompt_choice_async(
        const string &options,
        std::function<void(char)> callback,
        std::function<void(void)> _cancel_callback) {
        
        auto put_prompt = [options]() {
            std::stringstream prompt_str{};
            prompt_str << "[";
            for(size_t i = 0; i < options.length(); i++) {
                if(i != 0) {
                    prompt_str << "/";
                }
                prompt_str << options[i];
            }
            prompt_str << "] ";
            LOG(prompt_str.str() << std::endl);
        };

        put_prompt();

        istream_callback_lock.lock();
        istream_callback = [this, options, callback, put_prompt](std::string response) {
            if(response.length() > 1 || (options.find(response[0]) == string::npos)) {
                LOG("Invalid option." << std::endl);
                put_prompt();
                return false;
            } else {
                callback(response[0]);
                return true;
            }
        };
        cancel_callback = _cancel_callback;
        istream_callback_lock.unlock();
    }


    string Terminal::prompt_list_choice(const std::vector<std::pair<string, string>> options) {
        std::condition_variable flag;
        std::mutex flag_m;
        std::unique_lock<std::mutex> flag_lock(flag_m);

        string response{""};

        prompt_list_choice_async(options,
        [&response, &flag](string _response) {
            response = _response;
            flag.notify_all();
        },
        [&response, &flag]() {
            flag.notify_all();
        });

        flag.wait(flag_lock);
        return response;
    }


    void Terminal::prompt_list_choice_async(
        const std::vector<std::pair<string, string>> options,
        std::function<void(string)> callback,
        std::function<void(void)> _cancel_callback) {

        auto put_prompt = [options]() {
            for(const auto& opt : options) {
                LOG(opt.first << ") " << opt.second << std::endl);
            }
            LOG(">" << std::endl);
        };

        put_prompt();

        istream_callback_lock.lock();
        istream_callback = [this, options, callback, put_prompt](std::string response) {
            for(const auto& opt: options) {
                if(opt.first == response) {
                    callback(response);
                    return true;
                }
            }
            LOG("Invalid option." << std::endl);
            put_prompt();
            return false;
        };
        cancel_callback = _cancel_callback;
        istream_callback_lock.unlock();
    }

    void Terminal::cancel_prompt() {
        istream_callback_lock.lock();
        istream_callback = {};
        istream_callback_lock.unlock();
        if(cancel_callback) {
            cancel_callback();
            cancel_callback = {};
        }
    }


    void Terminal::istream_listener() {
        pthread_setname_np(pthread_self(), "fmergestdin");
        // Ignore SIGINT (only used to stop the blocking read)
        register_trivial_sigint();

        std::string user_string;
        while(true) {
            char user_char;
            int ret = read(STDIN_FILENO, &user_char, 1);
            if(ret > 0) {
                if(user_char == '\n') {
                    // Forward to callback function
                    istream_callback_lock.lock();
                    if(istream_callback) {
                        // True indicates the callback should be disabled after this
                        if(istream_callback(user_string)) {
                            istream_callback = {};
                            cancel_callback = {};
                        }
                    }
                    istream_callback_lock.unlock();
                    user_string.clear();
                } else {
                    user_string.push_back(user_char);
                }
            } else if(ret == -1) {
                if(errno == EINTR) return;
                
                print_clib_error("read");
            } else if(ret == 0) {
                LOG("[Warning] Unexpected EOF reached for stdin");
                return;
            }
        }
    }


    void Terminal::print(string printable) {
        if(printable.empty()) {
            return;
        }
        bool contains_trailing_nl = printable.back() == '\n';
        bool contains_nl = printable.find('\n') != string::npos;

        if(!last_line.empty()) {
            // Start printing from the previous line, so that we append to it
            cursor_to_last_line();
        }
        // Print and clear any remanents that may be left from the footer
        for(char c : printable) {
            if(c == '\n') os << "\033[K";
            os << c;
        }
        // Delete the last line buffer if the last line contained a newline
        if(contains_nl) {
            last_line.clear();
        }

        if(!contains_trailing_nl) {
            os << std::endl;
            // Put the partial line into the last_line buffer to continue with next time.
            last_line += printable.substr(printable.find_last_of('\n') + 1);
        }
        // Redraw the footer, if applicable
        if(!persistent_footer.empty()) {
            os << persistent_footer << '\r';
        }

        os << std::flush;
    }


    int Terminal::sync() {
        // Put the internal buffer onto the terminal
        // str() is inherited from stringbuf
        print(this->str());
        this->str("");
        return 0;
    }


    void Terminal::cursor_to_last_line() {
        if(last_line.length() > 0) {
            // Only valid for line lengths greater than 1 character
            int last_line_rows = (last_line.length() - 1) / terminal_width + 1;
            os << "\033[" << last_line_rows << "F";
        }
    }
}