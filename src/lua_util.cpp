#include <lua.hpp>

int lua_getset(lua_State* L, bool& value, int idx) {
    int type;
    switch ((type = lua_type(L, idx))) {
    case LUA_TNONE:
    case LUA_TNIL:
        lua_pushboolean(L, value);
        return 1;
    case LUA_TBOOLEAN:
        value = lua_toboolean(L, idx);
        return 0;
    default:
        luaL_error(L, "Expected 'bool', got type '%s'", lua_typename(L, type));
        return 0;
    }
}

int lua_getset(lua_State* L, float& value, int idx) {
    int type;
    switch ((type = lua_type(L, idx))) {
    case LUA_TNONE:
    case LUA_TNIL:
        lua_pushnumber(L, value);
        return 1;
    case LUA_TNUMBER:
        value = static_cast<float>(lua_tonumber(L, idx));
        return 0;
    default:
        luaL_error(L, "Expected 'number', got type '%s'",
                   lua_typename(L, type));
        return 0;
    }
}

int lua_getset(lua_State* L, int& value, int idx) {
    int type;
    switch ((type = lua_type(L, idx))) {
    case LUA_TNONE:
    case LUA_TNIL:
        lua_pushnumber(L, static_cast<lua_Number>(value));
        return 1;
    case LUA_TNUMBER:
        value = static_cast<int>(lua_tonumber(L, idx));
        return 0;
    default:
        luaL_error(L, "Expected 'number', got type '%s'",
                   lua_typename(L, type));
        return 0;
    }
}
