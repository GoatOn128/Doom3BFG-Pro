#pragma once

#include <cstdint>
#include <vector>

class idGameChunkToInstallerChunks {
public:
  struct Chunks {
    bool valid{};
    std::vector<int> chunks;
    std::vector<int> langMask;
  };

  bool Read(const char* specificationFile, std::uint32_t languageMask);
  std::vector<Chunks> chunkList;
};

class idStreamingInstallManager {
public:
  idStreamingInstallManager();
  ~idStreamingInstallManager();
  void Init();
  bool IsChunkInstalled(int chunkId, bool fastStorage) const;
  float GetTimeEstimateForChunk(int chunkId) const;
  float GetDownloadProgressForChunk(int chunkId) const;
  int GetChunkIdForResourceContainerIndex(int containerIndex) const;
  void PrioritizeChunksForMapLoad(int chunkId);
  void Pump();

  bool IsInitialized() const { return initializied; }
  bool HasSpecification() const { return hasSpecification; }
  const std::vector<int>& LastInstallerPriority() const { return lastInstallerPriority; }

private:
  bool IsValidChunk(int chunkId) const;
  bool initializied{};
  bool hasSpecification{};
  std::vector<int> resourceContainerIndexToGameChunk;
  idGameChunkToInstallerChunks gameToInstallerChunks;
  std::vector<int> lastInstallerPriority;
};

extern idStreamingInstallManager streamingInstallMgrLocal;
extern idStreamingInstallManager* streamingInstallMgr;
