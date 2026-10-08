#pragma once

#include <qrcode.h>

#include <cstddef>
#include <cstdint>

namespace papyrix::codes {

constexpr uint8_t MAX_QR_VERSION = 20;
constexpr size_t QR_BUFFER_BYTES = ((4 * MAX_QR_VERSION + 17) * (4 * MAX_QR_VERSION + 17) + 7) / 8;

struct QrSymbol {
  QRCode code{};
  uint8_t modules[QR_BUFFER_BYTES]{};
  bool valid = false;

  QrSymbol() = default;
  QrSymbol(const QrSymbol&) = delete;
  QrSymbol& operator=(const QrSymbol&) = delete;

  bool module(uint8_t x, uint8_t y) const {
    if (!valid || x >= code.size || y >= code.size) return false;
    QRCode view = code;
    return qrcode_getModule(&view, x, y);
  }
};

bool encodeQrCode(const char* text, QrSymbol& symbol);

struct CodeRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  int modulePixels = 0;
};

// QR quiet zone is four modules on every side. Barcode quiet zones are encoded
// into their module data and must remain inside this rectangle.
CodeRect qrCodeRect(int screenWidth, int screenHeight, int modules);
CodeRect barcodeRect(int screenWidth, int screenHeight, size_t modules);

}  // namespace papyrix::codes
