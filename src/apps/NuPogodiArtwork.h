#pragma once

#include <cstdint>

namespace papyrix::games::art {

struct Bitmap {
  int16_t x;
  int16_t y;
  uint16_t width;
  uint16_t height;
  const uint8_t* data;
};

struct Artwork {
  uint16_t width;
  uint16_t height;
  Bitmap background;
  Bitmap bodies[2];
  Bitmap baskets[4];
  Bitmap eggs[4][5];
  Bitmap misses[3];
  Bitmap digits[3][7];
  Bitmap rabbit[2];
  Bitmap gameB;
};

}  // namespace papyrix::games::art

#if __has_include("NuPogodiArtwork.generated.h")
#define PAPYRIX_NU_ORIGINAL_ART_AVAILABLE 1
#include "NuPogodiArtwork.generated.h"
#else
#define PAPYRIX_NU_ORIGINAL_ART_AVAILABLE 0
#endif
