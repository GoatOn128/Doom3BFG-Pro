#include "engine/sound/occlusion/soundrouting.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

constexpr int kMaxSoundBspAreas = 4096;
constexpr int kPositionOffset = 0;
constexpr int kDistanceOffset = 24576;
constexpr int kQueuedOffset = 40960;
constexpr int kQueueOffset = 45056;
constexpr int kRequiredTempSize = 61440;

std::int16_t ClampCoordinate(float value) {
  const int integer = static_cast<int>(value);
  return static_cast<std::int16_t>(std::clamp(integer, -32768, 32767));
}

int Distance(const std::int16_t* position, const sbspFace_t& face) {
  const float x = static_cast<float>(position[0] - face.center[0]);
  const float y = static_cast<float>(position[1] - face.center[1]);
  const float z = static_cast<float>(position[2] - face.center[2]);
  return static_cast<int>(std::nearbyint(std::sqrt(x * x + y * y + z * z)));
}

}  // namespace

void SoundRoutingJob(soundRoutingParms_t* parms) {
  std::memset(parms->route, 0, sizeof(int) * parms->numAreas);
  if (parms->listenerAreaNum == 0) {
    return;
  }
  if (parms->temp == nullptr || parms->tempSize < kRequiredTempSize ||
      parms->numAreas > kMaxSoundBspAreas) {
    return;
  }

  auto* positions = reinterpret_cast<std::int16_t*>(parms->temp + kPositionOffset);
  auto* distances = reinterpret_cast<std::uint32_t*>(parms->temp + kDistanceOffset);
  auto* queued = parms->temp + kQueuedOffset;
  auto* queue = reinterpret_cast<int*>(parms->temp + kQueueOffset);
  std::memset(distances, 0xFF, sizeof(std::uint32_t) * kMaxSoundBspAreas);
  std::memset(queued, 0, kMaxSoundBspAreas);

  const int listener = parms->listenerAreaNum;
  distances[listener] = 0;
  positions[listener * 3 + 0] = ClampCoordinate(parms->listenerOrigin.x);
  positions[listener * 3 + 1] = ClampCoordinate(parms->listenerOrigin.y);
  positions[listener * 3 + 2] = ClampCoordinate(parms->listenerOrigin.z);
  int readIndex = 0;
  int writeIndex = 1;
  queue[0] = listener;
  queued[listener] = 1;

  while (readIndex != writeIndex) {
    const int areaNumber = queue[readIndex++ & (kMaxSoundBspAreas - 1)];
    queued[areaNumber] = 0;
    const sbspArea_t& area = parms->flatBSP.areas[areaNumber];
    if (area.numFaces > 128) {
      continue;
    }

    for (unsigned int faceOffset = 0; faceOffset < area.numFaces; ++faceOffset) {
      const int signedFaceIndex =
          parms->flatBSP.faceIndex[area.firstFace + faceOffset];
      const sbspFace_t& face =
          parms->flatBSP.faces[std::abs(signedFaceIndex)];
      const unsigned int candidate =
          distances[areaNumber] +
          static_cast<unsigned int>(Distance(&positions[areaNumber * 3], face));
      const int neighboringArea =
          std::abs(face.faceAreas[signedFaceIndex >= 0 ? 1 : 0]);
      if (neighboringArea >= parms->numAreas || candidate >= distances[neighboringArea]) {
        continue;
      }

      distances[neighboringArea] = candidate;
      parms->route[neighboringArea] = -signedFaceIndex;
      positions[neighboringArea * 3 + 0] = face.center[0];
      positions[neighboringArea * 3 + 1] = face.center[1];
      positions[neighboringArea * 3 + 2] = face.center[2];
      if (queued[neighboringArea] == 0) {
        queue[writeIndex++ & (kMaxSoundBspAreas - 1)] = neighboringArea;
        queued[neighboringArea] = 1;
      }
    }
  }
}
