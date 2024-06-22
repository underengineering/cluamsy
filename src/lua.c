#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include <stdint.h>

#include "common.h"

lua_State *L = NULL;

static char HOOK_KEY = 0;
static HANDLE g_hookHandle = NULL;

static void lua_pushnumber64(lua_State *L, uint64_t value) {
    lua_createtable(L, 0, 2);
    lua_pushnumber(L, (lua_Number)(value >> 32));
    lua_setfield(L, -2, "hi");
    lua_pushnumber(L, (lua_Number)(value & 0xFFFFFFFF));
    lua_setfield(L, -2, "lo");
}

static uint64_t lua_tonumber64(lua_State *L, int idx) {
    int type;
    switch ((type = lua_type(L, idx))) {
    case LUA_TSTRING:
    case LUA_TNUMBER:
        return (uint64_t)lua_tonumber(L, idx);
    case LUA_TTABLE:
        lua_insert(L, idx);

        lua_getfield(L, -1, "hi");
        uint32_t hi = (uint32_t)lua_tonumber(L, -1);

        lua_getfield(L, -2, "lo");
        uint32_t lo = (uint32_t)lua_tonumber(L, -1);

        lua_pop(L, 2); // Pop hi & lo

        return ((uint64_t)hi << 32) | lo;
    default:
        char message[256];
        int message_length =
            snprintf(message, sizeof(message),
                     "Invalid argument #1 to lua_tonumber64: '%s'",
                     lua_typename(L, type));
        lua_pushlstring(L, message, message_length);
        lua_error(L);
        break;
    }

    return 0;
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM w_param,
                                      LPARAM l_param) {

    lua_pushlightuserdata(L, &HOOK_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);

    lua_pushnumber(L, code);
    lua_pushnumber64(L, w_param);
    lua_pushnumber64(L, l_param);
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG("lua_pcall: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return CallNextHookEx(g_hookHandle, code, w_param, l_param);
    }

    LRESULT result = lua_tonumber64(L, -1);
    lua_pop(L, 1);

    return result;
}

static int lua_lib_register_keyboard_hook(lua_State *L) {
    luaL_checktype(L, -1, LUA_TFUNCTION);
    if (g_hookHandle != NULL) {
        lua_pushliteral(L,
                        "Cannot register more than 1 low level keyboard hook");
        lua_error(L);
        return 0;
    }

    // Save function to the register
    lua_pushlightuserdata(L, &HOOK_KEY);
    lua_insert(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);

    g_hookHandle =
        SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)LowLevelKeyboardProc, 0, 0);
    return 0;
}

static int lua_lib_call_next_hook(lua_State *L) {
    int code = (int)lua_tonumber(L, -1);
    WPARAM w_param = lua_tonumber64(L, -2);
    LPARAM l_param = lua_tonumber64(L, -3);

    LRESULT result = CallNextHookEx(g_hookHandle, code, w_param, l_param);
    lua_pushnumber64(L, result);

    return 1;
}

static void lua_state_push_libs(void) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_lib_register_keyboard_hook);
    lua_setfield(L, -2, "register_keyboard_hook");

    lua_pushcfunction(L, lua_lib_call_next_hook);
    lua_setfield(L, -2, "call_next_hook");

    lua_setglobal(L, "cluamsy");
}

void lua_state_init(void) {
    LOG("Creating lua state");

    L = luaL_newstate();
    luaL_openlibs(L);
    lua_state_push_libs();

    int status = luaL_loadfile(L, "main.lua");
    LOG("luaL_loadfile: %d", status);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        LOG("lua_pcall fail: %s", lua_tostring(L, -1));
}

void lua_state_close(void) {
    LOG("Closing lua state");

    lua_close(L);

    if (g_hookHandle != NULL)
        UnhookWindowsHookEx(g_hookHandle);
}
