#pragma once

#include <array>
#include <ostream>
#include <iomanip>
#include <mutex>
#include <condition_variable>


namespace fmerge {
    
    std::string to_string(std::array<unsigned char, 16> uuid);


    class SyncBarrier {
    public:
        SyncBarrier() : timeout(0) {}
        SyncBarrier(int _timeout) : timeout(_timeout) {}

        // Waits until notified (either before or after we start waiting).
        // Returns true if it timed out.
        inline bool wait() {
            std::unique_lock lk(mtx);
            if(timeout <= 0) {
                cv.wait(lk, [this]{ return proceed; });
            } else {
                auto cond = cv.wait_for(lk, std::chrono::seconds(timeout), [this]{ return proceed; });
                if(cond == false) return true;
            }
            return false;
        }

        inline void notify() {
            std::lock_guard lk(mtx);
            proceed = true;
            cv.notify_all();
        }
    private:
        bool proceed;
        std::mutex mtx;
        std::condition_variable cv;
        int timeout;
    };
}