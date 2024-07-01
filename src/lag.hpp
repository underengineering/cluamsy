#pragma once

#include "module.hpp"
#include "packet.hpp"

using namespace std::chrono_literals;

class LagModule : public Module {
private:
    // Threshold for how many packet to throttle at most
    static inline size_t MAX_PACKETS = 1024;

public:
    LagModule() {
        m_display_name = "Lag";
        m_short_name = "lag";
    }

    virtual bool draw();

    virtual void enable();
    virtual void disable();

    virtual std::optional<std::chrono::milliseconds> process();

private:
    bool m_inbound = true;
    bool m_outbound = true;
    float m_chance = 10.f;

    std::chrono::milliseconds m_lag_time = 200ms;
    std::list<PacketNode> m_lagged_packets;
};
