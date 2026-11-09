#pragma once

#include "engine/decls/decl.h"
#include "engine/models/particles/jobs/particlegen.h"

#include <cstddef>
#include <cstdint>

class idParticleMeshDistribution : public idResource {
public:
  __forceinline idParticleMeshDistribution()
      : mesh(nullptr), meshTris(nullptr), binaryTimestamp(-1),
        recoveredPadding(0) {}
  ~idParticleMeshDistribution() override;

  idResourceList* GetResourceList() const override;
  bool ReloadIfStale() override;
  void LoadResource() override;

  particleMeshDist_t* mesh{};
  particleMeshDistTri_t* meshTris{};
  int binaryTimestamp{-1};
  std::uint32_t recoveredPadding{};

  static idTypedResourceList<idParticleMeshDistribution> resourceList;

private:
  void FreeData();
  bool LoadBinary(const char* filename);
};

template <>
idParticleMeshDistribution*
idTypedResourceList<idParticleMeshDistribution>::Alloc(const char* name) const;
template <>
int idTypedResourceList<idParticleMeshDistribution>::AllocSize() const;

static_assert(sizeof(idParticleMeshDistribution) == 72,
              "idParticleMeshDistribution ABI drift");
static_assert(offsetof(idParticleMeshDistribution, mesh) == 48,
              "particle distribution mesh offset drift");
static_assert(offsetof(idParticleMeshDistribution, binaryTimestamp) == 64,
              "particle distribution timestamp offset drift");
