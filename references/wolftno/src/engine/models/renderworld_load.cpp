#include "engine/models/renderworld.h"

#include "engine/models/rendermodeldecal.h"
#include "engine/models/renderworld_load.h"
#include "engine/models/static/staticmodel.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/jobs/render/renderwalkbsp.h"
#include "engine/renderer/renderworldcommitted.h"
#include "shared/idlib/geometry/polytope.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/lexer.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

template bool idList<idStaticModelSurface, 84>::Resize(int);

namespace {

idRenderWorldRuntimeCallbacks loadCallbacks;

void AddBounds(idBounds& destination, const idBounds& source) {
  destination.b[0].x = std::min(destination.b[0].x, source.b[0].x);
  destination.b[0].y = std::min(destination.b[0].y, source.b[0].y);
  destination.b[0].z = std::min(destination.b[0].z, source.b[0].z);
  destination.b[1].x = std::max(destination.b[1].x, source.b[1].x);
  destination.b[1].y = std::max(destination.b[1].y, source.b[1].y);
  destination.b[1].z = std::max(destination.b[1].z, source.b[1].z);
}

void FreeAreaData(idRenderWorldCommitted* world) {
  delete[] world->worldAreas;
  delete[] world->worldAreaSubspaceBounds;
  delete[] world->worldAreaGeometryBounds;
  world->worldAreas = nullptr;
  world->worldAreaSubspaceBounds = nullptr;
  world->worldAreaGeometryBounds = nullptr;
  world->numWorldAreas = 0;
}

void FreeNodeData(idRenderWorldCommitted* world) {
  delete[] static_cast<areaNode_t*>(world->areaNodes);
  delete[] world->areaNodeBounds;
  delete[] world->areaNodeOcclusionIndex;
  world->areaNodes = nullptr;
  world->areaNodeBounds = nullptr;
  world->areaNodeOcclusionIndex = nullptr;
  world->numAreaNodes = 0;
}

void AppendVirtualTexture(idList<idVirtualTexture*, 61>& textures,
                          idVirtualTexture* texture) {
  if (texture == nullptr)
    return;
  for (int index = 0; index < textures.Num(); ++index) {
    if (textures[index] == texture)
      return;
  }
  textures.Append(texture);
}

idStr MapModelName(const idStr& mapName, const char* suffix) {
  idStr result(mapName);
  result.StripFileExtension();
  result.AppendPath(suffix);
  return result;
}

struct vertexCopyEntry_t {
  unsigned mask;
  int size;
};

int BuildVertexCopyTable(unsigned vertexMask, vertexCopyEntry_t (&table)[8]) {
  int count = 0;
  if ((vertexMask & 0x20U) != 0U)
    table[count++] = {0x20U, 8};
  else if ((vertexMask & 0x01U) != 0U)
    table[count++] = {0x01U, 12};
  if ((vertexMask & 0x40U) != 0U)
    table[count++] = {0x40U, 4};
  else if ((vertexMask & 0x02U) != 0U)
    table[count++] = {0x02U, 8};
  if ((vertexMask & 0x04U) != 0U)
    table[count++] = {0x04U, 4};
  if ((vertexMask & 0x10U) != 0U)
    table[count++] = {0x10U, 4};
  if ((vertexMask & 0x08U) != 0U)
    table[count++] = {0x08U, 4};
  return count;
}

int VertexSizeForMask(unsigned vertexMask) {
  return static_cast<int>(
      4U * (vertexMask & 2U) + ((vertexMask & 1U) != 0U ? 12U : 0U) +
      (vertexMask & 4U) + ((vertexMask >> 1U) & 4U) +
      ((vertexMask >> 2U) & 4U) +
      ((vertexMask & 0x20U) != 0U ? 0xFFFFFFFCU : 0U) +
      ((vertexMask & 0x40U) != 0U ? 0xFFFFFFFCU : 0U) +
      ((vertexMask & 0x1000U) != 0U ? 12U : 0U) +
      ((vertexMask >> 9U) & 4U));
}

} // namespace

void SetRenderWorldLoadRuntimeCallbacks(
    const idRenderWorldRuntimeCallbacks& callbacks) {
  loadCallbacks = callbacks;
}

worldArea_t::worldArea_t() {
  modelRefs.ClearFree();
  drawLightRefs.ClearFree();
  approximatedLightRefs.ClearFree();
  approximatedLightPrivateLightRefs.ClearFree();
  worldSurfaces.ClearFree();
}

worldArea_t::~worldArea_t() = default;

void idVirtualTexture::SetMapName(const char* mapName_) {
  idStrStatic<128> temporary;
  temporary = mapName_;
  mapName = temporary;
}

void idRenderWorldLocal::AssignNodeOcclusionIndices_r(
    int nodeNum, int depth, int& numOcclusionIndices) {
  auto* nodes = static_cast<areaNode_t*>(committedWorld->areaNodes);
  if (depth == 6) {
    if (numOcclusionIndices < 64)
      committedWorld->areaNodeOcclusionIndex[nodeNum] =
          numOcclusionIndices++;
    return;
  }

  const int firstChild = nodes[nodeNum].children[0];
  if (firstChild > 0)
    AssignNodeOcclusionIndices_r(firstChild, depth + 1,
                                 numOcclusionIndices);
  const int secondChild = nodes[nodeNum].children[1];
  if (secondChild > 0)
    AssignNodeOcclusionIndices_r(secondChild, depth + 1,
                                 numOcclusionIndices);
}

idBounds idRenderWorldLocal::BuildNodeBounds_r(int nodeNum, idPlane* planes,
                                               int numPlanes) {
  if (nodeNum < 0) {
    const int areaIndex = -1 - nodeNum;
    worldArea_t& area = committedWorld->worldAreas[areaIndex];
    idBounds subspaceBounds =
        idPolytope::ConvexPolytopeBounds(planes, numPlanes);
    idBounds geometryBounds;
    geometryBounds.Clear();
    for (int index = 0; index < area.worldSurfaces.Num(); ++index) {
      const idRenderModelSurface& surface = area.worldSurfaces[index];
      if (surface.geometry != nullptr)
        AddBounds(geometryBounds, surface.geometry->bounds);
    }
    if (geometryBounds.b[0].x > geometryBounds.b[1].x) {
      const idVec3 center = (subspaceBounds.b[0] + subspaceBounds.b[1]) * 0.5f;
      geometryBounds.b[0] = center;
      geometryBounds.b[1] = center;
    }
    committedWorld->worldAreaSubspaceBounds[areaIndex] = subspaceBounds;
    AddBounds(committedWorld->worldAreaSubspaceBounds[areaIndex],
              geometryBounds);
    committedWorld->worldAreaGeometryBounds[areaIndex] = geometryBounds;
    return committedWorld->worldAreaSubspaceBounds[areaIndex];
  }

  auto* nodes = static_cast<areaNode_t*>(committedWorld->areaNodes);
  const areaNode_t& node = nodes[nodeNum];
  idBounds bounds;
  if (node.children[0] != 0 && node.children[1] != 0) {
    planes[numPlanes] = {-node.plane.a, -node.plane.b, -node.plane.c,
                         -node.plane.d};
    bounds = BuildNodeBounds_r(node.children[0], planes, numPlanes + 1);
    planes[numPlanes] = node.plane;
    AddBounds(bounds,
              BuildNodeBounds_r(node.children[1], planes, numPlanes + 1));
  } else if (node.children[0] != 0) {
    planes[numPlanes] = {-node.plane.a, -node.plane.b, -node.plane.c,
                         -node.plane.d};
    bounds = BuildNodeBounds_r(node.children[0], planes, numPlanes + 1);
  } else {
    planes[numPlanes] = node.plane;
    bounds = BuildNodeBounds_r(node.children[1], planes, numPlanes + 1);
  }
  committedWorld->areaNodeBounds[nodeNum] = bounds;
  return bounds;
}

void idRenderWorldLocal::ParseNodes(idLexer* source) {
  source->ExpectTokenString("{");
  int nodeCount = source->ParseInt();
  committedWorld->numAreaNodes = nodeCount;
  if (nodeCount < 0) {
    source->Error("R_ParseNodes: bad numAreaNodes");
    return;
  }
  if (committedWorld->isComboMap && nodeCount > 8192) {
    source->Error("R_ParseNodes: numAreaNodes exceeds %d", 8192);
    return;
  }
  const bool emptyTree = nodeCount == 0;
  if (emptyTree)
    nodeCount = 1;
  FreeNodeData(committedWorld);
  committedWorld->numAreaNodes = nodeCount;
  committedWorld->areaNodes = new areaNode_t[nodeCount]{};
  committedWorld->areaNodeBounds = new idBounds[nodeCount]{};
  committedWorld->areaNodeOcclusionIndex = new int[nodeCount];

  auto* nodes = static_cast<areaNode_t*>(committedWorld->areaNodes);
  if (emptyTree) {
    nodes[0].plane = {0.0f, 0.0f, 1.0f, 131072.0f};
    nodes[0].children[0] = -1;
    nodes[0].children[1] = 0;
    committedWorld->areaNodeOcclusionIndex[0] = -1;
  } else {
    for (int index = 0; index < nodeCount; ++index) {
      source->Parse1DMatrix(4, &nodes[index].plane.a);
      nodes[index].children[0] = source->ParseInt();
      nodes[index].children[1] = source->ParseInt();
      committedWorld->areaNodeOcclusionIndex[index] = -1;
    }
  }
  source->ExpectTokenString("}");
}

void idRenderWorldLocal::ParseInterAreaPortals(idLexer* source) {
  source->ExpectTokenString("{");
  const int areaCount = source->ParseInt();
  committedWorld->numWorldAreas = areaCount;
  if (areaCount < 0 || areaCount > 8192) {
    source->Error("R_ParseInterAreaPortals: bad numPortalAreas");
    return;
  }
  FreeAreaData(committedWorld);
  committedWorld->numWorldAreas = areaCount;
  committedWorld->worldAreas = new worldArea_t[areaCount];
  committedWorld->worldAreaSubspaceBounds = new idBounds[areaCount];
  committedWorld->worldAreaGeometryBounds = new idBounds[areaCount];
  if (source->ParseInt() < 0) {
    source->Error("R_ParseInterAreaPortals: bad numInterAreaPortals");
    return;
  }
  source->ExpectTokenString("}");
}

void idRenderWorldLocal::ParseAreas(idLexer* source) {
  committedWorld->hasWorldSurfaceOffsets = true;
  source->ExpectTokenString("{");
  const int areaCount = source->ParseInt();
  committedWorld->numWorldAreas = areaCount;
  if (areaCount < 0 || areaCount > 8192) {
    source->Error("R_ParseAreas: bad numAreas");
    return;
  }
  FreeAreaData(committedWorld);
  committedWorld->numWorldAreas = areaCount;
  committedWorld->worldAreas = new worldArea_t[areaCount];
  committedWorld->worldAreaSubspaceBounds = new idBounds[areaCount];
  committedWorld->worldAreaGeometryBounds = new idBounds[areaCount];
  for (int index = 0; index < areaCount; ++index) {
    worldArea_t& area = committedWorld->worldAreas[index];
    area.firstSurfaceInWorldModel = source->ParseInt();
    area.worldSurfaces.SetNum(source->ParseInt());
  }
  source->ExpectTokenString("}");
}

void idRenderWorldLocal::ClearWorld() {
  FreeAreaData(committedWorld);
  FreeNodeData(committedWorld);
  committedWorld->numWorldAreas = 1;
  committedWorld->worldAreas = new worldArea_t[1];
  committedWorld->worldAreas[0].areaNum = 0;
  committedWorld->worldAreas[0].firstSurfaceInWorldModel = 0;
  committedWorld->worldAreas[0].binaryModelId = -1;
  committedWorld->worldAreaSubspaceBounds = new idBounds[1];
  committedWorld->worldAreaGeometryBounds = new idBounds[1];
  constexpr float worldLimit = 131072.0f;
  const idBounds maximumWorldBounds{{{-worldLimit, -worldLimit, -worldLimit},
                                      {worldLimit, worldLimit, worldLimit}}};
  committedWorld->worldAreaSubspaceBounds[0] = maximumWorldBounds;
  committedWorld->worldAreaGeometryBounds[0] = maximumWorldBounds;

  committedWorld->numAreaNodes = 1;
  committedWorld->areaNodes = new areaNode_t[1]{};
  committedWorld->areaNodeBounds = new idBounds[1]{};
  committedWorld->areaNodeOcclusionIndex = new int[1];
  auto* node = static_cast<areaNode_t*>(committedWorld->areaNodes);
  node->plane = {0.0f, 0.0f, 1.0f, worldLimit};
  node->children[0] = -1;
  node->children[1] = 0;
  committedWorld->areaNodeOcclusionIndex[0] = -1;
  committedWorld->binaryModelNames.Clear();
  committedWorld->binaryModelState.Clear();
  committedWorld->binaryModelHash.Clear();
  if (loadCallbacks.setBinaryModelState != nullptr)
    loadCallbacks.setBinaryModelState(committedWorld, "world", true, true);
  skyModel = nullptr;
}

void idRenderWorldLocal::ParseEnvAreas(idLexer* source) {
  source->ExpectTokenString("{");
  committedWorld->dynamicEnvironments.ClearFree();
  const int count = source->ParseInt();
  idToken token;
  for (int index = 0; index < count; ++index) {
    source->ReadToken(token);
    const idDeclEnv* environment =
        loadCallbacks.findEnvironmentDecl != nullptr
            ? loadCallbacks.findEnvironmentDecl(token.data)
            : nullptr;
    idBounds bounds;
    source->Parse1DMatrix(6, &bounds.b[0].x);
    const float volume = source->ParseFloat();
    const int planeCount = source->ParseInt();
    idPlane planes[16]{};
    for (int plane = 0; plane < planeCount && plane < 16; ++plane)
      source->Parse1DMatrix(4, &planes[plane].a);
    AddEnvironment(bounds, environment, planes, planeCount, volume);
  }
  source->ExpectTokenString("}");
}

void idRenderWorldLocal::ParseEnvArea(const char* worldName, idLexer* source,
                                      int version) {
  source->ExpectTokenString("{");
  idToken token;
  idStr envDeclPath;
  idStr userEnvMap;
  source->ExpectTokenString("envDecl");
  source->ReadToken(token);
  envDeclPath = token;
  source->ExpectTokenString("userEnvMap");
  source->ParseRestOfLine(token);
  userEnvMap = token;
  source->ExpectTokenString("volume");
  const float volume = source->ParseFloat();
  source->ExpectTokenString("bounds");
  idBounds bounds;
  source->Parse1DMatrix(6, &bounds.b[0].x);
  source->ExpectTokenString("numPlanes");
  const int planeCount = source->ParseInt();
  source->ExpectTokenString("planes");
  source->ExpectTokenString("{");
  idStaticList<idPlane, 16> planes;
  planes.SetNum(planeCount);
  for (int index = 0; index < planeCount; ++index)
    source->Parse1DMatrix(4, &planes[index].a);
  source->ExpectTokenString("}");

  idVec3 specularOrigin{};
  idColor specularTint{0.0f, 0.0f, 0.0f, 1.0f};
  idVec2 specularPowers{2.0f, 64.0f};
  idVec2 specularScales{0.5f, 2.0f};
  if (version > 1) {
    source->ExpectTokenString("specularOrigin");
    source->Parse1DMatrix(3, &specularOrigin.x);
  }
  if (version > 2) {
    source->ExpectTokenString("specularTint");
    source->Parse1DMatrix(4, &specularTint.r);
    source->ExpectTokenString("specularPowers");
    source->Parse1DMatrix(2, &specularPowers.x);
    source->ExpectTokenString("specularScales");
    source->Parse1DMatrix(2, &specularScales.x);
  }
  source->ExpectTokenString("}");

  const idDeclEnv* environment =
      loadCallbacks.findEnvironmentDecl != nullptr
          ? loadCallbacks.findEnvironmentDecl(envDeclPath.data)
          : nullptr;
  const bool customEnvironment = !userEnvMap.IsEmpty() ||
      specularOrigin.x != 0.0f || specularOrigin.y != 0.0f ||
      specularOrigin.z != 0.0f;
  if (customEnvironment && loadCallbacks.buildEnvironmentDecl != nullptr) {
    idStr areaName;
    areaName.Format("area%d", committedWorld->dynamicEnvironments.Num());
    environment = loadCallbacks.buildEnvironmentDecl(
        areaName.data, envDeclPath.data, worldName, userEnvMap.data,
        specularOrigin, specularTint, specularPowers, specularScales);
  }
  AddEnvironment(bounds, environment, planes.list, planes.Num(), volume);
}

bool idRenderWorldLocal::LoadEnvAreas(const char* worldName,
                                      const char* fileName) {
  auto source = std::make_unique<idLexer>(592);
  if (!source->LoadFile(fileName, false)) {
    idLib::Printf("idRenderWorldLocal::InitFromMap: %s not found\n", fileName);
    return false;
  }
  idToken token;
  if (!source->ReadToken(token))
    idLib::Error("idRenderWorldLocal::LoadEnvAreas: couldn't read envareas file id");
  int version = 0;
  if (token.Icmp("mapAreasFile001") == 0)
    version = 1;
  else if (token.Icmp("mapAreasFile002") == 0)
    version = 2;
  else if (token.Icmp("mapAreasFile003") == 0)
    version = 3;
  else
    idLib::Error(
        "idRenderWorldLocal::LoadEnvAreas: bad id '%s' instead of ( '%s', '%s' or '%s' )",
        token.data, "mapAreasFile003", "mapAreasFile002", "mapAreasFile001");

  unsigned expectedCount = 0;
  while (source->ReadToken(token)) {
    if (token == "numEnvAreas")
      expectedCount = static_cast<unsigned>(source->ParseInt());
    else if (token == "envArea")
      ParseEnvArea(worldName, source.get(), version);
  }
  if (expectedCount !=
      static_cast<unsigned>(committedWorld->dynamicEnvironments.Num())) {
    idLib::Error(
        "idRenderWorldLocal::LoadEnvAreas: failed to parse correct number of envAreas %d vs %d",
        committedWorld->dynamicEnvironments.Num(), expectedCount);
  }
  return true;
}
void idRenderWorldLocal::SpecificationComplete(bool preloadPageFileCache) {
  idLib::Printf("---------- idRenderWorldLocal::SpecificationComplete ----------\n");
  constexpr float worldLimit = 131072.0f;
  const idBounds maximumWorldBounds{{{-worldLimit, -worldLimit, -worldLimit},
                                      {worldLimit, worldLimit, worldLimit}}};
  if (committedWorld->numAreaNodes <= 0) {
    committedWorld->areaNodeBounds[0] = maximumWorldBounds;
    if (committedWorld->numWorldAreas > 0) {
      committedWorld->worldAreaSubspaceBounds[0] = maximumWorldBounds;
      committedWorld->worldAreaGeometryBounds[0] = maximumWorldBounds;
    }
  } else {
    idBounds worldBounds;
    worldBounds.Clear();
    for (int area = 0; area < committedWorld->numWorldAreas; ++area) {
      for (int surface = 0;
           surface < committedWorld->worldAreas[area].worldSurfaces.Num();
           ++surface) {
        const idTriangles* geometry =
            committedWorld->worldAreas[area].worldSurfaces[surface].geometry;
        if (geometry != nullptr)
          AddBounds(worldBounds, geometry->bounds);
      }
    }
    if (skyModel != nullptr)
      AddBounds(worldBounds, skyModel->referenceBounds);
    if (worldBounds.b[0].x > worldBounds.b[1].x)
      worldBounds = maximumWorldBounds;
    auto planes = std::make_unique<idPlane[]>(
        static_cast<std::size_t>(committedWorld->numAreaNodes + 6));
    idPolytope::ConvexPolytopeFromBounds(worldBounds, planes.get());
    BuildNodeBounds_r(0, planes.get(), 6);
    int numOcclusionIndices = 0;
    auto* nodes = static_cast<areaNode_t*>(committedWorld->areaNodes);
    if (nodes[0].children[0] > 0)
      AssignNodeOcclusionIndices_r(nodes[0].children[0], 1,
                                   numOcclusionIndices);
    if (nodes[0].children[1] > 0)
      AssignNodeOcclusionIndices_r(nodes[0].children[1], 1,
                                   numOcclusionIndices);
  }

  if (committedWorld->dynamicEnvironments.Num() == 0) {
    const idDeclEnv* defaultEnvironment =
        loadCallbacks.findEnvironmentDecl != nullptr
            ? loadCallbacks.findEnvironmentDecl("default")
            : nullptr;
    if (defaultEnvironment == nullptr)
      idLib::Error("Invalid or missing environment decl 'default' !");
    AddEnvironment(maximumWorldBounds, defaultEnvironment, nullptr, 0, 0.0f);
  }

  int modelCount = renderModels.Num();
  while (modelCount > 0 && renderModels[modelCount - 1] == nullptr)
    --modelCount;
  renderModels.SetNum(modelCount);
  int lightCount = renderLights.Num();
  while (lightCount > 0 && renderLights[lightCount - 1] == nullptr)
    --lightCount;
  renderLights.SetNum(lightCount);

  idStr strippedName(mapName);
  strippedName.StripPath();
  strippedName.StripFileExtension();
  if (loadCallbacks.beginVirtualTextureSetup != nullptr)
    loadCallbacks.beginVirtualTextureSetup(strippedName.data);

  virtualTextures.ClearFree();
  megaTextures.ClearFree();
  if (loadCallbacks.virtualTextureForPageSource != nullptr) {
    for (int modelIndex = 0; modelIndex < renderModels.Num(); ++modelIndex) {
      const idRenderModel* model = renderModels[modelIndex];
      if (model == nullptr)
        continue;
      for (int surfaceIndex = 0; surfaceIndex < model->surfaces.Num();
           ++surfaceIndex) {
        const idRenderModelSurface& surface = model->surfaces[surfaceIndex];
        if (surface.geometry != nullptr && surface.material != nullptr &&
            surface.material->pageFlavor == ID_VTF_BAKE) {
          AppendVirtualTexture(
              virtualTextures,
              loadCallbacks.virtualTextureForPageSource(
                  surface.material->pageSource));
        }
      }
    }
    for (int areaIndex = 0; areaIndex < committedWorld->numWorldAreas;
         ++areaIndex) {
      const worldArea_t& area = committedWorld->worldAreas[areaIndex];
      for (int surfaceIndex = 0; surfaceIndex < area.worldSurfaces.Num();
           ++surfaceIndex) {
        const idRenderModelSurface& surface = area.worldSurfaces[surfaceIndex];
        if (surface.geometry != nullptr && surface.material != nullptr &&
            surface.material->pageFlavor == ID_VTF_BAKE) {
          AppendVirtualTexture(
              virtualTextures,
              loadCallbacks.virtualTextureForPageSource(
                  surface.material->pageSource));
        }
      }
    }
  }
  for (int index = 0; index < virtualTextures.Num(); ++index)
    virtualTextures[index]->SetMapName(mapName.data);
  if (preloadPageFileCache && loadCallbacks.preloadVirtualTextures != nullptr)
    loadCallbacks.preloadVirtualTextures(virtualTextures.list,
                                         virtualTextures.Num());
  specificationCompleteHasBeenCalled = true;
}

bool idRenderWorldLocal::ParseProc(const char* fileName) {
  committedWorld->hasWorldSurfaceOffsets = false;
  auto source = std::make_unique<idLexer>(592);
  if (!source->LoadFile(fileName, false)) {
    idLib::Printf("idRenderWorldLocal::InitFromMap: %s not found\n", fileName);
    ClearWorld();
    return false;
  }
  idToken token;
  if (!source->ReadToken(token)) {
    idLib::Warning("idRenderWorldLocal::InitFromMap: couldn't read proc file id");
    return true;
  }
  if (token.Icmp("mapProcFile005") != 0 &&
      token.Icmp("mapProcFile006") != 0 &&
      token.Icmp("mapProcFile007") != 0) {
    idLib::Warning("idRenderWorldLocal::InitFromMap: bad id '%s' instead of '%s'",
                   token.data, "mapProcFile007");
    return true;
  }

  while (source->ReadToken(token)) {
    if (token == "interAreaPortals") {
      ParseInterAreaPortals(source.get());
    } else if (token == "areas") {
      ParseAreas(source.get());
    } else if (token == "nodes") {
      ParseNodes(source.get());
    } else if (token == "binaryModels") {
      source->ExpectTokenString("{");
      const int count = source->ParseInt();
      for (int index = 0; index < count; ++index) {
        source->ReadToken(token);
        source->ParseBool();
        if (loadCallbacks.setBinaryModelState != nullptr)
          loadCallbacks.setBinaryModelState(committedWorld, token.data,
                                            false, true);
      }
      source->ExpectTokenString("}");
    } else if (token == "envAreas") {
      ParseEnvAreas(source.get());
    } else {
      source->Error("idRenderWorldLocal::InitFromMap: bad token \"%s\"",
                    token.data);
    }
  }
  return true;
}
bool idRenderWorldLocal::InitFromMap(const char* name, bool comboMap,
                                     bool toolMap) {
  if (everInitialized)
    idLib::FatalError("idRenderWorldLocal::InitFromMap: everInitialized was set");
  everInitialized = true;

  transparencyRenderModel = new idRenderModelTransparency;
  AddRenderModel(transparencyRenderModel);
  transparencyRenderModel->CommitThisFrame();
  decalRenderModel = new idRenderModelDecal(true);
  AddRenderModel(decalRenderModel);
  decalRenderModel->CommitThisFrame();
  virtualTextures.ClearFree();
  megaTextures.ClearFree();

  if (name == nullptr || *name == '\0') {
    mapName = "<FREED>";
    committedWorld->mapName = mapName.data;
    ClearWorld();
    return true;
  }

  idStr baseName(name);
  baseName.StripPath();
  idStr fileName;
  fileName.Format(comboMap ? "%s/_combo/%s" : "%s/%s", name,
                  baseName.data);
  fileName.SetFileExtension("proc");
  if (!ParseProc(fileName.data))
    return false;

  fileName.Format(comboMap ? "%s/_combo/_world.envareas"
                           : "%s/_world.envareas",
                  name);
  LoadEnvAreas(name, fileName.data);
  mapName = name;
  mapName.StripFileExtension();
  committedWorld->mapName = mapName.data;
  committedWorld->isComboMap = comboMap;
  committedWorld->isToolsWorld = toolMap;
  committedWorld->hasWorldGeometry = true;
  committedWorld->transparencyRenderModel = transparencyRenderModel->committed;
  if (committedWorld->numWorldAreas == 0)
    ClearWorld();

  const char* worldSuffix = comboMap ? "_combo/_world.bmodel"
                                     : "_world.bmodel";
  const idStr mapWorldModelName = MapModelName(mapName, worldSuffix);
  idStaticModel* generatedWorldModel = nullptr;
  if (!committedWorld->hasWorldSurfaceOffsets) {
    generatedWorldModel = loadCallbacks.createStaticModel != nullptr
                              ? loadCallbacks.createStaticModel()
                              : nullptr;
    if (generatedWorldModel == nullptr)
      idLib::Error("idRenderWorldLocal::InitFromMap: static model factory is unavailable");
    generatedWorldModel->SetName(mapWorldModelName.data);
    for (int areaIndex = 0; areaIndex < committedWorld->numWorldAreas;
         ++areaIndex) {
      idStr areaSuffix;
      areaSuffix.Format(comboMap ? "_combo/_area%i.bmodel"
                                 : "_area%i.bmodel",
                        areaIndex);
      const idStr areaModelName = MapModelName(mapName, areaSuffix.data);
      idStaticModel* areaModel =
          loadCallbacks.loadStaticModel != nullptr
              ? loadCallbacks.loadStaticModel(areaModelName.data, false, false)
              : nullptr;
      if (areaModel == nullptr)
        idLib::Error("idRenderWorldLocal::InitFromMap: couldn't load %s",
                     areaModelName.data);
      worldArea_t& area = committedWorld->worldAreas[areaIndex];
      area.worldSurfaces.SetNum(areaModel->surfaces.Num());
      area.firstSurfaceInWorldModel = generatedWorldModel->surfaces.Num();
      for (int surfaceIndex = 0; surfaceIndex < areaModel->surfaces.Num();
           ++surfaceIndex)
        generatedWorldModel->surfaces.Append(areaModel->surfaces[surfaceIndex]);
      areaModel->surfaces.num = 0;
      if (loadCallbacks.releaseTemporaryStaticModel != nullptr)
        loadCallbacks.releaseTemporaryStaticModel(areaModel);
    }
    if (loadCallbacks.finishStaticModel != nullptr)
      loadCallbacks.finishStaticModel(generatedWorldModel, 0);
    if (loadCallbacks.registerStaticModel != nullptr)
      loadCallbacks.registerStaticModel(generatedWorldModel);
  }

  worldStaticModel = loadCallbacks.loadStaticModel != nullptr
                         ? loadCallbacks.loadStaticModel(
                               mapWorldModelName.data, true, false)
                         : generatedWorldModel;
  if (worldStaticModel == nullptr)
    idLib::Error("idRenderWorldLocal::InitFromMap: couldn't load %s",
                 mapWorldModelName.data);

  int totalVertices = 0;
  int totalIndexes = 0;
  unsigned worldVertexMask = 0;
  for (int areaIndex = 0; areaIndex < committedWorld->numWorldAreas;
       ++areaIndex) {
    worldArea_t& area = committedWorld->worldAreas[areaIndex];
    int binaryModelId = -1;
    for (int surfaceIndex = 0; surfaceIndex < area.worldSurfaces.Num();
         ++surfaceIndex) {
      const int modelSurfaceIndex =
          area.firstSurfaceInWorldModel + surfaceIndex;
      if (modelSurfaceIndex < 0 ||
          modelSurfaceIndex >= worldStaticModel->surfaces.Num())
        idLib::Error("idRenderWorldLocal::InitFromMap: invalid world surface offset");
      const idStaticModelSurface& source =
          worldStaticModel->surfaces[modelSurfaceIndex];
      idRenderModelSurface& destination = area.worldSurfaces[surfaceIndex];
      destination = static_cast<const idRenderModelSurface&>(source);
      destination.geometryIsReference = true;
      area.areaNum = areaIndex;
      if (destination.geometry != nullptr) {
        totalVertices += destination.geometry->numVerts;
        totalIndexes += destination.geometry->numIndexes;
        worldVertexMask |= destination.geometry->vertexMask;
      }
      if (binaryModelId != -3) {
        if (binaryModelId == -1)
          binaryModelId = destination.binaryModelId;
        else if (destination.binaryModelId == -1)
          binaryModelId = -3;
        else if (binaryModelId != destination.binaryModelId)
          binaryModelId = -2;
      }
    }
    area.binaryModelId = binaryModelId == -3 ? -1 : binaryModelId;
  }

  if (comboMap) {
    vertexCopyEntry_t copyTable[8]{};
    const int copyEntryCount = BuildVertexCopyTable(worldVertexMask, copyTable);
    const int vertexSize = VertexSizeForMask(worldVertexMask);
    if (totalIndexes == 0 || totalVertices == 0)
      idLib::Error("idRenderWorldLocal::InitFromMap: no world geometry");
    worldVertexBuffer.AllocBufferObject(nullptr, totalVertices * vertexSize,
                                        static_cast<int>(worldVertexMask),
                                        BU_DYNAMIC);
    worldIndexBuffer.AllocBufferObject(nullptr, totalIndexes * 2, BU_DYNAMIC);
    auto* destinationVertices = static_cast<std::uint8_t*>(
        worldVertexBuffer.MapBuffer(BM_WRITE));
    auto* destinationIndexes = static_cast<std::uint8_t*>(
        worldIndexBuffer.MapBuffer(BM_WRITE));
    std::uint8_t* mappedVertices = destinationVertices;
    std::uint8_t* mappedIndexes = destinationIndexes;
    int vertexOffset = 0;
    int indexOffset = 0;
    if (mappedVertices != nullptr && mappedIndexes != nullptr) {
      for (int areaIndex = 0; areaIndex < committedWorld->numWorldAreas;
           ++areaIndex) {
        worldArea_t& area = committedWorld->worldAreas[areaIndex];
        for (int surfaceIndex = 0; surfaceIndex < area.worldSurfaces.Num();
             ++surfaceIndex) {
          idRenderModelSurface& surface = area.worldSurfaces[surfaceIndex];
          idTriangles* source = surface.geometry;
          if (source == nullptr ||
              ((worldVertexMask & 0x20U) != 0U &&
               (source->vertexMask & 0x20U) == 0U) ||
              ((worldVertexMask & 0x40U) != 0U &&
               (source->vertexMask & 0x40U) == 0U))
            continue;
          const int vertexBytes = vertexSize * source->numVerts;
          const int indexBytes = 2 * source->numIndexes;
          if (source->vertexBuffer.GetSize() == 0) {
            idTriangles::UpdateIndexBuffer(source);
            idTriangles::UpdateVertexBuffer(source);
          }
          auto* sourceVertices = static_cast<std::uint8_t*>(
              source->vertexBuffer.MapBuffer(BM_READ));
          auto* sourceIndexes = static_cast<std::uint8_t*>(
              source->indexBuffer.MapBuffer(BM_READ));
          if (sourceVertices == nullptr || sourceIndexes == nullptr)
            continue;
          for (int vertex = 0; vertex < source->numVerts; ++vertex) {
            for (int entry = 0; entry < copyEntryCount; ++entry) {
              const std::size_t fieldSize =
                  static_cast<std::size_t>(copyTable[entry].size);
              if ((source->vertexMask & copyTable[entry].mask) != 0U) {
                std::memmove(mappedVertices, sourceVertices, fieldSize);
                sourceVertices += fieldSize;
              } else {
                std::memset(mappedVertices, 0, fieldSize);
              }
              mappedVertices += fieldSize;
            }
          }
          std::memmove(mappedIndexes, sourceIndexes,
                       static_cast<std::size_t>(indexBytes));
          mappedIndexes += indexBytes;
          source->vertexBuffer.UnmapBuffer();
          source->indexBuffer.UnmapBuffer();

          auto* consolidated = new idTriangles{};
          surface.geometryIsReference = false;
          surface.geometry = consolidated;
          consolidated->bounds = source->bounds;
          consolidated->numIndexes = source->numIndexes;
          consolidated->numVerts = source->numVerts;
          consolidated->detailOffset = source->detailOffset;
          consolidated->vsb = source->vsb;
          consolidated->vertexBuffer.Reference(worldVertexBuffer,
                                                vertexOffset, vertexBytes);
          consolidated->indexBuffer.Reference(worldIndexBuffer,
                                               indexOffset, indexBytes);
          consolidated->vertexMask =
              worldVertexMask ^ ((worldVertexMask ^ source->vertexMask) &
                                 0x200U);
          vertexOffset += vertexBytes;
          indexOffset += indexBytes;
        }
      }
    }
    if (destinationVertices != nullptr)
      worldVertexBuffer.UnmapBuffer();
    if (destinationIndexes != nullptr)
      worldIndexBuffer.UnmapBuffer();
  }

  const idStr skyModelName = MapModelName(
      mapName, comboMap ? "_combo/sky.bmodel" : "sky.bmodel");
  skyModel = AllocStaticModel(skyModelName.data, false);
  if (skyModel != nullptr) {
    skyModel->g.flags0 |= 0x14U;
    skyModel->g.flags1 |= 0x20U;
    skyModel->CommitThisFrame();
    skyModel->g.modelDepthSort = MODEL_DEPTH_SORT_SKY;
  }

  idStr occlusionBaseModelName(mapName);
  occlusionBaseModelName += "_occlusion.bmodel";
  occlusionBaseModel = AllocStaticModel(occlusionBaseModelName.data, false);
  if (occlusionBaseModel != nullptr) {
    occlusionBaseModel->Commit();
    committedWorld->occlusionBaseModel = occlusionBaseModel->committed;
    const int modelIndex = occlusionBaseModel->GetIndex();
    if (modelIndex >= 0 &&
        modelIndex < committedWorld->committedRenderModels.Num())
      committedWorld->committedRenderModels[modelIndex] =
          occlusionBaseModel->committed;
  }
  if (loadCallbacks.loadGlobalShadows != nullptr)
    loadCallbacks.loadGlobalShadows(&committedWorld->globalShadows,
                                    mapName.data);
  if (loadCallbacks.loadAmbientLighting != nullptr)
    loadCallbacks.loadAmbientLighting(&committedWorld->ambientLighting,
                                      mapName.data);

  const idStr mapModelName = MapModelName(mapName, baseName.data);
  idStr foliageFileName(mapModelName);
  foliageFileName.SetFileExtension("bfoliage");
  if (loadCallbacks.loadFoliageModel != nullptr)
    loadCallbacks.loadFoliageModel(this, foliageFileName.data);
  if (committedWorld->binaryModelNames.Num() != 0)
    SetBinaryModelState(committedWorld->binaryModelNames[0].data, true);
  return true;
}
