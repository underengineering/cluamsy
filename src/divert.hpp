#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mutex>
#include <optional>
#include <thread>

#include "module.hpp"

class WinDivert {
public:
    WinDivert();
    ~WinDivert() { stop(); };

    std::optional<std::string> start(const std::string& filter);
    bool stop();

    const std::vector<std::shared_ptr<Module>>& modules() const {
        return m_modules;
    }

private:
    struct ThreadData {
        HANDLE divert_handle;
        const std::vector<std::shared_ptr<Module>>& modules;

        std::mutex& packets_mutex;
        std::condition_variable& packets_condvar;
        bool& stop;
    };

    static void read_thread(ThreadData thread_data);
    static void write_thread(ThreadData thread_data);

private:
    std::vector<std::shared_ptr<Module>> m_modules;

    bool m_stop = false;
    std::mutex m_packets_mutex;
    // Notified when packets are read in `read_thread`
    std::condition_variable m_packets_condvar;
    // Notified when threads need to stop
    std::condition_variable m_stop_condvar;

    bool m_started = false;
    std::thread m_read_thread;
    std::thread m_write_thread;
    HANDLE m_divert_handle = nullptr;
};

int divertStart(const char* filter, char buf[]);
