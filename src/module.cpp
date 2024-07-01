#include "module.hpp"

#include "drop.hpp"
#include "lag.hpp"
#include "throttle.hpp"

std::array<std::shared_ptr<Module>, 3> g_modules = {
    std::make_shared<LagModule>(),
    std::make_shared<DropModule>(),
    std::make_shared<ThrottleModule>(),
};
