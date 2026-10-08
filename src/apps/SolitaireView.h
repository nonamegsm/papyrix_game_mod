#pragma once

#include <cstdint>

#include "../core/Types.h"

namespace papyrix::games {
class Solitaire;
}

namespace papyrix::games_app::solitaire_view {

void reset(uint32_t seed);
bool handleButton(Button button);
bool tap(int x, int y);
void renderBoard();
bool won();
bool undo();
#ifdef TEST_BUILD
void loadForTest(const games::Solitaire& game);
#endif

}  // namespace papyrix::games_app::solitaire_view
