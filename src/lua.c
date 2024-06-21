#include "common.h"
#include "lauxlib.h"
#include "lualib.h"

lua_State *L = NULL;

void lua_state_init(void) {
    LOG("Creating lua state");

    L = luaL_newstate();
    luaL_openlibs(L);

    int status = luaL_loadfile(L, "main.lua");
    LOG("luaL_loadfile: %d\n", status);
}

void lua_state_close(void) {
    LOG("Closing lua state");

    lua_close(L);
}
