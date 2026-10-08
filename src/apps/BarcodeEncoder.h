#pragma once

#include <cstddef>
#include <cstdint>

namespace papyrix::codes {

enum class BarcodeFormat : uint8_t { Code128, Ean13 };

enum class BarcodeError {
  None,
  Empty,
  TooLong,
  InvalidCharacter,
  InvalidLength,
  InvalidChecksum,
};

constexpr size_t MAX_BARCODE_DATA_BYTES = 63;
constexpr size_t MAX_BARCODE_MODULES = 800;

struct BarcodeSymbol {
  uint8_t modules[MAX_BARCODE_MODULES]{};
  size_t moduleCount = 0;
  char data[MAX_BARCODE_DATA_BYTES + 1]{};
  BarcodeFormat format = BarcodeFormat::Code128;
};

BarcodeError encodeBarcode(BarcodeFormat format, const char* data, BarcodeSymbol& symbol);
const char* barcodeErrorMessage(BarcodeError error);

}  // namespace papyrix::codes
