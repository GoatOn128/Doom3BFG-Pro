#include "engine/sys/sys_streaming_install.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>
#include <fstream>

namespace {
constexpr std::uint32_t kSpecificationVersion = 37964103U;

bool ReadByte(std::istream& stream, std::uint8_t& value) {
  char byte{};
  if (!stream.read(&byte, 1)) return false;
  value = static_cast<std::uint8_t>(byte);
  return true;
}

bool ReadBigInt(std::istream& stream, std::uint32_t& value) {
  std::uint8_t bytes[4]{};
  if (!stream.read(reinterpret_cast<char*>(bytes), sizeof(bytes))) return false;
  value = (static_cast<std::uint32_t>(bytes[0]) << 24U) |
          (static_cast<std::uint32_t>(bytes[1]) << 16U) |
          (static_cast<std::uint32_t>(bytes[2]) << 8U) | bytes[3];
  return true;
}
}

idCVar sys_installToHDD("sys_installToHDD", "1", CVAR_BOOL, "Read streaming-install specifications");
idCVar sys_showInstallProgress("sys_showInstallProgress", "0", CVAR_BOOL, "Show install progress");
idCVar sys_debugGameInstall("sys_debugGameInstall", "0", CVAR_BOOL, "Print game-install diagnostics");
idStreamingInstallManager streamingInstallMgrLocal;

bool idGameChunkToInstallerChunks::Read(const char* specificationFile,
                                        std::uint32_t languageMask) {
  chunkList.clear();
  if (specificationFile == nullptr) return false;
  std::ifstream file(specificationFile, std::ios::binary);
  if (!file) return false;
  std::uint32_t version = 0;
  std::uint32_t chunkCount = 0;
  if (!ReadBigInt(file, version) || version != kSpecificationVersion ||
      !ReadBigInt(file, chunkCount) || chunkCount > 65536U) return false;
  chunkList.resize(chunkCount);
  for (Chunks& chunk : chunkList) {
    std::uint8_t valid = 0;
    std::uint32_t installerChunkCount = 0;
    if (!ReadByte(file, valid) || !ReadBigInt(file, installerChunkCount) ||
        installerChunkCount > 65536U) {
      chunkList.clear();
      return false;
    }
    chunk.valid = valid != 0;
    chunk.chunks.resize(installerChunkCount);
    chunk.langMask.resize(installerChunkCount);
    for (std::uint32_t i = 0; i < installerChunkCount; ++i) {
      std::uint32_t value = 0;
      if (!ReadBigInt(file, value)) return false;
      chunk.chunks[i] = static_cast<int>(value);
    }
    for (std::uint32_t i = 0; i < installerChunkCount; ++i) {
      std::uint32_t value = 0;
      if (!ReadBigInt(file, value)) return false;
      chunk.langMask[i] = static_cast<int>(value);
    }
    for (std::size_t i = chunk.chunks.size(); i-- > 0;) {
      if ((languageMask & static_cast<std::uint32_t>(chunk.langMask[i])) == 0) {
        chunk.chunks.erase(chunk.chunks.begin() + static_cast<std::ptrdiff_t>(i));
        chunk.langMask.erase(chunk.langMask.begin() + static_cast<std::ptrdiff_t>(i));
      }
    }
  }
  return true;
}

idStreamingInstallManager::idStreamingInstallManager() = default;
idStreamingInstallManager::~idStreamingInstallManager() = default;

bool idStreamingInstallManager::IsValidChunk(int chunkId) const {
  return chunkId >= 0 && chunkId < static_cast<int>(gameToInstallerChunks.chunkList.size()) &&
         gameToInstallerChunks.chunkList[static_cast<std::size_t>(chunkId)].valid;
}

bool idStreamingInstallManager::IsChunkInstalled(int, bool) const { return true; }

float idStreamingInstallManager::GetTimeEstimateForChunk(int) const { return 0.0f; }

float idStreamingInstallManager::GetDownloadProgressForChunk(int) const {
  return initializied ? 1.0f : 0.0f;
}

int idStreamingInstallManager::GetChunkIdForResourceContainerIndex(int containerIndex) const {
  return hasSpecification && containerIndex >= 0 &&
                 containerIndex < static_cast<int>(resourceContainerIndexToGameChunk.size())
             ? resourceContainerIndexToGameChunk[static_cast<std::size_t>(containerIndex)]
             : 0;
}

void idStreamingInstallManager::PrioritizeChunksForMapLoad(int chunkId) {
  lastInstallerPriority.clear();
  if (!initializied || gameToInstallerChunks.chunkList.empty()) return;
  const int count = static_cast<int>(gameToInstallerChunks.chunkList.size());
  for (int offset = 0; offset < count; ++offset) {
    const int gameChunk = (chunkId + offset) % count;
    if (gameChunk == 0 || !IsValidChunk(gameChunk)) continue;
    const idGameChunkToInstallerChunks::Chunks& item =
        gameToInstallerChunks.chunkList[static_cast<std::size_t>(gameChunk)];
    lastInstallerPriority.insert(lastInstallerPriority.end(), item.chunks.begin(), item.chunks.end());
  }
}

void idStreamingInstallManager::Init() {
  initializied = true;
  hasSpecification = false;
  resourceContainerIndexToGameChunk.clear();
  if (!sys_installToHDD.GetBool()) return;
  constexpr std::uint32_t englishLanguageMask = 1U;
  hasSpecification = gameToInstallerChunks.Read("gamechunktoinstallchunk.gic", englishLanguageMask);
  if (!hasSpecification) return;
  for (int gameChunk = 0; gameChunk < static_cast<int>(gameToInstallerChunks.chunkList.size());
       ++gameChunk) {
    if (gameToInstallerChunks.chunkList[static_cast<std::size_t>(gameChunk)].valid)
      resourceContainerIndexToGameChunk.push_back(gameChunk);
  }
}
