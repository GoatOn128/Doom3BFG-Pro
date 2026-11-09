#pragma once

#include "engine/decls/declentitydef.h"
#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/math/plane.h"
#include "shared/idlib/text/lexer.h"

class idGame;
class idMapFile;
class idMapEntity;

constexpr int OLD_MAP_VERSION = 1;
constexpr int CURRENT_MAP_VERSION = 3;

struct idGroupState {
  enum state_t : unsigned {
    GROUPSTATE_NORMAL,
    GROUPSTATE_SELECTED,
    GROUPSTATE_GHOSTED,
    GROUPSTATE_HIDDEN,
    GROUPSTATE_EXPANDED,
    GROUPSTATE_LAST
  };
  idStr groupName;
  unsigned state{};
};

struct idRefMapDef {
  idStr mapName;
  idStr worldSpawnText;
  bool operator==(const idRefMapDef& rhs) const {
    return idStr::Icmp(mapName.data, rhs.mapName.data) == 0 &&
           idStr::Icmp(worldSpawnText.data, rhs.worldSpawnText.data) == 0;
  }
};

class idMapGroups {
public:
  virtual ~idMapGroups();
  static void ParseGroups(idLexer& src, idList<idStr>& groups);
  void RenameReferenceGroups(const char* refname);
  void WriteGroups(idFile* fp, const char* tabs) const;

  idList<idStr> groupList;
};

class idMapPrimitive : public idMapGroups {
public:
  enum mapPrimitive_t { TYPE_INVALID = -1, TYPE_BRUSH = 0, TYPE_REFERENCE = 1 };
  idMapPrimitive() : type(TYPE_INVALID) {}
  ~idMapPrimitive() override;
  virtual unsigned GetGeometryCRC() const = 0;
  virtual void Transform(const idVec3& origin, const idMat3& axis) = 0;
  virtual bool Write(idFile* fp, int primitiveNum) const = 0;
  mapPrimitive_t GetType() const { return type; }

protected:
  mapPrimitive_t type;
};

class idMapBrushSide {
public:
  idMapBrushSide() : material(), plane{}, texMat{}, origin{} {}
  const char* GetMaterial() const;
  const idPlane& GetPlane() const;
  void SetMaterial(const char* value) { material = value; }
  void SetPlane(const idPlane& value) { plane = value; }
  void SetTextureMatrix(const idVec3 value[2]) { texMat[0] = value[0]; texMat[1] = value[1]; }

  idStr material;
  idPlane plane;
  idVec3 texMat[2];
  idVec3 origin;
};

class idMapBrush final : public idMapPrimitive {
public:
  idMapBrush();
  ~idMapBrush() override;
  static idMapBrush* Parse(idLexer& src, const idVec3& origin, bool newFormat,
                           int version = CURRENT_MAP_VERSION);
  static idMapBrush* ParseQ3(idLexer& src, const idVec3& origin);
  bool Write(idFile* fp, int primitiveNum) const override;
  unsigned GetGeometryCRC() const override;
  void Transform(const idVec3& origin, const idMat3& axis) override;
  int GetNumSides() const { return sides.Num(); }
  idMapBrushSide* GetSide(int i) const;
  int AddSide(idMapBrushSide* side) { return sides.Append(side); }

  idList<idMapBrushSide*> sides;
};

class idMapModel {
public:
  idMapModel();
  ~idMapModel();
  int GetNumPrimitives() const { return primitives.Num(); }
  idMapPrimitive* GetPrimitive(int i) const { return primitives[i]; }
  int AddPrimitive(idMapPrimitive* primitive) { return primitives.Append(primitive); }
  void MovePrimitives(idMapModel* model);
  void TransformPrimitives(const idVec3& origin, const idMat3& axis);

  idStr name;
  idList<idMapPrimitive*> primitives;
};

class idMapReference final : public idMapPrimitive {
public:
  idMapReference();
  ~idMapReference() override;
  static idMapReference* Parse(idLexer& src, int version, bool expandReferences,
                               idMapEntity* parent);
  void ExpandReference(idMapFile* mapFile, idMapEntity* mapEnt);
  unsigned GetGeometryCRC() const override;
  void Transform(const idVec3&, const idMat3&) override {}
  bool Write(idFile* fp, int refNum) const override;

  idStr mapName;
  idStr worldSpawnText;
  idMapFile* map;
};

class idMapEntity final : public idMapGroups {
public:
  idMapEntity();
  ~idMapEntity() override;
  static idMapEntity* Parse(idMapFile* mapFile, idLexer& src, int version,
                            idMapEntity* parent);
  void SetEntityDef(idDeclEntityDef* def);
  void Transform(const idVec3& origin, const idMat3& axis);
  void FixMapModelNames(const char* mapName, const char* name);
  void FixTargetNames(const char* baseName);
  void ClearLayerList() { layerList.ClearFree(); }

  idDeclEntityDef* entityDef;
  idMapModel model;
  idMapEntity* parentReference;
  idStr refId;
  idList<idStr> layerList;
};

class idMapEditorStates {
public:
  idMapEditorStates();
  virtual ~idMapEditorStates();
  void Parse(idLexer& src);

  idList<idGroupState> groupStates;
  idList<idGroupState> layerStates;
  idList<idGroupState> groupColors;
};

class idMapFile {
public:
  idMapFile();
  virtual ~idMapFile();
  bool Parse(const char* filename, bool osPath = false, idMapEntity* parent = nullptr);
  bool ParseSingle(idLexer& src);
  bool Reload();
  bool Write(const char* fileName, bool osPath = false);
  bool WriteSounds(const char* fileName, bool osPath = false);
  bool WriteEntities(const char* fileName, bool osPath = false);
  int GetNumEntities() const { return entities.Num(); }
  idMapEntity* GetEntity(int i) const;
  int AddEntity(idMapEntity* mapEnt);
  idMapEntity* FindEntity(const char* entityName, int* index = nullptr);
  void RemoveEntity(idMapEntity* mapEnt);
  void AddReferenceMap(const char* mapName, const char* worldSpawnText);
  int FindReferenceMap(const char* mapName) const;
  idStr CopyMapEntities(const idMapFile* map, const char* refId);
  void ClearCopiedEntities(const idStr& refname);
  void SetGeometryCRC();
  void ConvertMapModelNames();
  static idStr GetMapModelName(const char* mapName, const char* entityName) {
    idStr result(mapName ? mapName : "");
    result.Append("/");
    result.Append(entityName ? entityName : "");
    return result;
  }

  idStr name;
  int version;
  unsigned fileTime;
  unsigned geometryCRC;
  bool hasPrimitiveData;
  idList<idMapEntity*> entities;
  idHashIndex entityNameHash;
  idMapEditorStates groupStates;
  idList<idRefMapDef> referenceMaps;
  idMapEntity* parentReference;
  bool expandReferences;
  idGame* game;
  bool entitiesAreReference;
  bool ignoreGroupInfo;
};

static_assert(sizeof(idGroupState) == 48, "idGroupState ABI drift");
static_assert(sizeof(idRefMapDef) == 80, "idRefMapDef ABI drift");
static_assert(sizeof(idMapGroups) == 32, "idMapGroups ABI drift");
static_assert(sizeof(idMapPrimitive) == 40, "idMapPrimitive ABI drift");
static_assert(sizeof(idMapBrushSide) == 96, "idMapBrushSide ABI drift");
static_assert(sizeof(idMapBrush) == 64, "idMapBrush ABI drift");
static_assert(sizeof(idMapReference) == 128, "idMapReference ABI drift");
static_assert(sizeof(idMapModel) == 64, "idMapModel ABI drift");
static_assert(sizeof(idMapEntity) == 176, "idMapEntity ABI drift");
static_assert(sizeof(idMapEditorStates) == 80, "idMapEditorStates ABI drift");
static_assert(sizeof(idMapFile) == 264, "idMapFile ABI drift");
