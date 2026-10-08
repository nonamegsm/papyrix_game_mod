#include <SDCardManager.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "QrCodeStore.h"
#include "test_utils.h"

using papyrix::codes::MAX_QR_CODES;
using papyrix::codes::MAX_QR_DATA_BYTES;
using papyrix::codes::MAX_QR_NAME_BYTES;
using papyrix::codes::QrCodeEntry;
using papyrix::codes::QrCodeInfo;
using papyrix::codes::QrCodeStore;
using papyrix::codes::StoreResult;

namespace {

uint8_t code(StoreResult result) { return static_cast<uint8_t>(result); }

bool hasPath(const std::vector<std::string>& paths, const std::string& path) {
  return std::find(paths.begin(), paths.end(), path) != paths.end();
}

std::string qrPath(unsigned id) {
  char path[48] = {};
  std::snprintf(path, sizeof(path), "/.papyrix/apps/qrcodes/%02u.json", id);
  return path;
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("QrCodeStore");

  {
    SdMan.reset();
    QrCodeStore store;
    uint8_t savedId = 99;
    runner.expectEq(code(StoreResult::Ok), code(store.save(-1, "Boarding pass", "line one\nline two", savedId)),
                    "create accepts UTF-8 text with newline payload");
    runner.expectEq(uint8_t(0), savedId, "first create uses slot zero");

    QrCodeEntry entry = {};
    QrCodeStore rebootedStore;
    runner.expectEq(code(StoreResult::Ok), code(rebootedStore.load(savedId, entry)), "reboot-style reload succeeds");
    runner.expectEq(uint8_t(0), entry.id, "loaded id matches");
    runner.expectEqual("Boarding pass", entry.name, "loaded name matches");
    runner.expectEqual("line one\nline two", entry.data, "loaded data preserves newline bytes");
  }

  {
    SdMan.reset();
    QrCodeStore store;
    uint8_t savedId = 99;
    runner.expectEq(code(StoreResult::Ok), code(store.save(-1, "../escape", "payload", savedId)),
                    "name that looks like traversal is stored as data only");
    const auto paths = SdMan.writtenFilePaths();
    runner.expectTrue(hasPath(paths, qrPath(0)), "final path is numeric slot");
    runner.expectFalse(hasPath(paths, "/.papyrix/apps/qrcodes/../escape.json"), "name never becomes filename");
  }

  {
    SdMan.reset();
    QrCodeStore store;
    uint8_t savedId = 99;
    runner.expectEq(code(StoreResult::Ok), code(store.save(-1, "Atomic", "old", savedId)), "seed atomic record");
    SdMan.setSyncResult(false);
    runner.expectEq(code(StoreResult::StorageError), code(store.save(savedId, "Atomic", "new", savedId)),
                    "sync failure rejects update");
    SdMan.setSyncResult(true);

    QrCodeEntry entry = {};
    runner.expectEq(code(StoreResult::Ok), code(store.load(0, entry)), "record remains readable after sync failure");
    runner.expectEqual("old", entry.data, "sync failure retains previous published data");
    runner.expectFalse(SdMan.exists("/.papyrix/apps/qrcodes/00.json.tmp"), "sync failure removes temp file");

    SdMan.setRenameResult(false);
    runner.expectEq(code(StoreResult::StorageError), code(store.save(savedId, "Atomic", "rename fail", savedId)),
                    "overwrite publish failure rejects update");
    SdMan.setRenameResult(true);
    runner.expectEq(code(StoreResult::Ok), code(store.load(0, entry)), "record remains readable after publish failure");
    runner.expectEqual("old", entry.data, "publish failure retains previous data");
  }

  {
    SdMan.reset();
    QrCodeStore store;
    uint8_t savedId = 99;
    runner.expectEq(code(StoreResult::Ok), code(store.save(-1, "Backup", "before power loss", savedId)),
                    "seed backup fallback record");
    runner.expectTrue(SdMan.rename("/.papyrix/apps/qrcodes/00.json", "/.papyrix/apps/qrcodes/00.json.bak"),
                      "simulate interrupted replace after backup publish");
    QrCodeEntry entry = {};
    runner.expectEq(code(StoreResult::Ok), code(store.load(0, entry)), "load falls back to backup when final missing");
    runner.expectEqual("before power loss", entry.data, "backup fallback preserves old data");
    runner.expectEq(code(StoreResult::Ok), code(store.save(-1, "Next", "payload", savedId)),
                    "create succeeds when backup-only occupied slot exists");
    runner.expectEq(uint8_t(1), savedId, "create skips backup-only occupied slot");
  }

  {
    SdMan.reset();
    QrCodeStore store;
    uint8_t savedId = 99;
    runner.expectEq(code(StoreResult::Ok), code(store.save(-1, "Delete me", "payload", savedId)), "seed delete record");
    runner.expectEq(code(StoreResult::Ok), code(store.remove(savedId)), "delete existing record");
    QrCodeEntry entry = {};
    runner.expectEq(code(StoreResult::NotFound), code(store.load(savedId, entry)), "deleted record is missing");
    runner.expectEq(code(StoreResult::NotFound), code(store.remove(savedId)),
                    "delete missing record returns not found");
  }

  {
    SdMan.reset();
    QrCodeStore store;
    uint8_t savedId = 99;
    runner.expectEq(code(StoreResult::InvalidName), code(store.save(-1, "", "payload", savedId)),
                    "empty name rejected");
    runner.expectEq(code(StoreResult::InvalidName), code(store.save(-1, " \t", "payload", savedId)),
                    "blank name rejected");
    runner.expectEq(code(StoreResult::InvalidName), code(store.save(-1, "bad\nname", "payload", savedId)),
                    "control character in name rejected");
    runner.expectEq(code(StoreResult::InvalidName),
                    code(store.save(-1, std::string(MAX_QR_NAME_BYTES + 1, 'n').c_str(), "payload", savedId)),
                    "oversized name rejected");
    runner.expectEq(code(StoreResult::InvalidName),
                    code(store.save(-1, std::string("\xE2\x82", 2).c_str(), "payload", savedId)),
                    "invalid UTF-8 name rejected");

    runner.expectEq(code(StoreResult::InvalidData), code(store.save(-1, "Name", "", savedId)), "empty data rejected");
    runner.expectEq(code(StoreResult::InvalidData),
                    code(store.save(-1, "Name", std::string(MAX_QR_DATA_BYTES + 1, 'd').c_str(), savedId)),
                    "oversized data rejected");
    runner.expectEq(code(StoreResult::InvalidData),
                    code(store.save(-1, "Name", std::string("\xF4\x90\x80\x80", 4).c_str(), savedId)),
                    "invalid UTF-8 data rejected");
  }

  {
    SdMan.reset();
    QrCodeStore store;
    uint8_t savedId = 99;
    bool filled = true;
    for (int i = 0; i < static_cast<int>(MAX_QR_CODES); ++i) {
      const std::string name = "QR " + std::to_string(i);
      filled = filled && store.save(-1, name.c_str(), "payload", savedId) == StoreResult::Ok;
      runner.expectEq(static_cast<uint8_t>(i), savedId, "create fills slots in ascending order");
    }
    runner.expectTrue(filled, "all QR slots can be filled");
    runner.expectEq(code(StoreResult::Full), code(store.save(-1, "Overflow", "payload", savedId)),
                    "create past capacity returns full");
    runner.expectEq(code(StoreResult::NotFound), code(store.save(16, "Bad id", "payload", savedId)),
                    "out-of-range explicit id rejected");
  }

  {
    SdMan.reset();
    QrCodeStore store;
    uint8_t savedId = 99;
    SdMan.setWriteLimit(4);
    runner.expectEq(code(StoreResult::StorageError), code(store.save(-1, "Storage", "payload", savedId)),
                    "short write reports storage error");
    runner.expectFalse(SdMan.exists("/.papyrix/apps/qrcodes/00.json.tmp"), "short write removes temp file");
  }

  {
    SdMan.reset();
    SdMan.registerFile(qrPath(3), "{bad json");
    QrCodeStore store;
    QrCodeInfo infos[MAX_QR_CODES] = {};
    size_t count = 99;
    runner.expectEq(code(StoreResult::Ok), code(store.list(infos, MAX_QR_CODES, count)),
                    "list skips malformed record without failing");
    runner.expectEq(size_t(0), count, "malformed record is not listed");
    QrCodeEntry entry = {};
    runner.expectEq(code(StoreResult::StorageError), code(store.load(3, entry)),
                    "direct load reports malformed record");
    SdMan.registerFile(qrPath(4), "{\"id\":4,\"name\":\"Bad\\u0000Name\",\"data\":\"payload\"}");
    runner.expectEq(code(StoreResult::StorageError), code(store.load(4, entry)),
                    "direct load rejects stored NUL in name");
  }

  return runner.allPassed() ? 0 : 1;
}
