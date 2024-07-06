#pragma once

#include <chrono>
#include <list>
#include <vector>

#include <windivert.h>

struct PacketNode {
    std::vector<char> packet;
    WINDIVERT_ADDRESS addr;
    std::chrono::steady_clock::time_point captured_at;
};

extern std::list<PacketNode> g_packets;
