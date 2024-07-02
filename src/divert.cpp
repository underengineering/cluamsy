#include <cassert>
#include <chrono>
#include <memory.h>
#include <thread>
#include <windivert.h>

#include "common.hpp"
#include "divert.hpp"
#include "module.hpp"
#include "packet.hpp"

#include "drop.hpp"
#include "lag.hpp"
#include "throttle.hpp"

static constexpr INT16 DIVERT_PRIORITY = 0;
static constexpr UINT64 QUEUE_LEN = 2 << 10;
static constexpr UINT64 QUEUE_TIME = 2 << 9;

WinDivert::WinDivert() {
    m_modules.emplace_back(std::make_shared<LagModule>());
    m_modules.emplace_back(std::make_shared<DropModule>());
    m_modules.emplace_back(std::make_shared<ThrottleModule>());
}

std::optional<std::string> WinDivert::start(const std::string& filter) {
    LOG("Starting");

    m_divert_handle = WinDivertOpen(filter.c_str(), WINDIVERT_LAYER_NETWORK,
                                    DIVERT_PRIORITY, 0);
    if (m_divert_handle == INVALID_HANDLE_VALUE) {
        auto last_error = GetLastError();
        if (last_error == ERROR_INVALID_PARAMETER) {
            m_divert_handle = nullptr;
            return "Failed to start filtering: filter syntax error";
        }

        std::string error(512, '\0');
        snprintf(error.data(), error.capacity(),
                 "Failed to start filtering: failed to open device "
                 "(code: %lu).\n"
                 "Make sure you run clumsy as Administrator.",
                 last_error);

        m_divert_handle = nullptr;
        return error;
    }

    WinDivertSetParam(m_divert_handle, WINDIVERT_PARAM_QUEUE_LENGTH, QUEUE_LEN);
    WinDivertSetParam(m_divert_handle, WINDIVERT_PARAM_QUEUE_TIME, QUEUE_TIME);
    LOG("WinDivert internal queue Len: %llu, queue time: %llu", QUEUE_LEN,
        QUEUE_TIME);

    // Initialize modules
    for (auto& module : m_modules) {
        if (module->m_enabled)
            module->enable();

        module->m_was_enabled = module->m_enabled;
    }

    LOG("Starting threads");

    ThreadData thread_data = {
        .divert_handle = m_divert_handle,
        .modules = m_modules,

        .packets_mutex = m_packets_mutex,
        .packets_condvar = m_packets_condvar,
        .stop = m_stop,
    };

    m_read_thread = std::thread(read_thread, thread_data);
    m_write_thread = std::thread(write_thread, thread_data);

    m_started = true;
    return std::nullopt;
}

bool WinDivert::stop() {
    if (!m_divert_handle)
        return false;

    LOG("Stopping");
    const auto success = WinDivertClose(m_divert_handle);
    assert(success);

    m_divert_handle = nullptr;

    LOG("Waiting for read thread");
    m_read_thread.join();

    // Notify write thread about shutdown
    {
        m_stop = true;

        std::unique_lock lock(m_packets_mutex);
        m_packets_condvar.notify_one();
    }

    LOG("Waiting for write thread");
    m_write_thread.join();

    m_stop = false;

    // Write thread should've sent all packets already
    assert(g_packets.empty());

    // Run post-disable module cleanups
    for (auto& module : m_modules) {
        if (module->m_enabled && module->m_was_enabled)
            module->disable();
    }

    return true;
}

void WinDivert::read_thread(ThreadData thread_data) {
    while (true) {
        // Allocate a packet
        std::vector<char> packet(WINDIVERT_MTU_MAX);

        UINT read;
        WINDIVERT_ADDRESS address;
        if (!WinDivertRecv(thread_data.divert_handle, packet.data(),
                           static_cast<UINT>(packet.size()), &read, &address)) {
            auto last_error = GetLastError();
            if (last_error == ERROR_INVALID_HANDLE ||
                last_error == ERROR_OPERATION_ABORTED) {
                // treat closing handle as quit
                LOG("Handle died or operation aborted. Exit loop.");
                return;
            }

            LOG("Failed to recv a packet (%lu)", last_error);
            continue;
        }

        packet.resize(read);
        packet.shrink_to_fit();

        // Insert received packet into the packet queue
        {
            std::unique_lock lock(thread_data.packets_mutex);

            g_packets.emplace_back<PacketNode>({
                .packet = std::move(packet),
                .addr = address,
                .captured_at = std::chrono::steady_clock::now(),
            });

            // Notify write thread
            thread_data.packets_condvar.notify_one();
        }
    }
}

void WinDivert::write_thread(ThreadData thread_data) {
    std::optional<std::chrono::milliseconds> wait_timeout;
    const auto wait_predicate = [&thread_data] {
        return !g_packets.empty() || thread_data.stop;
    };

    while (true) {
        // Wait for packets
        std::unique_lock lock(thread_data.packets_mutex);
        if (wait_timeout) {
            thread_data.packets_condvar.wait_for(lock, *wait_timeout,
                                                 wait_predicate);
            wait_timeout = std::nullopt;
        } else {
            thread_data.packets_condvar.wait(lock, wait_predicate);
        }

        if (thread_data.stop) {
            LOG("Stopping");
            break;
        }

        // Run modules
        for (const auto& module : thread_data.modules) {
            if (module->m_enabled) {
                // Initialize it if it wasn't
                if (!module->m_was_enabled) {
                    module->enable();
                    module->m_was_enabled = true;
                }

                const auto schedule_after = module->process();
                if (schedule_after) {
                    if (wait_timeout) {
                        wait_timeout = std::chrono::milliseconds(std::min(
                            wait_timeout->count(), schedule_after->count()));
                    } else {
                        wait_timeout = schedule_after;
                    }
                }
            } else if (module->m_was_enabled) {
                module->disable();
                module->m_was_enabled = false;
            }
        }

        // Send all packets
        for (const auto& packet : g_packets) {
            UINT send;
            if (!WinDivertSend(thread_data.divert_handle, packet.packet.data(),
                               static_cast<UINT>(packet.packet.size()), &send,
                               &packet.addr)) {
                LOG("Failed to send a packet (%lu)", GetLastError());
            } else if (send < packet.packet.size()) {
                LOG("IT HAPPENED");
            }
        }

        g_packets.clear();
    }
}
