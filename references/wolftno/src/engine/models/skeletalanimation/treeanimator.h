#pragma once

#include "engine/models/rendermodel.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "engine/models/skeletalanimation/taginfo.h"
#include "engine/models/skeletalanimation/userchannelexpression.h"

#include <cstddef>
#include <cstdint>

class idDeclSkins;
class idJointGroupCollection;
class idPropsCollection;

enum meshKitComponents_t : int {
  MESH_KIT_HEADS = 0,
  MESH_KIT_GEAR = 1,
  MESH_KIT_GORE = 2,
  MESH_KIT_NUM = 3,
};

class idTreeAnimator : public idRenderModel {
public:
  static constexpr int MAX_SUB_MESHES = 63;
  static constexpr int NUM_BLENDSHAPE_BUFFERS = 2;

  struct blendShape_t {
    using idListAllocReturnsReferenceMarker = void;

    std::uint8_t* map{};
    idList<idUserChannelExpression, 5> blendShapeExpressions;
    float multiplier{};
    std::uint32_t recoveredPadding{};
  };

  struct blendShapeBuffer_t {
    idVertexBuffer* object{};
    int offset{};
    int size{};
  };

  explicit idTreeAnimator(const idDeclMD6* declMD6);
  ~idTreeAnimator() override;

  void SyncJoints(int time);
  void UpdateTime(int time);
  void ShowSkel();
  void DrawBounds(const idBounds& modelSpaceBounds, const idVec4& color,
                  bool depthTest);

  void ReferenceJointAnimator(idJointAnimator* animator);
  int GetMeshIndex(const char* meshName) const;
  void ShowMesh(int meshIndex);
  void HideMesh(int meshIndex);
  void HideAllMeshes();
  idStr ShowKit(meshKitComponents_t kitType, const char* name);
  void HideKit(meshKitComponents_t kitType, const char* name);
  void HideKitsExcept(meshKitComponents_t kitType, const char* name);
  void HideAllKits(meshKitComponents_t kitType);
  void SetKitSkin(const char* skin);
  idStrStatic<256> GetSkinName() const;
  const idDeclSkins* GetSkins() const;

  int NumJoints() const;
  const char* GetJointName(idJointIndex_t jointIndex) const;
  idJointHandle_t GetJointHandle(idJointIndex_t jointIndex) const;
  idJointIndex_t GetJointIndex(const char* jointName) const;
  idJointIndex_t GetJointIndex(const idJointHandle_t& jointHandle) const;

  bool GetModelSpaceJointBindTransform(idJointIndex_t jointIndex,
                                       idVec3& origin, idMat3& axis) const;
  bool GetModelSpaceJointPosition(animationPose_t pose,
                                  idJointIndex_t jointIndex,
                                  idVec3& origin) const;
  bool GetModelSpaceJointAxis(animationPose_t pose,
                              idJointIndex_t jointIndex,
                              idMat3& axis) const;
  bool GetModelSpaceJointTransform(animationPose_t pose,
                                   idJointIndex_t jointIndex,
                                   idVec3& origin, idMat3& axis) const;
  bool GetWorldSpaceJointPosition(animationPose_t pose,
                                  idJointIndex_t jointIndex,
                                  idVec3& origin) const;
  bool GetWorldSpaceJointAxis(animationPose_t pose,
                              idJointIndex_t jointIndex,
                              idMat3& axis) const;
  bool GetWorldSpaceJointTransform(animationPose_t pose,
                                   idJointIndex_t jointIndex,
                                   idVec3& origin, idMat3& axis) const;

  bool GetModelSpaceAttachmentTransform(idJointIndex_t jointIndex,
                                        const idVec3& translationOffset,
                                        const idQuat& rotationOffset,
                                        idVec3& origin, idMat3& axis) const;
  bool GetModelSpaceAttachmentPosition(idJointIndex_t jointIndex,
                                       const idVec3& translationOffset,
                                       const idQuat& rotationOffset,
                                       idVec3& origin) const;
  bool GetModelSpaceAttachmentAxis(idJointIndex_t jointIndex,
                                   const idVec3& translationOffset,
                                   const idQuat& rotationOffset,
                                   idMat3& axis) const;
  bool GetWorldSpaceAttachmentTransform(idJointIndex_t jointIndex,
                                        const idVec3& translationOffset,
                                        const idQuat& rotationOffset,
                                        idVec3& origin, idMat3& axis) const;
  bool GetWorldSpaceAttachmentPosition(idJointIndex_t jointIndex,
                                       const idVec3& translationOffset,
                                       const idQuat& rotationOffset,
                                       idVec3& origin) const;
  bool GetWorldSpaceAttachmentAxis(idJointIndex_t jointIndex,
                                   const idVec3& translationOffset,
                                   const idQuat& rotationOffset,
                                   idMat3& axis) const;

  bool GetModelSpaceTagTransform(const tagData_t& tagData, idVec3& origin,
                                 idMat3& axis) const;
  bool GetWorldSpaceTagTransform(const tagData_t& tagData, idVec3& origin,
                                 idMat3& axis) const;
  bool GetWorldSpaceTagPosition(const tagData_t& tagData,
                                idVec3& origin) const;
  bool GetWorldSpaceTagAxis(const tagData_t& tagData, idMat3& axis) const;

  bool GetModelSpaceJointFrameDeltas(const idJointIndex_t* indices,
                                     int numIndices, idVec3* translations,
                                     idMat3* rotations) const;
  bool GetModelSpaceReferenceJointFrameDeltas(
      const idJointIndex_t* indices, int numIndices, idVec3* translations,
      idMat3* rotations) const;
  bool GetWorldSpaceJointFrameDeltas(const idJointIndex_t* indices,
                                     int numIndices, idVec3* translations,
                                     idMat3* rotations) const;
  bool GetModelSpaceAttachmentFrameDeltas(
      idJointIndex_t jointIndex, const idVec3& translationOffset,
      const idQuat& rotationOffset, idVec3& translationDelta,
      idMat3& rotationDelta) const;
  bool GetWorldSpaceAttachmentFrameDeltas(
      idJointIndex_t jointIndex, const idVec3& translationOffset,
      const idQuat& rotationOffset, idVec3& translationDelta,
      idMat3& rotationDelta) const;

  idJointGroupCollection* GetJointGroupCollection() const;
  idPropsCollection* GetPropsCollection();
  idPropsCollection* GetPropsCollection() const;

  void ApplyOriginDeltas(const idMat3& axis, const idVec3& origin,
                         idMat3& outAxis, idVec3& outOrigin,
                         const idVec3& extraScale) const;
  void ApplyOriginDeltas(const idQuat& axis, const idVec3& origin,
                         idQuat& outAxis, idVec3& outOrigin,
                         const idVec3& extraScale) const;
  const idQuat& GetLastOriginRotation() const;
  void GetLastOriginDeltas(idVec3& translation) const;
  void GetLastOriginDeltas(idQuat& rotation, idVec3& translation) const;
  void GetLastOriginDeltas(idMat3& rotation, idVec3& translation) const;
  void GetTotalOriginTransform(idQuat& rotation, idVec3& translation) const;
  int GetLastOriginDeltaTime() const;
  bool IsDeltaFudged() const;
  void SetRemoveOriginTranslation(bool removeTranslation);
  bool GetRemoveOriginTranslation() const;
  void SetRemoveOriginRotation(bool removeRotation);
  bool GetRemoveOriginRotation() const;
  idVec3 GetVisualOffset() const;
  void SetOriginDeltaLookAhead(bool lookAhead);

  static const idMD6Anim* LookupAnimByAlias(const idDeclMD6* declaration,
                                            const char* animationName);
  void SetFrameFromAnim(const idMD6Anim* animation, int frame,
                        bool updateRenderModel,
                        bool checkForOriginTranslation,
                        const idVec3* translationScale);
  void SetFrameFromAnimWithJointMods(
      const idMD6Anim* animation, int frame, bool updateRenderModel,
      bool checkForOriginTranslation, const idVec3* translationScale,
      const idMD6Blend::jointMod_t* jointMods, int numJointMods);
  const idMD6Anim* GetSingleAnimFrame(idJointMat* joints,
                                      const idMD6Anim* animation, int time,
                                      const idVec3& offset,
                                      bool removeOriginOffset) const;
  const idMD6Anim* GetSingleAnimFrame(idJointMat* joints,
                                      const char* animationName, int time,
                                      const idVec3& offset,
                                      bool removeOriginOffset) const;
  const idMD6Anim* GetSingleAnimFrame(idJointMat* joints,
                                      const idAliasHandle_t& animationAlias,
                                      int time, const idVec3& offset,
                                      bool removeOriginOffset) const;
  void GetJointOffsetAndRotation(const idMD6Anim* animation, int time,
                                 const idVec3& offset,
                                 bool removeOriginOffset, int jointIndex,
                                 idVec3& jointOffset, idMat3& jointAxis) const;

  void ClearMorphPoints();
  void SetMorphSkin(const char* skinName, int skinIndex, float morphAmount,
                    bool silentError);
  void MorphLocation(float morphAmount, int morphIndex);

  void InitializeSurfaces();
  void InitializeSurfaceParms();
  void InitializeMorphBuffers();
  void InitializeBlendShapes();
  void InitializeMorphMaps();
  void FreeSurfaceParms();
  void FreeMorphBuffers();
  void FreeBlendShapes();

  void Save(idFile* file) const override;
  bool Load(idFile* file) override;
  bool CommitSubclass() override;
  const idList<sourceSurface_t, 5>* GetSourceSurfaces() const override;
  decalHandle_t AddDecalFromPoint(const decalParams_t* decalParms,
                                  int startTime, const idVec3& position,
                                  const idVec3& direction,
                                  idJointIndex_t jointId);
  void SerializeSnapshot(idSerializer* serializer, bool serializeParmBlock);

  const idDeclMD6* decl;
  idJointAnimator* jointAnimator;
  idList<idParmBlock*, 5> surfaceParms;
  std::uint64_t meshVisibilityMask;
  std::uint64_t meshUsabilityMask;
  std::int16_t morphSkin;
  std::uint16_t recoveredMorphPadding{};
  float initialMorphValue;
  std::uint8_t treeFlags;
  std::uint8_t recoveredFlagPadding[7]{};
  idList<idUserChannelExpression, 5> wrinkleMapExpressions;
  idList<blendShape_t, 30> blendShapes;
  idList<blendShapeBuffer_t, 5> blendShapeBuffers;
  idVertexBuffer morphMapBuffers[NUM_BLENDSHAPE_BUFFERS];
  int currentBlendShapeBuffer;
  std::uint8_t recoveredTailPadding[12]{};

private:
  static constexpr std::uint8_t TREE_JOINT_ANIM_REF = 1u << 0;
  static constexpr std::uint8_t TREE_SKIP_SERIALIZATION = 1u << 1;
  static constexpr std::uint8_t TREE_UPDATE_BLEND_SHAPES = 1u << 2;

  idVec3 ScaleTranslation(const idVec3& translation) const;
  idVec3 ModelToWorld(const idVec3& modelPosition) const;
  idMat3 FrameDeltaAxis() const;
  idVec3 FrameDeltaOrigin() const;
};

static_assert(sizeof(idTreeAnimator::blendShape_t) == 0x28);
static_assert(offsetof(idTreeAnimator::blendShape_t, blendShapeExpressions) ==
              0x08);
static_assert(offsetof(idTreeAnimator::blendShape_t, multiplier) == 0x20);
static_assert(sizeof(idTreeAnimator::blendShapeBuffer_t) == 0x10);
static_assert(offsetof(idTreeAnimator, decl) == 0x880);
static_assert(offsetof(idTreeAnimator, jointAnimator) == 0x888);
static_assert(offsetof(idTreeAnimator, surfaceParms) == 0x890);
static_assert(offsetof(idTreeAnimator, meshVisibilityMask) == 0x8A8);
static_assert(offsetof(idTreeAnimator, morphSkin) == 0x8B8);
static_assert(offsetof(idTreeAnimator, wrinkleMapExpressions) == 0x8C8);
static_assert(offsetof(idTreeAnimator, blendShapes) == 0x8E0);
static_assert(offsetof(idTreeAnimator, blendShapeBuffers) == 0x8F8);
static_assert(offsetof(idTreeAnimator, morphMapBuffers) == 0x910);
static_assert(offsetof(idTreeAnimator, currentBlendShapeBuffer) == 0x940);
static_assert(sizeof(idTreeAnimator) == 0x950);
