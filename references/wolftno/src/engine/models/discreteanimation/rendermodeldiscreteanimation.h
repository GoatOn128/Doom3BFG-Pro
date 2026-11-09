#pragma once

#include "engine/models/discreteanimation/discreteanimationmodeldata.h"
#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"

class idCVar;
class idDeclBreakable;

class alignas(16) idRenderModelDiscreteAnimation : public idRenderModel {
public:
  idRenderModelDiscreteAnimation(const char* modelName,
                                 const idDeclBreakable* decl);

  void Update();
  void SerializeSnapshot(idSerializer* serializer, bool serializeParmBlock);
  void GetPieceTransform(int index, idVec3& position,
                         idMat3& orientation) const;
  void SetPieceTransform(int index, const idVec3& position,
                         const idMat3& orientation);
  void CollapsePiece(int index);
  void SetBasePiecePosition(const idVec3& position,
                            const idMat3& orientation);
  void GetPieceTraceModel(int index, idTraceModel& traceModel);
  const idMaterial* GetPieceTraceMaterial(int index) const;
  void Save(idFile* file) const override;
  bool Load(idFile* file) override;
  void Reset();

protected:
  ~idRenderModelDiscreteAnimation() override;
  bool CommitSubclass() override;

private:
  void FreeData();
  void LoadModelData();
  bool LoadModel(const char* modelName);

public:
  // Public for recovered layout/type-info parity; mutation goes through the
  // piece API in production code.
  const idDiscreteAnimationModelData* modelData{};
  const idDeclBreakable* breakableDecl{};
  int modelTimestamp{-1};
  idJointBuffer jointBuffers[2];
  int currentJointBuffer{};
  idList<idJointMat, 59> transforms;
  idList<bool, 59> collapsed;
  bool changed{};

  friend class RenderModelDiscreteAnimationValidationAccess;
};

extern idCVar r_noBreakableShadows;

static_assert(offsetof(idRenderModelDiscreteAnimation, modelData) == 0x880,
              "discrete render-model data offset drift");
static_assert(offsetof(idRenderModelDiscreteAnimation, breakableDecl) == 0x888,
              "discrete render-model decl offset drift");
static_assert(offsetof(idRenderModelDiscreteAnimation, modelTimestamp) == 0x890,
              "discrete render-model timestamp offset drift");
static_assert(offsetof(idRenderModelDiscreteAnimation, jointBuffers) == 0x898,
              "discrete render-model joint-buffer offset drift");
static_assert(offsetof(idRenderModelDiscreteAnimation, currentJointBuffer) ==
                  0x8c8,
              "discrete render-model current-buffer offset drift");
static_assert(offsetof(idRenderModelDiscreteAnimation, transforms) == 0x8d0,
              "discrete render-model transforms offset drift");
static_assert(offsetof(idRenderModelDiscreteAnimation, collapsed) == 0x8e8,
              "discrete render-model collapsed offset drift");
static_assert(offsetof(idRenderModelDiscreteAnimation, changed) == 0x900,
              "discrete render-model changed offset drift");
static_assert(sizeof(idRenderModelDiscreteAnimation) == 0x910,
              "idRenderModelDiscreteAnimation ABI drift");
