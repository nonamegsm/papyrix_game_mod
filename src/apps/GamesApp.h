#pragma once

#include "MiniApp.h"

#ifdef TEST_BUILD
namespace papyrix::games {
class Game2048;
}
#endif

namespace papyrix::games_app {

void enter(Core& core);
bool update(Core& core);
bool render(Core& core);
void exit(Core& core);
bool handleEvent(Core& core, const Event& event);

#ifdef TEST_BUILD
void load2048ForTest(const games::Game2048& board);
#endif

}  // namespace papyrix::games_app
