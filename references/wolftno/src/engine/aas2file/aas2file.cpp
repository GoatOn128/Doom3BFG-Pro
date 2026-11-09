#include "engine/aas2file/aas2file.h"

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/text/lexer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

namespace {
constexpr std::array<char, 8> kAAS2Magic{'W', 'T', 'N', 'A', 'A', 'S', '2', '\0'};
constexpr std::uint32_t kAAS2Version = 1;

template <typename T> bool ReadValue(std::istream& stream, T& value) {
  return static_cast<bool>(stream.read(reinterpret_cast<char*>(&value), sizeof(value)));
}
template <typename T> void WriteValue(std::ostream& stream, const T& value) {
  stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
template <typename T> bool ReadArray(std::istream& stream, std::vector<T>& values) {
  std::uint32_t count{};
  if (!ReadValue(stream, count) || count > 16'000'000u) return false;
  values.resize(count);
  return count == 0 || static_cast<bool>(stream.read(reinterpret_cast<char*>(values.data()),
                                                      sizeof(T) * count));
}
template <typename T> void WriteArray(std::ostream& stream, const std::vector<T>& values) {
  const std::uint32_t count = static_cast<std::uint32_t>(values.size());
  WriteValue(stream, count);
  if (count) stream.write(reinterpret_cast<const char*>(values.data()), sizeof(T) * count);
}
bool ReadString(std::istream& stream, idStr& value) {
  std::uint32_t length{};
  if (!ReadValue(stream, length) || length > 1'048'576u) return false;
  std::string text(length, '\0');
  if (length && !stream.read(text.data(), length)) return false;
  value = text.c_str();
  return true;
}
void WriteString(std::ostream& stream, const idStr& value) {
  const std::uint32_t length = static_cast<std::uint32_t>(value.Length());
  WriteValue(stream, length);
  if (length) stream.write(value.data, length);
}
bool PointInside(const idVec3& point, const idBounds& bounds, float epsilon = 0.0f) {
  return point.x >= bounds[0].x - epsilon && point.x <= bounds[1].x + epsilon &&
         point.y >= bounds[0].y - epsilon && point.y <= bounds[1].y + epsilon &&
         point.z >= bounds[0].z - epsilon && point.z <= bounds[1].z + epsilon;
}
bool BoundsIntersect(const idBounds& a, const idBounds& b) {
  for (int axis = 0; axis < 3; ++axis) {
    if (a[1][axis] < b[0][axis] || a[0][axis] > b[1][axis]) return false;
  }
  return true;
}
idVec3 BoundsCenter(const idBounds& bounds) { return (bounds[0] + bounds[1]) * 0.5f; }

bool ParseVec3(idLexer* source, idVec3& value) {
  float values[3]{};
  if (!source || !source->Parse1DMatrix(3, values)) return false;
  value = {values[0], values[1], values[2]};
  return true;
}
bool ParseBoundsValue(idLexer* source, idBounds& value) {
  return source && ParseVec3(source, value[0]) && ParseVec3(source, value[1]);
}
bool ParseCount(idLexer* source, int& count) {
  if (!source) return false;
  count = source->ParseInt();
  return !source->HadError() && count >= 0 && count <= 16'000'000;
}
bool ParseNameList(idLexer* source, std::vector<aas2Name_t>* names) {
  if (!source || !names) return false;
  int count{};
  if (!ParseCount(source, count)) return false;
  names->clear();
  names->reserve(count);
  for (int i = 0; i < count; ++i) {
    idToken token;
    if (!source->ReadToken(token)) return false;
    names->push_back({token});
  }
  return true;
}
template <typename T> int FindName(const std::vector<T>& names, const char* value) {
  if (!value) return -1;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (idStr::Icmp(names[i].name.data, value) == 0) return static_cast<int>(i);
  }
  return -1;
}
template <typename T> bool GetName(const std::vector<T>& names, int index, const T** output) {
  if (!output || index < 0 || static_cast<std::size_t>(index) >= names.size()) return false;
  *output = &names[index];
  return true;
}
} // namespace

aas2Cover_t::aas2Cover_t() = default;
aas2Traversal_t::aas2Traversal_t() { Clear(); }
void aas2Traversal_t::Clear() {
  startArea = goalArea = nameIndex = interactionEntityIndex = reachabilityIndex = -1;
  flags = 0;
  start = end = {};
}
void aas2Traversal_t::CalcExtrusionPoint(idVec3& extrusionPoint, const idVec3& up) const {
  idVec3 direction = end - start;
  const float projected = direction * up;
  direction -= up * projected;
  if (direction.Normalize() == 0.0f) direction = {1.0f, 0.0f, 0.0f};
  extrusionPoint = (start + end) * 0.5f + direction * 8.0f;
}
aas2HintNode_t::aas2HintNode_t() { bounds.Clear(); }
aas2Area_t::aas2Area_t() { bounds.Clear(); }

idAAS2Settings::idAAS2Settings() = default;
bool idAAS2Settings::ParseBool(idLexer* source, bool* value) {
  if (!source || !value) return false;
  *value = source->ParseBool();
  return !source->HadError();
}
bool idAAS2Settings::ParseInt(idLexer* source, int* value) {
  if (!source || !value) return false;
  *value = source->ParseInt();
  return !source->HadError();
}
bool idAAS2Settings::ParseFloat(idLexer* source, float* value) {
  if (!source || !value) return false;
  bool error{};
  *value = source->ParseFloat(&error);
  return !error;
}
bool idAAS2Settings::ParseBounds(idLexer* source, idBounds* value) {
  return value && ParseBoundsValue(source, *value);
}
bool idAAS2Settings::ValidForBounds(const idBounds* value) const {
  if (!value) return false;
  const idVec3 size = (*value)[1] - (*value)[0];
  const idVec3 required = boundingBox[1] - boundingBox[0];
  return size.x >= required.x && size.y >= required.y && size.z >= required.z;
}
bool idAAS2Settings::ReadFromFile(idLexer* source) {
  if (!source || !source->ExpectTokenString("{")) return false;
  idToken key;
  while (source->ReadToken(key)) {
    if (key == "}") return !source->HadError();
    if (key == "boundingBox") {
      if (!ParseBounds(source, &boundingBox)) return false;
    } else if (key == "maxStepHeight") {
      if (!ParseFloat(source, &maxStepHeight)) return false;
    } else if (key == "maxBarrierHeight") {
      if (!ParseFloat(source, &maxBarrierHeight)) return false;
    } else if (key == "maxFallHeight") {
      if (!ParseInt(source, &maxFallHeight)) return false;
    } else if (key == "usePatches") {
      if (!ParseBool(source, &usePatches)) return false;
    } else {
      source->SkipRestOfLine();
    }
  }
  return false;
}
bool idAAS2Settings::WriteToFileBinary(idFile* file) const {
  return file && file->Write(this, sizeof(*this)) == sizeof(*this);
}
bool idAAS2Settings::ReadFromFileBinary(idFile* file) {
  return file && file->Read(this, sizeof(*this)) == sizeof(*this);
}

idAAS2File::idAAS2File() {
  auto& resources = const_cast<std::vector<idAAS2File*>&>(GetResourceList());
  resources.push_back(this);
  MakeDefault();
}
idAAS2File::~idAAS2File() {
  auto& resources = const_cast<std::vector<idAAS2File*>&>(GetResourceList());
  resources.erase(std::remove(resources.begin(), resources.end(), this), resources.end());
}
const std::vector<idAAS2File*>& idAAS2File::GetResourceList() {
  static std::vector<idAAS2File*> resources;
  return resources;
}
void idAAS2File::Clear() {
  vertices.clear(); planes.clear(); edges.clear(); areas.clear(); nodes.clear();
  reachabilities.clear(); traversals.clear(); cover.clear(); hintNodes.clear(); portals.clear();
  clusters.clear(); trees.clear(); animNames.clear(); dependencyNames.clear();
  interactionEntityNames.clear(); traversalEntityNames.clear();
  mapCRC = timestamp = 0;
  valid = false;
}
void idAAS2File::MakeDefault() {
  Clear();
  name = "default";
  vertices = {{-64.0f, -64.0f, 0.0f}, {64.0f, -64.0f, 0.0f},
              {64.0f, 64.0f, 0.0f}, {-64.0f, 64.0f, 0.0f}};
  edges = {{{0, 1}, 0}, {{1, 2}, 0}, {{2, 3}, 0}, {{3, 0}, 0}};
  aas2Area_t area;
  area.edges = {0, 1, 2, 3};
  areas.push_back(area);
  trees.push_back({0, {0.0f, 0.0f, 1.0f}, {0}});
  CalculateAreaBounds();
  valid = true;
}
std::size_t idAAS2File::MemorySize() const {
  std::size_t bytes = sizeof(*this) + vertices.capacity() * sizeof(idVec3) +
                      planes.capacity() * sizeof(idPlane) + edges.capacity() * sizeof(aas2Edge_t) +
                      areas.capacity() * sizeof(aas2Area_t) + nodes.capacity() * sizeof(aas2Node_t) +
                      reachabilities.capacity() * sizeof(aas2Reachability_t) +
                      traversals.capacity() * sizeof(aas2Traversal_t);
  for (const auto& area : areas) bytes += area.edges.capacity() * sizeof(int) + area.reachabilities.capacity() * sizeof(int);
  return bytes;
}
void idAAS2File::EndianSwap() {
  // Recovered runtime is little-endian; serialization is intentionally fixed to little-endian.
}

bool idAAS2File::WriteBinary(const char* fileName, const char* binaryFileName,
                             unsigned mapFileCRC, unsigned sourceTimestamp) const {
  if (!binaryFileName) return false;
  std::ofstream stream(binaryFileName, std::ios::binary | std::ios::trunc);
  if (!stream) return false;
  stream.write(kAAS2Magic.data(), kAAS2Magic.size());
  WriteValue(stream, kAAS2Version); WriteValue(stream, mapFileCRC); WriteValue(stream, sourceTimestamp);
  WriteString(stream, name); idStr source(fileName ? fileName : ""); WriteString(stream, source);
  WriteValue(stream, settings.boundingBox); WriteValue(stream, settings.maxStepHeight);
  WriteValue(stream, settings.maxBarrierHeight); WriteValue(stream, settings.maxFallHeight);
  WriteValue(stream, settings.usePatches);
  WriteArray(stream, vertices); WriteArray(stream, planes); WriteArray(stream, edges);
  const std::uint32_t areaCount = static_cast<std::uint32_t>(areas.size()); WriteValue(stream, areaCount);
  for (const auto& area : areas) {
    WriteValue(stream, area.bounds); WriteValue(stream, area.center); WriteValue(stream, area.flags);
    WriteValue(stream, area.travelFlags); WriteValue(stream, area.treeNum);
    WriteArray(stream, area.edges); WriteArray(stream, area.reachabilities);
  }
  const std::uint32_t reachCount = static_cast<std::uint32_t>(reachabilities.size()); WriteValue(stream, reachCount);
  for (const auto& reach : reachabilities) {
    WriteValue(stream, reach.fromArea); WriteValue(stream, reach.toArea); WriteValue(stream, reach.travelFlags); WriteString(stream, reach.name);
  }
  WriteArray(stream, nodes); WriteArray(stream, traversals); WriteArray(stream, cover);
  WriteArray(stream, hintNodes); WriteArray(stream, portals);
  return static_cast<bool>(stream);
}

bool idAAS2File::LoadBinary(const char* fileName, const char* binaryFileName,
                            unsigned expectedCRC, unsigned expectedTimestamp) {
  if (!binaryFileName) return false;
  std::ifstream stream(binaryFileName, std::ios::binary);
  std::array<char, 8> magic{}; std::uint32_t version{}, fileCRC{}, fileTimestamp{};
  if (!stream.read(magic.data(), magic.size()) || magic != kAAS2Magic ||
      !ReadValue(stream, version) || version != kAAS2Version || !ReadValue(stream, fileCRC) ||
      !ReadValue(stream, fileTimestamp) || (expectedCRC && fileCRC != expectedCRC) ||
      (expectedTimestamp && fileTimestamp != expectedTimestamp)) return false;
  Clear();
  if (!ReadString(stream, name) || !ReadString(stream, sourcePath) ||
      !ReadValue(stream, settings.boundingBox) || !ReadValue(stream, settings.maxStepHeight) ||
      !ReadValue(stream, settings.maxBarrierHeight) || !ReadValue(stream, settings.maxFallHeight) ||
      !ReadValue(stream, settings.usePatches) || !ReadArray(stream, vertices) ||
      !ReadArray(stream, planes) || !ReadArray(stream, edges)) return false;
  std::uint32_t areaCount{};
  if (!ReadValue(stream, areaCount) || areaCount > 16'000'000u) return false;
  areas.resize(areaCount);
  for (auto& area : areas) {
    if (!ReadValue(stream, area.bounds) || !ReadValue(stream, area.center) ||
        !ReadValue(stream, area.flags) || !ReadValue(stream, area.travelFlags) ||
        !ReadValue(stream, area.treeNum) || !ReadArray(stream, area.edges) ||
        !ReadArray(stream, area.reachabilities)) return false;
  }
  std::uint32_t reachCount{};
  if (!ReadValue(stream, reachCount) || reachCount > 16'000'000u) return false;
  reachabilities.resize(reachCount);
  for (auto& reach : reachabilities) {
    if (!ReadValue(stream, reach.fromArea) || !ReadValue(stream, reach.toArea) ||
        !ReadValue(stream, reach.travelFlags) || !ReadString(stream, reach.name)) return false;
  }
  if (!ReadArray(stream, nodes) || !ReadArray(stream, traversals) || !ReadArray(stream, cover) ||
      !ReadArray(stream, hintNodes) || !ReadArray(stream, portals)) return false;
  mapCRC = fileCRC; timestamp = fileTimestamp;
  if (fileName) sourcePath = fileName;
  valid = true;
  if (trees.empty()) {
    bspTree_t tree; tree.areas.resize(areas.size());
    for (std::size_t i = 0; i < areas.size(); ++i) tree.areas[i] = static_cast<int>(i);
    trees.push_back(std::move(tree));
  }
  return true;
}
void idAAS2File::LoadResource() { valid = !sourcePath.IsEmpty() && LoadResource(sourcePath.data); }
bool idAAS2File::LoadResource(const char* path) {
  if (!path) return false;
  sourcePath = path;
  return valid = LoadBinary(path, path, 0, 0);
}
bool idAAS2File::ReloadIfStale() { return !sourcePath.IsEmpty() && LoadBinary(sourcePath.data, sourcePath.data, 0, 0); }

bool idAAS2File::ParseIndex(idLexer* source, std::vector<int>* indexes) {
  if (!source || !indexes) return false; int count{}; if (!ParseCount(source, count)) return false;
  indexes->resize(count); for (int& value : *indexes) value = source->ParseInt(); return !source->HadError();
}
bool idAAS2File::ParseVertices(idLexer* source) {
  int count{}; if (!ParseCount(source, count)) return false; vertices.resize(count);
  for (auto& vertex : vertices) if (!ParseVec3(source, vertex)) return false; return true;
}
bool idAAS2File::ParseObstaclePVS(idLexer* source) { return source && source->SkipBracedSection(); }
bool idAAS2File::ParseNames(idLexer* source, std::vector<aas2Name_t>* names) { return ParseNameList(source, names); }
bool idAAS2File::ParseAnimNames(idLexer* source, std::vector<aas2AnimName_t>* names) { return ParseNameList(source, names); }
bool idAAS2File::ParseDependencyNames(idLexer* source, std::vector<aas2DependencyName_t>* names) { return ParseNameList(source, names); }
bool idAAS2File::ParseInteractionEntityNames(idLexer* source, std::vector<aas2InteractionEntityName_t>* names) { return ParseNameList(source, names); }
bool idAAS2File::ParseTraversalEntityNames(idLexer* source, std::vector<aas2TraversalEntityName_t>* names) { return ParseNameList(source, names); }
bool idAAS2File::ParsePlanes(idLexer* source) {
  int count{}; if (!ParseCount(source, count)) return false; planes.resize(count);
  for (auto& plane : planes) {
    float values[4]{};
    if (!source->Parse1DMatrix(4, values)) return false;
    plane = {values[0], values[1], values[2], values[3]};
  }
  return true;
}
bool idAAS2File::ParseEdges(idLexer* source) {
  int count{}; if (!ParseCount(source, count)) return false; edges.resize(count);
  for (auto& edge : edges) { edge.vertex[0] = source->ParseInt(); edge.vertex[1] = source->ParseInt(); edge.flags = source->ParseInt(); }
  return !source->HadError();
}
bool idAAS2File::ParseReachabilities(idLexer* source) {
  int count{}; if (!ParseCount(source, count)) return false; reachabilities.resize(count);
  for (auto& reach : reachabilities) {
    reach.fromArea = source->ParseInt(); reach.toArea = source->ParseInt(); reach.travelFlags = source->ParseInt();
    idToken token; if (!source->ReadToken(token)) return false; reach.name = token;
  } return !source->HadError();
}
bool idAAS2File::ParseAreas(idLexer* source) {
  int count{}; if (!ParseCount(source, count)) return false; areas.resize(count);
  for (auto& area : areas) {
    if (!ParseBoundsValue(source, area.bounds)) return false;
    area.flags = static_cast<std::uint16_t>(source->ParseInt()); area.travelFlags = source->ParseInt(); area.treeNum = source->ParseInt();
    if (!ParseIndex(source, &area.edges) || !ParseIndex(source, &area.reachabilities)) return false;
  } CalculateAreaBounds(); return !source->HadError();
}
bool idAAS2File::ParseNodes(idLexer* source) {
  int count{}; if (!ParseCount(source, count)) return false; nodes.resize(count);
  for (auto& node : nodes) {
    float values[4]{};
    if (!source->Parse1DMatrix(4, values)) return false;
    node.plane = {values[0], values[1], values[2], values[3]};
    node.children[0]=source->ParseInt(); node.children[1]=source->ParseInt();
    if(!ParseBoundsValue(source,node.bounds)) return false;
  }
  return !source->HadError();
}
bool idAAS2File::ParsePortals(idLexer* source) {
  int count{}; if (!ParseCount(source, count)) return false; portals.resize(count);
  for(auto& p:portals){p.areas[0]=source->ParseInt();p.areas[1]=source->ParseInt();} return !source->HadError();
}
bool idAAS2File::ParseClusters(idLexer* source) {
  int count{}; if(!ParseCount(source,count))return false;clusters.resize(count);for(auto& c:clusters)if(!ParseIndex(source,&c.areas))return false;return true;
}
bool idAAS2File::ParseCover(idLexer* source) {
  int count{};if(!ParseCount(source,count))return false;cover.resize(count);for(auto& c:cover){if(!ParseVec3(source,c.origin)||!ParseVec3(source,c.normal))return false;c.dir=c.normal;c.areaNum=source->ParseInt();c.valid=true;}return !source->HadError();
}
bool idAAS2File::ParseTraversalPoints(idLexer* source) {
  int count{};if(!ParseCount(source,count))return false;traversals.resize(count);for(auto& t:traversals){t.startArea=source->ParseInt();t.goalArea=source->ParseInt();t.flags=source->ParseInt();if(!ParseVec3(source,t.start)||!ParseVec3(source,t.end))return false;}return !source->HadError();
}
bool idAAS2File::ParseHintNodes(idLexer* source) {
  int count{};if(!ParseCount(source,count))return false;hintNodes.resize(count);for(auto& h:hintNodes){h.areaNum=source->ParseInt();if(!ParseBoundsValue(source,h.bounds))return false;}return !source->HadError();
}
bool idAAS2File::ParseTrees(idLexer* source) {
  int count{};if(!ParseCount(source,count))return false;trees.resize(count);for(auto& t:trees){t.rootNode=source->ParseInt();if(!ParseVec3(source,t.floorNormal)||!ParseIndex(source,&t.areas))return false;}return !source->HadError();
}

void idAAS2File::CalculateAreaBounds() {
  for (auto& area : areas) {
    if (!area.edges.empty()) {
      area.bounds.Clear();
      for (int encoded : area.edges) {
        const int edgeNum = std::abs(encoded); if (edgeNum < 0 || static_cast<std::size_t>(edgeNum) >= edges.size()) continue;
        const auto& edge = edges[edgeNum];
        for (int vertex : edge.vertex) if (vertex >= 0 && static_cast<std::size_t>(vertex) < vertices.size()) area.bounds.AddPoint(vertices[vertex]);
      }
    }
    area.center = BoundsCenter(area.bounds);
  }
}
bool idAAS2File::GetAASAnim(int index, const aas2AnimName_t** value) const { return GetName(animNames,index,value); }
int idAAS2File::GetAASAnimIndexByName(const char* value) const { return FindName(animNames,value); }
bool idAAS2File::GetAASDependency(int index, const aas2DependencyName_t** value) const { return GetName(dependencyNames,index,value); }
int idAAS2File::GetAASDependencyIndexByName(const char* value) const { return FindName(dependencyNames,value); }
bool idAAS2File::GetAASInteractionEntity(int index, const aas2InteractionEntityName_t** value) const { return GetName(interactionEntityNames,index,value); }
int idAAS2File::GetAASInteractionEntityIndexByName(const char* value) const { return FindName(interactionEntityNames,value); }
bool idAAS2File::GetAASTraversalNameIndex(int index, const aas2TraversalEntityName_t** value) const { return GetName(traversalEntityNames,index,value); }
int idAAS2File::GetAASTraversalNameIndexByName(const char* value) const { return FindName(traversalEntityNames,value); }
int idAAS2File::GetAASTraversalIndexByNameIndex(int index) const { for(std::size_t i=0;i<traversals.size();++i)if(traversals[i].nameIndex==index)return static_cast<int>(i);return -1; }
bool idAAS2File::SetTraversalFlag(int index,int flags){if(index<0||static_cast<std::size_t>(index)>=traversals.size())return false;traversals[index].flags|=flags;return true;}
bool idAAS2File::ClearTraversalFlag(int index,int flags){if(index<0||static_cast<std::size_t>(index)>=traversals.size())return false;traversals[index].flags&=~flags;return true;}
bool idAAS2File::GetAASTraversalAreas(int index,int* startArea,int* goalArea)const{if(!startArea||!goalArea||index<0||static_cast<std::size_t>(index)>=traversals.size())return false;*startArea=traversals[index].startArea;*goalArea=traversals[index].goalArea;return true;}
int idAAS2File::FindReachabilityByName(const char* value)const{if(!value)return -1;for(std::size_t i=0;i<reachabilities.size();++i)if(idStr::Icmp(reachabilities[i].name.data,value)==0)return static_cast<int>(i);return -1;}
int idAAS2File::GetTraversalsForReachability(int index,std::vector<int>* out)const{if(!out)return 0;out->clear();for(std::size_t i=0;i<traversals.size();++i)if(traversals[i].reachabilityIndex==index)out->push_back(static_cast<int>(i));return static_cast<int>(out->size());}
int idAAS2File::GetTraversalsForInteractableEntity(int index,std::vector<int>* out)const{if(!out)return 0;out->clear();for(std::size_t i=0;i<traversals.size();++i)if(traversals[i].interactionEntityIndex==index)out->push_back(static_cast<int>(i));return static_cast<int>(out->size());}
void idAAS2File::FlagNoPushAreas(){for(auto& area:areas)if((area.flags&AAS2_AREA_REACHABLE)==0)area.flags|=AAS2_AREA_NO_PUSH;}
void idAAS2File::ResetCover(){for(auto& item:cover)item.valid=true;}
int idAAS2File::GetTreeForFloorNormal(const idVec3* normal)const{if(!normal)return -1;float best=-2.0f;int result=-1;for(std::size_t i=0;i<trees.size();++i){float dot=trees[i].floorNormal*(*normal);if(dot>best){best=dot;result=static_cast<int>(i);}}return result;}
int idAAS2File::GetTreeForArea(int areaNum)const{return areaNum>=0&&static_cast<std::size_t>(areaNum)<areas.size()?areas[areaNum].treeNum:-1;}
int idAAS2File::GetNumAreasInTree(int treeNum)const{return treeNum>=0&&static_cast<std::size_t>(treeNum)<trees.size()?static_cast<int>(trees[treeNum].areas.size()):0;}
