#define TOML_IMPLEMENTATION
#include "config.hpp"

#include "common.hpp"

extern std::optional<std::unordered_map<std::string, toml::table>>
parse_config(const std::filesystem::path& path) {
    LOG("Loading config file");

    std::ifstream file(path);
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

        configs.emplace(key, *table);
    }

    return configs;
}
