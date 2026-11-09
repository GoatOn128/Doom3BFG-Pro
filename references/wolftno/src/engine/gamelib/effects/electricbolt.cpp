#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#define WOLFTNO_RETAIL_STATICLIST_FORCEINLINE_CTOR_ABI
#define WOLFTNO_RETAIL_BEAMNODE_TRIVIAL_DEFAULT_ABI
#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/gamelib/effects/electricbolt.h"

#include "engine/decls/declsimple.h"
#include "engine/decls/decltable.h"
#include "engine/models/renderworld.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/mathlib.h"
#pragma warning(pop)

#undef WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#undef WOLFTNO_RETAIL_STATICLIST_FORCEINLINE_CTOR_ABI
#undef WOLFTNO_RETAIL_BEAMNODE_TRIVIAL_DEFAULT_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#undef WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <cmath>
#include <cstdint>
#include <xmmintrin.h>

class idEntity;
class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

idRenderDebug* GetGameLibRenderDebug();

template <typename Type>
void* idListArrayNew(int count, memTag_t tag, bool zeroBuffer);

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

namespace {

struct idRotationView {
  idVec3 origin;
  idVec3 vec;
  float angle;
  idMat3 axis;
  bool axisValid;
};

static_assert(sizeof(idRotationView) == sizeof(idRotation),
              "electric-bolt rotation view ABI drift");

}  // namespace

static_assert(sizeof(idDeclElectricBolt) == 0x100,
              "electric-bolt declaration ABI drift");
static_assert(offsetof(idDeclElectricBolt, material) == 0x60,
              "electric-bolt material offset drift");
static_assert(offsetof(idDeclElectricBolt, maxDeviation) == 0x80,
              "electric-bolt deviation offset drift");
static_assert(offsetof(idDeclElectricBolt, branchProbability) == 0x9C,
              "electric-bolt branch probability offset drift");
static_assert(offsetof(idDeclElectricBolt, jitterTable) == 0xC0,
              "electric-bolt jitter table offset drift");
static_assert(offsetof(idDeclElectricBolt, useBranchOverride) == 0xE8,
              "electric-bolt branch-override flag offset drift");
static_assert(offsetof(idDeclElectricBolt, branchOverride) == 0xF0,
              "electric-bolt branch-override offset drift");

idCVar g_eboltDebug(
    "g_eboltDebug", "0", 2,
    "1=display wireframe brightness, 2=display wireframe gradient, "
    "3=display wireframe bolt reveal",
    static_cast<argCompletionRef_t>(nullptr));

__forceinline float idVec3::NormalizeFast() {
  const float oldX = x;
  const float oldY = y;
  const float oldZ = z;
  const float lengthSquared =
      ((oldX * oldX) + (oldY * oldY)) + (oldZ * oldZ);
  const __m128 clamped = _mm_max_ps(
      _mm_set1_ps(lengthSquared), idMath::vector_float_smallestNonDenorm);
  const float inverseLength =
      1.0f / _mm_sqrt_ps(clamped).m128_f32[0];
  x *= inverseLength;
  y = oldY * inverseLength;
  z = oldZ * inverseLength;
  return inverseLength * lengthSquared;
}

template <>
idList<eboltControlNode_t, 5>&
idList<eboltControlNode_t, 5>::operator=(
    const idList<eboltControlNode_t, 5>& other) {
  if (this == &other) {
    return *this;
  }

  ClearFree();
  if (listStatic != 0) {
    num = other.num < size ? other.num : size;
  } else {
    num = other.num;
    size = other.size;
    granularity = other.granularity;
    memTag = other.memTag;
    if (size > 0) {
      list = static_cast<eboltControlNode_t*>(
          idListArrayNew<eboltControlNode_t>(
              size, static_cast<memTag_t>(memTag), false));
    }
  }

  for (int index = 0; index < num; ++index) {
    list[index].startPos = other.list[index].startPos;
    list[index].endPos = other.list[index].endPos;
  }
  return *this;
}

template <>
__declspec(noinline) bool
idList<eboltControlNode_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearFree();
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }
  list = static_cast<eboltControlNode_t*>(
      idListArrayResize<eboltControlNode_t>(
          list, size, newSize, static_cast<memTag_t>(memTag), false));
  if (list == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

template <>
idStaticList<eboltControlNode_t, 32>&
idStaticList<eboltControlNode_t, 32>::operator=(
    const idStaticList<eboltControlNode_t, 32>& other) {
  static_cast<idList<eboltControlNode_t, 5>&>(*this) =
      static_cast<const idList<eboltControlNode_t, 5>&>(other);
  for (int index = 0; index < 32; ++index) {
    staticList[index] = other.staticList[index];
  }
  return *this;
}

template <>
__declspec(noinline) void* idListArrayResize<segment_t>(
    void* oldPointer, int oldCount, int newCount, memTag_t tag,
    bool zeroBuffer) {
  auto* oldSegments = static_cast<segment_t*>(oldPointer);
  segment_t* replacement = nullptr;
  if (newCount > 0) {
    replacement = static_cast<segment_t*>(
        idMem::Singleton().AllocWithLocation(
            zeroBuffer
                ? "w:\\build\\tech5\\shared\\idlib\\containers/List.h(27) : "
                  "tag"
                : "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : "
                  "tag",
            sizeof(segment_t) * static_cast<std::size_t>(newCount), tag,
            zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP));
    if (replacement == nullptr) {
      return nullptr;
    }
    const int copyCount = oldCount < newCount ? oldCount : newCount;
    for (int index = 0; index < copyCount; ++index) {
      replacement[index] = oldSegments[index];
    }
  }
  idMem::Singleton().Free(oldSegments, ALIGN_16);
  return replacement;
}

template <>
__declspec(noinline) bool idList<segment_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearFree();
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }
  list = static_cast<segment_t*>(idListArrayResize<segment_t>(
      list, size, newSize, static_cast<memTag_t>(memTag), false));
  if (list == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

template <>
__declspec(noinline) bool
idList<beamNodeParms_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearFree();
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }

  auto* replacement = static_cast<beamNodeParms_t*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
          sizeof(beamNodeParms_t) * static_cast<std::size_t>(newSize),
          static_cast<memTag_t>(memTag), false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  if (replacement != nullptr) {
    const int copyCount = size < newSize ? size : newSize;
    for (int index = 0; index < copyCount; ++index) {
      replacement[index] = list[index];
    }
    idMem::Singleton().Free(list, ALIGN_16);
  }
  list = replacement;
  if (list == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

template <>
__forceinline idList<segment_t, 5>::AllocResult
idList<segment_t, 5>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      newSize = required + granularity - 1 -
                ((required + granularity - 1) % granularity);
    } else {
      for (newSize = 1; newSize < required; newSize *= 2) {
      }
    }
    Resize(newSize);
  }
  if (num >= size) {
    return list[size - 1];
  }
  return list[num++];
}

template <>
__forceinline idList<beamNodeParms_t, 5>::AllocResult
idList<beamNodeParms_t, 5>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      newSize = required + granularity - 1 -
                ((required + granularity - 1) % granularity);
    } else {
      for (newSize = 1; newSize < required; newSize *= 2) {
      }
    }
    Resize(newSize);
  }
  if (num >= size) {
    return list[size - 1];
  }
  return list[num++];
}

idElectricBolt::idElectricBolt()
    : eboltDecl(nullptr), beamModel(nullptr), controlNodes(), startTime(0),
      diversity(0) {}

idElectricBolt::~idElectricBolt() {
  eboltDecl = nullptr;
  beamModel = nullptr;
}

void idElectricBolt::Init(idRenderModelBeam* newBeamModel,
                          const idDeclElectricBolt* newDeclaration) {
  beamModel = newBeamModel;
  eboltDecl = newDeclaration;
}

void idElectricBolt::UpdateControlNodes(
    idStaticList<eboltControlNode_t, MAX_CONTROL_NODES>& newControlNodes) {
  controlNodes = newControlNodes;
}

void idElectricBolt::StartElectricBolt(
    int newStartTime,
    idStaticList<eboltControlNode_t, MAX_CONTROL_NODES>& newControlNodes,
    float newDiversity) {
  startTime = newStartTime;
  diversity = static_cast<int>(newDiversity * 32767.0f);
  controlNodes = newControlNodes;
}

void idElectricBolt::StopElectricBolt() {
  controlNodes.SetNum(0);
}

static void SubdivideBolt_r(
    const idVec3& startPosition, const idVec3& endPosition,
    boltStats_t& stats, int diversity, int subdivisionLevel,
    int branchLevel, idStaticList<segment_t, 128>& segments,
    idRenderModelBeam* beamModel,
    const idDeclElectricBolt* electricBoltDecl, int numberOfSubdivisions,
    int time, idRandom2 random, float deviation) {
  idVec3 midpoint{
      (endPosition.x + startPosition.x) * 0.5f,
      (endPosition.y + startPosition.y) * 0.5f,
      (endPosition.z + startPosition.z) * 0.5f};
  idVec3 forward{endPosition.x - startPosition.x,
                 endPosition.y - startPosition.y,
                 endPosition.z - startPosition.z};
  forward.NormalizeFast();
  const idMat3 axis = forward.ToMat3();

  float jitterUp = 0.0f;
  float jitterLeft = 0.0f;
  if (electricBoltDecl->jitterTable != nullptr) {
    const float deltaX = midpoint.x - stats.boltStartPos.x;
    const float deltaY = midpoint.y - stats.boltStartPos.y;
    const float deltaZ = midpoint.z - stats.boltStartPos.z;
    const float lengthFraction =
        (((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ)) *
        stats.invTotalLength;
    int elapsed = time - stats.boltStartTime;
    if (elapsed < 0) {
      elapsed = 0;
    }
    const float jitterLookupTime =
        (static_cast<float>(elapsed) * electricBoltDecl->jitterSpeed) +
        lengthFraction;
    float falloff = 1.0f;
    if (electricBoltDecl->jitterFalloffTable != nullptr) {
      falloff = electricBoltDecl->jitterFalloffTable->table->TableLookup(
          lengthFraction, true);
    }
    const float jitter =
        electricBoltDecl->jitterTable->table->TableLookup(jitterLookupTime,
                                                          true) *
        falloff;
    const std::uint32_t jitterSeed =
        1664525u * static_cast<std::uint32_t>(static_cast<int>(
                       static_cast<float>(static_cast<int>(random.seed)) *
                       lengthFraction)) +
        1013904223u;
    const float upMagnitude =
        (static_cast<float>((jitterSeed >> 10) & 0x7FFFu) *
         0.000030518509f) *
            (electricBoltDecl->jitterUpMag.y -
             electricBoltDecl->jitterUpMag.x) +
        electricBoltDecl->jitterUpMag.x;
    const std::uint32_t leftSeed =
        (1664525u * jitterSeed) + 1013904223u;
    const float leftMagnitude =
        (static_cast<float>((leftSeed >> 10) & 0x7FFFu) *
         0.000030518509f) *
            (electricBoltDecl->jitterLeftMag.y -
             electricBoltDecl->jitterLeftMag.x) +
        electricBoltDecl->jitterLeftMag.x;
    const float decay =
        ::expf(-(static_cast<float>(subdivisionLevel) *
                 electricBoltDecl->jitterDecay));
    jitterUp = (upMagnitude * decay) * jitter;
    jitterLeft = (leftMagnitude * decay) * jitter;
  }

  const std::uint32_t firstSeed =
      (1664525u * random.seed) + 1013904223u;
  const float leftOffset =
      (((static_cast<float>((firstSeed >> 10) & 0x7FFFu) *
         0.000030518509f) *
            2.0f -
        1.0f) *
       deviation) +
      jitterLeft;
  const std::uint32_t secondSeed =
      (1664525u * firstSeed) + 1013904223u;
  random.seed = secondSeed;
  const float upOffset =
      (((static_cast<float>((secondSeed >> 10) & 0x7FFFu) *
         0.000030518509f) *
            2.0f -
        1.0f) *
       deviation) +
      jitterUp;
  midpoint.x = (midpoint.x + (axis.mat[1].x * leftOffset)) +
               (axis.mat[2].x * upOffset);
  midpoint.y = (midpoint.y + (axis.mat[1].y * leftOffset)) +
               (axis.mat[2].y * upOffset);
  midpoint.z = (midpoint.z + (axis.mat[1].z * leftOffset)) +
               (axis.mat[2].z * upOffset);

  if (subdivisionLevel < numberOfSubdivisions) {
    SubdivideBolt_r(startPosition, midpoint, stats, diversity,
                    subdivisionLevel + 1, branchLevel, segments, beamModel,
                    electricBoltDecl, numberOfSubdivisions, time, random,
                    deviation * 0.5f);
    SubdivideBolt_r(midpoint, endPosition, stats, diversity,
                    subdivisionLevel + 1, branchLevel, segments, beamModel,
                    electricBoltDecl, numberOfSubdivisions, time, random,
                    deviation * 0.5f);
    return;
  }

  segment_t& firstSegment = segments.Alloc();
  firstSegment.startPos = startPosition;
  firstSegment.endPos = midpoint;
  float deltaX = startPosition.x - stats.boltStartPos.x;
  float deltaY = startPosition.y - stats.boltStartPos.y;
  float deltaZ = startPosition.z - stats.boltStartPos.z;
  firstSegment.lengthFrac =
      (((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ)) *
      stats.invTotalLength;
  ++stats.numNodes;

  segment_t& secondSegment = segments.Alloc();
  secondSegment.startPos = midpoint;
  secondSegment.endPos = endPosition;
  deltaX = midpoint.x - stats.boltStartPos.x;
  deltaY = midpoint.y - stats.boltStartPos.y;
  deltaZ = midpoint.z - stats.boltStartPos.z;
  secondSegment.lengthFrac =
      (((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ)) *
      stats.invTotalLength;
  ++stats.numNodes;

  std::uint32_t branchSeed = random.seed;
  for (int index = 0; index < stats.numNodes; ++index) {
    branchSeed = (1664525u * branchSeed) + 1013904223u;
  }
  branchSeed = (1664525u * branchSeed) + 1013904223u;
  const float branchRoll =
      static_cast<float>((branchSeed >> 10) & 0x7FFFu) * 0.000030518509f;
  if (!(electricBoltDecl->branchProbability > branchRoll &&
        branchLevel < electricBoltDecl->maxBranchLevels)) {
    return;
  }

  idVec3 direction{midpoint.x - startPosition.x,
                   midpoint.y - startPosition.y,
                   midpoint.z - startPosition.z};
  direction.NormalizeFast();
  idMat3 rotation = direction.ToMat3();
  const std::uint32_t angleSeed =
      (1664525u * branchSeed) + 1013904223u;
  const float branchAngle =
      (static_cast<float>((angleSeed >> 10) & 0x7FFFu) *
       0.000030518509f) *
      electricBoltDecl->maxBranchAngle;
  idRotationView perpendicularRotation;
  perpendicularRotation.origin = vec3_origin;
  perpendicularRotation.vec = rotation.mat[2];
  perpendicularRotation.angle = branchAngle;
  perpendicularRotation.axisValid = false;
  perpendicularRotation.axis = mat3_identity;
  rotation *= reinterpret_cast<const idRotation*>(&perpendicularRotation)
                  ->ToMat3();

  const std::uint32_t lengthSeed =
      (1664525u * angleSeed) + 1013904223u;
  const float branchLength =
      (static_cast<float>((lengthSeed >> 10) & 0x7FFFu) *
       0.000030518509f) *
          (electricBoltDecl->branchLength.y -
           electricBoltDecl->branchLength.x) +
      electricBoltDecl->branchLength.x;
  const idVec3 branchEnd{
      (branchLength * rotation.mat[0].x) + midpoint.x,
      midpoint.y + (branchLength * rotation.mat[0].y),
      midpoint.z + (rotation.mat[0].z * branchLength)};

  if (g_eboltDebug.valueInteger == 4) {
    GetGameLibRenderDebug()->DebugArrow(idColor::colorGreen, startPosition,
                                        midpoint, 4.0f, 0, false);
    GetGameLibRenderDebug()->DebugArrow(idColor::colorRed, startPosition,
                                        branchEnd, 4.0f, 0, false);
  }

  idStaticList<segment_t, 128> branchSegments;
  branchSegments.num = 1;
  branchSegments.staticList[0].startPos = startPosition;
  branchSegments.staticList[0].endPos = branchEnd;
  deltaX = startPosition.x - stats.boltStartPos.x;
  deltaY = startPosition.y - stats.boltStartPos.y;
  deltaZ = startPosition.z - stats.boltStartPos.z;
  branchSegments.staticList[0].lengthFrac =
      (((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ)) *
      stats.invTotalLength;
  const int branchCount = ++stats.numBranches;
  GenerateBolt(branchSegments, stats, beamModel, electricBoltDecl, time,
               diversity + branchCount, branchLevel + 1,
               ::powf(0.5f, static_cast<float>(branchLevel)) *
                   electricBoltDecl->maxBranchDeviation,
               electricBoltDecl->branchSubdivisions);
}

void GenerateBolt(idStaticList<segment_t, 128>& currentSegments,
                  boltStats_t& stats, idRenderModelBeam* beamModel,
                  const idDeclElectricBolt* electricBoltDecl,
                  int currentTime, int diversity, int branchLevel,
                  float maximumDeviation, int maximumSubdivisions) {
  const int randomValue =
      diversity ^
      static_cast<int>(((static_cast<unsigned int>(
                             static_cast<std::uint16_t>(stats.numBranches))
                         << 10) +
                        1024u) &
                       0x7FFFu);
  float brightnessScale = 1.0f;
  if (electricBoltDecl->brightnessTable != nullptr) {
    brightnessScale =
        electricBoltDecl->brightnessTable->table->TableLookup(
            static_cast<float>(currentTime - stats.boltStartTime) * 0.001f,
            true);
  }

  if (branchLevel > 0 && electricBoltDecl->useBranchOverride) {
    if (g_eboltDebug.valueInteger != 0) {
      idColor color = idColor::colorCyan;
      switch (g_eboltDebug.valueInteger) {
        case 1: {
          const float value =
              (::powf(0.5f, static_cast<float>(branchLevel)) *
               electricBoltDecl->brightness) *
              brightnessScale;
          color.r = value;
          color.g = value;
          color.b = value;
          color.a = value;
          break;
        }
        case 2: {
          const float rawFraction = currentSegments.list[0].lengthFrac;
          float fraction = 0.0f;
          if (rawFraction >= 0.0f) {
            fraction = rawFraction <= 1.0f ? rawFraction : 1.0f;
          }
          const float value = ::fmaxf(0.2f, 1.0f - fraction);
          color.r = value;
          color.g = value;
          color.b = value;
          color.a = value;
          break;
        }
        case 3: {
          const int revealTime = electricBoltDecl->revealTime;
          if (revealTime != 0 &&
              currentTime <= stats.boltStartTime + revealTime) {
            float value = 1.0f;
            if (((static_cast<float>(currentTime) -
                  static_cast<float>(stats.boltStartTime)) /
                 static_cast<float>(revealTime)) <=
                currentSegments.list[0].lengthFrac) {
              value = 0.0f;
            }
            color.r = value;
            color.g = value;
            color.b = value;
            color.a = value;
          } else {
            color = idColor::colorWhite;
          }
          break;
        }
        default:
          break;
      }
      GetGameLibRenderDebug()->DebugLine(
          color, currentSegments.list[0].startPos,
          currentSegments.list[0].endPos, 0, false);
    } else {
      const int frameCount = electricBoltDecl->branchOverride.frames;
      int frame = 0;
      if (frameCount != 0) {
        const std::uint32_t frameSeed =
            (1664525u * static_cast<std::uint32_t>(randomValue)) +
            1013904223u;
        frame = static_cast<int>((frameSeed >> 10) & 0x7FFFu) % frameCount;
      }
      float frameFraction = 1.0f;
      if (frameCount > 0) {
        frameFraction = 1.0f / static_cast<float>(frameCount);
      }
      const float textureStart =
          (static_cast<float>(frame) * frameFraction) -
          ::floorf(static_cast<float>(frame) * frameFraction);
      const float brightness =
          (::powf(0.5f, static_cast<float>(branchLevel)) *
           electricBoltDecl->brightness) *
          brightnessScale;
      const segment_t& segment = currentSegments.list[0];
      beamNodeParms_t node;
      node.startPos = segment.startPos;
      node.endPos = segment.endPos;
      float red = brightness * electricBoltDecl->color.x;
      float green = brightness * electricBoltDecl->color.y;
      float blue = brightness * electricBoltDecl->color.z;
      float alpha = brightness * electricBoltDecl->color.w;
      node.sMinMax = {0.0f, 1.0f};
      node.tMinMax = {textureStart, textureStart + frameFraction};
      node.halfWidth =
          static_cast<float>(electricBoltDecl->branchOverride.width) * 0.5f;
      node.orientVec = vec3_origin;

      if (electricBoltDecl->applyGradient) {
        const float rawFraction = segment.lengthFrac;
        float fraction = 0.0f;
        if (rawFraction >= 0.0f) {
          fraction = rawFraction <= 1.0f ? rawFraction : 1.0f;
        }
        const float gradient = ::fmaxf(0.2f, 1.0f - fraction);
        red *= gradient;
        green *= gradient;
        blue *= gradient;
        alpha *= gradient;
      }
      const int revealTime = electricBoltDecl->revealTime;
      if (revealTime != 0 &&
          currentTime <= stats.boltStartTime + revealTime) {
        alpha = ((static_cast<float>(currentTime) -
                  static_cast<float>(stats.boltStartTime)) /
                 static_cast<float>(revealTime)) <= segment.lengthFrac
                    ? 0.0f
                    : 1.0f;
      }

      int converted = static_cast<int>(red * 255.0f);
      std::uint8_t redByte;
      if (converted < 0) {
        redByte = 0;
      } else if (converted > 255) {
        redByte = 255;
      } else {
        redByte = static_cast<std::uint8_t>(converted);
      }
      converted = static_cast<int>(green * 255.0f);
      std::uint8_t greenByte;
      if (converted < 0) {
        greenByte = 0;
      } else if (converted > 255) {
        greenByte = 255;
      } else {
        greenByte = static_cast<std::uint8_t>(converted);
      }
      converted = static_cast<int>(blue * 255.0f);
      std::uint8_t blueByte;
      if (converted < 0) {
        blueByte = 0;
      } else if (converted > 255) {
        blueByte = 255;
      } else {
        blueByte = static_cast<std::uint8_t>(converted);
      }
      converted = static_cast<int>(alpha * 255.0f);
      std::uint8_t alphaByte;
      if (converted < 0) {
        alphaByte = 0;
      } else if (converted > 255) {
        alphaByte = 255;
      } else {
        alphaByte = static_cast<std::uint8_t>(converted);
      }
      node.color[0][0] = redByte;
      node.color[0][1] = greenByte;
      node.color[0][2] = blueByte;
      node.color[0][3] = alphaByte;
      node.color[1][0] = redByte;
      node.color[1][1] = greenByte;
      node.color[1][2] = blueByte;
      node.color[1][3] = alphaByte;
      *reinterpret_cast<std::uint64_t*>(&node.tangent[0][0]) = 0;
      *reinterpret_cast<std::uint64_t*>(&node.tangent[2][0]) = 0;
      beamModel->DrawBeam(node, electricBoltDecl->branchOverride.material,
                          BEAM_ORIENT_VIEWER);
    }
    return;
  }

  idStaticList<segment_t, 128> segments;
  segments.num = 0;
  for (int index = 0; index < currentSegments.num; ++index) {
    SubdivideBolt_r(
        currentSegments.list[index].startPos,
        currentSegments.list[index].endPos, stats, diversity, 0, branchLevel,
        segments, beamModel, electricBoltDecl, maximumSubdivisions,
        currentTime, idRandom2{static_cast<std::uint32_t>(randomValue)},
        maximumDeviation);
  }
  if (segments.num == 0) {
    return;
  }

  if (g_eboltDebug.valueInteger != 0) {
    for (int index = 0; index < segments.num; ++index) {
      idColor color = idColor::colorCyan;
      if (g_eboltDebug.valueInteger == 1) {
        const float value =
            ::powf(0.5f, static_cast<float>(branchLevel)) *
            electricBoltDecl->brightness;
        color.r = value;
        color.g = value;
        color.b = value;
        color.a = value;
      } else if (g_eboltDebug.valueInteger == 2) {
        const float rawFraction = segments.list[index].lengthFrac;
        float fraction = 0.0f;
        if (rawFraction >= 0.0f) {
          fraction = rawFraction <= 1.0f ? rawFraction : 1.0f;
        }
        const float value = ::fmaxf(0.2f, 1.0f - fraction);
        color.r = value;
        color.g = value;
        color.b = value;
        color.a = value;
      } else if (g_eboltDebug.valueInteger == 3) {
        const int revealTime = electricBoltDecl->revealTime;
        if (revealTime != 0 &&
            currentTime <= stats.boltStartTime + revealTime) {
          const float value =
              ((static_cast<float>(currentTime) -
                static_cast<float>(stats.boltStartTime)) /
               static_cast<float>(revealTime)) <=
                      segments.list[index].lengthFrac
                  ? 0.0f
                  : 1.0f;
          color.r = value;
          color.g = value;
          color.b = value;
          color.a = value;
        } else {
          color = idColor::colorWhite;
        }
      }
      GetGameLibRenderDebug()->DebugLine(
          color, segments.list[index].startPos, segments.list[index].endPos,
          0, false);
    }
  } else {
    idStaticList<beamNodeParms_t, 128> beamNodes;
    beamNodes.num = 0;
    const int segmentCount = segments.num;
    const float inverseSegmentCount =
        1.0f / static_cast<float>(segmentCount);
    const float branchScale =
        ::powf(0.5f, static_cast<float>(branchLevel));
    const float startWidth =
        branchLevel <= 0
            ? electricBoltDecl->startWidth
            : (branchScale * electricBoltDecl->branchStartWidth) * 2.0f;
    const float endWidth =
        branchLevel <= 0
            ? electricBoltDecl->endWidth
            : (branchScale * electricBoltDecl->branchEndWidth) * 2.0f;
    const float brightness =
        (branchScale * electricBoltDecl->brightness) * brightnessScale;
    const float widthDelta = endWidth - startWidth;

    for (int index = 0; index < segmentCount; ++index) {
      const segment_t& segment = segments.list[index];
      beamNodeParms_t& node = beamNodes.Alloc();
      node.startPos = segment.startPos;
      node.endPos = segment.endPos;
      float red = brightness * electricBoltDecl->color.x;
      float green = brightness * electricBoltDecl->color.y;
      float blue = brightness * electricBoltDecl->color.z;
      float alpha = brightness * electricBoltDecl->color.w;
      if (electricBoltDecl->applyGradient) {
        const float rawFraction = segment.lengthFrac;
        float fraction = 0.0f;
        if (rawFraction >= 0.0f) {
          fraction = rawFraction <= 1.0f ? rawFraction : 1.0f;
        }
        const float gradient = ::fmaxf(0.2f, 1.0f - fraction);
        red *= gradient;
        green *= gradient;
        blue *= gradient;
        alpha *= gradient;
      }
      const int revealTime = electricBoltDecl->revealTime;
      if (revealTime != 0 &&
          currentTime <= stats.boltStartTime + revealTime) {
        alpha = ((static_cast<float>(currentTime) -
                  static_cast<float>(stats.boltStartTime)) /
                 static_cast<float>(revealTime)) <= segment.lengthFrac
                    ? 0.0f
                    : 1.0f;
      }

      int converted = static_cast<int>(red * 255.0f);
      std::uint8_t redByte;
      if (converted < 0) {
        redByte = 0;
      } else if (converted > 255) {
        redByte = 255;
      } else {
        redByte = static_cast<std::uint8_t>(converted);
      }
      converted = static_cast<int>(green * 255.0f);
      std::uint8_t greenByte;
      if (converted < 0) {
        greenByte = 0;
      } else if (converted > 255) {
        greenByte = 255;
      } else {
        greenByte = static_cast<std::uint8_t>(converted);
      }
      converted = static_cast<int>(blue * 255.0f);
      std::uint8_t blueByte;
      if (converted < 0) {
        blueByte = 0;
      } else if (converted > 255) {
        blueByte = 255;
      } else {
        blueByte = static_cast<std::uint8_t>(converted);
      }
      converted = static_cast<int>(alpha * 255.0f);
      std::uint8_t alphaByte;
      if (converted < 0) {
        alphaByte = 0;
      } else if (converted > 255) {
        alphaByte = 255;
      } else {
        alphaByte = static_cast<std::uint8_t>(converted);
      }
      node.color[0][0] = redByte;
      node.color[0][1] = greenByte;
      node.color[0][2] = blueByte;
      node.color[0][3] = alphaByte;
      node.color[1][0] = redByte;
      node.color[1][1] = greenByte;
      node.color[1][2] = blueByte;
      node.color[1][3] = alphaByte;
      node.sMinMax.x = static_cast<float>(index) * inverseSegmentCount;
      node.sMinMax.y =
          (static_cast<float>(index) * inverseSegmentCount) +
          inverseSegmentCount;
      node.tMinMax = {0.0f, 1.0f};
      node.halfWidth =
          ((widthDelta *
            (static_cast<float>(index) /
             (static_cast<float>(segmentCount) - 1.0f))) +
           startWidth) *
          0.5f;
      node.orientVec = vec3_origin;
      *reinterpret_cast<std::uint64_t*>(&node.tangent[0][0]) = 0;
      *reinterpret_cast<std::uint64_t*>(&node.tangent[2][0]) = 0;
    }
    beamModel->DrawBeam(beamNodes.list, beamNodes.num,
                        electricBoltDecl->material, BEAM_ORIENT_VIEWER);
  }
}

void idElectricBolt::Update(int currentTime) {
  if (beamModel == nullptr || eboltDecl == nullptr || controlNodes.num == 0) {
    return;
  }

  idStaticList<segment_t, 128> segments;
  segments.num = 0;
  for (int index = 0; index < controlNodes.num; ++index) {
    segment_t& segment = segments.Alloc();
    segment.startPos = controlNodes.list[index].startPos;
    segment.endPos = controlNodes.list[index].endPos;
    segment.lengthFrac = 1.0f;
  }

  const eboltControlNode_t* const nodes = controlNodes.list;
  const int finalIndex = controlNodes.num - 1;
  const float deltaX = nodes[finalIndex].endPos.x - nodes[0].startPos.x;
  const float deltaY = nodes[finalIndex].endPos.y - nodes[0].startPos.y;
  const float deltaZ = nodes[finalIndex].endPos.z - nodes[0].startPos.z;
  boltStats_t stats;
  stats.numNodes = 0;
  stats.numBranches = 0;
  stats.invTotalLength =
      1.0f /
      (((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ));
  stats.boltStartPos = nodes[0].startPos;
  stats.boltStartTime = startTime;
  GenerateBolt(segments, stats, beamModel, eboltDecl, currentTime, diversity,
               0, eboltDecl->maxDeviation, eboltDecl->subdivisions);
}
