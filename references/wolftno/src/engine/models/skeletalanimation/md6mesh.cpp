#include "engine/models/skeletalanimation/md6mesh.h"

#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <new>

idCVar r_skipBlendShapes("r_skipBlendShapes", "0", CVAR_BOOL,
                         "If no blendshape data will be setup");

namespace {

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

int ReadBigInt(idFile* file) {
  std::uint32_t value = 0;
  file->Read(&value, sizeof(value));
  return static_cast<int>(ByteSwap32(value));
}

float ReadBigFloat(idFile* file) {
  std::uint32_t value = 0;
  file->Read(&value, sizeof(value));
  value = ByteSwap32(value);
  float result = 0.0f;
  std::memcpy(&result, &value, sizeof(result));
  return result;
}

void SwapBigFloat(float& value) {
  std::uint32_t word = 0;
  std::memcpy(&word, &value, sizeof(word));
  word = ByteSwap32(word);
  std::memcpy(&value, &word, sizeof(word));
}

void ReadBinaryString(idFile* file, idStr& value) {
  int length = 0;
  file->Read(&length, sizeof(length));
  if (length <= 0) {
    value.Empty();
    return;
  }
  value.Fill(' ', length);
  file->Read(&value[0], static_cast<std::size_t>(length));
}

void ReadBigBounds(idFile* file, idBounds& bounds) {
  bounds.b[0].x = ReadBigFloat(file);
  bounds.b[0].y = ReadBigFloat(file);
  bounds.b[0].z = ReadBigFloat(file);
  bounds.b[1].x = ReadBigFloat(file);
  bounds.b[1].y = ReadBigFloat(file);
  bounds.b[1].z = ReadBigFloat(file);
}

void ReadBigDrawVerts(idFile* file, idDrawVert* verts, int count) {
  file->Read(verts, sizeof(idDrawVert) * static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    float* values = &verts[index].xyz.x;
    for (int component = 0; component < 5; ++component) {
      std::uint32_t word = 0;
      std::memcpy(&word, &values[component], sizeof(word));
      word = ByteSwap32(word);
      std::memcpy(&values[component], &word, sizeof(word));
    }
  }
}

template <typename T>
void SetSerializedCount(idList<T, 5>& list, int count) {
  list.ResizeForPort(count);
  list.num = std::min(count, list.size);
}

} // namespace

std::uint64_t idMD6Mesh::Memory() const {
  std::uint64_t result = 256;
  if (vertexes.num > 0)
    result += 56ull * static_cast<std::uint64_t>(vertexes.num);
  if (indexes.num > 0)
    result += 2ull * static_cast<std::uint64_t>(indexes.num);
  if (stSets.num > 0) {
    result += 40ull * static_cast<std::uint64_t>(stSets.num);
    for (int index = 0; index < stSets.num; ++index) {
      if (stSets.list[index].st.num > 0)
        result += 8ull * static_cast<std::uint64_t>(stSets.list[index].st.num);
    }
  }
  return result;
}

void idMD6Mesh::CreateHashST() {
  if (stSets.num <= 0) {
    hashST = 0;
    return;
  }
  const std::size_t capacity =
      32u + 4u * static_cast<std::size_t>(stSets.num);
  char* hashName = new (std::nothrow) char[capacity];
  if (hashName == nullptr) {
    idLib::Error("idMD6Mesh::CreateHashST: allocation failed");
    return;
  }
  int length = std::snprintf(hashName, capacity, "%d", stSets.num);
  for (int index = 0; index < stSets.num; ++index) {
    length += std::snprintf(
        hashName + length, capacity - static_cast<std::size_t>(length), "_%u",
        static_cast<unsigned>(stSets.list[index].slot));
  }
  hashST = idStr::Hash(hashName);
  delete[] hashName;
}

void idMD6Mesh::FreeLoadData() {
  if (stSets.num > 0) {
    stset_t& first = stSets.list[0];
    const int dataSize = static_cast<int>(sizeof(idVec2)) * first.st.num;
    if (first.vb != nullptr) {
      if (dataSize <= (first.vb->size & 0x7fffffff))
        first.vb->Update(first.st.list, dataSize, 9216);
      else {
        first.vb->FreeBufferObject();
        first.vb->AllocBufferObject(first.st.list, dataSize, 9216, BU_STATIC);
      }
    } else {
      first.vb = new (std::nothrow) idVertexBuffer;
      if (first.vb != nullptr)
        first.vb->AllocBufferObject(first.st.list, dataSize, 9216, BU_STATIC);
    }
    tris->vertexMask |= 0x2000u;
  }
  idTriangles::UpdateIndexBuffer(tris);
  idTriangles::UpdateVertexBuffer(tris);
  vertexes.ClearFree();
  indexes.ClearFree();
  for (int index = 0; index < stSets.num; ++index)
    stSets.list[index].st.ClearFree();
  tris->FreeCPUData();
}

void idMD6Mesh::Free() {
  if (tris != nullptr) {
    delete tris;
    tris = nullptr;
  }
  for (int index = 0; index < stSets.num; ++index) {
    delete stSets.list[index].vb;
    stSets.list[index].vb = nullptr;
  }
  stSets.ClearFree();
}

namespace {

template <typename T, int Tag>
bool ResizeMD6List(idList<T, Tag>& value, int newSize) {
  if (newSize <= 0) {
    if (value.listStatic == 0 || value.listStatic == 2) {
      delete[] value.list;
      value.list = nullptr;
      value.size = 0;
    }
    value.num = 0;
    return true;
  }
  if (newSize == value.size || value.listStatic != 0) {
    if (value.num > newSize)
      value.num = newSize;
    return true;
  }
  T* replacement = new (std::nothrow) T[newSize];
  if (replacement == nullptr)
    return false;
  const int copyCount = std::min(value.num, newSize);
  for (int index = 0; index < copyCount; ++index)
    replacement[index] = value.list[index];
  delete[] value.list;
  value.list = replacement;
  value.size = newSize;
  if (value.num > newSize)
    value.num = newSize;
  return true;
}

} // namespace

template <>
bool idList<idMD6Mesh::stset_t, 19>::Resize(int newSize) {
  return ResizeMD6List(*this, newSize);
}

template <>
bool idList<idMD6Mesh::blendShape_t, 5>::Resize(int newSize) {
  return ResizeMD6List(*this, newSize);
}

void idMD6Mesh::LoadMesh_Binary(idFile* file, unsigned int magic) {
  (void)magic;
  idStr materialName;
  ReadBinaryString(file, name);
  ReadBinaryString(file, materialName);
  if (!idMaterial::IsValidMaterialName(materialName.data, true, true)) {
    idLib::Error("Model '%s' has been generated with an invalid materialname '%s'",
                 name.data, materialName.data);
  }
  material = FindMaterialOrAutoMaterial(materialName.data);
  file->Read(&allowSkinRemapping, sizeof(allowSkinRemapping));
  numTexCoordSets = ReadBigInt(file);
  numVerts = ReadBigInt(file);
  numTris = ReadBigInt(file);
  ReadBigBounds(file, bounds);

  tris->numVerts = numVerts;
  tris->numIndexes = 3 * numTris;
  idTriangles::AllocStaticTriSurfVerts(tris, numVerts);
  idTriangles::AllocStaticTriSurfIndexes(tris, 3 * numTris);
  ReadBigDrawVerts(file, tris->verts, numVerts);
  file->Read(tris->indexes,
             sizeof(std::uint16_t) * static_cast<std::size_t>(3 * numTris));
  for (int index = 0; index < 3 * numTris; ++index)
    tris->indexes[index] = ByteSwap16(tris->indexes[index]);

  jointOffset = ReadBigInt(file);
  numJoints = ReadBigInt(file);
  const int numExtraST = ReadBigInt(file);
  stSets.ResizeForPort(numExtraST);
  stSets.num = std::min(numExtraST, stSets.size);
  for (int index = 0; index < stSets.num; ++index) {
    stSets.list[index].slot = 0;
    stSets.list[index].vb = nullptr;
    stSets.list[index].st.ClearFree();
  }
  for (int index = 0; index < stSets.num; ++index) {
    stset_t& set = stSets.list[index];
    set.st.ResizeForPort(numVerts);
    set.st.num = std::min(numVerts, set.st.size);
    file->Read(&set.slot, sizeof(set.slot));
    file->Read(set.st.list,
               sizeof(idVec2) * static_cast<std::size_t>(numVerts));
    for (int vertex = 0; vertex < numVerts; ++vertex) {
      SwapBigFloat(set.st.list[vertex].x);
      SwapBigFloat(set.st.list[vertex].y);
    }
  }

  bool hasMorphMap = false;
  file->Read(&hasMorphMap, sizeof(hasMorphMap));
  if (hasMorphMap) {
    const int morphSize = 4 * numVerts;
    if (feature_hq.valueInteger != 0) {
      SetSerializedCount(morphMap, morphSize);
      file->Read(morphMap.list, static_cast<std::size_t>(morphSize));
    } else {
      idList<std::uint8_t, 5> discarded;
      SetSerializedCount(discarded, morphSize);
      file->Read(discarded.list, static_cast<std::size_t>(morphSize));
    }
  }

  const int numBlendShapes = ReadBigInt(file);
  const int blendDataSize = 3 * numVerts;
  if (r_skipBlendShapes.valueInteger == 0 && feature_hq.valueInteger != 0) {
    blendShapes.ResizeForPort(numBlendShapes);
    blendShapes.num = std::min(numBlendShapes, blendShapes.size);
    for (int index = 0; index < blendShapes.num; ++index) {
      blendShape_t& shape = blendShapes.list[index];
      shape.blendShapeData.granularity = 4;
      SetSerializedCount(shape.blendShapeData, blendDataSize);
      ReadBinaryString(file, shape.blendShapeExpression);
      file->Read(shape.blendShapeData.list,
                 static_cast<std::size_t>(blendDataSize));
      shape.multiplier = ReadBigFloat(file);
    }
  } else {
    idList<std::uint8_t, 5> discarded;
    SetSerializedCount(discarded, blendDataSize);
    idStr expression;
    for (int index = 0; index < numBlendShapes; ++index) {
      ReadBinaryString(file, expression);
      file->Read(discarded.list, static_cast<std::size_t>(blendDataSize));
      (void)ReadBigFloat(file);
    }
  }

  tris->bounds = bounds;
  CreateHashST();
}
