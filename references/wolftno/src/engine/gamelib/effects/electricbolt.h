#pragma once

#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/models/rendermodelbeam.h"
#pragma warning(pop)
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/math/random.h"

#include <cstddef>

class idCVar;
class idDeclElectricBolt;

extern idCVar g_eboltDebug;
extern bool forceIdListsToReallocateEveryAppend;

struct eboltControlNode_t {
  __forceinline eboltControlNode_t() noexcept
      : startPos{0.0f, 0.0f, 0.0f}, endPos{0.0f, 0.0f, 0.0f} {}

  idVec3 startPos;
  idVec3 endPos;
};

struct segment_t {
  using idListAllocReturnsReferenceMarker = void;

  idVec3 startPos;
  idVec3 endPos;
  float lengthFrac;
};

struct boltStats_t {
  int numNodes;
  int numBranches;
  float invTotalLength;
  idVec3 boltStartPos;
  int boltStartTime;
};

template <>
idList<eboltControlNode_t, 5>&
idList<eboltControlNode_t, 5>::operator=(
    const idList<eboltControlNode_t, 5>& other);
template <>
bool idList<eboltControlNode_t, 5>::Resize(int newSize);
template <>
idStaticList<eboltControlNode_t, 32>&
idStaticList<eboltControlNode_t, 32>::operator=(
    const idStaticList<eboltControlNode_t, 32>& other);

template <>
bool idList<segment_t, 5>::Resize(int newSize);
template <>
idList<segment_t, 5>::AllocResult idList<segment_t, 5>::Alloc();
template <>
bool idList<beamNodeParms_t, 5>::Resize(int newSize);
template <>
idList<beamNodeParms_t, 5>::AllocResult
idList<beamNodeParms_t, 5>::Alloc();

class idElectricBolt {
public:
  static constexpr int MAX_CONTROL_NODES = 32;

  idElectricBolt();
  ~idElectricBolt();

  void Init(idRenderModelBeam* beamModel,
            const idDeclElectricBolt* electricBoltDecl);
  void StartElectricBolt(
      int startTime,
      idStaticList<eboltControlNode_t, MAX_CONTROL_NODES>& controlNodes,
      float diversity);
  void StopElectricBolt();
  void UpdateControlNodes(
      idStaticList<eboltControlNode_t, MAX_CONTROL_NODES>& controlNodes);
  void Update(int currentTime);

  const idDeclElectricBolt* eboltDecl;
  idRenderModelBeam* beamModel;
  idStaticList<eboltControlNode_t, MAX_CONTROL_NODES> controlNodes;
  int startTime;
  int diversity;
};

void GenerateBolt(idStaticList<segment_t, 128>& currentSegments,
                  boltStats_t& stats, idRenderModelBeam* beamModel,
                  const idDeclElectricBolt* electricBoltDecl,
                  int currentTime, int diversity, int branchLevel,
                  float maximumDeviation, int maximumSubdivisions);

static_assert(sizeof(eboltControlNode_t) == 0x18,
              "electric-bolt control-node ABI drift");
static_assert(sizeof(segment_t) == 0x1C,
              "electric-bolt segment ABI drift");
static_assert(offsetof(segment_t, lengthFrac) == 0x18,
              "electric-bolt segment fraction offset drift");
static_assert(sizeof(boltStats_t) == 0x1C,
              "electric-bolt statistics ABI drift");
static_assert(offsetof(boltStats_t, invTotalLength) == 0x08,
              "electric-bolt inverse-length offset drift");
static_assert(offsetof(boltStats_t, boltStartPos) == 0x0C,
              "electric-bolt start-position offset drift");
static_assert(sizeof(idStaticList<eboltControlNode_t, 32>) == 0x318,
              "electric-bolt control-list ABI drift");
static_assert(sizeof(idStaticList<segment_t, 128>) == 0xE18,
              "electric-bolt segment-list ABI drift");
static_assert(sizeof(idStaticList<beamNodeParms_t, 128>) == 0x3418,
              "electric-bolt beam-node-list ABI drift");
static_assert(offsetof(idElectricBolt, controlNodes) == 0x10,
              "electric-bolt control-list offset drift");
static_assert(offsetof(idElectricBolt, startTime) == 0x328,
              "electric-bolt start-time offset drift");
static_assert(offsetof(idElectricBolt, diversity) == 0x32C,
              "electric-bolt diversity offset drift");
static_assert(sizeof(idElectricBolt) == 0x330,
              "electric-bolt object ABI drift");
