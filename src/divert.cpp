#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <memory.h>
#include <thread>
#include <windivert.h>

#include "SDL_events.h"
#include "common.hpp"
#include "dense_buffers.hpp"
#include "divert.hpp"
#include "events.hpp"
#include "module.hpp"
#include "packet.hpp"

#include "bandwidth.hpp"
#include "drop.hpp"
#include "duplicate.hpp"
#include "lag.hpp"
#include "throttle.hpp"

static constexpr INT16 DIVERT_PRIORITY = 0;
static constexpr UINT64 QUEUE_LEN = 2 << 10;

WinDivert::WinDivert() {
    m_modules.emplace_back(std::make_shared<LagModule>());
    m_modules.emplace_back(std::make_shared<DropModule>());
    m_modules.emplace_back(std::make_shared<ThrottleModule>());
    m_modules.emplace_back(std::make_shared<BandwidthModule>());
    m_modules.emplace_back(std::make_shared<DuplicateModule>());
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
                 "Make sure you run cluamsy as Administrator.",
                 last_error);

        m_divert_handle = nullptr;
        return error;
    }

    WinDivertSetParam(m_divert_handle, WINDIVERT_PARAM_QUEUE_LENGTH,
                      QUEUE_LENGTH);
    WinDivertSetParam(m_divert_handle, WINDIVERT_PARAM_QUEUE_TIME, QUEUE_TIME);
    LOG("WinDivert internal queue length: %llu, queue time: %llu", QUEUE_LEN,
        QUEUE_TIME);

    // Initialize modules
    for (auto& module : m_modules) {
        if (module->m_enabled)
            module->enable();

        module->m_was_enabled = module->m_enabled;
    }

    LOG("Starting threads");

    m_stop_event_handle = CreateEvent(nullptr, false, false, nullptr);
    ThreadData thread_data = {
        .divert_handle = m_divert_handle,
        .stop_event_handle = m_stop_event_handle,
        .modules = m_modules,
    };

    m_thread = std::thread(thread, thread_data);

    return std::nullopt;
}

bool WinDivert::stop() {
    if (!m_divert_handle)
        return false;

    LOG("Stopping");

    SetEvent(m_stop_event_handle);

    LOG("Waiting for the windivert thread");
    m_thread.join();

    const auto success = WinDivertClose(m_divert_handle);
    assert(success);

    m_divert_handle = nullptr;

    CloseHandle(m_stop_event_handle);
    m_stop_event_handle = nullptr;

    // Write thread should've sent all packets already
    assert(g_packets.empty());

    // Run post-disable module cleanups
    for (auto& module : m_modules) {
        if (module->m_enabled && module->m_was_enabled)
            module->disable();
    }

    LOG("WinDivert stopped");

    return true;
}

struct PendingWrite {
    // Strong reference to the buffer being send
    std::optional<DenseBufferArray> buffer;
    std::array<WINDIVERT_ADDRESS, WinDivert::MAX_PACKETS> addresses;
};

void WinDivert::thread(ThreadData thread_data) {
    auto read_event_handle = CreateEvent(nullptr, false, false, nullptr);
    auto write_event_handle = CreateEvent(nullptr, false, false, nullptr);

    OVERLAPPED read_overlap{};
    read_overlap.hEvent = read_event_handle;

    OVERLAPPED write_overlap{};
    write_overlap.hEvent = write_event_handle;

    std::optional<PendingWrite> pending_write;

    // Start reading
    auto dense_buffer = std::make_shared<std::vector<char>>(BUFFER_SIZE);
    std::array<WINDIVERT_ADDRESS, MAX_PACKETS> read_addresses{};
    UINT read_addresses_length = sizeof(read_addresses);
    WinDivertRecvEx(thread_data.divert_handle, dense_buffer->data(),
                    dense_buffer->size(), nullptr, 0, read_addresses.data(),
                    &read_addresses_length, &read_overlap);

    const auto stage_write = [&] {
        pending_write = PendingWrite{
            .buffer = std::nullopt,
            .addresses = {},
        };

        // Find biggest contiguous dense buffer slice
        auto it = g_packets.cbegin();
        const auto& first_slice = it->packet;
        auto contiguous_slice_offset = first_slice.offset();
        size_t write_packet_count = 0;
        for (; it != g_packets.cend(); ++it, write_packet_count++) {
            const auto& slice = it->packet;

            // Check for buffer equality
            if (slice != first_slice)
                break;

            // Check if there is an hole
            if (contiguous_slice_offset != slice.offset())
                break;

            pending_write->addresses[write_packet_count] = it->addr;
            contiguous_slice_offset += slice.size();
        }

        // Write
        pending_write->buffer = DenseBufferArray(first_slice.buffer());

        const auto* buffer_data =
            first_slice.buffer()->data() + first_slice.offset();
        const auto buffer_size = contiguous_slice_offset - first_slice.offset();
        // LOG("Writing %zu/%zu packets at once, slice_offset=%zu "
        //     "buffer_offset=%zu size=%zu",
        //     write_packet_count, g_packets.size(), first_slice.offset(),
        //     contiguous_slice_offset, buffer_size);
        WinDivertSendEx(thread_data.divert_handle, buffer_data, buffer_size,
                        nullptr, 0, pending_write->addresses.data(),
                        write_packet_count * sizeof(WINDIVERT_ADDRESS),
                        &write_overlap);
        g_packets.erase(g_packets.cbegin(), it);
    };

    std::optional<std::chrono::milliseconds> wait_timeout;
    const std::array<HANDLE, 3> events{
        write_event_handle,
        read_event_handle,
        thread_data.stop_event_handle,
    };
    auto should_stop = false;
    while (true) {
        const auto res = WaitForMultipleObjects(
            events.size(), events.data(), false,
            wait_timeout ? wait_timeout->count() : INFINITE);
        wait_timeout = std::nullopt;

        auto stop = false;
        switch (res) {
        // READ
        case WAIT_OBJECT_0 + 1: {
            DWORD read = 0;
            if (!GetOverlappedResult(thread_data.divert_handle, &read_overlap,
                                     &read, true)) {
                const auto error = GetLastError();
                if (error == ERROR_INVALID_HANDLE ||
                    error == ERROR_OPERATION_ABORTED) {
                    LOG("Overlapped read failed: invalid windivert handle");
                    stop = true;
                    break;
                }

                LOG("Overlapped read failed: %lu", error);
                continue;
            }

            // Convert to dense buffer array
            dense_buffer->resize(read);

            DenseBufferArray dense_buffers(std::move(dense_buffer));
            dense_buffer = std::make_shared<std::vector<char>>(
                BUFFER_SIZE); // Allocate a new buffer
            // LOG("new packets buf size %zu", BUFFER_SIZE);

            const auto* const data = dense_buffers.buffer()->data();
            const auto packet_count =
                read_addresses_length / sizeof(WINDIVERT_ADDRESS);
            size_t offset = 0;
            const auto current_timestamp = std::chrono::steady_clock::now();
            for (size_t i = 0; i < packet_count; i++) {
                const auto* const iphdr =
                    std::bit_cast<PWINDIVERT_IPHDR>(data + offset);

                uint16_t packet_size = 0;
                if (iphdr->Version == 4) {
                    packet_size = ntohs(iphdr->Length);
                    // LOG("pkt id %i ttl %i size %i", iphdr->Id, iphdr->TTL,
                    //     packet_size);
                } else {
                    const auto* const ipv6hdr =
                        std::bit_cast<PWINDIVERT_IPV6HDR>(data + offset);
                    packet_size =
                        htons(ipv6hdr->Length) + sizeof(WINDIVERT_IPV6HDR);
                    // LOG("pkt6 id %i ttl %i size %d", ipv6hdr->FlowLabel0,
                    //     ipv6hdr->HopLimit, packet_size);
                }

                g_packets.emplace_back(PacketNode{
                    .packet = dense_buffers.slice(offset, packet_size),
                    .addr = read_addresses[i],
                    .captured_at = current_timestamp,
                });

                offset += packet_size;
            }

            // Read again
            read_addresses_length = sizeof(read_addresses);
            WinDivertRecvEx(thread_data.divert_handle, dense_buffer->data(),
                            dense_buffer->size(), nullptr, 0,
                            read_addresses.data(), &read_addresses_length,
                            &read_overlap);

            [[fallthrough]];
        }
        case WAIT_TIMEOUT: {
            // Run modules
            auto dirty = false;
            for (const auto& module : thread_data.modules) {
                if (module->m_enabled) {
                    // Initialize it if it wasn't
                    if (!module->m_was_enabled) {
                        module->enable();
                        module->m_was_enabled = true;

                        dirty = true;
                    }

                    const auto result = module->process();
                    if (result.schedule_after && wait_timeout) {
                        wait_timeout = std::chrono::milliseconds(
                            std::min(wait_timeout->count(),
                                     result.schedule_after->count()));
                    } else if (result.schedule_after) {
                        wait_timeout = result.schedule_after;
                    }

                    dirty |= result.dirty;
                } else if (module->m_was_enabled) {
                    module->disable();
                    module->m_was_enabled = false;

                    dirty = true;
                }
            }

            // Notify main thread to redraw
            if (dirty) {
                SDL_Event event{events::REDRAW};
                SDL_PushEvent(&event);
            }

            if (!pending_write && !g_packets.empty())
                stage_write();

            break;
        }
        // WRITE
        case WAIT_OBJECT_0: {
            DWORD write = 0;
            if (!GetOverlappedResult(thread_data.divert_handle, &write_overlap,
                                     &write, true)) {
                const auto error = GetLastError();
                if (error == ERROR_INVALID_HANDLE ||
                    error == ERROR_OPERATION_ABORTED) {
                    LOG("Overlapped read failed: invalid windivert handle");
                    stop = true;
                    break;
                }

                LOG("Overlapped write failed: %lu", error);
                break;
            }

            // LOG("Wrote %lu bytes, pending %zu", write, g_packets.size());

            if (g_packets.empty() && should_stop) {
                LOG("Stopping");
                stop = true;
                break;
            }

            pending_write = std::nullopt;
            if (!g_packets.empty())
                stage_write();

            break;
        }
        // STOP
        case WAIT_OBJECT_0 + 2:
            if (pending_write)
                should_stop = true;
            else
                stop = true;

            break;
        case WAIT_FAILED: {
            LOG("WaitForMultipleObjects failed: %lu", GetLastError());
            stop = true;
            break;
        }
        }

        if (stop)
            break;
    }

    CloseHandle(read_event_handle);
    CloseHandle(write_event_handle);
}
