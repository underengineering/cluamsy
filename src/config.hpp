#include <optional>
#include <string>
#include <unordered_map>

struct Config {
    std::string filter;
};

extern std::optional<std::unordered_map<std::string, Config>> parse_config();
