#include "engine/sound/streamcache.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {
std::mutex pinnedMutex;
struct pinnedStorage_t {
  std::vector<std::unique_ptr<unsigned char[]>> lines;
  std::unique_ptr<idFile> ownedFile;
};
std::unordered_map<pinnedCacheLines_t*, pinnedStorage_t> pinnedStorage;
}

int SoundReadCachedData(idFile* file, std::int64_t offset, int length, void* buffer,
                        bool, int, int) {
  if (file == nullptr || offset < 0 || length < 0) return 0;
  if (buffer == nullptr) {
    const std::int64_t available = std::max<std::int64_t>(0, file->Length() - offset);
    return static_cast<int>(std::min<std::int64_t>(available, length));
  }
  return static_cast<int>(file->ReadOfs(offset, buffer, static_cast<std::size_t>(length)));
}

bool SoundPinCachedData(idFile* file, std::int64_t offset, int length,
                        pinnedCacheLines_t* lines, bool persistence, int, int granularity) {
  if (file == nullptr || lines == nullptr || offset < 0 || length < 0 || granularity <= 0)
    return false;
  SoundUnpinCachedData(lines);
  lines->cacheFile = file;
  lines->ofs = offset;
  lines->length = length;
  lines->extra = persistence ? 1 : 0;
  pinnedStorage_t storage;
  constexpr int cacheLineSize = 0x10000;
  const std::int64_t available = std::max<std::int64_t>(0, file->Length() - offset);
  int remaining = static_cast<int>(std::min<std::int64_t>(available, length));
  const int readableLength = remaining;
  std::int64_t cursor = offset;
  while (remaining > 0 && lines->numLines < 24) {
    const int wanted = std::min(remaining, cacheLineSize);
    auto data = std::make_unique<unsigned char[]>(static_cast<std::size_t>(wanted));
    const int read = static_cast<int>(file->ReadOfs(cursor, data.get(), wanted));
    if (read <= 0) break;
    lines->lineData[lines->numLines] = data.get();
    lines->lineLength[lines->numLines] = read;
    ++lines->numLines;
    storage.lines.push_back(std::move(data));
    cursor += read;
    remaining -= read;
    if (read != wanted) break;
  }
  if (remaining != 0 || (length != 0 && readableLength == 0)) {
    SoundUnpinCachedData(lines);
    return false;
  }
  std::lock_guard<std::mutex> lock(pinnedMutex);
  pinnedStorage.emplace(lines, std::move(storage));
  return true;
}

void SoundUnpinCachedData(pinnedCacheLines_t* lines) {
  if (lines == nullptr) return;
  {
    std::lock_guard<std::mutex> lock(pinnedMutex);
    pinnedStorage.erase(lines);
  }
  lines->cacheFile = nullptr;
  lines->ofs = 0;
  lines->length = 0;
  lines->numLines = 0;
  lines->extra = 0;
  std::fill(std::begin(lines->lineData), std::end(lines->lineData), nullptr);
  std::fill(std::begin(lines->lineLength), std::end(lines->lineLength), 0);
}

void SoundAdoptPinnedFile(pinnedCacheLines_t* lines, idFile* file) {
  if (lines == nullptr || file == nullptr) return;
  std::lock_guard<std::mutex> lock(pinnedMutex);
  auto found = pinnedStorage.find(lines);
  if (found != pinnedStorage.end()) found->second.ownedFile.reset(file);
}
