#include "module.hpp"

#include "drop.hpp"
#include "throttle.hpp"

std::array<std::shared_ptr<Module>, 2> g_modules = {
    std::make_shared<DropModule>(), std::make_shared<ThrottleModule>()};
