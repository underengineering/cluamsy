#pragma once

#include <lua.hpp>

extern int lua_getset(lua_State* L, bool& value, int idx);
extern int lua_getset(lua_State* L, float& value, int idx);
extern int lua_getset(lua_State* L, int& value, int idx);
