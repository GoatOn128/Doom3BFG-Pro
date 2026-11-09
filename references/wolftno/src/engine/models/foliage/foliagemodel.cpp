#include "engine/models/foliage/foliagemodel.h"

#include "engine/renderer/declmaterial.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdint>

namespace {

constexpr std::uint32_t FOLIAGE_MAGIC_6 = 0x464F4C06u;
constexpr std::uint32_t FOLIAGE_MAGIC_7 = 0x464F4C07u;

std::uint16_t Big16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t Big32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00FF0000u) |
         ((value >> 8) & 0x0000FF00u) | (value >> 24);
}

float BigFloat(float value) {
  union {
    float f;
    std::uint32_t u;
  } bits{value};
  bits.u = Big32(bits.u);
  return bits.f;
}

bool ReadExact(idFile* file, void* data, std::size_t size) {
  return file != nullptr && file->Read(data, size) == size;
}

void SetupDataPointers(fm_subTreeDataPtrs_t& pointers,
                       fm_subTreeData_t* data) {
  auto* base = reinterpret_cast<std::uint8_t*>(data);
  pointers.foliage = reinterpret_cast<fm_foliage_t*>(base + data->foliageOffset);
  pointers.attributes =
      reinterpret_cast<fm_attribute_t*>(base + data->attributeOffset);
}

void SetupTreePointers(fm_subTreeTreeNodePtrs_t& pointers,
                       fm_subTreeTreeNodes_t* data) {
  auto* base = reinterpret_cast<std::uint8_t*>(data);
  pointers.treeNodes =
      reinterpret_cast<fm_treeNode_t*>(base + data->treeNodeOffset);
  pointers.leafInfo =
      reinterpret_cast<fm_leafInfo_t*>(base + data->leafInfoOffset);
}

std::int16_t ToShort(float value) {
  return static_cast<std::int16_t>(
      std::clamp(static_cast<int>(value), -32768, 32767));
}

std::uint8_t FloatToByte(float value) {
  return static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(value), 0, 255));
}

} // namespace

idCVar r_foliageStartFadeDist(
    "r_foliageStartFadeDist", "3000", CVAR_FLOAT,
    "start fading foliage out at this range");
idCVar r_foliageSmallFadeDistMax(
    "r_foliageSmallFadeDistMax", "5000", CVAR_FLOAT,
    "small foliage will be completely faded out at this range");
idCVar r_foliageBigFadeDistMax(
    "r_foliageBigFadeDistMax", "7000", CVAR_FLOAT,
    "all foliage will be completely faded out at this range");

idTypedResourceList<idFoliageModel> idFoliageModel::resourceList(
    "foliageModel");

void idFoliageModel::GetFoliageSubTreesForView_r(
    const fm_model_t& model, const std::int16_t (*const frustumVerts)[8],
    int* subTreeNums, int& numSubTrees, int nodeNum) {
  if ((nodeNum & 0x8000) != 0) {
    subTreeNums[numSubTrees++] = nodeNum & 0x7FFF;
    return;
  }

  for (;;) {
    const fm_treeNode_t& node = model.treeNodes[nodeNum];
    const std::int16_t* vertices = frustumVerts[node.planeType];
    bool front = false;
    bool back = false;
    for (int index = 7; index >= 0; --index) {
      front = front || vertices[index] >= node.planeDist;
      back = back || vertices[index] <= node.planeDist;
    }
    if (front)
      GetFoliageSubTreesForView_r(model, frustumVerts, subTreeNums,
                                  numSubTrees, node.children[0]);
    if (!back)
      return;
    nodeNum = node.children[1];
    if ((nodeNum & 0x8000) != 0) {
      subTreeNums[numSubTrees++] = nodeNum & 0x7FFF;
      return;
    }
  }
}

void idFoliageModel::InitializeAttributes(
    fm_model_t* model, idList<const idDeclFoliage*, 5>& uniqueDecls,
    unsigned int magic) {
  if (model == nullptr)
    return;
  for (int subTreeIndex = 0; subTreeIndex < model->numSubTrees;
       ++subTreeIndex) {
    fm_subTreeDataPtrs_t pointers;
    SetupDataPointers(pointers, model->subTrees[subTreeIndex].data);
    for (int foliageIndex = 0;
         foliageIndex < model->subTrees[subTreeIndex].data->numFoliage;
         ++foliageIndex) {
      fm_foliage_t& foliage = pointers.foliage[foliageIndex];
      fm_attribute_t& attribute = pointers.attributes[foliage.attribIdx];
      const idDeclFoliage* decl = uniqueDecls[attribute.declIdx];
      const idVec4& atlas = decl->material->atlasScaleBias;
      attribute.sortScaleBias[0] = atlas.x;
      attribute.sortScaleBias[1] = atlas.y;
      attribute.sortScaleBias[2] = atlas.z;
      attribute.sortScaleBias[3] = atlas.w;
      attribute.opaque = static_cast<std::uint8_t>(
          -static_cast<int>(decl->isOpaque));
      if (magic <= FOLIAGE_MAGIC_6) {
        foliage.upVec[0] = FloatToByte(128.0f);
        foliage.upVec[1] = FloatToByte(128.0f);
        foliage.upVec[2] = FloatToByte(255.5f);
      }
    }
  }
}

int idFoliageModel::GetFoliageSubTreesForView(
    const fm_model_t& model, const foliageRenderView_t* viewData,
    int* subTreeNums) {
  if (model.numTreeNodes == 0) {
    subTreeNums[0] = 0;
    return 1;
  }

  idVec3 vertices[8];
  const float nearDistance = viewData->nearDist;
  const float farDistance = viewData->farDist;
  const float horizontal = viewData->fov[0];
  const float vertical = viewData->fov[1];
  const float distances[2] = {nearDistance, farDistance};
  for (int side = 0; side != 2; ++side) {
    const float distance = distances[side];
    for (int corner = 0; corner != 4; ++corner) {
      const float leftSign = (corner == 0 || corner == 3) ? 1.0f : -1.0f;
      const float upSign = (corner == 0 || corner == 1) ? 1.0f : -1.0f;
      idVec3& vertex = vertices[side * 4 + corner];
      vertex.x = ((distance * viewData->viewFwd.x + viewData->viewOrg.x) +
                  (distance * viewData->viewLeft.x) * horizontal * leftSign) +
                 (distance * viewData->viewUp.x) * vertical * upSign;
      vertex.y = ((distance * viewData->viewFwd.y + viewData->viewOrg.y) +
                  (distance * viewData->viewLeft.y) * horizontal * leftSign) +
                 (distance * viewData->viewUp.y) * vertical * upSign;
      vertex.z = ((distance * viewData->viewFwd.z + viewData->viewOrg.z) +
                  (distance * viewData->viewLeft.z) * horizontal * leftSign) +
                 (distance * viewData->viewUp.z) * vertical * upSign;
    }
  }

  std::int16_t shortVertices[3][8];
  for (int vertex = 0; vertex != 8; ++vertex) {
    shortVertices[0][vertex] = ToShort(vertices[vertex].x);
    shortVertices[1][vertex] = ToShort(vertices[vertex].y);
    shortVertices[2][vertex] = ToShort(vertices[vertex].z);
  }
  int count = 0;
  GetFoliageSubTreesForView_r(model, shortVertices, subTreeNums, count, 0);
  return count;
}

void idFoliageModel::FreeData() {
  if (foliageModelData.subTrees != nullptr) {
    for (int index = 0; index < foliageModelData.numSubTrees; ++index) {
      delete[] reinterpret_cast<std::uint8_t*>(foliageModelData.subTrees[index].data);
      delete[] reinterpret_cast<std::uint8_t*>(foliageModelData.subTrees[index].treeNodes);
    }
    delete[] foliageModelData.subTrees;
  }
  delete[] foliageModelData.treeNodes;
  foliageModelData.numTreeNodes = 0;
  foliageModelData.numSubTrees = 0;
  foliageModelData.treeNodes = nullptr;
  foliageModelData.subTrees = nullptr;
  stats.numFoliageNodes = 0;
  stats.numTreeNodes = 0;
  stats.numSubTrees = 0;
  sharedAttributes.ClearFree();
}

void idFoliageModel::SwapModelTreeNodes(fm_model_t* model) {
  for (int index = 0; index < model->numTreeNodes; ++index) {
    fm_treeNode_t& node = model->treeNodes[index];
    node.planeType = Big16(node.planeType);
    node.planeDist = static_cast<std::int16_t>(
        Big16(static_cast<std::uint16_t>(node.planeDist)));
    node.children[0] = Big16(node.children[0]);
    node.children[1] = Big16(node.children[1]);
  }
}

idFoliageModel::~idFoliageModel() { FreeData(); }

idResourceList* idFoliageModel::GetResourceList() const {
  return &resourceList;
}

void idFoliageModel::SwapModelSubTrees(fm_subTreeData_t* data,
                                       fm_subTreeTreeNodes_t* treeNodes) {
  fm_subTreeTreeNodePtrs_t treePointers;
  SetupTreePointers(treePointers, treeNodes);
  for (int index = 0; index < treeNodes->numTreeNodes; ++index) {
    fm_treeNode_t& node = treePointers.treeNodes[index];
    node.planeType = Big16(node.planeType);
    node.planeDist = static_cast<std::int16_t>(
        Big16(static_cast<std::uint16_t>(node.planeDist)));
    node.children[0] = Big16(node.children[0]);
    node.children[1] = Big16(node.children[1]);
  }
  for (int index = 0; index < treeNodes->numLeafInfo; ++index) {
    fm_leafInfo_t& leaf = treePointers.leafInfo[index];
    for (int bound = 0; bound != 2; ++bound)
      for (int axis = 0; axis != 3; ++axis)
        leaf.b[bound][axis] = static_cast<std::int16_t>(
            Big16(static_cast<std::uint16_t>(leaf.b[bound][axis])));
    leaf.foliageOffset = Big16(leaf.foliageOffset);
  }

  fm_subTreeDataPtrs_t dataPointers;
  SetupDataPointers(dataPointers, data);
  for (int index = 0; index < data->numAttributes; ++index) {
    fm_attribute_t& attribute = dataPointers.attributes[index];
    for (float& value : attribute.sortScaleBias)
      value = BigFloat(value);
    attribute.declIdx = Big16(attribute.declIdx);
    attribute.quadWidth = Big16(attribute.quadWidth);
    attribute.quadHeight = Big16(attribute.quadHeight);
    attribute.pad = static_cast<int>(
        Big32(static_cast<std::uint32_t>(attribute.pad)));
    attribute.opaque = 0;
    attribute.pad = 0;
  }
  for (int index = 0; index < data->numFoliage; ++index) {
    for (std::int16_t& coordinate : dataPointers.foliage[index].xyz)
      coordinate = static_cast<std::int16_t>(
          Big16(static_cast<std::uint16_t>(coordinate)));
  }
}

idFoliageModel::idFoliageModel()
    : foliageModelData{}, sharedAttributes{}, stats{} {}

bool idFoliageModel::Load_Binary(const char* foliageFileName) {
  idStr fileName(foliageFileName);
  fileName.SetFileExtension("bfoliage");
  idFileLocal file(fileSystem != nullptr
                       ? fileSystem->OpenFileRead(fileName.data, true, false)
                       : nullptr);
  if (file.file == nullptr) {
    idLib::Printf("couldn't open file %s for read\n", fileName.data);
    return false;
  }

  std::uint32_t magic = 0;
  if (!ReadExact(file.file, &magic, sizeof(magic)))
    return false;
  magic = Big32(magic);
  if (magic != FOLIAGE_MAGIC_7 && magic != FOLIAGE_MAGIC_6) {
    idLib::Warning("%s is not a binary foliage file", fileName.data);
    FreeData();
    return false;
  }

  std::uint32_t numSharedAttributes = 0;
  if (!ReadExact(file.file, &numSharedAttributes, sizeof(numSharedAttributes)))
    return false;
  numSharedAttributes = Big32(numSharedAttributes);
  sharedAttributes.SetNum(static_cast<int>(numSharedAttributes));
  for (std::uint32_t index = 0; index < numSharedAttributes; ++index) {
    int length = 0;
    if (!ReadExact(file.file, &length, sizeof(length)) || length < 0)
      return false;
    idStr decl;
    if (length > 0) {
      decl.Fill(' ', length);
      if (!ReadExact(file.file, &decl[0], static_cast<std::size_t>(length)))
        return false;
    }
    sharedAttributes[static_cast<int>(index)] =
        decl.data != nullptr
            ? static_cast<const idDeclFoliage*>(
                  idDeclFoliage::resourceList.FindWithInheritance(
                      decl.data, true, true))
            : nullptr;
  }

  std::uint32_t count = 0;
  if (!ReadExact(file.file, &count, sizeof(count)))
    return false;
  foliageModelData.numTreeNodes = static_cast<int>(Big32(count));
  foliageModelData.treeNodes =
      new fm_treeNode_t[foliageModelData.numTreeNodes];
  if (!ReadExact(file.file, foliageModelData.treeNodes,
                 sizeof(fm_treeNode_t) * foliageModelData.numTreeNodes))
    return false;
  SwapModelTreeNodes(&foliageModelData);

  if (!ReadExact(file.file, &count, sizeof(count)))
    return false;
  foliageModelData.numSubTrees = static_cast<int>(Big32(count));
  foliageModelData.subTrees = new fm_subTree_t[foliageModelData.numSubTrees]{};
  for (int index = 0; index < foliageModelData.numSubTrees; ++index) {
    fm_subTree_t& subTree = foliageModelData.subTrees[index];
    if (!ReadExact(file.file, subTree.bounds.b[0], 8) ||
        !ReadExact(file.file, subTree.bounds.b[1], 8))
      return false;
    for (int bound = 0; bound != 2; ++bound)
      for (int axis = 0; axis != 4; ++axis)
        subTree.bounds.b[bound][axis] = static_cast<std::int16_t>(
            Big16(static_cast<std::uint16_t>(subTree.bounds.b[bound][axis])));
    if (!ReadExact(file.file, &subTree.maxFoliageHeight, 2) ||
        !ReadExact(file.file, &subTree.totalDataSize, 4) ||
        !ReadExact(file.file, &subTree.totalTreeNodeSize, 2))
      return false;
    subTree.maxFoliageHeight = Big16(subTree.maxFoliageHeight);
    subTree.totalDataSize = static_cast<int>(
        Big32(static_cast<std::uint32_t>(subTree.totalDataSize)));
    subTree.totalTreeNodeSize = Big16(subTree.totalTreeNodeSize);
    subTree.data = reinterpret_cast<fm_subTreeData_t*>(
        new std::uint8_t[subTree.totalDataSize]);
    subTree.treeNodes = reinterpret_cast<fm_subTreeTreeNodes_t*>(
        new std::uint8_t[subTree.totalTreeNodeSize]);
    if (!ReadExact(file.file, subTree.data, subTree.totalDataSize) ||
        !ReadExact(file.file, subTree.treeNodes,
                   subTree.totalTreeNodeSize))
      return false;
    subTree.treeNodes->numTreeNodes = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.treeNodes->numTreeNodes)));
    subTree.treeNodes->treeNodeOffset = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.treeNodes->treeNodeOffset)));
    subTree.treeNodes->numLeafInfo = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.treeNodes->numLeafInfo)));
    subTree.treeNodes->leafInfoOffset = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.treeNodes->leafInfoOffset)));
    subTree.data->numFoliage = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.data->numFoliage)));
    subTree.data->foliageOffset = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.data->foliageOffset)));
    subTree.data->numAttributes = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.data->numAttributes)));
    subTree.data->attributeOffset = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.data->attributeOffset)));
    SwapModelSubTrees(subTree.data, subTree.treeNodes);
  }

  if (!ReadExact(file.file, &magic, sizeof(magic)))
    return false;
  magic = Big32(magic);
  if (magic != FOLIAGE_MAGIC_7 && magic != FOLIAGE_MAGIC_6) {
    idLib::Warning("%s is invalid", fileName.data);
    return false;
  }
  InitializeAttributes(&foliageModelData, sharedAttributes, magic);
  int foliageCount = 0;
  for (int index = 0; index < foliageModelData.numSubTrees; ++index)
    foliageCount += foliageModelData.subTrees[index].data->numFoliage;
  stats.numFoliageNodes = foliageCount;
  stats.numTreeNodes = foliageModelData.numTreeNodes;
  stats.numSubTrees = foliageModelData.numSubTrees;
  return true;
}

void idFoliageModel::LoadResource() {
  FreeData();
  if (!Load_Binary(name.str)) {
    SetResourceError("Couldn't load foliage '%s'", name.str);
    FreeData();
  }
}
