#define TOML_IMPLEMENTATION
#include "config.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "common.hpp"

extern std::optional<std::unordered_map<std::string, toml::table>>
parse_config() {
    LOG("Loading config file");

    std::ifstream file("cluamsy.ini");
    if (!file.is_open()) {
        LOG("Opening failed");
        return std::nullopt;
    }

    std::stringstream ss;
    ss << file.rdbuf();

    toml::table table;
    try {
        table = toml::parse(ss);
    } catch (const toml::parse_error& err) {
        LOG("Config parsing failed: %s", err.what());
        return std::nullopt;
    }

    std::unordered_map<std::string, toml::table> configs;
    for (const auto& [key, value] : table) {
        const auto* const table = value.as_table();
        if (table == nullptr) {
            LOG("Ignoring config key '%s'", key.data());
            continue;
        }

        LOG("entry '%s'", key.data());
        configs.emplace(key, *table);
    }

    return configs;
}
