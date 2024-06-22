#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iup.h>
#include <stdbool.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "common.h"

lua_State *g_luaState = NULL;

static char HOOK_KEY = 0;
static HANDLE g_hookHandle = NULL;

static void lua_pushkbdll(lua_State *L, KBDLLHOOKSTRUCT *value) {
    lua_createtable(L, 0, 3);

    lua_pushnumber(L, value->vkCode);
    lua_setfield(L, -2, "vk_code");

    lua_pushnumber(L, value->scanCode);
    lua_setfield(L, -2, "scan_code");

    lua_pushnumber(L, value->flags);
    lua_setfield(L, -2, "flags");
}

static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM w_param,
                                             LPARAM l_param) {

    lua_pushlightuserdata(g_luaState, &HOOK_KEY);
    lua_gettable(g_luaState, LUA_REGISTRYINDEX);

    lua_pushnumber(g_luaState, code);
    lua_pushnumber(g_luaState, (lua_Number)w_param);
    lua_pushkbdll(g_luaState, (KBDLLHOOKSTRUCT *)l_param);
    if (lua_pcall(g_luaState, 3, 1, 0) != LUA_OK) {
        LOG("lua_pcall: %s", lua_tostring(g_luaState, -1));
        lua_pop(g_luaState, 1);
        return CallNextHookEx(g_hookHandle, code, w_param, l_param);
    }

    bool call_next = lua_toboolean(g_luaState, -1);
    lua_pop(g_luaState, 1);

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

static int timer_callback(Ihandle *handle) {
    // Get callback from registry
    lua_pushlightuserdata(g_luaState, handle);
    lua_gettable(g_luaState, LUA_REGISTRYINDEX);

    if (lua_pcall(g_luaState, 0, 0, 0) != LUA_OK) {
        LOG("timer_callback: %s", lua_tostring(g_luaState, -1));
        lua_pop(g_luaState, 1);
    }

    return 0;
}

static int lua_lib_timer_run(lua_State *L) {
    luaL_checktype(L, 1, LUA_TUSERDATA);

    bool run = lua_toboolean(L, 2);

    Ihandle *handle = *(Ihandle **)lua_touserdata(L, 1);
    IupSetAttribute(handle, "RUN", run ? "YES" : "NO");

    return 1;
}

static int lua_lib_timer_set_time(lua_State *L) {
    luaL_checktype(L, 1, LUA_TUSERDATA);

    const char *time = luaL_checkstring(L, 2);
    Ihandle *handle = *(Ihandle **)lua_touserdata(L, 1);
    IupSetAttribute(handle, "TIME", time);

    return 1;
}

static int lua_lib_timer_set_callback(lua_State *L) {
    luaL_checktype(L, 1, LUA_TUSERDATA);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    Ihandle *handle = *(Ihandle **)lua_touserdata(L, 1);
    IupSetCallback(handle, "ACTION_CB", timer_callback);

    // Place callback in the registry
    lua_pushlightuserdata(L, handle);
    lua_pushvalue(L, 2);
    lua_settable(L, LUA_REGISTRYINDEX);

    return 1;
}

static int lua_lib_timer_destroy(lua_State *L) {
    luaL_checktype(L, -1, LUA_TUSERDATA);

    Ihandle *handle = *(Ihandle **)lua_touserdata(L, 1);
    IupDestroy(handle);

    // Remove callback from registry
    lua_pushlightuserdata(L, handle);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);

    return 1;
}

static int lua_lib_create_timer(lua_State *L) {
    luaL_checknumber(L, -1);

    Ihandle *timer = IupTimer();

    // Create timer userdata
    void *ud = lua_newuserdata(L, sizeof(void *));
    memcpy(ud, &timer, sizeof(void *));

    luaL_setmetatable(L, "Timer");

    return 1;
}

static void lua_state_push_modules(void) {
    lua_newtable(g_luaState);
    for (int idx = 0; idx < MODULE_CNT; idx++) {
        Module *mod = modules[idx];
        LOG("Pushing module '%s'", mod->shortName);

        lua_newtable(g_luaState);
        mod->push_lua_functions(g_luaState);
        lua_setfield(g_luaState, -2, mod->shortName);
    }

    lua_setfield(g_luaState, -2, "modules");
}

static void lua_state_push_libs(void) {
    lua_newtable(g_luaState);

    lua_pushcfunction(g_luaState, lua_lib_register_keyboard_hook);
    lua_setfield(g_luaState, -2, "register_keyboard_hook");

    {
        // Create Timer metatable
        luaL_newmetatable(g_luaState, "Timer");

        lua_pushcfunction(g_luaState, lua_lib_timer_run);
        lua_setfield(g_luaState, -2, "run");

        lua_pushcfunction(g_luaState, lua_lib_timer_set_time);
        lua_setfield(g_luaState, -2, "set_time");

        lua_pushcfunction(g_luaState, lua_lib_timer_set_callback);
        lua_setfield(g_luaState, -2, "set_callback");

        lua_pushcfunction(g_luaState, lua_lib_timer_destroy);
        lua_setfield(g_luaState, -2, "destroy");

        lua_pushvalue(g_luaState, -1);
        lua_setfield(g_luaState, -2, "__index");

        lua_pop(g_luaState, 1);
    }
    lua_pushcfunction(g_luaState, lua_lib_create_timer);
    lua_setfield(g_luaState, -2, "create_timer");

    lua_state_push_modules();

    lua_setglobal(g_luaState, "cluamsy");
}

void lua_state_init(void) {
    LOG("Creating lua state");

    g_luaState = luaL_newstate();
    luaL_openlibs(g_luaState);
    lua_state_push_libs();

    int status = luaL_loadfile(g_luaState, "main.lua");
    LOG("luaL_loadfile: %d", status);
    if (lua_pcall(g_luaState, 0, 0, 0) != LUA_OK)
        LOG("lua_pcall fail: %s", lua_tostring(g_luaState, -1));
}

void lua_state_close(void) {
    LOG("Closing lua state");

    lua_close(g_luaState);

    if (g_hookHandle != NULL)
        UnhookWindowsHookEx(g_hookHandle);
}
