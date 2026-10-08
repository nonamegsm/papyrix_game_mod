#pragma once

#include <cstdint>

#include "../core/Types.h"

namespace papyrix::games_app::solitaire_view {

void reset(uint32_t seed);
bool handleButton(Button button);
bool tap(int x, int y);
void renderBoard();
bool won();
bool undo();

}  // namespace papyrix::games_app::solitaire_view
