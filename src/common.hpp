#pragma once

#include <lua.hpp>
#include <windivert.h>

#include "packet.hpp"

#define CLUMSY_VERSION "0.3"
#define MSG_BUFSIZE 512
#define FILTER_BUFSIZE 1024
#define NAME_SIZE 16
#define MODULE_CNT 8
#define ICON_UPDATE_MS 200

#define CONTROLS_HANDLE "__CONTROLS_HANDLE"
#define SYNCED_VALUE "__SYNCED_VALUE"
#define INTEGER_MAX "__INTEGER_MAX"
#define INTEGER_MIN "__INTEGER_MIN"
#define FIXED_MAX "__FIXED_MAX"
#define FIXED_MIN "__FIXED_MIN"
#define FIXED_EPSILON 0.01

// workaround stupid vs2012 runtime check.
// it would show even when seeing explicit "(short)(i);"
#define I2S(x) ((short)((x) & 0xFFFF))

#ifdef __MINGW32__
#define INLINE_FUNCTION __inline__
#else
#define INLINE_FUNCTION __inline
#endif

// my mingw seems missing some of the functions
// undef all mingw linked interlock* and use __atomic gcc builtins
#ifdef __MINGW32__
// and 16 seems to be broken
#ifdef InterlockedAnd16
#undef InterlockedAnd16
#endif
#define InterlockedAnd16(p, val)                                               \
    (__atomic_and_fetch((short*)(p), (val), __ATOMIC_SEQ_CST))

#ifdef InterlockedExchange16
#undef InterlockedExchange16
#endif
#define InterlockedExchange16(p, val)                                          \
    (__atomic_exchange_n((short*)(p), (val), __ATOMIC_SEQ_CST))

#ifdef InterlockedIncrement16
#undef InterlockedIncrement16
#endif
#define InterlockedIncrement16(p)                                              \
    (__atomic_add_fetch((short*)(p), 1, __ATOMIC_SEQ_CST))

#ifdef InterlockedDecrement16
#undef InterlockedDecrement16
#endif
#define InterlockedDecrement16(p)                                              \
    (__atomic_sub_fetch((short*)(p), 1, __ATOMIC_SEQ_CST))

#endif

#ifdef _DEBUG
#define ABORT() assert(0)
#define LOG(fmt, ...) fprintf(stderr, __FUNCTION__ ": " fmt "\n", ##__VA_ARGS__)

// check for assert
#ifndef assert
// some how vs can't trigger debugger on assert, which is really stupid
#define assert(x)                                                              \
    do {                                                                       \
        if (!(x)) {                                                            \
            DebugBreak();                                                      \
        }                                                                      \
    } while (0)
#endif

#else
#define LOG(fmt, ...)
#define ABORT()
// #define assert(x)
#endif

// lua
extern lua_State* g_luaState;
extern void lua_state_init(void);
extern void lua_state_close(void);

// status for sending packets,
#define SEND_STATUS_NONE 0
#define SEND_STATUS_SEND 1
#define SEND_STATUS_FAIL -1
extern volatile short sendState;

// Iup GUI
void showStatus(const char* line);

// WinDivert
int divertStart(const char* filter, char buf[]);
void divertStop();

// utils
// STR to convert int macro to string
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

inline bool calcChance(float chance) {
    return static_cast<float>(rand()) / RAND_MAX * 100.f < chance;
}

// inline helper for inbound outbound check
static INLINE_FUNCTION BOOL checkDirection(BOOL outboundPacket,
                                           short handleInbound,
                                           short handleOutbound) {
    return (handleInbound && !outboundPacket) ||
           (handleOutbound && outboundPacket);
}

// wraped timeBegin/EndPeriod to keep calling safe and end when exit
#define TIMER_RESOLUTION 4
void startTimePeriod();
void endTimePeriod();

// elevate
BOOL IsElevated();
BOOL IsRunAsAdmin();
BOOL tryElevate(HWND hWnd, BOOL silent);
