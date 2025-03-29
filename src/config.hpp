#include <optional>
#include <string>
#include <toml.hpp>
#include <unordered_map>

extern std::optional<std::unordered_map<std::string, toml::table>>
parse_config();
