#include <cstddef>
#include <cstdint>
#include <cstring>

#include "apps/BarcodeEncoder.h"
#include "test_utils.h"

namespace {

using papyrix::codes::BarcodeError;
using papyrix::codes::BarcodeFormat;
using papyrix::codes::BarcodeSymbol;

void expectError(TestUtils::TestRunner& runner, BarcodeError expected, BarcodeError actual, const char* testName) {
  runner.expectEq(static_cast<int>(expected), static_cast<int>(actual), testName);
}

void expectFormat(TestUtils::TestRunner& runner, BarcodeFormat expected, BarcodeFormat actual, const char* testName) {
  runner.expectEq(static_cast<int>(expected), static_cast<int>(actual), testName);
}

void expectModules(TestUtils::TestRunner& runner, const BarcodeSymbol& symbol, const char* expected,
                   const char* testName) {
  const size_t expectedLength = std::strlen(expected);
  bool matches = symbol.moduleCount == expectedLength;
  if (matches) {
    for (size_t i = 0; i < expectedLength; ++i) {
      const uint8_t expectedModule = expected[i] == '1' ? 1 : 0;
      if (symbol.modules[i] != expectedModule) {
        matches = false;
        break;
      }
    }
  }
  runner.expectTrue(matches, testName);
}

void expectRange(TestUtils::TestRunner& runner, const BarcodeSymbol& symbol, size_t start, size_t count, uint8_t value,
                 const char* testName) {
  bool matches = start + count <= symbol.moduleCount;
  for (size_t i = 0; matches && i < count; ++i) {
    matches = symbol.modules[start + i] == value;
  }
  runner.expectTrue(matches, testName);
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("BarcodeEncoder");

  {
    BarcodeSymbol symbol{};
    const BarcodeError error = papyrix::codes::encodeBarcode(BarcodeFormat::Ean13, "400638133393", symbol);
    expectError(runner, BarcodeError::None, error, "EAN-13 accepts 12 digits");
    runner.expectEqual("4006381333931", symbol.data, "EAN-13 stores canonical checksum digit");
    expectFormat(runner, BarcodeFormat::Ean13, symbol.format, "EAN-13 stores format");
    runner.expectEq(size_t{117}, symbol.moduleCount, "EAN-13 module count includes quiet zones");
    expectRange(runner, symbol, 0, 11, 0, "EAN-13 left quiet zone is 11 modules");
    expectRange(runner, symbol, symbol.moduleCount - 11, 11, 0, "EAN-13 right quiet zone is 11 modules");
    expectModules(runner, symbol,
                  "00000000000"
                  "101"
                  "0001101"
                  "0100111"
                  "0101111"
                  "0111101"
                  "0001001"
                  "0110011"
                  "01010"
                  "1000010"
                  "1000010"
                  "1000010"
                  "1110100"
                  "1000010"
                  "1100110"
                  "101"
                  "00000000000",
                  "EAN-13 4006381333931 matches known modules");
  }

  {
    BarcodeSymbol symbol{};
    const BarcodeError error = papyrix::codes::encodeBarcode(BarcodeFormat::Ean13, "4006381333931", symbol);
    expectError(runner, BarcodeError::None, error, "EAN-13 accepts valid 13-digit checksum");
    runner.expectEqual("4006381333931", symbol.data, "EAN-13 valid checksum remains canonical");
  }

  {
    BarcodeSymbol symbol{};
    symbol.moduleCount = 7;
    symbol.data[0] = 'x';
    symbol.modules[0] = 1;
    const BarcodeError error = papyrix::codes::encodeBarcode(BarcodeFormat::Ean13, "4006381333932", symbol);
    expectError(runner, BarcodeError::InvalidChecksum, error, "EAN-13 rejects bad checksum");
    runner.expectEq(size_t{0}, symbol.moduleCount, "failure clears module count");
    runner.expectEq('\0', symbol.data[0], "failure clears data");
    runner.expectEq(uint8_t{0}, symbol.modules[0], "failure clears modules");
  }

  {
    BarcodeSymbol symbol{};
    expectError(runner, BarcodeError::InvalidLength,
                papyrix::codes::encodeBarcode(BarcodeFormat::Ean13, "12345678901", symbol),
                "EAN-13 rejects short data");
    expectError(runner, BarcodeError::InvalidCharacter,
                papyrix::codes::encodeBarcode(BarcodeFormat::Ean13, "40063813339X", symbol),
                "EAN-13 rejects non-digits");
  }

  {
    BarcodeSymbol symbol{};
    const BarcodeError error = papyrix::codes::encodeBarcode(BarcodeFormat::Code128, "AB", symbol);
    expectError(runner, BarcodeError::None, error, "Code 128 accepts printable ASCII");
    runner.expectEqual("AB", symbol.data, "Code 128 stores input");
    expectFormat(runner, BarcodeFormat::Code128, symbol.format, "Code 128 stores format");
    runner.expectEq(size_t{77}, symbol.moduleCount, "Code 128 B module count for two characters");
    expectRange(runner, symbol, 0, 10, 0, "Code 128 left quiet zone is 10 modules");
    expectRange(runner, symbol, symbol.moduleCount - 10, 10, 0, "Code 128 right quiet zone is 10 modules");
    expectModules(runner, symbol,
                  "0000000000"
                  "11010010000"
                  "10100011000"
                  "10001011000"
                  "11110101110"
                  "1100011101011"
                  "0000000000",
                  "Code 128 B AB uses weighted modulo-103 checksum 33");
  }

  {
    BarcodeSymbol symbol{};
    const BarcodeError error = papyrix::codes::encodeBarcode(BarcodeFormat::Code128, "123456", symbol);
    expectError(runner, BarcodeError::None, error, "Code 128 C accepts even-length digits");
    runner.expectEq(size_t{88}, symbol.moduleCount, "Code 128 C module count for three digit pairs");
    expectModules(runner, symbol,
                  "0000000000"
                  "11010011100"
                  "10110011100"
                  "10001011000"
                  "11100010110"
                  "10001101110"
                  "1100011101011"
                  "0000000000",
                  "Code 128 C 123456 uses pair values and checksum 44");
  }

  {
    BarcodeSymbol symbol{};
    expectError(runner, BarcodeError::None, papyrix::codes::encodeBarcode(BarcodeFormat::Code128, " !~", symbol),
                "Code 128 accepts full printable ASCII boundary");
    expectError(runner, BarcodeError::InvalidCharacter,
                papyrix::codes::encodeBarcode(BarcodeFormat::Code128, "line\nbreak", symbol),
                "Code 128 rejects control characters");
  }

  {
    char maxData[papyrix::codes::MAX_BARCODE_DATA_BYTES + 1]{};
    for (size_t i = 0; i < papyrix::codes::MAX_BARCODE_DATA_BYTES; ++i) {
      maxData[i] = 'A';
    }
    BarcodeSymbol symbol{};
    expectError(runner, BarcodeError::None, papyrix::codes::encodeBarcode(BarcodeFormat::Code128, maxData, symbol),
                "Code 128 accepts max-cap data");
    runner.expectEq(papyrix::codes::MAX_BARCODE_DATA_BYTES, std::strlen(symbol.data),
                    "Code 128 stores max-cap data with terminator");
    runner.expectEq(size_t{748}, symbol.moduleCount, "Code 128 max-cap data fits module storage");
  }

  {
    char tooLong[papyrix::codes::MAX_BARCODE_DATA_BYTES + 1];
    for (size_t i = 0; i < sizeof(tooLong); ++i) {
      tooLong[i] = 'A';
    }
    BarcodeSymbol symbol{};
    expectError(runner, BarcodeError::TooLong, papyrix::codes::encodeBarcode(BarcodeFormat::Code128, tooLong, symbol),
                "bounded scan rejects 64 non-NUL bytes as too long");
  }

  {
    BarcodeSymbol symbol{};
    expectError(runner, BarcodeError::Empty, papyrix::codes::encodeBarcode(BarcodeFormat::Code128, "", symbol),
                "empty Code 128 input is rejected");
    expectError(runner, BarcodeError::Empty, papyrix::codes::encodeBarcode(BarcodeFormat::Ean13, nullptr, symbol),
                "null input is rejected as empty");
  }

  {
    runner.expectTrue(std::strlen(papyrix::codes::barcodeErrorMessage(BarcodeError::InvalidCharacter)) > 0,
                      "error messages are available");
  }

  return runner.allPassed() ? 0 : 1;
}
