#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <optional>
#include <thread>

#include "module.hpp"

class WinDivert {
public:
    static const inline size_t QUEUE_LENGTH = 4096;
    static const inline size_t QUEUE_TIME = 100;
    static const inline size_t BUFFER_SIZE = 0xffff;
    static const inline size_t MAX_PACKETS = 32;

public:
    WinDivert();

    WinDivert(const WinDivert&) = delete;
    WinDivert(WinDivert&&) = delete;
    WinDivert& operator=(const WinDivert&) = delete;
    WinDivert& operator=(WinDivert&&) = delete;

    ~WinDivert() { stop(); };

    std::optional<std::string> start(const std::string& filter);
    bool stop();

    [[nodiscard]] const std::vector<std::shared_ptr<Module>>& modules() const {
        return m_modules;
    }

private:
    struct ThreadData {
        HANDLE divert_handle;
        HANDLE stop_event_handle;
        const std::vector<std::shared_ptr<Module>>& modules;
    };

    static void thread(ThreadData thread_data);

private:
    std::vector<std::shared_ptr<Module>> m_modules;

    std::thread m_thread;
    HANDLE m_divert_handle = nullptr;
    HANDLE m_stop_event_handle = nullptr;
};
