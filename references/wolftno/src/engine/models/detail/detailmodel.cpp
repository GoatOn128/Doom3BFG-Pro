#include "engine/models/detail/detailmodel.h"

#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdint>

namespace {

constexpr std::uint32_t DETAIL_MAGIC = 0x44455403u;

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

void SetupDataPointers(dm_subTreeDataPtrs_t& pointers,
                       dm_subTreeData_t* data) {
  auto* base = reinterpret_cast<std::uint8_t*>(data);
  pointers.detail =
      reinterpret_cast<dm_detail_t*>(base + data->detailOffset);
  pointers.attributes =
      reinterpret_cast<dm_attribute_t*>(base + data->attributeOffset);
}

std::int16_t ToShort(float value) {
  return static_cast<std::int16_t>(
      std::clamp(static_cast<int>(value), -32768, 32767));
}

} // namespace

idCVar r_detailFadeDistMax(
    "r_detailFadeDistMax", "3500", CVAR_FLOAT,
    "detail models will be completely faded out at this range");

template <>
idDetailModel* idTypedResourceList<idDetailModel>::Alloc(
    const char* resourceName) const {
  idDetailModel* resource = new idDetailModel;
  resource->SetName(resourceName);
  return resource;
}

idTypedResourceList<idDetailModel> idDetailModel::resourceList("detailModel");
idStaticList<detailPaletteEntry_t, 128> idDetailModel::paletteEntries;
idStaticList<detailPaletteVert_t, 1024> idDetailModel::paletteVerts;
idStaticList<std::uint16_t, 4096> idDetailModel::paletteIndices;

void idDetailModel::GetDetailSubTreesForView_r(
    const dm_model_t& model, const std::int16_t (*frustumVerts)[8],
    int* subTreeNums, int& numSubTrees, int nodeNum) {
  if ((nodeNum & 0x8000) != 0) {
    subTreeNums[numSubTrees++] = nodeNum & 0x7FFF;
    return;
  }
  for (;;) {
    const dm_treeNode_t& node = model.treeNodes[nodeNum];
    const std::int16_t* vertices = frustumVerts[node.planeType];
    bool front = false;
    bool back = false;
    for (int index = 7; index >= 0; --index) {
      front = front || vertices[index] >= node.planeDist;
      back = back || vertices[index] <= node.planeDist;
    }
    if (front)
      GetDetailSubTreesForView_r(model, frustumVerts, subTreeNums,
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

int idDetailModel::GetDetailSubTreesForView(
    const dm_model_t& model, const detailRenderView_t* viewData,
    int* subTreeNums) {
  if (model.numTreeNodes == 0) {
    subTreeNums[0] = 0;
    return 1;
  }
  idVec3 vertices[8];
  const float distances[2] = {viewData->nearDist, viewData->farDist};
  for (int side = 0; side < 2; ++side) {
    const float distance = distances[side];
    for (int corner = 0; corner < 4; ++corner) {
      const float leftSign = (corner == 0 || corner == 3) ? 1.0f : -1.0f;
      const float upSign = (corner == 0 || corner == 1) ? 1.0f : -1.0f;
      idVec3& vertex = vertices[side * 4 + corner];
      for (int axis = 0; axis < 3; ++axis) {
        vertex[axis] =
            ((distance * viewData->viewFwd[axis] + viewData->viewOrg[axis]) +
             distance * viewData->viewLeft[axis] * viewData->fov[0] *
                 leftSign) +
            distance * viewData->viewUp[axis] * viewData->fov[1] * upSign;
      }
    }
  }
  std::int16_t shortVertices[3][8];
  for (int vertex = 0; vertex < 8; ++vertex) {
    shortVertices[0][vertex] = ToShort(vertices[vertex].x);
    shortVertices[1][vertex] = ToShort(vertices[vertex].y);
    shortVertices[2][vertex] = ToShort(vertices[vertex].z);
  }
  int count = 0;
  GetDetailSubTreesForView_r(model, shortVertices, subTreeNums, count, 0);
  return count;
}

void idDetailModel::SwapModelTreeNodes(dm_model_t* model) {
  for (int index = 0; index < model->numTreeNodes; ++index) {
    dm_treeNode_t& node = model->treeNodes[index];
    node.planeType = Big16(node.planeType);
    node.planeDist = static_cast<std::int16_t>(
        Big16(static_cast<std::uint16_t>(node.planeDist)));
    node.children[0] = Big16(node.children[0]);
    node.children[1] = Big16(node.children[1]);
    node.counts[0] = Big32(node.counts[0]);
    node.counts[1] = Big32(node.counts[1]);
  }
}

idDetailModel::~idDetailModel() { FreeData(); }

idResourceList* idDetailModel::GetResourceList() const {
  return &resourceList;
}

void idDetailModel::SwapModelSubTrees(dm_subTreeData_t* data,
                                      dm_subTreeTreeNodes_t* treeNodes) {
  dm_subTreeTreeNodePtrs_t treePointers{};
  SetupSubTreePtrsFromTreeNodes(treePointers, treeNodes);
  for (int index = 0; index < treeNodes->numTreeNodes; ++index) {
    dm_treeNode_t& node = treePointers.treeNodes[index];
    node.planeType = Big16(node.planeType);
    node.planeDist = static_cast<std::int16_t>(
        Big16(static_cast<std::uint16_t>(node.planeDist)));
    node.children[0] = Big16(node.children[0]);
    node.children[1] = Big16(node.children[1]);
    node.counts[0] = Big32(node.counts[0]);
    node.counts[1] = Big32(node.counts[1]);
  }

  dm_subTreeDataPtrs_t dataPointers{};
  SetupDataPointers(dataPointers, data);
  for (int index = 0; index < data->numAttributes; ++index) {
    dm_attribute_t& attribute = dataPointers.attributes[index];
    attribute.declIdx = Big16(attribute.declIdx);
    attribute.paletteIdx = Big16(attribute.paletteIdx);
    auto* padding = reinterpret_cast<std::uint32_t*>(attribute.pad);
    for (int word = 0; word < 7; ++word)
      padding[word] = Big32(padding[word]);
  }
  for (int index = 0; index < data->numDetail; ++index) {
    dm_detail_t& detail = dataPointers.detail[index];
    for (std::int16_t& coordinate : detail.xyz)
      coordinate = static_cast<std::int16_t>(
          Big16(static_cast<std::uint16_t>(coordinate)));
    detail.rotation.x = BigFloat(detail.rotation.x);
    detail.rotation.y = BigFloat(detail.rotation.y);
    detail.rotation.z = BigFloat(detail.rotation.z);
  }
}

idDetailModel::idDetailModel()
    : detailModelData{}, sharedAttributes{}, stats{} {}

void idDetailModel::FreeData() {
  if (detailModelData.subTrees != nullptr) {
    for (int index = 0; index < detailModelData.numSubTrees; ++index) {
      delete[] reinterpret_cast<std::uint8_t*>(
          detailModelData.subTrees[index].data);
      delete[] reinterpret_cast<std::uint8_t*>(
          detailModelData.subTrees[index].treeNodes);
    }
    delete[] detailModelData.subTrees;
  }
  delete[] detailModelData.treeNodes;
  detailModelData = {};
  sharedAttributes.ClearFree();
  stats = {};
}

bool idDetailModel::Load_Binary(const char* detailFileName) {
  idStr fileName(detailFileName);
  fileName.SetFileExtension("bdetail");
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
  if (magic != DETAIL_MAGIC) {
    idLib::Warning("%s is not a binary detail file", fileName.data);
    FreeData();
    return false;
  }

  std::uint32_t numSharedAttributes = 0;
  if (!ReadExact(file.file, &numSharedAttributes,
                 sizeof(numSharedAttributes)))
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
            ? static_cast<const idDeclDetail*>(
                  idDeclDetail::resourceList.FindWithInheritance(
                      decl.data, true, true))
            : nullptr;
  }

  std::uint32_t count = 0;
  if (!ReadExact(file.file, &count, sizeof(count)))
    return false;
  detailModelData.numTreeNodes = static_cast<int>(Big32(count));
  detailModelData.treeNodes =
      new dm_treeNode_t[detailModelData.numTreeNodes];
  if (!ReadExact(file.file, detailModelData.treeNodes,
                 sizeof(dm_treeNode_t) * detailModelData.numTreeNodes))
    return false;
  SwapModelTreeNodes(&detailModelData);

  if (!ReadExact(file.file, &count, sizeof(count)))
    return false;
  detailModelData.numSubTrees = static_cast<int>(Big32(count));
  detailModelData.subTrees = new dm_subTree_t[detailModelData.numSubTrees]{};
  for (int index = 0; index < detailModelData.numSubTrees; ++index) {
    dm_subTree_t& subTree = detailModelData.subTrees[index];
    if (!ReadExact(file.file, subTree.bounds.b[0], 8) ||
        !ReadExact(file.file, subTree.bounds.b[1], 8))
      return false;
    for (int bound = 0; bound < 2; ++bound)
      for (int axis = 0; axis < 4; ++axis)
        subTree.bounds.b[bound][axis] = static_cast<std::int16_t>(
            Big16(static_cast<std::uint16_t>(
                subTree.bounds.b[bound][axis])));
    if (!ReadExact(file.file, &subTree.maxDetailHeight, 2) ||
        !ReadExact(file.file, &subTree.totalDataSize, 4) ||
        !ReadExact(file.file, &subTree.totalTreeNodeSize, 2))
      return false;
    subTree.maxDetailHeight = Big16(subTree.maxDetailHeight);
    subTree.totalDataSize = static_cast<int>(
        Big32(static_cast<std::uint32_t>(subTree.totalDataSize)));
    subTree.totalTreeNodeSize = Big16(subTree.totalTreeNodeSize);
    subTree.data = reinterpret_cast<dm_subTreeData_t*>(
        new std::uint8_t[subTree.totalDataSize]);
    subTree.treeNodes = reinterpret_cast<dm_subTreeTreeNodes_t*>(
        new std::uint8_t[subTree.totalTreeNodeSize]);
    if (!ReadExact(file.file, subTree.data, subTree.totalDataSize) ||
        !ReadExact(file.file, subTree.treeNodes,
                   subTree.totalTreeNodeSize))
      return false;
    subTree.treeNodes->numTreeNodes = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.treeNodes->numTreeNodes)));
    subTree.treeNodes->treeNodeOffset = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.treeNodes->treeNodeOffset)));
    subTree.treeNodes->pad[0] = 0;
    subTree.treeNodes->pad[1] = 0;
    subTree.data->numDetail = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.data->numDetail)));
    subTree.data->detailOffset = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.data->detailOffset)));
    subTree.data->numAttributes = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.data->numAttributes)));
    subTree.data->attributeOffset = static_cast<int>(Big32(
        static_cast<std::uint32_t>(subTree.data->attributeOffset)));
    SwapModelSubTrees(subTree.data, subTree.treeNodes);
  }

  std::uint32_t paletteCount = 0;
  if (!ReadExact(file.file, &paletteCount, sizeof(paletteCount)))
    return false;
  paletteCount = Big32(paletteCount);
  paletteEntries.SetNum(static_cast<int>(paletteCount));
  if (!ReadExact(file.file, paletteEntries.list,
                 sizeof(detailPaletteEntry_t) * paletteCount))
    return false;
  for (std::uint32_t index = 0; index < paletteCount; ++index) {
    detailPaletteEntry_t& entry = paletteEntries[static_cast<int>(index)];
    entry.vertOffset = Big16(entry.vertOffset);
    entry.indexOffset = Big16(entry.indexOffset);
    entry.numVerts = Big16(entry.numVerts);
    entry.numIndices = Big16(entry.numIndices);
  }

  if (!ReadExact(file.file, &paletteCount, sizeof(paletteCount)))
    return false;
  paletteCount = Big32(paletteCount);
  paletteVerts.SetNum(static_cast<int>(paletteCount));
  if (!ReadExact(file.file, paletteVerts.list,
                 sizeof(detailPaletteVert_t) * paletteCount))
    return false;
  for (std::uint32_t index = 0; index < paletteCount; ++index) {
    detailPaletteVert_t& vertex = paletteVerts[static_cast<int>(index)];
    for (std::int16_t& coordinate : vertex.xyz)
      coordinate = static_cast<std::int16_t>(
          Big16(static_cast<std::uint16_t>(coordinate)));
    vertex.st[0] = Big16(vertex.st[0]);
    vertex.st[1] = Big16(vertex.st[1]);
  }

  if (!ReadExact(file.file, &paletteCount, sizeof(paletteCount)))
    return false;
  paletteCount = Big32(paletteCount);
  paletteIndices.SetNum(static_cast<int>(paletteCount));
  if (!ReadExact(file.file, paletteIndices.list,
                 sizeof(std::uint16_t) * paletteCount))
    return false;
  for (std::uint32_t index = 0; index < paletteCount; ++index)
    paletteIndices[static_cast<int>(index)] =
        Big16(paletteIndices[static_cast<int>(index)]);

  if (!ReadExact(file.file, &magic, sizeof(magic)))
    return false;
  magic = Big32(magic);
  if (magic != DETAIL_MAGIC) {
    idLib::Warning("%s is invalid", fileName.data);
    return false;
  }

  int detailCount = 0;
  for (int index = 0; index < detailModelData.numSubTrees; ++index)
    detailCount += detailModelData.subTrees[index].data->numDetail;
  stats.numDetailNodes = detailCount;
  stats.numTreeNodes = detailModelData.numTreeNodes;
  stats.numSubTrees = detailModelData.numSubTrees;
  return true;
}

void idDetailModel::LoadResource() {
  FreeData();
  if (!Load_Binary(name.str)) {
    SetResourceError("Couldn't load detailmodel '%s'", name.str);
    FreeData();
  }
}
