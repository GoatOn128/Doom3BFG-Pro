#pragma once

#include "engine/animation/md6.h"
#include "shared/idlib/containers/array.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idMD6Skel;

struct phaseTrackData_t {
  std::uint16_t totalSize{};
  std::uint16_t numFrames{};
  std::uint16_t ofsPhaseTable{};
  std::uint16_t ofsDistanceRemainingTable{};
  std::uint8_t pad[8]{};

  __forceinline const std::uint8_t* GetPhaseTable() const {
    return reinterpret_cast<const std::uint8_t*>(this) + ofsPhaseTable;
  }
  const float* GetDistanceRemainingTable() const {
    return ofsDistanceRemainingTable != 0
               ? reinterpret_cast<const float*>(
                     reinterpret_cast<const std::uint8_t*>(this) +
                     ofsDistanceRemainingTable)
               : nullptr;
  }
  __forceinline void Swap() {
    const auto swap16 = [](std::uint16_t value) {
      return static_cast<std::uint16_t>((value << 8U) | (value >> 8U));
    };
    totalSize = swap16(totalSize);
    numFrames = swap16(numFrames);
    ofsPhaseTable = swap16(ofsPhaseTable);
    ofsDistanceRemainingTable = swap16(ofsDistanceRemainingTable);
  }
};

using idMD6PhaseTrackSkelLookup = const idMD6Skel* (*)(const char* name,
                                                        bool load);
void SetMD6PhaseTrackSkelLookup(idMD6PhaseTrackSkelLookup lookup);

class idMD6PhaseTrack {
public:
  idMD6PhaseTrack();
  ~idMD6PhaseTrack();

  __forceinline void Free() {
    if (data != nullptr) {
      idMem::Singleton()->Free(data, ALIGN_16);
      data = nullptr;
    }
  }
  bool LoadBinary(idFile* file);
  bool WriteBinary(idFile* file);
  bool LoadText(const char* fileName, const idMD6Anim* anim);
  bool IsValid(const idMD6Anim* anim) const;
  bool HasDistanceRemainingTable() const {
    return data != nullptr && data->GetDistanceRemainingTable() != nullptr;
  }
  bool GetPhase(int frame, std::uint8_t& phase) const;
  bool GetFrame(std::uint8_t phase, std::int16_t& outFrame) const;
  bool GetRemainingTranslationDistance(int frame, float& distance) const {
    const float* table = data != nullptr ? data->GetDistanceRemainingTable()
                                         : nullptr;
    if (table == nullptr || frame < 0 || frame >= data->numFrames) {
      distance = 0.0f;
      return false;
    }
    distance = table[frame];
    return true;
  }
  std::uint16_t NumFrames() const {
    return data != nullptr ? data->numFrames : 0;
  }
  bool CreatePhaseTrack(const idMD6Skel* skel, const idMD6Anim* anim,
                        const idList<std::uint8_t, 5>& phaseTable,
                        const idList<float, 5>& distanceTable) {
    Free();
    data = CreatePhaseTrackData(skel, anim, phaseTable, distanceTable);
    return data != nullptr;
  }
  bool SetData(phaseTrackData_t* newData) {
    Free();
    data = newData;
    return data != nullptr;
  }

  phaseTrackData_t* data{};

private:
  phaseTrackData_t* CreatePhaseTrackData(
      const idMD6Skel* skel, const idMD6Anim* anim,
      const idList<std::uint8_t, 5>& phaseTable,
      const idList<float, 5>& distanceTable) const;
};

static_assert(sizeof(phaseTrackData_t) == 16,
              "phaseTrackData_t ABI drift");
static_assert(offsetof(phaseTrackData_t, numFrames) == 2,
              "phase-track frame-count offset drift");
static_assert(offsetof(phaseTrackData_t, ofsPhaseTable) == 4,
              "phase-track phase offset drift");
static_assert(offsetof(phaseTrackData_t, ofsDistanceRemainingTable) == 6,
              "phase-track distance offset drift");
static_assert(sizeof(idMD6PhaseTrack) == 8, "idMD6PhaseTrack ABI drift");
