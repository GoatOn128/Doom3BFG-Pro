#pragma once

#include "engine/decls/decl.h"
#include "engine/models/rendermodel.h"
#include "shared/idlib/bv/sphere.h"

#include <cstddef>
#include <cstdint>

class idCollisionModel;
class idGlassModelGenerator;
class idParticleMeshDistribution;

struct fractureEffectInfo_t {
  int type{};
  int content{};
  int flags{};
  idVec4 colorType{};
  idVec4 colorContent{};
};

class idGlassModelSurface : public idRenderModelSurface {
public:
  fractureEffectInfo_t GetEffectInfo() const;

  idSphere boundSphere{};
  int declIndex{};
};

class idGlassModel : public idResource {
public:
  idGlassModel();
  ~idGlassModel() override;

  const idGlassModelSurface& ConstSurface(int index) const;
  const char* GetDecl(int index) const;
  int GetSurfaceDeclIndex(int index) const;
  idCollisionModel* GetCollisionModel(int index) const;
  const idParticleMeshDistribution*
  GetParticleMeshDistribution(int index) const;
  fractureEffectInfo_t GetEffectInfo(int index) const;

  idTypedResourceList<idGlassModel>* GetResourceList() const override;
  bool ReloadIfStale() override;
  void LoadResource() override;

  idList<idGlassModelSurface, 5> surfaces;
  idList<idStr, 5> decls;
  idStr collisionBaseName;
  int binaryTimestamp{-1};
  int sourceTimestamp{-1};

  static idTypedResourceList<idGlassModel> resourceList;

private:
  static idStr GetBinaryFileName(const char* name);
  bool LoadBinary(const char* filename);

  friend class GlassModelValidationAccess;
};

template <>
idGlassModel*
idTypedResourceList<idGlassModel>::Alloc(const char* name) const;

static_assert(sizeof(fractureEffectInfo_t) == 44,
              "fracture effect info ABI drift");
static_assert(offsetof(fractureEffectInfo_t, type) == 0,
              "fracture type offset drift");
static_assert(offsetof(fractureEffectInfo_t, colorType) == 0x0c,
              "fracture type-color offset drift");
static_assert(offsetof(fractureEffectInfo_t, colorContent) == 0x1c,
              "fracture content-color offset drift");
static_assert(sizeof(idGlassModelSurface) == 0x80,
              "glass model surface ABI drift");
static_assert(offsetof(idGlassModelSurface, boundSphere) == 0x68,
              "glass surface sphere offset drift");
static_assert(offsetof(idGlassModelSurface, declIndex) == 0x78,
              "glass surface decl offset drift");
static_assert(sizeof(idGlassModel) == 0x90, "glass model ABI drift");
static_assert(offsetof(idGlassModel, surfaces) == 0x30,
              "glass surfaces offset drift");
static_assert(offsetof(idGlassModel, decls) == 0x48,
              "glass decls offset drift");
static_assert(offsetof(idGlassModel, collisionBaseName) == 0x60,
              "glass collision-name offset drift");
static_assert(offsetof(idGlassModel, binaryTimestamp) == 0x88,
              "glass binary timestamp offset drift");
static_assert(offsetof(idGlassModel, sourceTimestamp) == 0x8c,
              "glass source timestamp offset drift");
