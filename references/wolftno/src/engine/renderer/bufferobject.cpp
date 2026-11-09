#include "engine/renderer/bufferobject.h"

#include "engine/renderer/gl_api.h"
#include "engine/renderer/glconfig.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/renderersystem.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstdint>
#include <cstring>
#include <utility>

idCVar r_showBuffers("r_showBuffers", "0", CVAR_INTEGER, "");
idCVar r_amdWorkaroundMapBuffersOnCreate(
    "r_amdWorkaroundMapBuffersOnCreate", "0", CVAR_BOOL, "");
idCVar r_useUnsyncBufferObjects("r_useUnsyncBufferObjects", "0", CVAR_BOOL,
                                "");
idCVar bo_useUnsynchronizedPBO(
    "bo_useUnsynchronizedPBO", "0", CVAR_BOOL | CVAR_NOCHEAT,
    "use unsynchronized PBO updates on the PC");
idCVar bo_useInvalidateRangePBO("bo_useInvalidateRange", "0",
                                CVAR_BOOL | CVAR_NOCHEAT, "");

namespace {

constexpr int kOwnsBuffer = static_cast<int>(0x80000000U);
constexpr int kMapped = static_cast<int>(0x80000000U);

constexpr unsigned int GL_ARRAY_BUFFER_VALUE = 0x8892;
constexpr unsigned int GL_ELEMENT_ARRAY_BUFFER_VALUE = 0x8893;
constexpr unsigned int GL_UNIFORM_BUFFER_VALUE = 0x8A11;
constexpr unsigned int GL_PIXEL_PACK_BUFFER_VALUE = 0x88EB;
constexpr unsigned int GL_PIXEL_UNPACK_BUFFER_VALUE = 0x88EC;
constexpr unsigned int GL_READ_ONLY_VALUE = 0x88B8;
constexpr unsigned int GL_WRITE_ONLY_VALUE = 0x88B9;
constexpr unsigned int GL_STATIC_DRAW_VALUE = 0x88E4;
constexpr unsigned int GL_STREAM_DRAW_VALUE = 0x88E0;
constexpr unsigned int GL_STREAM_READ_VALUE = 0x88E1;
constexpr unsigned int GL_OUT_OF_MEMORY_VALUE = 0x0505;
constexpr unsigned int GL_MAP_READ_BIT_VALUE = 0x0001;
constexpr unsigned int GL_MAP_WRITE_BIT_VALUE = 0x0002;
constexpr unsigned int GL_MAP_INVALIDATE_RANGE_BIT_VALUE = 0x0004;
constexpr unsigned int GL_MAP_UNSYNCHRONIZED_BIT_VALUE = 0x0020;

void* glVBHeap = nullptr;
void* glIBHeap = nullptr;
void* glJBHeap = nullptr;
void* glPPHeap = nullptr;
void* glPUHeap = nullptr;

unsigned int APIName(const void* apiObject) {
  return static_cast<unsigned int>(
      reinterpret_cast<std::uintptr_t>(apiObject));
}

int AlignedSize(int size) { return (size + 15) & ~15; }

void ClearBuffer(int& size, int& offsetInOtherBuffer, void*& apiObject) {
  offsetInOtherBuffer = kOwnsBuffer;
  size = 0;
  apiObject = nullptr;
}

void MarkHeap(void*& heap) {
  if (heap == nullptr) {
    heap = &heap;
  }
}

void* AllocHeadless(const char* location, int size) {
  idMem* memory = idMem::Singleton();
  memory->PushHeap(HEAP_SYSTEMHEAP);
  void* result = nullptr;
  try {
    result = memory->AllocWithLocation(location, static_cast<std::size_t>(size),
                                       TAG_DXBUFFER, false, ALIGN_16,
                                       HEAP_DEFAULTHEAP);
  } catch (...) {
    memory->PopHeap();
    throw;
  }
  memory->PopHeap();
  return result;
}

void DeleteBuffer(void* apiObject) {
  const unsigned int buffer = APIName(apiObject);
  _glewDeleteBuffersARB(1, &buffer);
}

unsigned int GenerateBufferWithRetry(const char* syncMessage,
                                     const char* failureMessage) {
  unsigned int buffer = 0xFFFF;
  _glewGenBuffersARB(1, &buffer);
  if (buffer == 0xFFFF) {
    renderSystem->SyncRenderThread(false);
    idLib::Printf("%s", syncMessage);
    _glewGenBuffersARB(1, &buffer);
    if (buffer == 0xFFFF) {
      idLib::FatalError("%s", failureMessage);
    }
  }
  return buffer;
}

void* MapBufferRange(const void* apiObject, int offset, int size,
                     unsigned int target, bufferMapType_t mapType,
                     bool unbindTarget) {
  const unsigned int buffer = APIName(apiObject);
  void* mapped = nullptr;
  if (glConfig.directStateAccess) {
    switch (mapType) {
      case BM_READ:
        mapped = _glewMapNamedBufferEXT(buffer, GL_READ_ONLY_VALUE);
        if (mapped != nullptr) {
          mapped = static_cast<unsigned char*>(mapped) + offset;
        }
        break;
      case BM_READ_NOSYNC:
        mapped = _glewMapNamedBufferRangeEXT(
            buffer, offset, AlignedSize(size), GL_MAP_READ_BIT_VALUE);
        break;
      case BM_WRITE:
        mapped = _glewMapNamedBufferEXT(buffer, GL_WRITE_ONLY_VALUE);
        if (mapped != nullptr) {
          mapped = static_cast<unsigned char*>(mapped) + offset;
        }
        break;
      case BM_WRITE_NOSYNC:
        mapped = _glewMapNamedBufferRangeEXT(
            buffer, offset, AlignedSize(size),
            GL_MAP_WRITE_BIT_VALUE | GL_MAP_INVALIDATE_RANGE_BIT_VALUE |
                GL_MAP_UNSYNCHRONIZED_BIT_VALUE);
        break;
    }
    return mapped;
  }

  _glewBindBufferARB(target, buffer);
  switch (mapType) {
    case BM_READ:
      mapped = _glewMapBufferARB(target, GL_READ_ONLY_VALUE);
      if (mapped != nullptr) {
        mapped = static_cast<unsigned char*>(mapped) + offset;
      }
      break;
    case BM_READ_NOSYNC:
      mapped = _glewMapBufferRange(target, offset, AlignedSize(size),
                                   GL_MAP_READ_BIT_VALUE);
      break;
    case BM_WRITE:
      mapped = _glewMapBufferARB(target, GL_WRITE_ONLY_VALUE);
      if (mapped != nullptr) {
        mapped = static_cast<unsigned char*>(mapped) + offset;
      }
      break;
    case BM_WRITE_NOSYNC:
      mapped = _glewMapBufferRange(
          target, offset, AlignedSize(size),
          GL_MAP_WRITE_BIT_VALUE | GL_MAP_INVALIDATE_RANGE_BIT_VALUE |
              GL_MAP_UNSYNCHRONIZED_BIT_VALUE);
      break;
  }
  if (unbindTarget) {
    _glewBindBufferARB(target, 0);
  }
  return mapped;
}

void* TryMapCommon(const void* apiObject, int offset, int size,
                   unsigned int target, bufferMapType_t mapType,
                   bool unbindTarget, const char* syncMessage,
                   bool checkErrorsBeforeMessage,
                   bool messageBeforeSync) {
  if (r_useUnsyncBufferObjects.valueInteger != 0) {
    if (mapType == BM_READ_NOSYNC) {
      mapType = BM_READ;
    } else if (mapType == BM_WRITE_NOSYNC) {
      mapType = BM_WRITE;
    }
  }

  for (int attempt = 0; attempt < 2; ++attempt) {
    void* mapped = MapBufferRange(apiObject, offset, size, target, mapType,
                                  unbindTarget);
    if (mapped != nullptr) {
      return mapped;
    }
    if (attempt != 0) {
      break;
    }
    if (checkErrorsBeforeMessage) {
      GL_CheckErrors(true);
    }
    if (messageBeforeSync) {
      idLib::Printf("%s", syncMessage);
      renderSystem->SyncRenderThread(false);
    } else {
      renderSystem->SyncRenderThread(false);
      idLib::Printf("%s", syncMessage);
    }
  }
  return nullptr;
}

void UnmapCommon(const void* apiObject, unsigned int target,
                 bool unbindTarget, const char* failureMessage) {
  bool success;
  if (glConfig.directStateAccess) {
    success = _glewUnmapNamedBufferEXT(APIName(apiObject)) != 0;
  } else {
    _glewBindBufferARB(target, APIName(apiObject));
    success = _glewUnmapBufferARB(target) != 0;
    if (unbindTarget) {
      _glewBindBufferARB(target, 0);
    }
  }
  if (!success) {
    idLib::Printf("%s", failureMessage);
  }
}

void UpdateCommon(const void* apiObject, int offset, const void* data,
                  int updateSize, unsigned int target, bool unbindTarget) {
  const int alignedUpdateSize = AlignedSize(updateSize);
  if (glConfig.directStateAccess) {
    _glewNamedBufferSubDataEXT(APIName(apiObject), offset, alignedUpdateSize,
                               data);
  } else {
    _glewBindBufferARB(target, APIName(apiObject));
    _glewBufferSubDataARB(target, offset, alignedUpdateSize, data);
    if (unbindTarget) {
      _glewBindBufferARB(target, 0);
    }
  }
}

}  // namespace

void UnbindBufferObjects() {
  if (idLib::headless) {
    return;
  }
  GL_UnbindVAO();
  _glewBindBufferARB(GL_ARRAY_BUFFER_VALUE, 0);
  _glewBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_VALUE, 0);
  _glewBindBufferARB(GL_UNIFORM_BUFFER_VALUE, 0);
  if (glConfig.pixelBufferObjectAvailable) {
    _glewBindBufferARB(GL_PIXEL_PACK_BUFFER_VALUE, 0);
    _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE, 0);
  }
}

idVertexBuffer::idVertexBuffer()
    : size(0), offsetInOtherBuffer(kOwnsBuffer), apiObject(nullptr),
      usage(BU_DEFAULT) {}

idVertexBuffer::~idVertexBuffer() { FreeBufferObject(); }

void idVertexBuffer::FreeBufferObject() {
  if (!OwnsBuffer()) {
    ClearBuffer(size, offsetInOtherBuffer, apiObject);
    return;
  }
  if (apiObject == nullptr) {
    return;
  }
  if (idLib::headless) {
    idMem::Singleton()->Free(apiObject, ALIGN_16);
    apiObject = nullptr;
    return;
  }
  if (r_showBuffers.valueInteger != 0) {
    idLib::Printf("vertex buffer free %p, api %p (%i bytes)\n", this,
                  apiObject, GetSize());
  }
  GL_DeleteVertexBufferVAO(apiObject);
  DeleteBuffer(apiObject);
  ClearBuffer(size, offsetInOtherBuffer, apiObject);
}

void idVertexBuffer::Reference(const idVertexBuffer& other) {
  if (OwnsBuffer()) {
    FreeBufferObject();
  }
  size = other.GetSize();
  offsetInOtherBuffer = other.GetOffset();
  apiObject = other.apiObject;
  usage = other.usage;
}

void idVertexBuffer::Reference(const idVertexBuffer& other, int refOffset,
                               int refSize) {
  if (OwnsBuffer()) {
    FreeBufferObject();
  }
  size = refSize;
  offsetInOtherBuffer = other.GetOffset() + refOffset;
  apiObject = other.apiObject;
  usage = other.usage;
}

void idVertexBuffer::Update(const void* data, int updateSize,
                            int /*vertexMask*/) const {
  if (idLib::headless) {
    std::memmove(apiObject, data, static_cast<std::size_t>(updateSize));
    return;
  }
  if (updateSize > size) {
    idLib::FatalError("idVertexBuffer::Update: size overrun, %i > %i\n",
                      updateSize, GetSize());
  }
  UpdateCommon(apiObject, GetOffset(), data, updateSize,
               GL_ARRAY_BUFFER_VALUE, false);
}

void* idVertexBuffer::TryMapBuffer(bufferMapType_t mapType) const {
  if (idLib::headless) {
    size |= kMapped;
    return apiObject;
  }
  void* mapped = TryMapCommon(
      apiObject, GetOffset(), GetSize(), GL_ARRAY_BUFFER_VALUE, mapType, false,
      "idVertexBuffer::TryMapBuffer: SyncRenderThread required\n", false,
      false);
  if (mapped != nullptr) {
    size |= kMapped;
  }
  return mapped;
}

void* idVertexBuffer::MapBuffer(bufferMapType_t mapType) const {
  void* mapped = TryMapBuffer(mapType);
  if (!idLib::headless && mapped == nullptr) {
    idLib::Warning("idVertexBuffer::MapBuffer: failed");
  }
  return mapped;
}

void idVertexBuffer::UnmapBuffer() const {
  if (idLib::headless) {
    size &= ~kMapped;
    return;
  }
  UnmapCommon(apiObject, GL_ARRAY_BUFFER_VALUE, false,
              "idVertexBuffer::UnmapBuffer failed\n");
  size &= ~kMapped;
}

bool idVertexBuffer::AllocBufferObject(const void* data, int allocSize,
                                       int vertexMask,
                                       bufferUsageType_t allocUsage) {
  if (allocSize <= 0) {
    idLib::Error("idVertexBuffer::AllocBufferObject: allocSize = %i",
                 allocSize);
  }
  size = allocSize;
  usage = allocUsage;
  if (idLib::headless) {
    apiObject = AllocHeadless(
        "w:\\build\\tech5\\engine\\renderer\\BufferObject.cpp(237) : "
        "TAG_DXBUFFER",
        size);
    if (data != nullptr) {
      Update(data, allocSize, vertexMask);
    }
    return true;
  }

  GL_GetError();
  const unsigned int buffer = GenerateBufferWithRetry(
      "idVertexBuffer::AllocBufferObject: SyncRenderThread required\n",
      "idVertexBuffer::AllocBufferObject: failed");
  GL_UnbindVAO();
  _glewBindBufferARB(GL_ARRAY_BUFFER_VALUE, buffer);
  _glewBufferDataARB(GL_ARRAY_BUFFER_VALUE, AlignedSize(allocSize), nullptr,
                     GL_STATIC_DRAW_VALUE);
  apiObject = reinterpret_cast<void*>(static_cast<std::uintptr_t>(buffer));
  MarkHeap(glVBHeap);
  const bool allocationFailed = GL_GetError() == GL_OUT_OF_MEMORY_VALUE;
  if (allocationFailed) {
    idLib::Warning("idVertexBuffer::AllocBufferObject: allocation failed");
  }
  if (r_amdWorkaroundMapBuffersOnCreate.valueInteger != 0 &&
      !allocationFailed) {
    for (int count = 0; count < 5; ++count) {
      MapBuffer(BM_READ);
      UnmapBuffer();
    }
  }
  if (r_showBuffers.valueInteger != 0) {
    idLib::Printf("vertex buffer alloc %p, api %p (%i bytes)\n", this,
                  apiObject, GetSize());
  }
  if (data != nullptr) {
    Update(data, allocSize, vertexMask);
  }
  return !allocationFailed;
}

idIndexBuffer::idIndexBuffer()
    : size(0), offsetInOtherBuffer(kOwnsBuffer), apiObject(nullptr),
      usage(BU_DEFAULT) {}

idIndexBuffer::~idIndexBuffer() { FreeBufferObject(); }

void idIndexBuffer::FreeBufferObject() {
  if (!OwnsBuffer()) {
    ClearBuffer(size, offsetInOtherBuffer, apiObject);
    return;
  }
  if (apiObject == nullptr) {
    return;
  }
  if (idLib::headless) {
    idMem::Singleton()->Free(apiObject, ALIGN_16);
    apiObject = nullptr;
    return;
  }
  if (r_showBuffers.valueInteger != 0) {
    idLib::Printf("index buffer free %p, api %p (%i bytes)\n", this,
                  apiObject, GetSize());
  }
  GL_DeleteIndexBufferVAO(apiObject);
  DeleteBuffer(apiObject);
  ClearBuffer(size, offsetInOtherBuffer, apiObject);
}

void idIndexBuffer::Reference(const idIndexBuffer& other) {
  if (OwnsBuffer()) {
    FreeBufferObject();
  }
  size = other.GetSize();
  offsetInOtherBuffer = other.GetOffset();
  apiObject = other.apiObject;
  usage = other.usage;
}

void idIndexBuffer::Reference(const idIndexBuffer& other, int refOffset,
                              int refSize) {
  if (OwnsBuffer()) {
    FreeBufferObject();
  }
  size = refSize;
  offsetInOtherBuffer = other.GetOffset() + refOffset;
  apiObject = other.apiObject;
  usage = other.usage;
}

void idIndexBuffer::Update(const void* data, int updateSize) const {
  if (idLib::headless) {
    std::memmove(apiObject, data, static_cast<std::size_t>(updateSize));
    return;
  }
  if (updateSize > size) {
    idLib::FatalError("idIndexBuffer::Update: size overrun, %i > %i\n",
                      updateSize, GetSize());
  }
  UpdateCommon(apiObject, GetOffset(), data, updateSize,
               GL_ELEMENT_ARRAY_BUFFER_VALUE, false);
}

void* idIndexBuffer::TryMapBuffer(bufferMapType_t mapType) const {
  if (idLib::headless) {
    size |= kMapped;
    return apiObject;
  }
  void* mapped = TryMapCommon(
      apiObject, GetOffset(), GetSize(), GL_ELEMENT_ARRAY_BUFFER_VALUE,
      mapType, false,
      "idIndexBuffer::MapBuffer: SyncRenderThread required\n", false, false);
  if (mapped != nullptr) {
    size |= kMapped;
  }
  return mapped;
}

void* idIndexBuffer::MapBuffer(bufferMapType_t mapType) const {
  void* mapped = TryMapBuffer(mapType);
  if (!idLib::headless && mapped == nullptr) {
    idLib::Warning("idIndexBuffer::MapBuffer: failed");
  }
  return mapped;
}

void idIndexBuffer::UnmapBuffer() const {
  if (idLib::headless) {
    size &= ~kMapped;
    return;
  }
  UnmapCommon(apiObject, GL_ELEMENT_ARRAY_BUFFER_VALUE, false,
              "idIndexBuffer::UnmapBuffer failed\n");
  size &= ~kMapped;
}

bool idIndexBuffer::AllocBufferObject(const void* data, int allocSize,
                                      bufferUsageType_t allocUsage) {
  if (allocSize <= 0) {
    idLib::Error("idIndexBuffer::AllocBufferObject: allocSize = %i",
                 allocSize);
  }
  size = allocSize;
  usage = allocUsage;
  if (idLib::headless) {
    apiObject = AllocHeadless(
        "w:\\build\\tech5\\engine\\renderer\\BufferObject.cpp(873) : "
        "TAG_DXBUFFER",
        size);
    if (data != nullptr) {
      Update(data, allocSize);
    }
    return true;
  }

  GL_GetError();
  const unsigned int buffer = GenerateBufferWithRetry(
      "idIndexBuffer::AllocBufferObject: SyncRenderThread required\n",
      "idIndexBuffer::AllocBufferObject: failed");
  _glewBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_VALUE, buffer);
  _glewBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_VALUE, AlignedSize(allocSize),
                     nullptr, GL_STATIC_DRAW_VALUE);
  apiObject = reinterpret_cast<void*>(static_cast<std::uintptr_t>(buffer));
  MarkHeap(glIBHeap);
  const bool allocationFailed = GL_GetError() == GL_OUT_OF_MEMORY_VALUE;
  if (allocationFailed) {
    idLib::Warning("idIndexBuffer:AllocBufferObject: allocation failed");
  }
  if (r_amdWorkaroundMapBuffersOnCreate.valueInteger != 0 &&
      !allocationFailed) {
    for (int count = 0; count < 5; ++count) {
      MapBuffer(BM_READ);
      UnmapBuffer();
    }
  }
  if (r_showBuffers.valueInteger != 0) {
    idLib::Printf("index buffer alloc %p, api %p (%i bytes)\n", this,
                  apiObject, GetSize());
  }
  if (data != nullptr) {
    Update(data, allocSize);
  }
  return !allocationFailed;
}

idJointBuffer::idJointBuffer()
    : numJoints(0), offsetInOtherBuffer(kOwnsBuffer), apiObject(nullptr),
      flags(0) {}

idJointBuffer::~idJointBuffer() { FreeBufferObject(); }

void idJointBuffer::FreeBufferObject() {
  if (!OwnsBuffer()) {
    offsetInOtherBuffer = kOwnsBuffer;
    numJoints = 0;
    apiObject = nullptr;
    flags = 0;
    return;
  }
  if (apiObject == nullptr) {
    return;
  }
  if (idLib::headless) {
    idMem::Singleton()->Free(apiObject, ALIGN_16);
    apiObject = nullptr;
    return;
  }
  if (r_showBuffers.valueInteger != 0) {
    idLib::Printf("joint buffer free %p, api %p (%i joints)\n", this,
                  apiObject, GetNumJoints());
  }
  _glewBindBufferARB(GL_UNIFORM_BUFFER_VALUE, 0);
  DeleteBuffer(apiObject);
  offsetInOtherBuffer = kOwnsBuffer;
  numJoints = 0;
  apiObject = nullptr;
  flags = 0;
}

void idJointBuffer::Reference(const idJointBuffer& other, int jointRefOffset,
                              int numRefJoints) {
  FreeBufferObject();
  numJoints = numRefJoints;
  offsetInOtherBuffer =
      other.GetOffset() + 16 * jointRefOffset * (((~flags) & 1) | 2);
  apiObject = other.apiObject;
  flags = other.flags;
}

void idJointBuffer::Update(const float* joints, int numUpdateJoints) const {
  if (numUpdateJoints > GetNumJoints()) {
    idLib::FatalError("idJointBuffer::Update: size overrun, %i > %i\n",
                      numUpdateJoints, GetNumJoints());
  }
  const int updateSize =
      16 * numUpdateJoints * (((~flags) & 1) | 2);
  if (idLib::headless) {
    std::memmove(apiObject, joints, static_cast<std::size_t>(updateSize));
    return;
  }
  UpdateCommon(apiObject, GetOffset(), joints, updateSize,
               GL_UNIFORM_BUFFER_VALUE, false);
}

float* idJointBuffer::TryMapBuffer(bufferMapType_t mapType) const {
  if (idLib::headless) {
    numJoints |= kMapped;
    return static_cast<float*>(apiObject);
  }

  const unsigned int buffer = APIName(apiObject);
  void* mapped = nullptr;
  if (glConfig.directStateAccess) {
    if (mapType == BM_WRITE) {
      mapped = _glewMapNamedBufferEXT(buffer, GL_WRITE_ONLY_VALUE);
    } else if (mapType == BM_WRITE_NOSYNC) {
      mapped = _glewMapNamedBufferRangeEXT(
          buffer, GetOffset(), 48 * GetNumJoints(),
          GL_MAP_WRITE_BIT_VALUE | GL_MAP_INVALIDATE_RANGE_BIT_VALUE |
              GL_MAP_UNSYNCHRONIZED_BIT_VALUE);
    } else {
      mapped = _glewMapNamedBufferEXT(buffer, GL_READ_ONLY_VALUE);
      if (mapped != nullptr) {
        mapped = static_cast<unsigned char*>(mapped) + GetOffset();
      }
    }
  } else {
    _glewBindBufferARB(GL_UNIFORM_BUFFER_VALUE, buffer);
    if (mapType == BM_WRITE) {
      mapped = _glewMapBufferARB(GL_UNIFORM_BUFFER_VALUE, GL_WRITE_ONLY_VALUE);
    } else if (mapType == BM_WRITE_NOSYNC) {
      mapped = _glewMapBufferRange(
          GL_UNIFORM_BUFFER_VALUE, GetOffset(), 48 * GetNumJoints(),
          GL_MAP_WRITE_BIT_VALUE | GL_MAP_INVALIDATE_RANGE_BIT_VALUE |
              GL_MAP_UNSYNCHRONIZED_BIT_VALUE);
    } else {
      mapped = _glewMapBufferARB(GL_UNIFORM_BUFFER_VALUE, GL_READ_ONLY_VALUE);
      if (mapped != nullptr) {
        mapped = static_cast<unsigned char*>(mapped) + GetOffset();
      }
    }
  }
  if (mapped != nullptr) {
    numJoints |= kMapped;
  }
  return static_cast<float*>(mapped);
}

float* idJointBuffer::MapBuffer(bufferMapType_t mapType) const {
  float* mapped = TryMapBuffer(mapType);
  if (!idLib::headless && mapped == nullptr) {
    idLib::Warning("idJointBuffer::MapBuffer: failed");
  }
  return mapped;
}

void idJointBuffer::UnmapBuffer() const {
  if (idLib::headless) {
    numJoints &= ~kMapped;
    return;
  }
  UnmapCommon(apiObject, GL_UNIFORM_BUFFER_VALUE, false,
              "idJointBuffer::UnmapBuffer failed\n");
  numJoints &= ~kMapped;
}

void idJointBuffer::Swap(idJointBuffer& other) {
  std::swap(numJoints, other.numJoints);
  std::swap(offsetInOtherBuffer, other.offsetInOtherBuffer);
  std::swap(apiObject, other.apiObject);
  std::swap(flags, other.flags);
}

bool idJointBuffer::AllocBufferObject(const float* joints,
                                      int numAllocJoints) {
  if (numAllocJoints <= 0) {
    idLib::Error("idJointBuffer::AllocBufferObject: joints = %i",
                 numAllocJoints);
  }
  numJoints = numAllocJoints;
  const int allocSize = 48 * GetNumJoints();
  if (idLib::headless) {
    apiObject = AllocHeadless(
        "w:\\build\\tech5\\engine\\renderer\\BufferObject.cpp(2159) : "
        "TAG_DXBUFFER",
        allocSize);
    if (joints != nullptr) {
      Update(joints, numAllocJoints);
    }
    return true;
  }

  GL_GetError();
  unsigned int buffer = 0;
  _glewGenBuffersARB(1, &buffer);
  _glewBindBufferARB(GL_UNIFORM_BUFFER_VALUE, buffer);
  _glewBufferDataARB(GL_UNIFORM_BUFFER_VALUE, allocSize, nullptr,
                     GL_STREAM_DRAW_VALUE);
  _glewBindBufferARB(GL_UNIFORM_BUFFER_VALUE, 0);
  apiObject = reinterpret_cast<void*>(static_cast<std::uintptr_t>(buffer));
  MarkHeap(glJBHeap);
  const bool allocationFailed = GL_GetError() == GL_OUT_OF_MEMORY_VALUE;
  if (allocationFailed) {
    idLib::Warning("idJointBuffer::AllocBufferObject: allocation failed");
  }
  if (r_amdWorkaroundMapBuffersOnCreate.valueInteger != 0 &&
      !allocationFailed) {
    for (int count = 0; count < 5; ++count) {
      MapBuffer(BM_READ);
      UnmapBuffer();
    }
  }
  if (r_showBuffers.valueInteger != 0) {
    idLib::Printf("joint buffer alloc %p, api %p (%i joints)\n", this,
                  apiObject, GetNumJoints());
  }
  if (joints != nullptr) {
    Update(joints, numAllocJoints);
  }
  return !allocationFailed;
}

idConstantBuffer::~idConstantBuffer() { FreeBufferObject(); }

void idConstantBuffer::FreeBufferObject() {
  if (idLib::headless) {
    idMem::Singleton()->Free(apiObject, ALIGN_16);
    apiObject = nullptr;
    return;
  }
  if (offsetInOtherBuffer < 0) {
    if (apiObject == nullptr) {
      return;
    }
    if (r_showBuffers.valueInteger != 0) {
      idLib::Printf("constant buffer free %p, api %p (%i bytes)\n", this,
                    apiObject, size & 0x7FFFFFFF);
    }
    DeleteBuffer(apiObject);
    size = 0;
    apiObject = nullptr;
    offsetInOtherBuffer = kOwnsBuffer;
  } else {
    offsetInOtherBuffer = kOwnsBuffer;
    size = 0;
    apiObject = nullptr;
  }
}

idPixelPackBuffer::idPixelPackBuffer()
    : size(0), offsetInOtherBuffer(kOwnsBuffer), apiObject(nullptr) {}

void idPixelPackBuffer::Update(const void* data, int updateSize) const {
  if (idLib::headless) {
    std::memmove(apiObject, data, static_cast<std::size_t>(updateSize));
    return;
  }
  if (updateSize > size) {
    idLib::FatalError("idPixelPackBuffer::Update: size overrun, %i > %i\n",
                      updateSize, GetSize());
  }
  UpdateCommon(apiObject, GetOffset(), data, updateSize,
               GL_PIXEL_PACK_BUFFER_VALUE, true);
}

void* idPixelPackBuffer::TryMapBuffer(bufferMapType_t mapType) const {
  if (idLib::headless) {
    size |= kMapped;
    return apiObject;
  }
  void* mapped = TryMapCommon(
      apiObject, GetOffset(), GetSize(), GL_PIXEL_PACK_BUFFER_VALUE, mapType,
      true, "idPixelPackBuffer::MapBuffer: SyncRenderThread required\n", true,
      true);
  if (mapped != nullptr) {
    size |= kMapped;
  }
  return mapped;
}

void* idPixelPackBuffer::MapBuffer(bufferMapType_t mapType) const {
  void* mapped = TryMapBuffer(mapType);
  if (!idLib::headless && mapped == nullptr) {
    idLib::Warning("idPixelPackBuffer::MapBuffer: failed");
  }
  return mapped;
}

void idPixelPackBuffer::UnmapBuffer() const {
  if (idLib::headless) {
    size &= ~kMapped;
    return;
  }
  UnmapCommon(apiObject, GL_PIXEL_PACK_BUFFER_VALUE, true,
              "idPixelPackBuffer::UnmapBuffer failed\n");
  size &= ~kMapped;
}

bool idPixelPackBuffer::AllocBufferObject(const void* data, int allocSize) {
  if (allocSize <= 0) {
    idLib::Error("idPixelPackBuffer::AllocBufferObject: allocSize = %i",
                 allocSize);
  }
  size = allocSize;
  if (idLib::headless) {
    apiObject = AllocHeadless(
        "w:\\build\\tech5\\engine\\renderer\\BufferObject.cpp(2742) : "
        "TAG_DXBUFFER",
        size);
    if (data != nullptr) {
      Update(data, allocSize);
    }
    return true;
  }

  GL_GetError();
  const unsigned int buffer = GenerateBufferWithRetry(
      "idPixelPackBuffer::AllocBufferObject: SyncRenderThread required\n",
      "idPixelPackBuffer::AllocBufferObject: failed");
  _glewBindBufferARB(GL_PIXEL_PACK_BUFFER_VALUE, buffer);
  _glewBufferDataARB(GL_PIXEL_PACK_BUFFER_VALUE, AlignedSize(allocSize),
                     nullptr, GL_STREAM_READ_VALUE);
  apiObject = reinterpret_cast<void*>(static_cast<std::uintptr_t>(buffer));
  _glewBindBufferARB(GL_PIXEL_PACK_BUFFER_VALUE, 0);
  MarkHeap(glPPHeap);
  const bool allocationFailed = GL_GetError() == GL_OUT_OF_MEMORY_VALUE;
  if (allocationFailed) {
    idLib::Warning("idPixelPackBuffer::AllocBufferObject: allocation failed");
  }
  if (r_amdWorkaroundMapBuffersOnCreate.valueInteger != 0 &&
      !allocationFailed) {
    for (int count = 0; count < 5; ++count) {
      MapBuffer(BM_READ);
      UnmapBuffer();
    }
  }
  if (r_showBuffers.valueInteger != 0) {
    idLib::Printf("pixel pack buffer alloc %p, api %p (%i bytes)\n", this,
                  apiObject, GetSize());
  }
  if (data != nullptr) {
    Update(data, allocSize);
  }
  return !allocationFailed;
}

idPixelUnpackBuffer::idPixelUnpackBuffer()
    : size(0), offsetInOtherBuffer(kOwnsBuffer), apiObject(nullptr) {}

idPixelUnpackBuffer::~idPixelUnpackBuffer() { FreeBufferObject(); }

void idPixelUnpackBuffer::FreeBufferObject() {
  if (!OwnsBuffer()) {
    ClearBuffer(size, offsetInOtherBuffer, apiObject);
    return;
  }
  if (apiObject == nullptr) {
    return;
  }
  if (idLib::headless) {
    idMem::Singleton()->Free(apiObject, ALIGN_16);
    apiObject = nullptr;
    return;
  }
  if (r_showBuffers.valueInteger != 0) {
    idLib::Printf("pixel unpack buffer free %p, api %p (%i bytes)\n", this,
                  apiObject, GetSize());
  }
  DeleteBuffer(apiObject);
  ClearBuffer(size, offsetInOtherBuffer, apiObject);
}

void idPixelUnpackBuffer::Update(const void* data, int updateSize) const {
  if (updateSize > size) {
    idLib::FatalError(
        "idPixelUnpackBuffer::Update: size overrun, %i > %i\n", updateSize,
        GetSize());
  }
  if (idLib::headless) {
    std::memmove(apiObject, data, static_cast<std::size_t>(updateSize));
    return;
  }
  UpdateCommon(apiObject, GetOffset(), data, updateSize,
               GL_PIXEL_UNPACK_BUFFER_VALUE, true);
}

void* idPixelUnpackBuffer::TryMapBuffer(bufferMapType_t mapType) const {
  if (idLib::headless) {
    size |= kMapped;
    return apiObject;
  }

  bufferMapType_t effectiveMapType = mapType;
  if (r_useUnsyncBufferObjects.valueInteger != 0) {
    if (effectiveMapType == BM_READ_NOSYNC) {
      effectiveMapType = BM_READ;
    } else if (effectiveMapType == BM_WRITE_NOSYNC) {
      effectiveMapType = BM_WRITE;
    }
  }

  for (int attempt = 0; attempt < 2; ++attempt) {
    const unsigned int buffer = APIName(apiObject);
    void* mapped = nullptr;
    if (effectiveMapType != BM_WRITE_NOSYNC) {
      mapped = MapBufferRange(apiObject, GetOffset(), GetSize(),
                              GL_PIXEL_UNPACK_BUFFER_VALUE, effectiveMapType,
                              true);
    } else {
      unsigned int access = GL_MAP_WRITE_BIT_VALUE;
      if (bo_useInvalidateRangePBO.valueInteger != 0) {
        access |= GL_MAP_INVALIDATE_RANGE_BIT_VALUE;
      }
      if (bo_useUnsynchronizedPBO.valueInteger != 0) {
        access |= GL_MAP_UNSYNCHRONIZED_BIT_VALUE;
      }
      if (glConfig.directStateAccess) {
        mapped = _glewMapNamedBufferRangeEXT(
            buffer, GetOffset(), AlignedSize(GetSize()), access);
      } else {
        _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE, buffer);
        mapped = _glewMapBufferRange(GL_PIXEL_UNPACK_BUFFER_VALUE, GetOffset(),
                                     AlignedSize(GetSize()), access);
        _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE, 0);
      }
    }
    if (mapped != nullptr) {
      size |= kMapped;
      return mapped;
    }
    if (attempt == 0) {
      renderSystem->SyncRenderThread(false);
      idLib::Printf(
          "idPixelUnpackBuffer::MapBuffer: SyncRenderThread required\n");
    }
  }
  return nullptr;
}

void* idPixelUnpackBuffer::MapBuffer(bufferMapType_t mapType) const {
  void* mapped = TryMapBuffer(mapType);
  if (!idLib::headless && mapped == nullptr) {
    idLib::Warning("idPixelUnpackBuffer::MapBuffer: failed");
  }
  return mapped;
}

void idPixelUnpackBuffer::UnmapBuffer() const {
  if (idLib::headless) {
    size &= ~kMapped;
    return;
  }
  UnmapCommon(apiObject, GL_PIXEL_UNPACK_BUFFER_VALUE, true,
              "idPixelUnpackBuffer::UnmapBuffer failed\n");
  size &= ~kMapped;
}

bool idPixelUnpackBuffer::AllocBufferObject(const void* data, int allocSize) {
  if (allocSize <= 0) {
    idLib::Error("idPixelUnpackBuffer::AllocBufferObject: allocSize = %i",
                 allocSize);
  }
  size = allocSize;
  if (idLib::headless) {
    apiObject = AllocHeadless(
        "w:\\build\\tech5\\engine\\renderer\\BufferObject.cpp(3334) : "
        "TAG_DXBUFFER",
        size);
    if (data != nullptr) {
      Update(data, allocSize);
    }
    return true;
  }

  GL_GetError();
  const unsigned int buffer = GenerateBufferWithRetry(
      "idPixelUnpackBuffer::AllocBufferObject: SyncRenderThread required\n",
      "idPixelUnpackBuffer::AllocBufferObject: failed");
  _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE, buffer);
  _glewBufferDataARB(GL_PIXEL_UNPACK_BUFFER_VALUE, AlignedSize(allocSize),
                     nullptr, GL_STREAM_DRAW_VALUE);
  apiObject = reinterpret_cast<void*>(static_cast<std::uintptr_t>(buffer));
  _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE, 0);
  MarkHeap(glPUHeap);
  const bool allocationFailed = GL_GetError() == GL_OUT_OF_MEMORY_VALUE;
  if (allocationFailed) {
    idLib::Warning("idPixelUnpackBuffer::AllocBufferObject: allocation failed");
  }
  if (r_amdWorkaroundMapBuffersOnCreate.valueInteger != 0 &&
      !allocationFailed) {
    for (int count = 0; count < 5; ++count) {
      MapBuffer(BM_READ);
      UnmapBuffer();
    }
  }
  if (r_showBuffers.valueInteger != 0) {
    idLib::Printf("pixel unpack buffer alloc %p, api %p (%i bytes)\n", this,
                  apiObject, GetSize());
  }
  if (data != nullptr) {
    Update(data, allocSize);
  }
  return !allocationFailed;
}
