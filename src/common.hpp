#pragma once

#include <lua.hpp>
#include <windivert.h>

#ifdef _DEBUG
#define LOG(fmt, ...) fprintf(stderr, __FUNCTION__ ": " fmt "\n", ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

// lua
extern lua_State* g_luaState;
extern void lua_state_init(void);
extern void lua_state_close(void);

inline bool check_chance(float chance) {
    return static_cast<float>(rand()) / RAND_MAX * 100.f < chance;
}

// inline helper for inbound outbound check
inline bool check_direction(bool outbound_packet, bool handle_inbound,
                            bool handle_outbound) {
    return (handle_inbound && !outbound_packet) ||
           (handle_outbound && outbound_packet);
}

// elevate
BOOL IsElevated();
BOOL IsRunAsAdmin();
BOOL tryElevate(HWND hWnd, BOOL silent);
