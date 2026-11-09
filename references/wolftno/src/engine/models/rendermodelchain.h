#pragma once

#include "engine/models/rendermodel.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/math/curve.h"

#include <cstddef>
#include <cstdint>

class idMaterial;
class idStaticModel;

struct chainGenerateInfo_t {
  bool rotateRandom;
  bool smoothSkinning;
  std::uint8_t recoveredPadding[2]{};
  float startRotation;
};

using idRenderModelChainStaticLookup = idStaticModel* (*)(const char*, bool);
using idRenderModelChainMaterialLookup = const idMaterial* (*)(const char*);

void SetRenderModelChainStaticLookup(idRenderModelChainStaticLookup lookup);
void SetRenderModelChainMaterialLookup(idRenderModelChainMaterialLookup lookup);

class idRenderModelChain : public idRenderModel {
public:
  struct splinePoint_t {
    idVec3 value;
    float time;
  };

  idRenderModelChain();
  ~idRenderModelChain() override;

  int GetNumLinks() const;
  const idJointMat* GetLinkPosition(int id) const;
  void SetLinkPosition(int id, const idVec3& origin, const idMat3& axis);
  void SetLinkMorphAmount(int id, float amount);
  void RemoveSmoothSkinning(int id);
  bool CommitSubclass() override;
  void Save(idFile* file) const override;
  bool Load(idFile* file) override;

  void Build(const chainGenerateInfo_t& generateInfo, const idStr& modelName,
             const idMaterial* morphMaterial,
             const idCurve_NURBS<idVec3>& spline);
  void BuildModel(const idCurve_NURBS<idVec3>& spline);

  idJointBuffer jointBuffers[1];
  idVertexBuffer morphBuffers[2];
  int currentJointBuffer;
  int currentMorphBuffer;
  int numMorphVertices;
  int numMorphLinkVertices;
  int numLinks;
  float linkSize;
  idList<idJointMat, 85> transforms;
  idList<std::uint8_t, 85> morphMap;
  idList<splinePoint_t, 85> splineSettings;
  idStr chainLinkModel;
  const idMaterial* morphMaterial;
  std::uint8_t dirtyFlags;
  std::uint8_t recoveredDirtyPadding[3]{};
  chainGenerateInfo_t genInfo;
};

static_assert(sizeof(chainGenerateInfo_t) == 8,
              "chainGenerateInfo_t ABI drift");
static_assert(sizeof(idRenderModelChain::splinePoint_t) == 16,
              "chain spline point ABI drift");
static_assert(offsetof(idRenderModelChain, jointBuffers) == 0x880,
              "idRenderModelChain joint buffer offset drift");
static_assert(offsetof(idRenderModelChain, morphBuffers) == 0x898,
              "idRenderModelChain morph buffer offset drift");
static_assert(offsetof(idRenderModelChain, currentJointBuffer) == 0x8C8,
              "idRenderModelChain current buffer offset drift");
static_assert(offsetof(idRenderModelChain, numLinks) == 0x8D8,
              "idRenderModelChain link count offset drift");
static_assert(offsetof(idRenderModelChain, transforms) == 0x8E0,
              "idRenderModelChain transforms offset drift");
static_assert(offsetof(idRenderModelChain, morphMap) == 0x8F8,
              "idRenderModelChain morph map offset drift");
static_assert(offsetof(idRenderModelChain, splineSettings) == 0x910,
              "idRenderModelChain spline offset drift");
static_assert(offsetof(idRenderModelChain, chainLinkModel) == 0x928,
              "idRenderModelChain model name offset drift");
static_assert(offsetof(idRenderModelChain, morphMaterial) == 0x950,
              "idRenderModelChain material offset drift");
static_assert(offsetof(idRenderModelChain, dirtyFlags) == 0x958,
              "idRenderModelChain dirty flags offset drift");
static_assert(offsetof(idRenderModelChain, genInfo) == 0x95C,
              "idRenderModelChain generation info offset drift");
static_assert(sizeof(idRenderModelChain) == 0x968,
              "idRenderModelChain ABI drift");
