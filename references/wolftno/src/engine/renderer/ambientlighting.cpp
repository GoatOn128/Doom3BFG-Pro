#include "engine/renderer/ambientlighting.h"

#include "engine/framework/fileresource.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/color.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/str.h"

#include <cstdint>
#include <cstring>

namespace {

constexpr int AMBIENT_FILE_MAGIC = 0x21155701;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8U) | (value >> 8U));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000FFU) << 24U) |
         ((value & 0x0000FF00U) << 8U) |
         ((value & 0x00FF0000U) >> 8U) |
         ((value & 0xFF000000U) >> 24U);
}

using DebugLineFunction = void (*)(idRenderDebug*, const idVec4*,
                                   const idVec3*, const idVec3*, int, bool);
using DebugBoundsFunction = void (*)(idRenderDebug*, const idVec4*,
                                     const idBounds*, const idVec3*, int,
                                     bool);

void DebugLine(idRenderDebug* renderDebug, const idVec4& color,
               const idVec3& start, const idVec3& end, int lifetime,
               bool depthTest) {
  void** vtable = *reinterpret_cast<void***>(renderDebug);
  reinterpret_cast<DebugLineFunction>(vtable[2])(
      renderDebug, &color, &start, &end, lifetime, depthTest);
}

void DebugBounds(idRenderDebug* renderDebug, const idVec4& color,
                 const idBounds& bounds, const idVec3& origin, int lifetime,
                 bool depthTest) {
  void** vtable = *reinterpret_cast<void***>(renderDebug);
  reinterpret_cast<DebugBoundsFunction>(vtable[13])(
      renderDebug, &color, &bounds, &origin, lifetime, depthTest);
}

}  // namespace

void idAmbientLighting::FreeData() {
  if (ambientMap->data != nullptr) {
    idMem::Singleton()->Free(ambientMap->data, ALIGN_16);
    ambientMap->data = nullptr;
    ambientMap->samples = nullptr;
    ambientMap->nodes = nullptr;
  }
}

idAmbientLighting::~idAmbientLighting() {
  FreeData();
  idMem::Singleton()->Free(ambientMap, ALIGN_16);
}

idAmbientLighting::idAmbientLighting() {
  ambientMap = static_cast<ambientMapCPU_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
          sizeof(ambientMapCPU_t), TAG_RENDERER, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  if (ambientMap != nullptr) {
    std::memset(&ambientMap->forcedColors, 0,
                sizeof(ambientMap->forcedColors));
  }
  ambientMap->data = nullptr;
  ambientMap->dataLen = 0;
  ambientMap->useForcedColors = false;
  ambientMap->nodes = nullptr;
  ambientMap->samples = nullptr;
}

void idAmbientLighting::DebugDrawAmbientLightingSamples(
    idRenderDebug* renderDebug, const idVec3& position) const {
  if (ambientMap->data == nullptr) {
    return;
  }

  int cube[6];
  ambientSample_t cubeSamples[8]{};
  ambientMap->SamplesForPoint(position, cube, cubeSamples);

  idBounds bounds;
  bounds[0] = {static_cast<float>(cube[0]), static_cast<float>(cube[1]),
               static_cast<float>(cube[2])};
  bounds[1] = {static_cast<float>(cube[0] + cube[3]),
               static_cast<float>(cube[1] + cube[3]),
               static_cast<float>(cube[2] + cube[3])};
  DebugBounds(renderDebug,
              *reinterpret_cast<const idVec4*>(&idColor::colorCyan), bounds,
              vec3_origin, 1, true);

  for (int index = 0; index < 8; ++index) {
    const idVec3 corner = {
        bounds[(index >> 0) & 1].x,
        bounds[(index >> 1) & 1].y,
        bounds[(index >> 2) & 1].z,
    };
    const idVec3 sampleOrigin = {
        static_cast<float>(cubeSamples[index].origin[0]),
        static_cast<float>(cubeSamples[index].origin[1]),
        static_cast<float>(cubeSamples[index].origin[2]),
    };
    const idColor lineColor = idColor::colorRed;
    DebugLine(renderDebug,
              *reinterpret_cast<const idVec4*>(&lineColor), corner,
              sampleOrigin, 1, true);
  }
}

void idAmbientLighting::SwapNodes(ambientNode_t* nodes, int numNodes) {
  for (int nodeIndex = 0; nodeIndex < numNodes; ++nodeIndex) {
    for (std::uint16_t& child : nodes[nodeIndex].children) {
      child = ByteSwap16(child);
    }
  }
}

void idAmbientLighting::SwapSamples(ambientSample_t* samples,
                                    int numSamples) {
  for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
    for (std::int16_t& component : samples[sampleIndex].origin) {
      component = static_cast<std::int16_t>(
          ByteSwap16(static_cast<std::uint16_t>(component)));
    }
  }
}

void idAmbientLighting::LoadAmbientLighting(const char* mapName) {
  FreeData();

  idStr filename(mapName);
  filename += ".ambient";
  if (!fileSystem->FileExists(filename.data)) {
    return;
  }

  idFileResource* fileResource = static_cast<idFileResource*>(
      idFileResource::resourceList.Load(filename.data, false, false));
  if (fileResource == nullptr || static_cast<int>(fileResource->length) <= 0) {
    idLib::Warning(
        "idAmbientLighting::LoadAmbientLighting: file not found: %s",
        filename.data);
    return;
  }

  ambientMap->dataLen = static_cast<int>(fileResource->length);
  ambientMap->data = static_cast<ambientHeader_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\AmbientLighting.cpp(121) : TAG_FILE",
          static_cast<std::size_t>(ambientMap->dataLen), TAG_FILE, false,
          ALIGN_16, HEAP_DEFAULTHEAP));
  std::memmove(ambientMap->data, fileResource->data,
               static_cast<std::size_t>(ambientMap->dataLen));
  delete fileResource;

  ambientMap->data->magic = static_cast<int>(
      ByteSwap32(static_cast<std::uint32_t>(ambientMap->data->magic)));
  ambientMap->data->numNodes = static_cast<int>(
      ByteSwap32(static_cast<std::uint32_t>(ambientMap->data->numNodes)));
  ambientMap->data->numSamples = static_cast<int>(
      ByteSwap32(static_cast<std::uint32_t>(ambientMap->data->numSamples)));

  if (ambientMap->data->magic != AMBIENT_FILE_MAGIC) {
    idLib::Warning(
        "idAmbientLighting::LoadAmbientLighting: bad file magic on %s",
        filename.data);
    FreeData();
    return;
  }

  ambientMap->nodes = reinterpret_cast<ambientNode_t*>(ambientMap->data + 1);
  ambientMap->samples = reinterpret_cast<ambientSample_t*>(
      ambientMap->nodes + ambientMap->data->numNodes);
  SwapNodes(ambientMap->nodes, ambientMap->data->numNodes);
  SwapSamples(ambientMap->samples, ambientMap->data->numSamples);
  if (ambientMap->data->numNodes == 0) {
    ambientMap->data = nullptr;
  }
  ambientMap->useForcedColors = false;
}
