#pragma once

#include <algorithm>

#include "../core/Types.h"
#include "../ui/TouchLayout.h"

namespace papyrix::games_app {

// The direction pad and footer keep their existing, explicit hit targets.
// At corners, the top/bottom direction wins so one tap has one meaning.
inline Button touchMarginDirection(ui::touch::Point point, int width, int height) {
  constexpr int RESERVED_BOTTOM = 50 + 72;
  if (width < 88 || height <= RESERVED_BOTTOM + 88 || point.x < 0 || point.y < 0 || point.x >= width ||
      point.y >= height - RESERVED_BOTTOM) {
    return Button::Count;
  }
  const int horizontalBand = std::min(64, std::max(44, width / 8));
  const int verticalBand = std::min(64, std::max(44, height / 12));
  if (point.y < verticalBand) return Button::Up;
  if (point.y >= height - RESERVED_BOTTOM - verticalBand) return Button::Down;
  if (point.x < horizontalBand) return Button::Left;
  if (point.x >= width - horizontalBand) return Button::Right;
  return Button::Count;
}

}  // namespace papyrix::games_app
