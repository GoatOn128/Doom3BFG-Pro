#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class idAAS2;
class idCmdArgs;

struct idAASVisBitSet {
  explicit idAASVisBitSet(std::size_t bitCount = 0);

  idAASVisBitSet(const idAASVisBitSet&) = delete;
  idAASVisBitSet& operator=(const idAASVisBitSet&) = delete;

  std::size_t ByteCount() const { return (bits + 7u) / 8u; }

  std::size_t bits{0};
  std::uint8_t* buffer{nullptr};
  bool free{false};
};

struct idAASVisArea {
  explicit idAASVisArea(std::size_t bitCount = 0) : cellVis(bitCount) {}

  idAASVisBitSet cellVis;
};

class idAASVisPoint {
public:
  void Free();
  idAASVisArea* AddArea(std::size_t bitCount);
  std::size_t NumAreas() const { return visAreas.size(); }

private:
  friend class idAASVisCache;
  std::vector<std::unique_ptr<idAASVisArea>> visAreas;
};

class idAASVisCache {
public:
  idAASVisCache();
  ~idAASVisCache();

  void Init(const idAAS2* newAAS, int newSubDivisionSize,
            float newEyeHeight);
  std::uint64_t Size() const;
  static void Cmd_VisCacheInfo_f(const idCmdArgs* args);
  void Free();
  void Shutdown();

  idAASVisPoint* AddPoint();
  std::size_t NumPoints() const { return visPoints.size(); }
  const idAAS2* GetAAS() const { return aas; }
  int GetSubDivisionSize() const { return subDivisionSize; }
  float GetEyeHeight() const { return eyeHeight; }

private:
  const idAAS2* aas{nullptr};
  float eyeHeight{0.0F};
  int subDivisionSize{0};
  std::vector<std::unique_ptr<idAASVisPoint>> visPoints;
};
