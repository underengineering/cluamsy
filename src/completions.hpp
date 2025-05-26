#pragma once

#include <optional>
#include <string_view>
#include <vector>

using Completion = std::string_view;

std::optional<std::vector<Completion>>
get_filter_completions(std::string_view filter);
