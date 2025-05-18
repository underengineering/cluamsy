#pragma once

#include <SDL.h>
#include <cassert>

namespace events {

inline const auto REDRAW = SDL_RegisterEvents(1);

inline void queue_redraw() {
    SDL_Event event{events::REDRAW};
    assert(SDL_PushEvent(&event) > 0);
}

} // namespace events
