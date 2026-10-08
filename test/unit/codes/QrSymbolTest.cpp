#include <cstdint>
#include <string>

#include "apps/QrSymbol.h"
#include "test_utils.h"

namespace {

bool finderLooksIntact(const papyrix::codes::QrSymbol& symbol, uint8_t x, uint8_t y) {
  return symbol.module(x, y) && symbol.module(x + 6, y) && symbol.module(x, y + 6) && symbol.module(x + 6, y + 6) &&
         !symbol.module(x + 1, y + 1) && symbol.module(x + 3, y + 3);
}

std::string repeated(size_t count, char value) { return std::string(count, value); }

}  // namespace

int main() {
  TestUtils::TestRunner runner("QrSymbolTest");

  {
    papyrix::codes::QrSymbol symbol;
    runner.expectTrue(papyrix::codes::encodeQrCode(repeated(14, 'A').c_str(), symbol),
                      "version 1 byte-capacity boundary accepts 14 bytes at ECC medium");
    runner.expectEq(uint8_t{21}, symbol.code.size, "14-byte QR uses version 1");
  }

  {
    papyrix::codes::QrSymbol symbol;
    runner.expectTrue(papyrix::codes::encodeQrCode(repeated(15, 'A').c_str(), symbol),
                      "payload above version 1 capacity advances to version 2");
    runner.expectEq(uint8_t{25}, symbol.code.size, "15-byte QR uses version 2");
  }

  {
    papyrix::codes::QrSymbol symbol;
    runner.expectTrue(papyrix::codes::encodeQrCode(repeated(512, 'z').c_str(), symbol),
                      "store maximum QR payload is encodable");
    runner.expectEq(uint8_t{89}, symbol.code.size, "512-byte payload uses version 18 at ECC medium");
  }

  {
    papyrix::codes::QrSymbol symbol;
    runner.expectFalse(papyrix::codes::encodeQrCode(repeated(513, 'z').c_str(), symbol),
                       "payload above store maximum is rejected before qrcode library encode");
    runner.expectFalse(symbol.valid, "rejected oversized payload leaves symbol invalid");
  }

  {
    papyrix::codes::QrSymbol symbol;
    runner.expectFalse(papyrix::codes::encodeQrCode("", symbol), "empty QR payload is rejected");
    runner.expectFalse(papyrix::codes::encodeQrCode(nullptr, symbol), "null QR payload is rejected");
  }

  {
    papyrix::codes::QrSymbol symbol;
    runner.expectTrue(papyrix::codes::encodeQrCode("finder-pattern-check", symbol),
                      "QR encode succeeds for pattern test");
    const uint8_t lastFinder = static_cast<uint8_t>(symbol.code.size - 7);
    runner.expectTrue(finderLooksIntact(symbol, 0, 0), "top-left finder pattern remains readable");
    runner.expectTrue(finderLooksIntact(symbol, lastFinder, 0), "top-right finder pattern remains readable");
    runner.expectTrue(finderLooksIntact(symbol, 0, lastFinder), "bottom-left finder pattern remains readable");
    runner.expectFalse(symbol.module(symbol.code.size, 0), "module lookup rejects x outside QR bounds");
    runner.expectFalse(symbol.module(0, symbol.code.size), "module lookup rejects y outside QR bounds");
  }

  {
    papyrix::codes::QrSymbol invalid;
    runner.expectFalse(invalid.module(0, 0), "invalid symbol reports all modules white");
  }

  {
    const auto rect = papyrix::codes::qrCodeRect(480, 800, 21);
    runner.expectEq(15, rect.modulePixels, "QR rect scales version 1 with four-module quiet zone");
    runner.expectEq(435, rect.width, "QR rect width includes both quiet zones");
    runner.expectEq(435, rect.height, "QR rect height includes both quiet zones");
    runner.expectEq(22, rect.x, "QR rect centers quiet-zone canvas horizontally");
    runner.expectEq(182, rect.y, "QR rect centers quiet-zone canvas in content area");
  }

  {
    runner.expectEq(0, papyrix::codes::qrCodeRect(39, 800, 21).modulePixels, "QR rect rejects too-narrow screens");
    runner.expectEq(0, papyrix::codes::qrCodeRect(480, 219, 21).modulePixels, "QR rect rejects too-short screens");
    runner.expectEq(0, papyrix::codes::qrCodeRect(480, 800, 0).modulePixels,
                    "QR rect rejects nonpositive module counts");
  }

  return runner.allPassed() ? 0 : 1;
}
