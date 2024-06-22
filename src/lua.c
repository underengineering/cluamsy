#include <stdbool.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "common.h"

lua_State *L = NULL;

static char HOOK_KEY = 0;
static HANDLE g_hookHandle = NULL;

void lua_pushkbdll(lua_State *L, KBDLLHOOKSTRUCT *value) {
    lua_createtable(L, 0, 3);

    lua_pushnumber(L, value->vkCode);
    lua_setfield(L, -2, "vk_code");

    lua_pushnumber(L, value->scanCode);
    lua_setfield(L, -2, "scan_code");

    lua_pushnumber(L, value->flags);
    lua_setfield(L, -2, "flags");
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM w_param,
                                      LPARAM l_param) {

    lua_pushlightuserdata(L, &HOOK_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);

    lua_pushnumber(L, code);
    lua_pushnumber(L, (lua_Number)w_param);
    lua_pushkbdll(L, (KBDLLHOOKSTRUCT *)l_param);
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG("lua_pcall: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return CallNextHookEx(g_hookHandle, code, w_param, l_param);
    }

    bool call_next = lua_toboolean(L, -1);
    lua_pop(L, 1);

    return call_next ? CallNextHookEx(g_hookHandle, code, w_param, l_param)
                     : -1;
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

static void lua_state_push_modules(void) {
    LOG("stack before %i", lua_gettop(L));
    lua_newtable(L);
    for (int idx = 0; idx < MODULE_CNT; idx++) {
        Module *mod = modules[idx];
        LOG("Pushing module '%s'", mod->shortName);
        LOG("stack %i", lua_gettop(L));

        lua_newtable(L);
        mod->push_lua_functions(L);
        lua_setfield(L, -2, mod->shortName);
    }

    lua_setfield(L, -2, "modules");
    LOG("stack after %i", lua_gettop(L));
}

static void lua_state_push_libs(void) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_lib_register_keyboard_hook);
    lua_setfield(L, -2, "register_keyboard_hook");

    lua_state_push_modules();

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
