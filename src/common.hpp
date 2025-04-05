#pragma once

#include <cstdlib>
#include <limits>
#include <lua.hpp>
#include <windivert.h>

#ifndef NDEBUG
#define LOG(fmt, ...) fprintf(stderr, __FUNCTION__ ": " fmt "\n", ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

inline bool check_chance(float chance) {
    return static_cast<float>(rand()) / RAND_MAX * 100.f < chance;
}

// inline helper for inbound outbound check
inline bool check_direction(bool outbound_packet, bool handle_inbound,
                            bool handle_outbound) {
    return (handle_inbound && !outbound_packet) ||
           (handle_outbound && outbound_packet);
}

constexpr inline bool
almost_equal(float a, float b,
             float epsilon = std::numeric_limits<float>::epsilon()) {
    return std::abs(a - b) < epsilon;
}

// elevate
BOOL IsElevated();
BOOL IsRunAsAdmin();
BOOL tryElevate(HWND hWnd, BOOL silent);
