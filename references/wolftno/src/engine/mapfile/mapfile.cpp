#include "engine/mapfile/mapfile.h"

#include "shared/idlib/lib.h"
#include "shared/idlib/typeinfo/typeinfo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>

namespace {
template <class T> void DeletePointers(idList<T*>& list) {
  for (int i = 0; i < list.Num(); ++i)
    delete list[i];
  list.ClearFree();
}

template <class T> void AppendList(idList<T>& target, const idList<T>& source) {
  for (int i = 0; i < source.Num(); ++i)
    target.Append(source[i]);
}

bool Contains(const idList<idStr>& list, const idStr& value) {
  for (int i = 0; i < list.Num(); ++i)
    if (list[i] == value)
      return true;
  return false;
}

unsigned StringGeometryCRC(const char* text) {
  unsigned crc = 0;
  int index = 0;
  for (const char* p = text ? text : ""; *p; ++p, ++index)
    crc ^= static_cast<unsigned>(static_cast<int>(static_cast<signed char>(*p)) << (index & 3));
  return crc;
}

unsigned FloatBits(float value) {
  unsigned bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

void CollectEntityReferences(idTypeInfoNode* node, idList<idTypeInfoNode*>& nodes) {
  if (node == nullptr)
    return;
  if (node->type == "idEntity" || node->type == "idEntityPtr")
    nodes.Append(node);
  for (auto& child : node->children)
    CollectEntityReferences(child.get(), nodes);
}

void ParseStateBlock(idLexer& src, const char* keyword, idList<idGroupState>& output) {
  static const char* const tokens[] = {"normal", "selected", "ghosted", "hidden",
                                      "expanded", nullptr};
  if (!src.PeekTokenString(keyword) || !src.CheckTokenString(keyword))
    return;
  if (!src.ExpectTokenString("{"))
    return;
  idToken group;
  idToken state;
  while (src.ReadToken(group)) {
    if (group == "}" || group.type != TT_STRING ||
        !src.ExpectTokenType(TT_STRING, 0, state))
      break;
    int stateIndex = 0;
    while (tokens[stateIndex] != nullptr && idStr::Icmp(state.data, tokens[stateIndex]) != 0)
      ++stateIndex;
    if (tokens[stateIndex] == nullptr)
      continue;
    idGroupState entry;
    entry.groupName = group;
    entry.state = static_cast<unsigned>(stateIndex);
    bool duplicate = false;
    for (int i = 0; i < output.Num(); ++i)
      duplicate |= output[i].groupName == entry.groupName && output[i].state == entry.state;
    if (!duplicate)
      output.Append(entry);
  }
}

void ResetEntityHash(idMapFile& map) {
  map.entityNameHash.Clear();
}
} // namespace

bool idPlane::FromPoints(const idVec3& p1, const idVec3& p2, const idVec3& p3,
                         bool) {
  const idVec3 normal = (p1 - p2).Cross(p3 - p2);
  idVec3 unit = normal;
  if (unit.Normalize() == 0.0f)
    return false;
  a = unit.x;
  b = unit.y;
  c = unit.z;
  d = -(p2 * unit);
  return true;
}

const char* idMapBrushSide::GetMaterial() const { return material.data; }
const idPlane& idMapBrushSide::GetPlane() const { return plane; }

idMapGroups::~idMapGroups() = default;

void idMapGroups::ParseGroups(idLexer& src, idList<idStr>& groups) {
  if (!src.CheckTokenString("groups") || !src.ExpectTokenString("{"))
    return;
  idToken token;
  while (src.ReadToken(token)) {
    if (token == "}")
      break;
    if (token.type == TT_STRING && !Contains(groups, token))
      groups.Append(token);
  }
}

void idMapGroups::RenameReferenceGroups(const char* refname) {
  for (int i = 0; i < groupList.Num(); ++i) {
    idStr renamed("$");
    renamed.Append(refname ? refname : "");
    renamed.Append("/");
    renamed.Append(groupList[i]);
    groupList[i] = renamed;
  }
}

void idMapGroups::WriteGroups(idFile* fp, const char* tabs) const {
  if (fp == nullptr || groupList.Num() == 0)
    return;
  const char* indent = tabs ? tabs : "";
  fp->WriteFloatString("%sgroups {\n", indent);
  for (int i = 0; i < groupList.Num(); ++i) {
    idStr groupName(groupList[i]);
    if (groupName.Find("$func_reference") != -1) {
      const int slash = groupName.Find("/");
      if (slash >= 0)
        groupName = groupName.Right(groupName.Length() - slash - 1);
    }
    fp->WriteFloatString("%s\t\"%s\"\n", indent, groupName.data);
  }
  fp->WriteFloatString("%s}\n", indent);
}

idMapPrimitive::~idMapPrimitive() = default;

idMapBrush::idMapBrush() { type = TYPE_BRUSH; sides.ResizeForPort(8); }
idMapBrush::~idMapBrush() { DeletePointers(sides); }

idMapBrushSide* idMapBrush::GetSide(int i) const { return sides[i]; }

unsigned idMapBrush::GetGeometryCRC() const {
  unsigned crc = 0;
  for (int i = 0; i < sides.Num(); ++i) {
    const idMapBrushSide& side = *sides[i];
    crc ^= FloatBits(side.plane.a) ^ FloatBits(side.plane.b) ^ FloatBits(side.plane.c) ^
           FloatBits(side.plane.d) ^ StringGeometryCRC(side.material.data);
  }
  return crc;
}

void idMapBrush::Transform(const idVec3& translation, const idMat3& axis) {
  for (int i = 0; i < sides.Num(); ++i) {
    idPlane& plane = sides[i]->plane;
    const idVec3 normal = axis * idVec3{plane.a, plane.b, plane.c};
    plane.a = normal.x;
    plane.b = normal.y;
    plane.c = normal.z;
    plane.d -= normal * translation;
  }
}

bool idMapBrush::Write(idFile* fp, int primitiveNum) const {
  if (fp == nullptr)
    return false;
  fp->WriteFloatString("\t// primitive %d\n\t{\n", primitiveNum);
  WriteGroups(fp, "\t\t");
  fp->WriteFloatString("\t\tbrushDef3 {\n");
  for (int i = 0; i < sides.Num(); ++i) {
    const idMapBrushSide& side = *sides[i];
    fp->WriteFloatString("\t\t\t( %f %f %f %f ) ", side.plane.a, side.plane.b,
                         side.plane.c, side.plane.d);
    fp->WriteFloatString("( ( %f %f %f ) ( %f %f %f ) ) \"%s\" 0 0 0\n",
                         side.texMat[0].x, side.texMat[0].y, side.texMat[0].z,
                         side.texMat[1].x, side.texMat[1].y, side.texMat[1].z,
                         side.material.data);
  }
  fp->WriteFloatString("\t\t}\n\t}\n");
  return true;
}

idMapBrush* idMapBrush::Parse(idLexer& src, const idVec3& origin, bool newFormat,
                               int version) {
  if (!src.ExpectTokenString("{"))
    return nullptr;
  idList<idMapBrushSide*> parsed;
  idToken token;
  while (src.ReadToken(token)) {
    if (token == "}") {
      if (!src.ExpectTokenString("}")) {
        DeletePointers(parsed);
        return nullptr;
      }
      auto* brush = new idMapBrush;
      for (int i = 0; i < parsed.Num(); ++i)
        brush->sides.Append(parsed[i]);
      parsed.ClearFree();
      return brush;
    }
    while (token.type == TT_STRING && token != "(") {
      idToken value;
      if (!src.ReadTokenOnLine(value) || value.type != TT_STRING || !src.ReadToken(token)) {
        src.Error("idMapBrush::Parse: expected epair value string not found");
        DeletePointers(parsed);
        return nullptr;
      }
    }
    if (token != "(") {
      src.Error("idMapBrush::Parse: unexpected %s, expected ( or epair key string", token.data);
      DeletePointers(parsed);
      return nullptr;
    }
    src.UnreadToken();
    auto* side = new idMapBrushSide;
    parsed.Append(side);
    if (newFormat) {
      if (!src.Parse1DMatrix(4, &side->plane.a)) {
        src.Error("idMapBrush::Parse: unable to read brush side plane definition");
        DeletePointers(parsed);
        return nullptr;
      }
    } else {
      idVec3 points[3];
      if (!src.Parse1DMatrix(3, &points[0].x) || !src.Parse1DMatrix(3, &points[1].x) ||
          !src.Parse1DMatrix(3, &points[2].x)) {
        src.Error("idMapBrush::Parse: unable to read brush side plane definition");
        DeletePointers(parsed);
        return nullptr;
      }
      for (idVec3& point : points)
        point -= origin;
      side->plane.FromPoints(points[0], points[1], points[2]);
    }
    if (!src.Parse2DMatrix(2, 3, &side->texMat[0].x)) {
      src.Error("idMapBrush::Parse: unable to read brush side texture matrix");
      DeletePointers(parsed);
      return nullptr;
    }
    side->origin = origin;
    if (!src.ReadTokenOnLine(token)) {
      src.Error("idMapBrush::Parse: unable to read brush side material");
      DeletePointers(parsed);
      return nullptr;
    }
    if (version >= 2)
      side->material = token;
    else {
      side->material = "textures/";
      side->material.Append(token);
    }
    if (src.ReadTokenOnLine(token) && src.ReadTokenOnLine(token))
      src.ReadTokenOnLine(token);
  }
  src.Error("idMapBrush::Parse: unexpected EOF");
  DeletePointers(parsed);
  return nullptr;
}

idMapBrush* idMapBrush::ParseQ3(idLexer& src, const idVec3& origin) {
  idList<idMapBrushSide*> parsed;
  if (src.CheckTokenString("}"))
    return new idMapBrush;
  for (;;) {
    auto* side = new idMapBrushSide;
    parsed.Append(side);
    idVec3 points[3];
    if (!src.Parse1DMatrix(3, &points[0].x) || !src.Parse1DMatrix(3, &points[1].x) ||
        !src.Parse1DMatrix(3, &points[2].x)) {
      src.Error("idMapBrush::ParseQ3: unable to read brush side plane definition");
      DeletePointers(parsed);
      return nullptr;
    }
    for (idVec3& point : points)
      point -= origin;
    side->plane.FromPoints(points[0], points[1], points[2]);
    idToken token;
    if (!src.ReadTokenOnLine(token)) {
      src.Error("idMapBrush::ParseQ3: unable to read brush side material");
      DeletePointers(parsed);
      return nullptr;
    }
    side->material = "textures/";
    side->material.Append(token);
    src.ParseInt(); src.ParseInt(); src.ParseInt(); src.ParseFloat(); src.ParseFloat();
    side->texMat[0] = {1.0f / 32.0f, 0.0f, 0.0f};
    side->texMat[1] = {0.0f, 1.0f / 32.0f, 0.0f};
    side->origin = origin;
    if (src.ReadTokenOnLine(token) && src.ReadTokenOnLine(token))
      src.ReadTokenOnLine(token);
    if (src.CheckTokenString("}")) {
      auto* brush = new idMapBrush;
      for (int i = 0; i < parsed.Num(); ++i)
        brush->sides.Append(parsed[i]);
      parsed.ClearFree();
      return brush;
    }
  }
}

idMapModel::idMapModel() = default;
idMapModel::~idMapModel() { DeletePointers(primitives); }

void idMapModel::TransformPrimitives(const idVec3& origin, const idMat3& axis) {
  for (int i = 0; i < primitives.Num(); ++i)
    primitives[i]->Transform(origin, axis);
}

void idMapModel::MovePrimitives(idMapModel* model) {
  if (model == nullptr)
    return;
  AppendList(primitives, model->primitives);
  model->primitives.ClearFree();
}

idMapReference::idMapReference() : map(nullptr) {}
idMapReference::~idMapReference() { delete map; }

bool idMapReference::Write(idFile* fp, int refNum) const {
  if (fp == nullptr)
    return false;
  fp->WriteFloatString("\t// reference %d\n\t{\n", refNum);
  fp->WriteFloatString("\t\treference {\n\t\t\t\"%s\"\n\t\t}\n\t}\n", mapName.data);
  return true;
}

unsigned idMapReference::GetGeometryCRC() const { return StringGeometryCRC(mapName.data); }

idMapReference* idMapReference::Parse(idLexer& src, int, bool expand,
                                       idMapEntity* parent) {
  if (!src.ExpectTokenString("{"))
    return nullptr;
  std::unique_ptr<idMapReference> reference(new idMapReference);
  idToken token;
  if (!src.ExpectTokenType(TT_STRING, 0, token)) {
    src.Error("expected map file name not found");
    return nullptr;
  }
  reference->mapName = token;
  if (expand) {
    reference->map = new idMapFile;
    if (!reference->map->Parse(reference->mapName.data, false, parent))
      return nullptr;
  }
  src.ExpectTokenString("}");
  src.ExpectTokenString("}");
  return reference.release();
}

idMapEntity::idMapEntity()
    : entityDef(nullptr), model(), parentReference(nullptr), refId(), layerList() {}

idMapEntity::~idMapEntity() { delete entityDef; }

void idMapEntity::SetEntityDef(idDeclEntityDef* def) {
  delete entityDef;
  entityDef = def;
}

void idMapEntity::Transform(const idVec3& origin, const idMat3& axis) {
  if (entityDef == nullptr)
    return;
  if (idStr::Icmp(entityDef->GetClass(), "idWorldSpawn") == 0) {
    model.TransformPrimitives(origin, axis);
    return;
  }
  idTypeInfoTree tree;
  tree.Parse(entityDef->GetEntityStateWithInheritance());
  idVec3 position = tree.GetVec3("spawnPosition");
  idMat3 orientation = tree.GetMat3("spawnOrientation");
  position = axis * position + origin;
  orientation = orientation * axis;
  tree.SetVec3("spawnPosition", position);
  tree.SetMat3("spawnOrientation", orientation);
  entityDef->SetEntityState(&tree, true, false);
}

void idMapEntity::FixMapModelNames(const char* mapName, const char* entityName) {
  if (idLib::production == PROD_PRODUCTION || entityDef == nullptr)
    return;
  idTypeInfoTree tree;
  tree.Parse(entityDef->GetEntityStateWithInheritance());
  idStr modelName;
  if (!tree.GetStrType("renderModelInfo.model", modelName) || modelName.IsEmpty())
    return;
  idStr extension;
  modelName.ExtractFileExtension(extension);
  if (!(modelName.Icmpn("maps/", 5) == 0 || modelName.Icmpn("prefabs/", 8) == 0) ||
      !extension.IsEmpty())
    return;
  idStr base;
  modelName.ExtractFileBase(base);
  idStr newName(entityName);
  if (idStr::Icmp(base.data, newName.Right(base.Length()).data) != 0)
    return;
  newName = idMapFile::GetMapModelName(mapName, entityName);
  bool changed = newName != modelName;
  if (changed)
    tree.SetStrType("renderModelInfo.model", newName.data);
  idStr clipType;
  tree.GetStrType("clipModelInfo.type", clipType);
  if (clipType != "CLIPMODEL_CUSTOM" && clipType != "CLIPMODEL_NONE") {
    idStr oldClip;
    tree.GetStrType("clipModelInfo.clipModelName", oldClip);
    idStr newClip = idMapFile::GetMapModelName(mapName, entityName);
    if (newClip != oldClip) {
      tree.SetStrType("clipModelInfo.clipModelName", newClip.data);
      changed = true;
    }
  }
  if (changed)
    entityDef->SetEntityState(&tree, true, false);
}

void idMapEntity::FixTargetNames(const char* baseName) {
  if (entityDef == nullptr)
    return;
  idTypeInfoTree tree;
  tree.Parse(entityDef->GetEntityStateWithInheritance());
  idList<idTypeInfoNode*> nodes;
  CollectEntityReferences(&tree.root, nodes);
  for (int i = 0; i < nodes.Num(); ++i) {
    idStr value(nodes[i]->value);
    if (value.Length() >= 2 && value[0] == '"' && value[value.Length() - 1] == '"')
      value = idStr(value, 1, value.Length() - 1);
    if (!value.IsEmpty()) {
      idStr renamed(baseName ? baseName : "");
      renamed.Append("_");
      renamed.Append(value);
      nodes[i]->value = "\"";
      nodes[i]->value.Append(renamed);
      nodes[i]->value.Append("\"");
    }
  }
  entityDef->SetEntityState(&tree, true, false);
}

idMapEditorStates::idMapEditorStates() = default;
idMapEditorStates::~idMapEditorStates() = default;

void idMapEditorStates::Parse(idLexer& src) {
  ParseStateBlock(src, "groupstates", groupStates);
  ParseStateBlock(src, "layerstates", layerStates);
  if (!src.PeekTokenString("groupcolors") || !src.CheckTokenString("groupcolors") ||
      !src.ExpectTokenString("{"))
    return;
  idToken group;
  while (src.ReadToken(group)) {
    if (group == "}" || group.type != TT_STRING)
      break;
    const float red = src.ParseFloat();
    const float green = src.ParseFloat();
    const float blue = src.ParseFloat();
    src.ParseFloat();
    const auto channel = [](float value) {
      return static_cast<unsigned>(std::clamp(static_cast<int>(value * 255.0f + 0.5f), 0, 255));
    };
    idGroupState entry;
    entry.groupName = group;
    entry.state = channel(red) | (channel(green) << 8) | (channel(blue) << 16);
    bool duplicate = false;
    for (int i = 0; i < groupColors.Num(); ++i)
      duplicate |= groupColors[i].groupName == entry.groupName && groupColors[i].state == entry.state;
    if (!duplicate)
      groupColors.Append(entry);
  }
}

idMapFile::idMapFile()
    : name(), version(CURRENT_MAP_VERSION), fileTime(0), geometryCRC(0),
      hasPrimitiveData(false), entities(), entityNameHash(512, 8192), groupStates(),
      referenceMaps(), parentReference(nullptr), expandReferences(true), game(nullptr),
      entitiesAreReference(false), ignoreGroupInfo(false) {
  entities.ResizeForPort(1024);
}

idMapFile::~idMapFile() { DeletePointers(entities); }

idMapEntity* idMapFile::GetEntity(int i) const { return entities[i]; }

void idMapFile::SetGeometryCRC() {
  geometryCRC = 0;
  for (int i = 0; i < entities.Num(); ++i) {
    unsigned entityCRC = 0;
    for (int j = 0; j < entities[i]->model.primitives.Num(); ++j)
      entityCRC ^= entities[i]->model.primitives[j]->GetGeometryCRC();
    geometryCRC ^= entityCRC;
  }
}

idMapEntity* idMapFile::FindEntity(const char* entityName, int* index) {
  for (int current = entityNameHash.First(idStr::Hash(entityName)); current != -1;
       current = entityNameHash.Next(current)) {
    idMapEntity* entity = entities[current];
    if (entity == nullptr || entity->entityDef == nullptr) {
      idLib::Warning("idMapFile::FindEntity - Invalid or NULL entity in list (index = %d)", current);
      continue;
    }
    if (idStr::Icmp(entity->entityDef->name.str, entityName) == 0) {
      if (index)
        *index = current;
      return entity;
    }
  }
  return nullptr;
}

void idMapFile::RemoveEntity(idMapEntity* mapEnt) {
  if (mapEnt == nullptr || mapEnt->entityDef == nullptr)
    return;
  int index = -1;
  if (FindEntity(mapEnt->entityDef->name.str, &index) == nullptr)
    return;
  entityNameHash.RemoveIndex(idStr::Hash(mapEnt->entityDef->name.str), index);
  entities.RemoveIndex(index);
  delete mapEnt;
}

int idMapFile::FindReferenceMap(const char* mapName) const {
  for (int i = 0; i < referenceMaps.Num(); ++i)
    if (idStr::Icmp(referenceMaps[i].mapName.data, mapName) == 0)
      return i;
  return -1;
}

void idMapFile::ClearCopiedEntities(const idStr&) {
  entities.ClearFree();
  ResetEntityHash(*this);
  entitiesAreReference = false;
}

int idMapFile::AddEntity(idMapEntity* mapEnt) {
  entityNameHash.Add(idStr::Hash(mapEnt->entityDef->name.str), entities.Num());
  return entities.Append(mapEnt);
}

idStr idMapFile::CopyMapEntities(const idMapFile* map, const char* wantedRefId) {
  idStr refname;
  if (map == nullptr)
    return refname;
  for (int i = 0; i < map->entities.Num(); ++i) {
    idMapEntity* entity = map->entities[i];
    if (entity == nullptr)
      continue;
    if (wantedRefId != nullptr) {
      if (*wantedRefId ? idStr::Icmp(entity->refId.data, wantedRefId) != 0
                       : !entity->refId.IsEmpty())
        continue;
    }
    AddEntity(entity);
  }
  entitiesAreReference = true;
  return refname;
}

void idMapFile::AddReferenceMap(const char* mapName, const char* worldSpawnText) {
  idRefMapDef definition;
  definition.mapName = mapName;
  definition.worldSpawnText = worldSpawnText;
  for (int i = 0; i < referenceMaps.Num(); ++i)
    if (referenceMaps[i] == definition)
      return;
  referenceMaps.Append(definition);
}

void idMapFile::ConvertMapModelNames() {
  for (int i = 0; i < entities.Num(); ++i) {
    idDeclEntityDef* def = entities[i]->entityDef;
    if (def == nullptr)
      continue;
    idTypeInfoTree tree;
    tree.Parse(def->GetEntityState());
    idTypeInfoNode* node = tree.FindPath("renderModelInfo.model");
    if (node == nullptr)
      continue;
    idStr value(node->value);
    if (value.Length() >= 2 && value[0] == '"' && value[value.Length() - 1] == '"')
      value = idStr(value, 1, value.Length() - 1);
    if (idStr::Icmp(value.data, def->name.str) == 0) {
      idStr converted = GetMapModelName(name.data, value.data);
      node->value = "\"";
      node->value.Append(converted);
      node->value.Append("\"");
      def->SetEntityState(&tree, false, false);
    }
  }
}

bool idMapFile::Write(const char*, bool) { return false; }
bool idMapFile::WriteSounds(const char*, bool) { return false; }
bool idMapFile::WriteEntities(const char*, bool) { return false; }

idMapEntity* idMapEntity::Parse(idMapFile* mapFile, idLexer& src, int version,
                                 idMapEntity* parent) {
  idToken token;
  if (!src.ReadToken(token) || token != "{") {
    src.Error("idMapEntity::Parse: { not found, found %s", token.data);
    return nullptr;
  }
  std::unique_ptr<idMapEntity> entity(new idMapEntity);
  entity->parentReference = parent;
  idVec3 origin{};
  idVec3 firstBrushOrigin{};
  bool worldSpawn = false;
  bool foundBrush = false;

  if (!src.ReadToken(token)) {
    src.Error("idMapEntity::Parse: EOF without closing brace");
    return nullptr;
  }
  while (token != "}") {
    idList<idStr> groups;
    if (token == "groups") {
      src.UnreadToken();
      idMapGroups::ParseGroups(src, groups);
      if (!src.ReadToken(token))
        return nullptr;
    }
    if (token == "layers") {
      src.ExpectTokenString("{");
      while (src.ReadToken(token) && token != "}")
        if (token.type == TT_STRING && !Contains(entity->layerList, token))
          entity->layerList.Append(token);
      if (!src.ReadToken(token))
        return nullptr;
    }
    if (token == "entityDef" && entity->entityDef == nullptr) {
      idToken defName;
      src.ReadToken(defName);
      idStr text;
      src.ParseBracedSectionExact(text, true);
      AppendList(entity->groupList, groups);
      entity->entityDef = new idDeclEntityDef;
      entity->entityDef->SetName(defName.data);
      entity->entityDef->SetText(text.data);
      entity->entityDef->ReParse(false);
    } else if (token != "{") {
      idStr key(token);
      idToken value;
      src.ReadTokenOnLine(value);
      key.StripTrailingWhitespace();
      value.StripTrailingWhitespace();
      if (key.Icmp("origin") == 0)
        std::sscanf(value.data, "%f %f %f", &origin.x, &origin.y, &origin.z);
      else if (key.Icmp("classname") == 0 && value.Icmp("worldspawn") == 0)
        worldSpawn = true;
    } else {
      idMapGroups::ParseGroups(src, groups);
      if (!src.ReadToken(token))
        return nullptr;
      if (worldSpawn || entity->entityDef != nullptr)
        origin = {};
      if (token.Icmpn("brush", 5) == 0) {
        if (!foundBrush) {
          foundBrush = true;
          if (entity->entityDef != nullptr) {
            idTypeInfoTree tree;
            tree.Parse(entity->entityDef->GetEntityStateWithInheritance());
            firstBrushOrigin = tree.GetVec3("spawnPosition");
          }
        }
        const bool planeFormat = token.Icmp("brushDef2") == 0 || token.Icmp("brushDef3") == 0;
        idMapBrush* brush = idMapBrush::Parse(src, firstBrushOrigin, planeFormat, version);
        if (brush == nullptr)
          return nullptr;
        AppendList(brush->groupList, groups);
        entity->model.primitives.Append(brush);
      } else if (token.Icmpn("splinemesh", 10) == 0 || token.Icmpn("spline", 6) == 0) {
        src.SkipBracedSection(false);
      } else if (token.Icmp("reference") == 0) {
        idMapReference* reference = idMapReference::Parse(src, version, mapFile->expandReferences,
                                                          entity.get());
        if (reference != nullptr) {
          AppendList(reference->groupList, groups);
          if (mapFile->expandReferences)
            reference->ExpandReference(mapFile, entity.get());
          mapFile->AddReferenceMap(reference->mapName.data, reference->worldSpawnText.data);
          entity->model.primitives.Append(reference);
        } else {
          idLib::Warning("Invalid map reference in entity \"%s\"",
                         entity->entityDef ? entity->entityDef->name.str : "<unnamed>");
        }
      } else {
        src.UnreadToken();
        idMapBrush* brush = idMapBrush::ParseQ3(src, origin);
        if (brush == nullptr)
          return nullptr;
        entity->model.primitives.Append(brush);
      }
    }
    if (!src.ReadToken(token)) {
      src.Error("idMapEntity::Parse: EOF without closing brace");
      return nullptr;
    }
  }
  if (entity->entityDef == nullptr)
    return nullptr;
  entity->entityDef->SetInheritedClass(false);
  return entity.release();
}

void idMapReference::ExpandReference(idMapFile* mapFile, idMapEntity* mapEnt) {
  if (map == nullptr || mapFile == nullptr || mapEnt == nullptr || mapEnt->entityDef == nullptr)
    return;
  idStr baseName(mapEnt->entityDef->name.str);
  idTypeInfoTree tree;
  tree.Parse(mapEnt->entityDef->GetEntityStateWithInheritance());
  const bool zeroTransform = tree.GetBool("zeroTransform", false);
  idVec3 origin{};
  idMat3 axis = mat3_identity;
  bool rename = false;
  if (!zeroTransform) {
    origin = tree.GetVec3("spawnPosition");
    axis = tree.GetMat3("spawnOrientation");
    rename = true;
  }
  idMapEntity* destinationWorld = mapFile->entities.Num() > 0 ? mapFile->entities[0] : nullptr;
  for (int i = 0; i < map->entities.Num();) {
    idMapEntity* referenced = map->entities[i];
    if (referenced->refId.IsEmpty())
      referenced->refId = map->name;
    if (!zeroTransform)
      referenced->Transform(origin, axis);
    if (rename) {
      idStr renamed(baseName);
      renamed.Append("_");
      renamed.Append(referenced->entityDef->name.str);
      referenced->entityDef->SetName(renamed.data);
      referenced->FixMapModelNames(map->name.data, renamed.data);
      referenced->FixTargetNames(baseName.data);
    }
    referenced->RenameReferenceGroups(baseName.data);
    for (int p = 0; p < referenced->model.primitives.Num(); ++p)
      referenced->model.primitives[p]->RenameReferenceGroups(baseName.data);
    if (idStr::Icmp(referenced->entityDef->GetClass(), "idWorldSpawn") != 0) {
      mapFile->AddEntity(referenced);
      map->entities.RemoveIndex(i);
    } else {
      if (destinationWorld)
        destinationWorld->model.MovePrimitives(&referenced->model);
      worldSpawnText = referenced->entityDef->GetText();
      ++i;
    }
  }
  AppendList(mapFile->referenceMaps, map->referenceMaps);
}

bool idMapFile::ParseSingle(idLexer& src) {
  version = OLD_MAP_VERSION;
  fileTime = src.GetFileTime();
  DeletePointers(entities);
  ResetEntityHash(*this);
  idToken token;
  if (src.CheckTokenString("Version") && src.ReadTokenOnLine(token)) {
    if (token.type == TT_NUMBER) {
      token.NumberValue();
      version = static_cast<int>(token.intvalue);
    } else
      version = 0;
  }
  while (src.ReadToken(token)) {
    if (token == "entity" || token == "{") {
      if (token == "{")
        src.UnreadToken();
      else
        src.CheckTokenType(TT_NUMBER, 0, token);
      idMapEntity* entity = idMapEntity::Parse(this, src, version, parentReference);
      if (entity == nullptr) {
        idLib::Warning("Failed to parse entity!");
        break;
      }
      AddEntity(entity);
    } else if (token == "groupstates" || token == "groupcolors") {
      src.UnreadToken();
      groupStates.Parse(src);
    } else {
      break;
    }
  }
  SetGeometryCRC();
  for (int i = 0; i < entities.Num(); ++i) {
    entities[i]->model.name = GetMapModelName(name.data, entities[i]->entityDef->name.str);
    if (expandReferences)
      entities[i]->FixMapModelNames(name.data, entities[i]->entityDef->name.str);
  }
  if (version <= 2)
    ConvertMapModelNames();
  hasPrimitiveData = true;
  return true;
}

bool idMapFile::Parse(const char* filename, bool osPath, idMapEntity* parent) {
  idStr fullName(filename ? filename : "");
  name = filename ? filename : "";
  name.BackSlashesToSlashes();
  name.StripFileExtension();
  hasPrimitiveData = false;
  parentReference = parent;
  fullName.ToLower();
  fullName.DefaultFileExtension("map");
  idLexer src(560);
  if (!src.LoadFile(fullName.data, osPath) || !ParseSingle(src))
    return false;
  if (fullName.Right(8).Icmp("entities") != 0) {
    fullName.SetFileExtension("sound");
    idLexer soundLexer(560);
    if (soundLexer.LoadFile(fullName.data, osPath)) {
      idMapFile soundMap;
      soundMap.name = name;
      if (soundMap.ParseSingle(soundLexer)) {
        if (soundMap.entities.Num() > 0 && entities.Num() > 0)
          entities[0]->model.MovePrimitives(&soundMap.entities[0]->model);
        for (int i = 1; i < soundMap.entities.Num(); ++i)
          AddEntity(soundMap.entities[i]);
        if (soundMap.entities.Num() > 0)
          delete soundMap.entities[0];
        soundMap.entities.ClearFree();
      }
    }
  }
  return true;
}

bool idMapFile::Reload() {
  DeletePointers(entities);
  ResetEntityHash(*this);
  hasPrimitiveData = false;
  if (Parse(name.data, false, nullptr))
    return true;
  idLib::Warning("Map reload failed!");
  return false;
}
