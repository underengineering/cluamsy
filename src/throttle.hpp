#pragma once

#include <chrono>
#include <list>

#include "module.hpp"
#include "packet.hpp"

using namespace std::chrono_literals;

class ThrottleModule : public Module {
private:
    // Threshold for how many packet to throttle at most
    static inline size_t MAX_PACKETS = 1024;

public:
    ThrottleModule() {
        m_display_name = "Throttle";
        m_short_name = "throttle";
    }

    virtual bool draw();

    virtual void enable();
    virtual void disable();

    virtual std::optional<std::chrono::milliseconds> process();

private:
    void flush();

private:
    bool m_inbound = true;
    bool m_outbound = true;
    float m_chance = 10.f;
    std::chrono::milliseconds m_timeframe_ms = 200ms;
    bool m_drop_throttled = false;

    bool m_throttling = false;
    std::chrono::steady_clock::time_point m_start_point;
    std::list<PacketNode> m_throttle_list;
};
