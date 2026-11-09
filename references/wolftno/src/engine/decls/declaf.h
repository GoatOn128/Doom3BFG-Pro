#pragma once

#include "decl.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/math/vector.h"

#include <cstdint>

class idDeclMD6;
class idMaterial;
struct idJointMat;

enum declAFConstraintType_t {
  DECLAF_CONSTRAINT_INVALID,
  DECLAF_CONSTRAINT_FIXED,
  DECLAF_CONSTRAINT_BALLANDSOCKETJOINT,
  DECLAF_CONSTRAINT_UNIVERSALJOINT,
  DECLAF_CONSTRAINT_HINGE,
  DECLAF_CONSTRAINT_SLIDER,
  DECLAF_CONSTRAINT_SPRING,
};

enum declAFJointMod_t {
  DECLAF_JOINTMOD_AXIS,
  DECLAF_JOINTMOD_ORIGIN,
  DECLAF_JOINTMOD_BOTH,
};

using getJointTransform_t = bool (*)(void*, const idJointMat*, const char*, idVec3*, idMat3*);

class idAFVector {
public:
  enum afVectorType_t { VEC_COORDS, VEC_JOINT, VEC_BONECENTER, VEC_BONEDIR, VEC_AXIS };

  idAFVector() : type(VEC_COORDS), joint1(), joint2(), vec{}, negate(false) {}

  bool Parse(idParser& source);
  bool Finish(const char* fileName, getJointTransform_t getJointTransform,
              const idJointMat* frame, void* model);
  bool Finish(const char* fileName, getJointTransform_t getJointTransform,
              const idJointMat* frame, void* model, const idAFVector& shaft0,
              const idAFVector& shaft1);
  bool Write(idFile* file) const;

  afVectorType_t type;
  idAtomicString joint1;
  idAtomicString joint2;
  idVec3 vec;
  bool negate;
};

class idDeclAF;

class idDeclAF_Body {
public:
  idDeclAF_Body();
  void SetDefault(const idDeclAF* file);

  idAtomicString name;
  idAtomicString jointName;
  declAFJointMod_t jointMod;
  int modelType;
  idAtomicString modelName;
  idAFVector v1;
  idAFVector v2;
  int numSides;
  float width;
  float density;
  alignas(8) idAFVector origin;
  idAngles angles;
  int contents;
  int clipMask;
  bool selfCollision;
  bool noSyncCollide;
  bool clearClipMaskInSolid;
  float bouncyness;
  idMat3 inertiaScale;
  float linearFriction;
  float angularFriction;
  float contactFriction;
  float gravityScale;
  float buoyancyScale;
  idAtomicString containedJoints;
  idAFVector frictionDirection;
  idAFVector contactMotorDirection;
  idList<std::int16_t, 71> containedJointIndices;
};

class idDeclAF_Constraint {
public:
  enum afConstraintLimit_t { LIMIT_NONE = -1, LIMIT_CONE, LIMIT_PYRAMID };

  idDeclAF_Constraint();
  void SetDefault(const idDeclAF* file);

  idAtomicString name;
  idAtomicString body1;
  idAtomicString body2;
  declAFConstraintType_t type;
  float friction;
  float stretch;
  float compress;
  float damping;
  float restLength;
  float minLength;
  float maxLength;
  float minTwist;
  float maxTwist;
  idAFVector anchor;
  idAFVector anchor2;
  idAFVector shaft[2];
  idAFVector axis;
  afConstraintLimit_t limit;
  alignas(8) idAFVector limitAxis;
  float limitAngles[4];
  bool limitFudge;
  bool syncConstraint;
};

class idDeclAF final : public idDecl {
public:
  struct solverConstants_t {
    float errorReduction;
    float errorReductionMax;
    float lcpEpsilon;
    float maxLcpEpsilon;
    float limitErrorReduction;
    float limitErrorReductionMax;
    float limitLcpEpsilon;
    float contactErrorReduction;
    float contactErrorReductionMax;
    float contactLcpEpsilon;
    float universalErrorReduction;
    float universalErrorReductionMax;
    float universalTorsionLcpEpsilon;
    float minTwistErrorReduction;
    float maxTwistErrorReduction;
  };

  idDeclAF();
  ~idDeclAF() override;

  idDeclInfo* GetDeclInfo() const override;
  bool RebuildTextSource() override;
  const char* DefaultDefinition() const override;
  void Parse(idParser& source) override;
  void FreeData() override;

  void Finish(getJointTransform_t getJointTransform, const idJointMat* frame, void* model) const;

  static int ContentsFromString(const char* text);
  static const char* ContentsToString(int contents, idStr& text);
  static declAFJointMod_t JointModFromString(const char* text);

  bool modified;
  const idDeclMD6* modelDef;
  idAtomicString skin;
  float defaultBouncyness;
  float defaultLinearFriction;
  float defaultAngularFriction;
  float defaultContactFriction;
  float defaultConstraintFriction;
  float defaultGravityScale;
  float defaultBuoyancyScale;
  idVec2 suspendVelocity;
  idVec2 suspendAcceleration;
  float noMoveTime;
  float noMoveTranslation;
  float noMoveRotation;
  float minMoveTime;
  float maxMoveTime;
  int contents;
  int clipMask;
  solverConstants_t solverConstants;
  solverConstants_t syncSolverConstants;
  bool selfCollision;
  idAtomicString base;
  const idMaterial* clipMaterial;
  idList<idDeclAF_Body*, 71> bodies;
  idList<idDeclAF_Constraint*, 71> constraints;

  static idDeclInfoTemplate<idDeclAF> resourceList;

private:
  bool ParseContents(idParser& source, int& value) const;
  bool ParseSolverConstants(idParser& source, solverConstants_t& constants);
  bool ParseSettings(idParser& source);
  bool ParseBody(idParser& source);
  bool ParseFixed(idParser& source);
  bool ParseBallAndSocketJoint(idParser& source);
  bool ParseUniversalJoint(idParser& source);
  bool ParseHinge(idParser& source);
  bool ParseSlider(idParser& source);
  bool ParseSpring(idParser& source);

  void WriteSolverConstants(idFile* file, const solverConstants_t& constants,
                            const char* name, const char* indent) const;
  bool WriteBody(idFile* file, const idDeclAF_Body& body) const;
  bool WriteBallAndSocketJoint(idFile* file, const idDeclAF_Constraint& constraint) const;
  bool WriteUniversalJoint(idFile* file, const idDeclAF_Constraint& constraint) const;
  bool WriteHinge(idFile* file, const idDeclAF_Constraint& constraint) const;
  bool WriteSpring(idFile* file, const idDeclAF_Constraint& constraint) const;
  bool WriteSettings(idFile* file) const;
};

static_assert(sizeof(idAFVector) == 40, "idAFVector ABI drift");
static_assert(sizeof(idDeclAF_Body) == 368, "idDeclAF_Body ABI drift");
static_assert(sizeof(idDeclAF_Constraint) == 336, "idDeclAF_Constraint ABI drift");
static_assert(sizeof(idDeclAF::solverConstants_t) == 60, "AF solver constants ABI drift");
static_assert(sizeof(idDeclAF) == 368, "idDeclAF ABI drift");
