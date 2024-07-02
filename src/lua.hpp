#pragma once

#include <lua.hpp>
#include <memory>
#include <string>
#include <vector>

#include "module.hpp"

class Lua {
public:
    Lua() = delete;
    Lua(Lua&& other) {
        m_lua_state = other.m_lua_state;
        other.m_lua_state = nullptr;
    }
    Lua(const std::vector<std::shared_ptr<Module>>& modules);
    ~Lua();

    lua_State* state() const { return m_lua_state; };
    bool run_file(const std::string& file_name) const;

private:
    lua_State* m_lua_state;
};
