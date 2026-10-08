#include "BarcodeEncoder.h"

#include <cstddef>
#include <cstdint>

namespace papyrix::codes {
namespace {

constexpr uint8_t CODE128_START_B = 104;
constexpr uint8_t CODE128_START_C = 105;
constexpr uint8_t CODE128_STOP = 106;
constexpr size_t CODE128_QUIET_ZONE_MODULES = 10;
constexpr size_t EAN13_LEFT_QUIET_ZONE_MODULES = 11;
constexpr size_t EAN13_RIGHT_QUIET_ZONE_MODULES = 11;

constexpr uint8_t CODE128_PATTERNS[107][7] = {
    {2, 1, 2, 2, 2, 2, 0}, {2, 2, 2, 1, 2, 2, 0}, {2, 2, 2, 2, 2, 1, 0}, {1, 2, 1, 2, 2, 3, 0}, {1, 2, 1, 3, 2, 2, 0},
    {1, 3, 1, 2, 2, 2, 0}, {1, 2, 2, 2, 1, 3, 0}, {1, 2, 2, 3, 1, 2, 0}, {1, 3, 2, 2, 1, 2, 0}, {2, 2, 1, 2, 1, 3, 0},
    {2, 2, 1, 3, 1, 2, 0}, {2, 3, 1, 2, 1, 2, 0}, {1, 1, 2, 2, 3, 2, 0}, {1, 2, 2, 1, 3, 2, 0}, {1, 2, 2, 2, 3, 1, 0},
    {1, 1, 3, 2, 2, 2, 0}, {1, 2, 3, 1, 2, 2, 0}, {1, 2, 3, 2, 2, 1, 0}, {2, 2, 3, 2, 1, 1, 0}, {2, 2, 1, 1, 3, 2, 0},
    {2, 2, 1, 2, 3, 1, 0}, {2, 1, 3, 2, 1, 2, 0}, {2, 2, 3, 1, 1, 2, 0}, {3, 1, 2, 1, 3, 1, 0}, {3, 1, 1, 2, 2, 2, 0},
    {3, 2, 1, 1, 2, 2, 0}, {3, 2, 1, 2, 2, 1, 0}, {3, 1, 2, 2, 1, 2, 0}, {3, 2, 2, 1, 1, 2, 0}, {3, 2, 2, 2, 1, 1, 0},
    {2, 1, 2, 1, 2, 3, 0}, {2, 1, 2, 3, 2, 1, 0}, {2, 3, 2, 1, 2, 1, 0}, {1, 1, 1, 3, 2, 3, 0}, {1, 3, 1, 1, 2, 3, 0},
    {1, 3, 1, 3, 2, 1, 0}, {1, 1, 2, 3, 1, 3, 0}, {1, 3, 2, 1, 1, 3, 0}, {1, 3, 2, 3, 1, 1, 0}, {2, 1, 1, 3, 1, 3, 0},
    {2, 3, 1, 1, 1, 3, 0}, {2, 3, 1, 3, 1, 1, 0}, {1, 1, 2, 1, 3, 3, 0}, {1, 1, 2, 3, 3, 1, 0}, {1, 3, 2, 1, 3, 1, 0},
    {1, 1, 3, 1, 2, 3, 0}, {1, 1, 3, 3, 2, 1, 0}, {1, 3, 3, 1, 2, 1, 0}, {3, 1, 3, 1, 2, 1, 0}, {2, 1, 1, 3, 3, 1, 0},
    {2, 3, 1, 1, 3, 1, 0}, {2, 1, 3, 1, 1, 3, 0}, {2, 1, 3, 3, 1, 1, 0}, {2, 1, 3, 1, 3, 1, 0}, {3, 1, 1, 1, 2, 3, 0},
    {3, 1, 1, 3, 2, 1, 0}, {3, 3, 1, 1, 2, 1, 0}, {3, 1, 2, 1, 1, 3, 0}, {3, 1, 2, 3, 1, 1, 0}, {3, 3, 2, 1, 1, 1, 0},
    {3, 1, 4, 1, 1, 1, 0}, {2, 2, 1, 4, 1, 1, 0}, {4, 3, 1, 1, 1, 1, 0}, {1, 1, 1, 2, 2, 4, 0}, {1, 1, 1, 4, 2, 2, 0},
    {1, 2, 1, 1, 2, 4, 0}, {1, 2, 1, 4, 2, 1, 0}, {1, 4, 1, 1, 2, 2, 0}, {1, 4, 1, 2, 2, 1, 0}, {1, 1, 2, 2, 1, 4, 0},
    {1, 1, 2, 4, 1, 2, 0}, {1, 2, 2, 1, 1, 4, 0}, {1, 2, 2, 4, 1, 1, 0}, {1, 4, 2, 1, 1, 2, 0}, {1, 4, 2, 2, 1, 1, 0},
    {2, 4, 1, 2, 1, 1, 0}, {2, 2, 1, 1, 1, 4, 0}, {4, 1, 3, 1, 1, 1, 0}, {2, 4, 1, 1, 1, 2, 0}, {1, 3, 4, 1, 1, 1, 0},
    {1, 1, 1, 2, 4, 2, 0}, {1, 2, 1, 1, 4, 2, 0}, {1, 2, 1, 2, 4, 1, 0}, {1, 1, 4, 2, 1, 2, 0}, {1, 2, 4, 1, 1, 2, 0},
    {1, 2, 4, 2, 1, 1, 0}, {4, 1, 1, 2, 1, 2, 0}, {4, 2, 1, 1, 1, 2, 0}, {4, 2, 1, 2, 1, 1, 0}, {2, 1, 2, 1, 4, 1, 0},
    {2, 1, 4, 1, 2, 1, 0}, {4, 1, 2, 1, 2, 1, 0}, {1, 1, 1, 1, 4, 3, 0}, {1, 1, 1, 3, 4, 1, 0}, {1, 3, 1, 1, 4, 1, 0},
    {1, 1, 4, 1, 1, 3, 0}, {1, 1, 4, 3, 1, 1, 0}, {4, 1, 1, 1, 1, 3, 0}, {4, 1, 1, 3, 1, 1, 0}, {1, 1, 3, 1, 4, 1, 0},
    {1, 1, 4, 1, 3, 1, 0}, {3, 1, 1, 1, 4, 1, 0}, {4, 1, 1, 1, 3, 1, 0}, {2, 1, 1, 4, 1, 2, 0}, {2, 1, 1, 2, 1, 4, 0},
    {2, 1, 1, 2, 3, 2, 0}, {2, 3, 3, 1, 1, 1, 2},
};

constexpr uint8_t EAN13_L_PATTERNS[10][7] = {
    {0, 0, 0, 1, 1, 0, 1}, {0, 0, 1, 1, 0, 0, 1}, {0, 0, 1, 0, 0, 1, 1}, {0, 1, 1, 1, 1, 0, 1}, {0, 1, 0, 0, 0, 1, 1},
    {0, 1, 1, 0, 0, 0, 1}, {0, 1, 0, 1, 1, 1, 1}, {0, 1, 1, 1, 0, 1, 1}, {0, 1, 1, 0, 1, 1, 1}, {0, 0, 0, 1, 0, 1, 1},
};

constexpr uint8_t EAN13_G_PATTERNS[10][7] = {
    {0, 1, 0, 0, 1, 1, 1}, {0, 1, 1, 0, 0, 1, 1}, {0, 0, 1, 1, 0, 1, 1}, {0, 1, 0, 0, 0, 0, 1}, {0, 0, 1, 1, 1, 0, 1},
    {0, 1, 1, 1, 0, 0, 1}, {0, 0, 0, 0, 1, 0, 1}, {0, 0, 1, 0, 0, 0, 1}, {0, 0, 0, 1, 0, 0, 1}, {0, 0, 1, 0, 1, 1, 1},
};

constexpr uint8_t EAN13_R_PATTERNS[10][7] = {
    {1, 1, 1, 0, 0, 1, 0}, {1, 1, 0, 0, 1, 1, 0}, {1, 1, 0, 1, 1, 0, 0}, {1, 0, 0, 0, 0, 1, 0}, {1, 0, 1, 1, 1, 0, 0},
    {1, 0, 0, 1, 1, 1, 0}, {1, 0, 1, 0, 0, 0, 0}, {1, 0, 0, 0, 1, 0, 0}, {1, 0, 0, 1, 0, 0, 0}, {1, 1, 1, 0, 1, 0, 0},
};

constexpr bool EAN13_LEFT_G_PARITY[10][6] = {
    {false, false, false, false, false, false}, {false, false, true, false, true, true},
    {false, false, true, true, false, true},    {false, false, true, true, true, false},
    {false, true, false, false, true, true},    {false, true, true, false, false, true},
    {false, true, true, true, false, false},    {false, true, false, true, false, true},
    {false, true, false, true, true, false},    {false, true, true, false, true, false},
};

void clearSymbol(BarcodeFormat format, BarcodeSymbol& symbol) {
  symbol.moduleCount = 0;
  symbol.data[0] = '\0';
  symbol.format = format;
  for (size_t i = 0; i < MAX_BARCODE_MODULES; ++i) {
    symbol.modules[i] = 0;
  }
}

size_t boundedLength(const char* data) {
  if (data == nullptr) {
    return 0;
  }

  size_t length = 0;
  while (length <= MAX_BARCODE_DATA_BYTES && data[length] != '\0') {
    ++length;
  }
  return length;
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool isPrintableAscii(char c) { return c >= 32 && c <= 126; }

void appendModule(BarcodeSymbol& symbol, uint8_t module) {
  if (symbol.moduleCount < MAX_BARCODE_MODULES) {
    symbol.modules[symbol.moduleCount] = module;
  }
  ++symbol.moduleCount;
}

void appendQuiet(BarcodeSymbol& symbol, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    appendModule(symbol, 0);
  }
}

void appendBits(BarcodeSymbol& symbol, const uint8_t* bits, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    appendModule(symbol, bits[i]);
  }
}

void appendCode128Pattern(BarcodeSymbol& symbol, uint8_t code) {
  uint8_t value = 1;
  for (size_t i = 0; i < 7 && CODE128_PATTERNS[code][i] != 0; ++i) {
    for (size_t j = 0; j < CODE128_PATTERNS[code][i]; ++j) {
      appendModule(symbol, value);
    }
    value = value == 0 ? 1 : 0;
  }
}

void copyData(const char* data, size_t length, BarcodeSymbol& symbol) {
  for (size_t i = 0; i < length; ++i) {
    symbol.data[i] = data[i];
  }
  symbol.data[length] = '\0';
}

BarcodeError encodeCode128(const char* data, size_t length, BarcodeSymbol& symbol) {
  bool allDigits = true;
  for (size_t i = 0; i < length; ++i) {
    if (!isPrintableAscii(data[i])) {
      return BarcodeError::InvalidCharacter;
    }
    if (!isDigit(data[i])) {
      allDigits = false;
    }
  }

  const bool useCodeC = allDigits && (length % 2 == 0);
  uint32_t checksum = useCodeC ? CODE128_START_C : CODE128_START_B;
  size_t weight = 1;

  appendQuiet(symbol, CODE128_QUIET_ZONE_MODULES);
  appendCode128Pattern(symbol, useCodeC ? CODE128_START_C : CODE128_START_B);

  if (useCodeC) {
    for (size_t i = 0; i < length; i += 2) {
      const uint8_t code = static_cast<uint8_t>((data[i] - '0') * 10 + (data[i + 1] - '0'));
      appendCode128Pattern(symbol, code);
      checksum += code * weight;
      ++weight;
    }
  } else {
    for (size_t i = 0; i < length; ++i) {
      const uint8_t code = static_cast<uint8_t>(data[i] - 32);
      appendCode128Pattern(symbol, code);
      checksum += code * weight;
      ++weight;
    }
  }

  appendCode128Pattern(symbol, static_cast<uint8_t>(checksum % 103));
  appendCode128Pattern(symbol, CODE128_STOP);
  appendQuiet(symbol, CODE128_QUIET_ZONE_MODULES);
  copyData(data, length, symbol);
  return symbol.moduleCount <= MAX_BARCODE_MODULES ? BarcodeError::None : BarcodeError::TooLong;
}

uint8_t computeEan13Checksum(const char* digits12) {
  uint16_t sum = 0;
  for (size_t i = 0; i < 12; ++i) {
    const uint8_t digit = static_cast<uint8_t>(digits12[i] - '0');
    sum += (i % 2 == 0) ? digit : static_cast<uint8_t>(digit * 3);
  }
  return static_cast<uint8_t>((10 - (sum % 10)) % 10);
}

BarcodeError encodeEan13(const char* data, size_t length, BarcodeSymbol& symbol) {
  if (length != 12 && length != 13) {
    return BarcodeError::InvalidLength;
  }

  for (size_t i = 0; i < length; ++i) {
    if (!isDigit(data[i])) {
      return BarcodeError::InvalidCharacter;
    }
  }

  char canonical[14]{};
  for (size_t i = 0; i < 12; ++i) {
    canonical[i] = data[i];
  }
  canonical[12] = static_cast<char>('0' + computeEan13Checksum(canonical));
  canonical[13] = '\0';

  if (length == 13 && data[12] != canonical[12]) {
    return BarcodeError::InvalidChecksum;
  }

  appendQuiet(symbol, EAN13_LEFT_QUIET_ZONE_MODULES);
  constexpr uint8_t startGuard[] = {1, 0, 1};
  constexpr uint8_t middleGuard[] = {0, 1, 0, 1, 0};
  appendBits(symbol, startGuard, sizeof(startGuard));

  const uint8_t firstDigit = static_cast<uint8_t>(canonical[0] - '0');
  for (size_t i = 1; i <= 6; ++i) {
    const uint8_t digit = static_cast<uint8_t>(canonical[i] - '0');
    appendBits(symbol, EAN13_LEFT_G_PARITY[firstDigit][i - 1] ? EAN13_G_PATTERNS[digit] : EAN13_L_PATTERNS[digit], 7);
  }

  appendBits(symbol, middleGuard, sizeof(middleGuard));

  for (size_t i = 7; i <= 12; ++i) {
    const uint8_t digit = static_cast<uint8_t>(canonical[i] - '0');
    appendBits(symbol, EAN13_R_PATTERNS[digit], 7);
  }

  appendBits(symbol, startGuard, sizeof(startGuard));
  appendQuiet(symbol, EAN13_RIGHT_QUIET_ZONE_MODULES);
  copyData(canonical, 13, symbol);
  return symbol.moduleCount <= MAX_BARCODE_MODULES ? BarcodeError::None : BarcodeError::TooLong;
}

}  // namespace

BarcodeError encodeBarcode(BarcodeFormat format, const char* data, BarcodeSymbol& symbol) {
  clearSymbol(format, symbol);
  const size_t length = boundedLength(data);

  if (data == nullptr || length == 0) {
    return BarcodeError::Empty;
  }

  if (length > MAX_BARCODE_DATA_BYTES) {
    return BarcodeError::TooLong;
  }

  BarcodeError error = BarcodeError::InvalidLength;
  switch (format) {
    case BarcodeFormat::Code128:
      error = encodeCode128(data, length, symbol);
      break;
    case BarcodeFormat::Ean13:
      error = encodeEan13(data, length, symbol);
      break;
  }

  if (error != BarcodeError::None) {
    clearSymbol(format, symbol);
  }
  return error;
}

const char* barcodeErrorMessage(BarcodeError error) {
  switch (error) {
    case BarcodeError::None:
      return "none";
    case BarcodeError::Empty:
      return "empty barcode data";
    case BarcodeError::TooLong:
      return "barcode data is too long";
    case BarcodeError::InvalidCharacter:
      return "barcode data contains an invalid character";
    case BarcodeError::InvalidLength:
      return "barcode data has an invalid length";
    case BarcodeError::InvalidChecksum:
      return "barcode checksum is invalid";
  }

  return "unknown barcode error";
}

}  // namespace papyrix::codes
