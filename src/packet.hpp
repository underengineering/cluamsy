#pragma once

#include <chrono>
#include <windivert.h>

#include "dense_buffers.hpp"

struct PacketNode {
    DenseBufferArraySlice packet;
    WINDIVERT_ADDRESS addr;
    std::chrono::steady_clock::time_point captured_at;
};
