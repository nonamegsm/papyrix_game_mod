#include "QrSymbol.h"

#include <algorithm>
#include <cstring>

#include "QrCodeStore.h"

namespace papyrix::codes {

bool encodeQrCode(const char* text, QrSymbol& symbol) {
  symbol.valid = false;
  symbol.code = {};
  if (!text) return false;
  size_t length = 0;
  while (length <= MAX_QR_DATA_BYTES && text[length]) ++length;
  if (!length || length > MAX_QR_DATA_BYTES) return false;

  // QRCode 0.0.1 checks the raw module buffer, which also holds ECC words.
  // Preflight the usable byte-mode capacity at ECC_MEDIUM before encoding;
  // relying only on its return code can silently truncate a dense payload.
  static constexpr uint16_t BYTE_CAPACITY_MEDIUM[] = {14,  26,  42,  62,  84,  106, 122, 152, 180, 213,
                                                      251, 287, 331, 362, 412, 450, 504, 560, 624, 666};
  for (uint8_t version = 1; version <= MAX_QR_VERSION; ++version) {
    if (length > BYTE_CAPACITY_MEDIUM[version - 1]) continue;
    if (qrcode_getBufferSize(version) > sizeof(symbol.modules)) return false;
    if (qrcode_initText(&symbol.code, symbol.modules, version, ECC_MEDIUM, text) == 0) {
      symbol.valid = true;
      return true;
    }
  }
  return false;
}

CodeRect qrCodeRect(int screenWidth, int screenHeight, int modules) {
  if (modules <= 0 || screenWidth < 40 || screenHeight < 220) return {};
  const int size = modules + 8;
  const int pixels = std::min(screenWidth - 32, screenHeight - 180) / size;
  if (pixels < 1) return {};
  const int side = size * pixels;
  return {(screenWidth - side) / 2, 90 + (screenHeight - 180 - side) / 2, side, side, pixels};
}

CodeRect barcodeRect(int screenWidth, int screenHeight, size_t modules) {
  if (!modules || screenWidth < 40 || screenHeight < 220) return {};
  const int pixels = static_cast<int>((screenWidth - 32) / modules);
  if (pixels < 1) return {};
  const int width = static_cast<int>(modules) * pixels;
  const int height = std::min(200, screenHeight - 190);
  return {(screenWidth - width) / 2, 90 + (screenHeight - 190 - height) / 2, width, height, pixels};
}

}  // namespace papyrix::codes
