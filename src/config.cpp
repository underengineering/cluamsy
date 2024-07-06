#include "config.hpp"

#include "common.hpp"
#include <fstream>
#include <string>
#include <unordered_map>

extern std::optional<std::unordered_map<std::string, Config>> parse_config() {
    LOG("Loading config file");

    std::ifstream file("config.ini");
    if (!file.is_open()) {
        LOG("Opening failed");
        return std::nullopt;
    }

    std::unordered_map<std::string, Config> entries;
    std::optional<std::string> current_entry_name;

    std::string line;
    while (std::getline(file, line)) {
        const auto line_start_idx = line.find_first_not_of(' ');
        const auto line_end_idx = line.find_last_not_of(' ');
        if (line_end_idx == std::string::npos)
            continue;

        const auto line_trimmed =
            line.substr(line_start_idx, line_end_idx - line_start_idx + 1);
        if (line_trimmed.empty())
            continue;

        const auto is_entry =
            line_trimmed.starts_with('[') && line_trimmed.ends_with(']');
        if (!current_entry_name && !is_entry) {
            LOG("Expected entry name at line '%s'", line_trimmed.c_str());
            return std::nullopt;
        } else if (is_entry) {
            current_entry_name =
                line_trimmed.substr(1, line_trimmed.size() - 2);
            entries.emplace(*current_entry_name, "");
        } else {
            const auto key_end_idx = line_trimmed.find_first_of(" =");
            const auto key = line_trimmed.substr(0, key_end_idx);

            const auto value_start_idx =
                line_trimmed.find_first_not_of(' ', key_end_idx + 2);
            const auto value = line_trimmed.substr(value_start_idx);

            // TODO:
            if (key != "Filter") {
                LOG("Unexpected key '%s'", key.c_str());
                return std::nullopt;
            }

            LOG("Entry '%s': '%s' = '%s'", current_entry_name->c_str(),
                key.c_str(), value.c_str());

            entries[*current_entry_name].filter = value;
        }
    }

    return entries;
}
