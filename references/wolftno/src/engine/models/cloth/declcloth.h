#pragma once

#include "engine/decls/decl.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"

#include <cstdint>

class idDeclFX;
class idDeclMD6;
class idMaterial;
struct idJointMat;

struct idDeclClothRuntimeCallbacks {
  const idMaterial* (*findMaterial)(const char* name, bool makeDefault){};
  const idMaterial* (*parseMultiMaterial)(idParser& source,
                                          const char* context){};
  const idDeclFX* (*findFX)(const char* name, bool makeDefault){};
  const idDeclMD6* (*findMD6)(const char* name){};
  int (*getJointIndex)(const idDeclMD6* decl, const char* jointName){};
  bool (*getInverseBasePose)(const idDeclMD6* decl, int jointIndex,
                             idJointMat& pose){};
  int (*getParentJointIndex)(const idDeclMD6* decl, int jointIndex){};
};

void SetDeclClothRuntimeCallbacks(
    const idDeclClothRuntimeCallbacks* callbacks);

enum clothType_t {
  CLOTH_INVALID = 0,
  CLOTH_ROPE = 1,
  CLOTH_SQUARE = 2,
  CLOTH_GENERIC = 3,
  CLOTH_FLAG = 4,
};

enum clothSprings_t {
  CLOTH_SPRING_QUAD = 0,
  CLOTH_SPRING_CROSS = 1,
  CLOTH_SPRING_BANDED = 2,
};

class idDeclCloth : public idDecl {
public:
  struct particleInfo_t {
    idVec3 position{};
    std::int16_t jointIndex{-1};
    std::uint16_t recoveredPadding{};
    int forwardIndex{-1};
    int rightIndex{-1};
    bool anchored{};
    std::uint8_t recoveredTail[3]{};
  };

  struct springInfo_t {
    std::uint8_t index1{};
    std::uint8_t index2{};
  };

  struct ropeInfo {
    idList<idVec3, 5> positions;
    idList<idMat3, 5> inverseAxis;
    idList<std::int16_t, 5> indexOfJointToAttachTo;
  };

  idDeclCloth();
  ~idDeclCloth() override;
  const char* DefaultDefinition() const override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;
  void FreeData() override;

  int width{};
  int height{};
  float vSpacing{};
  float hSpacing{};
  float gravity{-1000.0f};
  float mass{10.0f};
  float friction{0.01f};
  float collisionFriction{0.01f};
  float springLength{1.0f};
  float springFriction{0.01f};
  float springMaxLengthMultiplier{2.0f};
  float bandSpringForce{1.0f};
  clothType_t type{CLOTH_INVALID};
  clothSprings_t springType{CLOTH_SPRING_BANDED};
  const idMaterial* material{};
  const idDeclMD6* clothMD6{};
  const idDeclFX* clothDeclFX{};
  idList<ropeInfo, 5> ropeData;
  idList<particleInfo_t, 5> particleData;
  idList<springInfo_t, 5> springData;

  static idDeclInfoTemplate<idDeclCloth> resourceList;
};

static_assert(sizeof(idDeclCloth::particleInfo_t) == 28,
              "idDeclCloth::particleInfo_t ABI drift");
static_assert(sizeof(idDeclCloth::springInfo_t) == 2,
              "idDeclCloth::springInfo_t ABI drift");
static_assert(sizeof(idDeclCloth::ropeInfo) == 72,
              "idDeclCloth::ropeInfo ABI drift");
static_assert(sizeof(idDeclCloth) == 232, "idDeclCloth ABI drift");
