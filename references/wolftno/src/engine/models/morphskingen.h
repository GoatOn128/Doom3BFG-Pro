#pragma once

#include "engine/animation/md6.h"
#include "engine/decls/decl.h"
#include "engine/models/skeletalanimation/md6morph.h"
#include "engine/renderer/triangles.h"

#include <cstddef>
#include <cstdint>

class idDecl;
class idStaticModel;
struct MorphSkinGenValidationAccess;

class idMorphDef {
public:
  idStr jointName;
  idVec3 pos;
  idVec3 size;
  std::uint32_t flags{};
  idStr fileName;
};

struct idMorphResourceContext {
  const idDeclMD6* decl{};
  const idList<idMorphDef, 5>* morphDefs{};
  const char* declFileName{};
  std::uint32_t declTimestampChecksum{0xffffffffu};
  std::uint32_t modelTimestamp{0xffffffffu};
  bool declLoadedResource{};
};

enum idMorphJointResolveResult {
  MORPH_JOINT_RESOLVED,
  MORPH_JOINT_NOT_FOUND,
  MORPH_JOINT_GROUP_EMPTY,
  MORPH_JOINT_INVALID,
};

using idMorphResourceLookup = bool (*)(const char* name,
                                       idMorphResourceContext& context);
using idMorphJointResolver = idMorphJointResolveResult (*)(
    const idDeclMD6* decl, const char* jointName, int& groupIndex,
    idVec3& transformedPosition, const idVec3& sourcePosition);
using idMorphDebugLine = void (*)(const idVec4& color, const idVec3& start,
                                  const idVec3& end, int lifetime,
                                  bool depthTest);

void SetMorphResourceLookup(idMorphResourceLookup lookup);
void SetMorphJointResolver(idMorphJointResolver resolver);
void SetMorphDebugLine(idMorphDebugLine debugLine);
void SetMorphForceGeneration(bool forceGeneration);

struct morphShape_t {
  enum shapeType_t : std::int32_t {
    NONE = 0,
    SKIN_MESH = 1,
    SKIN_SPHERE = 2,
    CLIP_SPHERE = 3,
    CLIP_ELLIPSOID = 4,
  };

  morphShape_t(idTriangles* morphTris, const idDrawVert* morphTriVerts,
               const char* materialName, const idDecl* decl);
  __forceinline bool MorphPoint(const idVec3& inPoint, idVec3& outPoint) {
    if (type == SKIN_MESH)
      return SkinMeshMorphPoint(inPoint, outPoint);
    if (type == SKIN_SPHERE)
      return SphereContainsPoint(inPoint, outPoint);
    return false;
  }
  __forceinline shapeType_t GetType() { return type; }
  __forceinline idVec3 GetCenter() { return center; }
  __forceinline float GetLength() { return length; }
  __forceinline float GetRadius() { return radius; }
  __forceinline idVec3 GetXAxis() { return xAxis; }
  __forceinline idVec3 GetYAxis() { return yAxis; }
  __forceinline idVec3 GetZAxis() { return zAxis; }

  shapeType_t type{NONE};
  idVec3 center{};
  idVec3 xAxis{};
  idVec3 yAxis{};
  idVec3 zAxis{};
  float length{};
  float radius{};
  float radiusSqr{};
  idList<idVec3, 5> points;

private:
  bool SkinMeshMorphPoint(const idVec3& inPoint, idVec3& outPoint);
  __forceinline bool SphereContainsPoint(const idVec3& inPoint,
                                         idVec3& outPoint) {
    outPoint = {};
    return (inPoint - center).LengthSqr() <= radiusSqr;
  }
  __forceinline bool CylContained(const idVec3& inPoint, idVec3& outPoint) {
    outPoint = {};
    const idVec3 fromCenter = inPoint - center;
    const float axialDistance = fromCenter * zAxis;
    if (axialDistance < 0.0f || axialDistance > length)
      return false;
    const idVec3 radial = fromCenter - zAxis * axialDistance;
    return radial.LengthSqr() <= radiusSqr;
  }
  bool MakeSkinMesh(idTriangles* morphTris,
                    const idDrawVert* morphTriVerts, const idDecl* decl);
  bool MakeSphere(idTriangles* morphTris, const idDrawVert* morphTriVerts,
                  const idDecl* decl);
  bool MakeEllipsoid(idTriangles* morphTris,
                     const idDrawVert* morphTriVerts, const idDecl* decl);
  __forceinline bool MakeCylinder(idTriangles*, const idDrawVert*,
                                  const idDecl*) {
    return false;
  }

public:
  ~morphShape_t();

private:
  friend class idMorphVertGen;
  friend struct MorphSkinGenValidationAccess;
};

class idMorphVertices : public idResource {
public:
  idMorphVertices();
  ~idMorphVertices() override;

  bool ReloadIfStale() override;
  idResourceList* GetResourceList() const override;
  void LoadResource() override;

  bool Load_Binary();
  bool Write_Binary();

  idList<idMD6MorphSet, 5> morphVertSet;
  std::uint32_t timestampDecl;
  std::uint32_t timestampModel;

  static idTypedResourceList<idMorphVertices> resourceList;
};

template <>
idMorphVertices*
idTypedResourceList<idMorphVertices>::Alloc(const char* name) const;
template <>
int idTypedResourceList<idMorphVertices>::AllocSize() const;

class idMorphVertGen {
public:
  static void GenerateMorphVertsFromFile(const idDeclMD6* decl,
                                         const char* generationFile,
                                         idMorphVertices& vertices);
  static void GenerateMorphVerts(const idDeclMD6* decl,
                                 const idList<idMorphDef, 5>& morphDefs,
                                 idMorphVertices& vertices);

private:
  static void ProcessModel(const idMD6Model& model, const idDeclMD6* decl,
                           const idMorphDef& morphDef,
                           idMorphVertices& vertices);
  static void OptimizeVertices(idMorphVertices& outputVertices,
                               idMorphVertices& inputVertices);

  friend struct MorphSkinGenValidationAccess;
};

static_assert(sizeof(idMorphDef) == 112);
static_assert(offsetof(idMorphDef, pos) == 40);
static_assert(offsetof(idMorphDef, fileName) == 72);
static_assert(sizeof(morphShape_t) == 88);
static_assert(offsetof(morphShape_t, center) == 4);
static_assert(offsetof(morphShape_t, points) == 64);
static_assert(sizeof(idMorphVertices) == 80);
static_assert(offsetof(idMorphVertices, morphVertSet) == 48);
static_assert(offsetof(idMorphVertices, timestampDecl) == 72);
