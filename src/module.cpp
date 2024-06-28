#include "module.hpp"

#include "drop.hpp"

std::array<std::shared_ptr<Module>, 1> g_modules = {
    std::make_shared<DropModule>()};
