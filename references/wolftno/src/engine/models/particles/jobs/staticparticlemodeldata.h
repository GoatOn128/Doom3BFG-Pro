#pragma once

#include "engine/decls/decl.h"
#include "shared/idlib/geometry/drawvert.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idStaticParticleModelData : public idResource {
public:
  idStaticParticleModelData();
  ~idStaticParticleModelData() override;

  void LoadResource() override;
  bool ReloadIfStale() override;
  idResourceList* GetResourceList() const override;

  static idTypedResourceList<idStaticParticleModelData> resourceList;

  std::uint32_t timestamp;
  std::uint32_t alignment52{};
  idDrawVert* staticVerts;
  int numStaticVerts;
  std::uint32_t tailPad{};

private:
  friend bool ValidateStaticParticleLoadBinary(idStaticParticleModelData&,
                                               const char*);
  friend bool ValidateStaticParticleWriteBinary(idStaticParticleModelData&,
                                                const char*);
  friend bool ValidateStaticParticleGenerate(idStaticParticleModelData&);
  bool LoadBinary(const char* fileName);
  bool WriteBinary(const char* fileName);
  bool Generate();
  void FreeData() {
    timestamp = static_cast<std::uint32_t>(-1);
    idMem::Singleton()->Free(staticVerts, ALIGN_16);
    staticVerts = nullptr;
    numStaticVerts = 0;
  }
};

template <>
idStaticParticleModelData*
idTypedResourceList<idStaticParticleModelData>::Alloc(
    const char* resourceName) const;

static_assert(sizeof(idStaticParticleModelData) == 72,
              "idStaticParticleModelData ABI drift");
static_assert(offsetof(idStaticParticleModelData, timestamp) == 48,
              "static particle timestamp offset drift");
static_assert(offsetof(idStaticParticleModelData, staticVerts) == 56,
              "static particle verts offset drift");
static_assert(offsetof(idStaticParticleModelData, numStaticVerts) == 64,
              "static particle count offset drift");
