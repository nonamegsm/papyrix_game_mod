#pragma once

#include <cstddef>
#include <cstdint>

namespace papyrix::codes {

static constexpr size_t MAX_QR_CODES = 16;
static constexpr size_t MAX_QR_NAME_BYTES = 48;
static constexpr size_t MAX_QR_DATA_BYTES = 512;

struct QrCodeInfo {
  uint8_t id;
  char name[MAX_QR_NAME_BYTES + 1];
};

struct QrCodeEntry {
  uint8_t id;
  char name[MAX_QR_NAME_BYTES + 1];
  char data[MAX_QR_DATA_BYTES + 1];
};

enum class StoreResult : uint8_t {
  Ok,
  NotFound,
  Full,
  InvalidName,
  InvalidData,
  StorageError,
};

const char* storeErrorMessage(StoreResult result);

class QrCodeStore {
 public:
  StoreResult list(QrCodeInfo* out, size_t capacity, size_t& count) const;
  StoreResult load(uint8_t id, QrCodeEntry& out) const;
  StoreResult save(int id, const char* name, const char* data, uint8_t& savedId) const;
  StoreResult remove(uint8_t id) const;

 private:
  StoreResult validateName(const char* name) const;
  StoreResult validateData(const char* data) const;
  bool ensureStoreDir() const;
  bool makePath(uint8_t id, bool temporary, char* out, size_t outSize) const;
  bool makeBackupPath(uint8_t id, char* out, size_t outSize) const;
  bool slotExists(uint8_t id) const;
  StoreResult readEntry(uint8_t id, QrCodeEntry& out) const;
  StoreResult readEntryFromPath(uint8_t id, const char* path, QrCodeEntry& out) const;
};

}  // namespace papyrix::codes
