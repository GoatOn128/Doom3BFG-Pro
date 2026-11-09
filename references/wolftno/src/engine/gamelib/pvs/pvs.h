#pragma once

#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/math/plane.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

struct pvsHandle_t {
  pvsHandle_t() = default;
  __forceinline explicit pvsHandle_t(std::nullptr_t) {}

  int i;
  unsigned int h;
};

struct pvsCurrent_t {
  pvsHandle_t handle;
  unsigned char* pvs;
};

struct pvsNode_t {
  unsigned int planeNum;
  short children[2];
};

struct pvsPortal_t {
  int firstVertex;
  int numVertices;
};

struct pvsArea_t {
  int firstPortal;
  int numPortals;
};

class idPVS {
public:
  struct header_t {
    int magic;
    int numPlanes;
    int numVertices;
    int numNodes;
    int numPortals;
    int numAreas;
    int numPVSBytes;
  };

  idPVS();
  virtual ~idPVS() noexcept(false);

  static __forceinline void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }

  void SetName(const char* name) { pvsName = name; }
  void SetBare(bool value) { bare = value; }
  void LoadResource();
  int GetPVSArea(const idVec3& point) const;
  int GetPVSAreas(const idBounds& bounds, int* areas, int maxAreas) const;
  int NumAreas() const { return header.numAreas; }
  pvsHandle_t DecodePVS(const int* sourceAreas,
                        int numSourceAreas) const;
  void FreePVS(pvsHandle_t handle) const;
  bool InPVS(pvsHandle_t handle, const int* targetAreas,
             int numTargetAreas) const;

  header_t header;
  idStr pvsName;
  unsigned char* dataStart;
  idPlane* planes;
  idVec3* vertices;
  pvsNode_t* nodes;
  pvsPortal_t* portals;
  pvsArea_t* areas;
  int* areaOffsets;
  unsigned char* areaPVS;
  mutable pvsCurrent_t currentPVS[8];
  bool bare;
  std::uint8_t recoveredPad111[7];

private:
  friend struct idPVSTestAccess;

  __declspec(noinline) int Alloc(bool bareAllocation);
  __declspec(noinline) void Free();
  __declspec(noinline) void Default();
  __declspec(noinline) void SwapHeader();
  __declspec(noinline) void SwapData();
};

static_assert(sizeof(pvsHandle_t) == 0x08, "PVS handle ABI drift");
static_assert(sizeof(pvsCurrent_t) == 0x10, "current PVS ABI drift");
static_assert(sizeof(pvsNode_t) == 0x08, "PVS node ABI drift");
static_assert(sizeof(pvsPortal_t) == 0x08, "PVS portal ABI drift");
static_assert(sizeof(pvsArea_t) == 0x08, "PVS area ABI drift");
static_assert(sizeof(idPVS::header_t) == 0x1C, "PVS header ABI drift");
static_assert(sizeof(idPVS) == 0x118, "idPVS ABI drift");
static_assert(offsetof(idPVS, header) == 0x08,
              "idPVS header offset drift");
static_assert(offsetof(idPVS, pvsName) == 0x28,
              "idPVS name offset drift");
static_assert(offsetof(idPVS, dataStart) == 0x50,
              "idPVS data offset drift");
static_assert(offsetof(idPVS, currentPVS) == 0x90,
              "idPVS current slots offset drift");
static_assert(offsetof(idPVS, bare) == 0x110,
              "idPVS bare offset drift");
