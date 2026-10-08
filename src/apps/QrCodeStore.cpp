#include "QrCodeStore.h"

#include <ArduinoJson.h>
#include <SDCardManager.h>

#include <cstdio>
#include <cstring>

#include "../network/WebFileNameValidation.h"

namespace papyrix::codes {
namespace {

constexpr const char* kStoreDir = "/.papyrix/apps/qrcodes";
constexpr const char* kPapyrixDir = "/.papyrix";
constexpr const char* kAppsDir = "/.papyrix/apps";
constexpr size_t kMaxStoredJsonBytes = 1280;
constexpr size_t kPathBufferBytes = 48;

bool isValidId(uint8_t id) { return id < MAX_QR_CODES; }

size_t boundedLength(const char* value, size_t maxBytesPlusOne) {
  if (!value) return 0;
  size_t length = 0;
  while (length <= maxBytesPlusOne && value[length] != '\0') {
    ++length;
  }
  return length;
}

bool hasNul(const char* value, size_t length) { return value && std::memchr(value, '\0', length) != nullptr; }

bool hasOnlySpace(const char* value, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
  }
  return true;
}

bool hasNameControl(const char* value, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (static_cast<unsigned char>(value[i]) < 0x20) return true;
  }
  return false;
}

bool hasUnsupportedDataControl(const char* value, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') return true;
  }
  return false;
}

StoreResult validateNameBytes(const char* name, size_t length) {
  if (!name || length == 0 || length > MAX_QR_NAME_BYTES || hasNul(name, length) || hasOnlySpace(name, length) ||
      hasNameControl(name, length) || !web::isValidUtf8(name, length)) {
    return StoreResult::InvalidName;
  }
  return StoreResult::Ok;
}

StoreResult validateDataBytes(const char* data, size_t length) {
  if (!data || length == 0 || length > MAX_QR_DATA_BYTES || hasNul(data, length) ||
      hasUnsupportedDataControl(data, length) || !web::isValidUtf8(data, length)) {
    return StoreResult::InvalidData;
  }
  return StoreResult::Ok;
}

void copyBounded(char* destination, size_t destinationSize, const char* source) {
  if (!destination || destinationSize == 0) return;
  if (!source) {
    destination[0] = '\0';
    return;
  }
  std::strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

}  // namespace

const char* storeErrorMessage(StoreResult result) {
  switch (result) {
    case StoreResult::Ok:
      return "OK";
    case StoreResult::NotFound:
      return "QR code not found";
    case StoreResult::Full:
      return "QR code storage is full";
    case StoreResult::InvalidName:
      return "Invalid QR code name";
    case StoreResult::InvalidData:
      return "Invalid QR code data";
    case StoreResult::StorageError:
      return "QR code storage error";
  }
  return "QR code storage error";
}

StoreResult QrCodeStore::validateName(const char* name) const {
  return validateNameBytes(name, boundedLength(name, MAX_QR_NAME_BYTES + 1));
}

StoreResult QrCodeStore::validateData(const char* data) const {
  return validateDataBytes(data, boundedLength(data, MAX_QR_DATA_BYTES + 1));
}

bool QrCodeStore::ensureStoreDir() const {
  if (!SdMan.exists(kPapyrixDir) && !SdMan.mkdir(kPapyrixDir)) return false;
  if (!SdMan.exists(kAppsDir) && !SdMan.mkdir(kAppsDir)) return false;
  return SdMan.exists(kStoreDir) || SdMan.mkdir(kStoreDir);
}

bool QrCodeStore::makePath(uint8_t id, bool temporary, char* out, size_t outSize) const {
  if (!isValidId(id) || !out || outSize < kPathBufferBytes) return false;
  const int written =
      std::snprintf(out, outSize, "%s/%02u.json%s", kStoreDir, static_cast<unsigned>(id), temporary ? ".tmp" : "");
  return written > 0 && static_cast<size_t>(written) < outSize;
}

bool QrCodeStore::makeBackupPath(uint8_t id, char* out, size_t outSize) const {
  if (!isValidId(id) || !out || outSize < kPathBufferBytes) return false;
  const int written = std::snprintf(out, outSize, "%s/%02u.json.bak", kStoreDir, static_cast<unsigned>(id));
  return written > 0 && static_cast<size_t>(written) < outSize;
}

bool QrCodeStore::slotExists(uint8_t id) const {
  char finalPath[kPathBufferBytes] = {};
  char backupPath[kPathBufferBytes] = {};
  if (!makePath(id, false, finalPath, sizeof(finalPath)) || !makeBackupPath(id, backupPath, sizeof(backupPath))) {
    return false;
  }
  return SdMan.exists(finalPath) || SdMan.exists(backupPath);
}

StoreResult QrCodeStore::readEntryFromPath(uint8_t id, const char* path, QrCodeEntry& out) const {
  FsFile file;
  if (!SdMan.openFileForRead("QRC", path, file)) return StoreResult::NotFound;

  const size_t fileSize = file.size();
  if (fileSize == 0 || fileSize > kMaxStoredJsonBytes) {
    file.close();
    return StoreResult::StorageError;
  }

  char json[kMaxStoredJsonBytes + 1] = {};
  const int bytesRead = file.read(json, fileSize);
  file.close();
  if (bytesRead < 0 || static_cast<size_t>(bytesRead) != fileSize) return StoreResult::StorageError;
  json[fileSize] = '\0';

  JsonDocument doc;
  if (deserializeJson(doc, json, fileSize) != DeserializationError::Ok) return StoreResult::StorageError;
  JsonObjectConst object = doc.as<JsonObjectConst>();
  if (object.isNull()) return StoreResult::StorageError;

  JsonVariantConst idValue = object["id"];
  JsonVariantConst nameValue = object["name"];
  JsonVariantConst dataValue = object["data"];
  if (!idValue.is<int>() || !nameValue.is<JsonString>() || !dataValue.is<JsonString>()) {
    return StoreResult::StorageError;
  }

  const int storedId = idValue.as<int>();
  const JsonString name = nameValue.as<JsonString>();
  const JsonString data = dataValue.as<JsonString>();
  if (storedId != static_cast<int>(id)) return StoreResult::StorageError;
  if (validateNameBytes(name.c_str(), name.size()) != StoreResult::Ok ||
      validateDataBytes(data.c_str(), data.size()) != StoreResult::Ok) {
    return StoreResult::StorageError;
  }

  out.id = id;
  copyBounded(out.name, sizeof(out.name), name.c_str());
  copyBounded(out.data, sizeof(out.data), data.c_str());
  return StoreResult::Ok;
}

StoreResult QrCodeStore::readEntry(uint8_t id, QrCodeEntry& out) const {
  if (!isValidId(id)) return StoreResult::NotFound;

  char finalPath[kPathBufferBytes] = {};
  char backupPath[kPathBufferBytes] = {};
  if (!makePath(id, false, finalPath, sizeof(finalPath)) || !makeBackupPath(id, backupPath, sizeof(backupPath))) {
    return StoreResult::StorageError;
  }

  const StoreResult finalResult = readEntryFromPath(id, finalPath, out);
  if (finalResult != StoreResult::NotFound) return finalResult;
  return readEntryFromPath(id, backupPath, out);
}

StoreResult QrCodeStore::list(QrCodeInfo* out, size_t capacity, size_t& count) const {
  count = 0;
  if (!out && capacity > 0) return StoreResult::StorageError;

  for (uint8_t id = 0; id < MAX_QR_CODES; ++id) {
    QrCodeEntry entry = {};
    const StoreResult result = readEntry(id, entry);
    if (result == StoreResult::NotFound) continue;
    if (result != StoreResult::Ok) continue;
    if (count < capacity) {
      out[count].id = entry.id;
      copyBounded(out[count].name, sizeof(out[count].name), entry.name);
    }
    ++count;
  }

  return StoreResult::Ok;
}

StoreResult QrCodeStore::load(uint8_t id, QrCodeEntry& out) const { return readEntry(id, out); }

StoreResult QrCodeStore::save(int id, const char* name, const char* data, uint8_t& savedId) const {
  StoreResult validation = validateName(name);
  if (validation != StoreResult::Ok) return validation;
  validation = validateData(data);
  if (validation != StoreResult::Ok) return validation;

  uint8_t targetId = 0;
  if (id < 0) {
    bool found = false;
    for (uint8_t candidate = 0; candidate < MAX_QR_CODES; ++candidate) {
      char path[kPathBufferBytes] = {};
      if (!makePath(candidate, false, path, sizeof(path))) return StoreResult::StorageError;
      if (!slotExists(candidate)) {
        targetId = candidate;
        found = true;
        break;
      }
    }
    if (!found) return StoreResult::Full;
  } else if (id < static_cast<int>(MAX_QR_CODES)) {
    targetId = static_cast<uint8_t>(id);
    if (!slotExists(targetId)) return StoreResult::NotFound;
  } else {
    return StoreResult::NotFound;
  }

  if (!ensureStoreDir()) return StoreResult::StorageError;

  char finalPath[kPathBufferBytes] = {};
  char tmpPath[kPathBufferBytes] = {};
  char backupPath[kPathBufferBytes] = {};
  if (!makePath(targetId, false, finalPath, sizeof(finalPath)) || !makePath(targetId, true, tmpPath, sizeof(tmpPath)) ||
      !makeBackupPath(targetId, backupPath, sizeof(backupPath))) {
    return StoreResult::StorageError;
  }

  FsFile file;
  if (!SdMan.openFileForWrite("QRC", tmpPath, file)) return StoreResult::StorageError;

  JsonDocument doc;
  doc["id"] = targetId;
  doc["name"] = name;
  doc["data"] = data;

  char output[kMaxStoredJsonBytes] = {};
  const size_t jsonSize = serializeJson(doc, output, sizeof(output));
  const bool serialized = jsonSize > 0 && jsonSize < sizeof(output);
  const bool wrote = serialized && file.write(reinterpret_cast<const uint8_t*>(output), jsonSize) == jsonSize;
  const bool synced = wrote && file.sync();
  file.close();

  if (!serialized || !wrote || !synced) {
    SdMan.remove(tmpPath);
    return StoreResult::StorageError;
  }

  const bool hasFinal = SdMan.exists(finalPath);
  const bool hasBackup = SdMan.exists(backupPath);
  if (hasFinal) {
    if (hasBackup && !SdMan.remove(backupPath)) {
      SdMan.remove(tmpPath);
      return StoreResult::StorageError;
    }
    if (!SdMan.rename(finalPath, backupPath)) {
      SdMan.remove(tmpPath);
      return StoreResult::StorageError;
    }
  }

  if (!SdMan.commitFile(tmpPath, finalPath)) {
    if (!SdMan.exists(finalPath) && SdMan.exists(backupPath)) {
      SdMan.rename(backupPath, finalPath);
    }
    SdMan.remove(tmpPath);
    return StoreResult::StorageError;
  }

  if (SdMan.exists(backupPath)) {
    SdMan.remove(backupPath);
  }

  savedId = targetId;
  return StoreResult::Ok;
}

StoreResult QrCodeStore::remove(uint8_t id) const {
  if (!isValidId(id)) return StoreResult::NotFound;
  char path[kPathBufferBytes] = {};
  char backupPath[kPathBufferBytes] = {};
  if (!makePath(id, false, path, sizeof(path)) || !makeBackupPath(id, backupPath, sizeof(backupPath))) {
    return StoreResult::StorageError;
  }
  const bool hasFinal = SdMan.exists(path);
  const bool hasBackup = SdMan.exists(backupPath);
  if (!hasFinal && !hasBackup) return StoreResult::NotFound;
  const bool finalOk = !hasFinal || SdMan.remove(path);
  const bool backupOk = !hasBackup || SdMan.remove(backupPath);
  return finalOk && backupOk ? StoreResult::Ok : StoreResult::StorageError;
}

}  // namespace papyrix::codes
