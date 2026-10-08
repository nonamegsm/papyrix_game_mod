#pragma once

#include "MiniApp.h"

namespace papyrix::barcode_app {
void enter(Core& core);
bool update(Core& core);
bool render(Core& core);
void exit(Core& core);
bool handleEvent(Core& core, const Event& event);
}  // namespace papyrix::barcode_app
