#pragma once

#include <list>
#include <vector>

#include <windivert.h>

struct PacketNode {
    std::vector<char> packet;
    WINDIVERT_ADDRESS addr;
    DWORD timestamp; // ! timestamp isn't filled when creating node since it's
                     // only needed for lag
};

extern std::list<PacketNode> g_packets;
