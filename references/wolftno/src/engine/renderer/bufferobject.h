#pragma once

#include <cstddef>
#include <cstdint>

class idCVar;

enum bufferMapType_t : int {
  BM_READ = 0,
  BM_READ_NOSYNC = 1,
  BM_WRITE = 2,
  BM_WRITE_NOSYNC = 3,
};

enum bufferUsageType_t : int {
  BU_DEFAULT = 0,
  BU_STATIC = 1,
  BU_DYNAMIC = 2,
  BU_STAGING = 3,
  BU_STREAM = BU_STAGING,
};

void UnbindBufferObjects();

class idVertexBuffer {
public:
  idVertexBuffer();
  ~idVertexBuffer();

  bool AllocBufferObject(const void* data, int allocSize, int vertexMask,
                         bufferUsageType_t allocUsage);
  void FreeBufferObject();
  void Reference(const idVertexBuffer& other);
  void Reference(const idVertexBuffer& other, int refOffset, int refSize);
  void Update(const void* data, int updateSize, int vertexMask) const;
  void* TryMapBuffer(bufferMapType_t mapType) const;
  void* MapBuffer(bufferMapType_t mapType) const;
  void UnmapBuffer() const;

  void Reference(const idVertexBuffer* other) {
    if (other != nullptr) Reference(*other);
  }
  void Reference(const idVertexBuffer* other, int refOffset, int refSize) {
    if (other != nullptr) Reference(*other, refOffset, refSize);
  }
  void* MapBuffer(int mapType) const {
    return MapBuffer(static_cast<bufferMapType_t>(mapType));
  }

  int GetSize() const { return size & 0x7FFFFFFF; }
  int GetAllocedSize() const { return (GetSize() + 15) & ~15; }
  void* GetAPIObject() const { return apiObject; }
  int GetOffset() const { return offsetInOtherBuffer & 0x7FFFFFFF; }
  bool IsMapped() const { return size < 0; }
  bool OwnsBuffer() const { return offsetInOtherBuffer < 0; }

  mutable int size;
  int offsetInOtherBuffer;
  void* apiObject;
  bufferUsageType_t usage;
};

class idIndexBuffer {
public:
  idIndexBuffer();
  ~idIndexBuffer();

  bool AllocBufferObject(const void* data, int allocSize,
                         bufferUsageType_t allocUsage);
  void FreeBufferObject();
  void Reference(const idIndexBuffer& other);
  void Reference(const idIndexBuffer& other, int refOffset, int refSize);
  void Update(const void* data, int updateSize) const;
  void* TryMapBuffer(bufferMapType_t mapType) const;
  void* MapBuffer(bufferMapType_t mapType) const;
  void UnmapBuffer() const;

  void Reference(const idIndexBuffer* other) {
    if (other != nullptr) Reference(*other);
  }
  void Reference(const idIndexBuffer* other, int refOffset, int refSize) {
    if (other != nullptr) Reference(*other, refOffset, refSize);
  }
  void* MapBuffer(int mapType) const {
    return MapBuffer(static_cast<bufferMapType_t>(mapType));
  }

  int GetSize() const { return size & 0x7FFFFFFF; }
  int GetAllocedSize() const { return (GetSize() + 15) & ~15; }
  void* GetAPIObject() const { return apiObject; }
  int GetOffset() const { return offsetInOtherBuffer & 0x7FFFFFFF; }
  bool IsMapped() const { return size < 0; }
  bool OwnsBuffer() const { return offsetInOtherBuffer < 0; }

  mutable int size;
  int offsetInOtherBuffer;
  void* apiObject;
  bufferUsageType_t usage;
};

class idJointBuffer {
public:
  idJointBuffer();
  ~idJointBuffer();

  bool AllocBufferObject(const float* joints, int numAllocJoints);
  void FreeBufferObject();
  void Reference(const idJointBuffer& other, int jointRefOffset,
                 int numRefJoints);
  void Update(const float* joints, int numUpdateJoints) const;
  float* TryMapBuffer(bufferMapType_t mapType) const;
  float* MapBuffer(bufferMapType_t mapType) const;
  void UnmapBuffer() const;
  void Swap(idJointBuffer& other);

  void Reference(const idJointBuffer* other, int jointRefOffset,
                 int numRefJoints) {
    if (other != nullptr) Reference(*other, jointRefOffset, numRefJoints);
  }
  float* MapBuffer(int mapType) const {
    return MapBuffer(static_cast<bufferMapType_t>(mapType));
  }

  int GetNumJoints() const { return numJoints & 0x7FFFFFFF; }
  int GetAllocedSize() const {
    return GetNumJoints() * 16 * (((~flags) & 1) | 2);
  }
  void* GetAPIObject() const { return apiObject; }
  int GetOffset() const { return offsetInOtherBuffer & 0x7FFFFFFF; }
  bool IsMapped() const { return numJoints < 0; }
  bool OwnsBuffer() const { return offsetInOtherBuffer < 0; }

  mutable int numJoints;
  int offsetInOtherBuffer;
  void* apiObject;
  int flags;
};

class idConstantBuffer {
public:
  idConstantBuffer()
      : size(0), offsetInOtherBuffer(static_cast<int>(0x80000000U)),
        apiObject(nullptr) {}
  ~idConstantBuffer();
  void FreeBufferObject();

  int size;
  int offsetInOtherBuffer;
  void* apiObject;
};

class idPixelPackBuffer {
public:
  idPixelPackBuffer();

  bool AllocBufferObject(const void* data, int allocSize);
  void Update(const void* data, int updateSize) const;
  void* TryMapBuffer(bufferMapType_t mapType) const;
  void* MapBuffer(bufferMapType_t mapType) const;
  void UnmapBuffer() const;

  void* MapBuffer(int mapType) const {
    return MapBuffer(static_cast<bufferMapType_t>(mapType));
  }
  int GetSize() const { return size & 0x7FFFFFFF; }
  int GetAllocedSize() const { return (GetSize() + 15) & ~15; }
  int GetOffset() const { return offsetInOtherBuffer & 0x7FFFFFFF; }
  bool IsMapped() const { return size < 0; }

  mutable int size;
  int offsetInOtherBuffer;
  void* apiObject;
};

class idPixelUnpackBuffer {
public:
  idPixelUnpackBuffer();
  ~idPixelUnpackBuffer();

  bool AllocBufferObject(const void* data, int allocSize);
  void FreeBufferObject();
  void Update(const void* data, int updateSize) const;
  void* TryMapBuffer(bufferMapType_t mapType) const;
  void* MapBuffer(bufferMapType_t mapType) const;
  void UnmapBuffer() const;

  void* MapBuffer(int mapType) const {
    return MapBuffer(static_cast<bufferMapType_t>(mapType));
  }
  int GetSize() const { return size & 0x7FFFFFFF; }
  int GetAllocedSize() const { return (GetSize() + 15) & ~15; }
  int GetOffset() const { return offsetInOtherBuffer & 0x7FFFFFFF; }
  bool IsMapped() const { return size < 0; }
  bool OwnsBuffer() const { return offsetInOtherBuffer < 0; }

  mutable int size;
  int offsetInOtherBuffer;
  void* apiObject;
};

extern idCVar r_showBuffers;
extern idCVar r_amdWorkaroundMapBuffersOnCreate;
extern idCVar r_useUnsyncBufferObjects;
extern idCVar bo_useUnsynchronizedPBO;
extern idCVar bo_useInvalidateRangePBO;

static_assert(BM_READ == 0 && BM_READ_NOSYNC == 1 && BM_WRITE == 2 &&
                  BM_WRITE_NOSYNC == 3,
              "buffer map enum drift");
static_assert(BU_DEFAULT == 0 && BU_STATIC == 1 && BU_DYNAMIC == 2 &&
                  BU_STAGING == 3,
              "buffer usage enum drift");
static_assert(offsetof(idVertexBuffer, usage) == 0x10,
              "vertex-buffer usage offset drift");
static_assert(sizeof(idVertexBuffer) == 0x18, "vertex-buffer ABI drift");
static_assert(sizeof(idIndexBuffer) == 0x18, "index-buffer ABI drift");
static_assert(offsetof(idJointBuffer, flags) == 0x10,
              "joint-buffer flags offset drift");
static_assert(sizeof(idJointBuffer) == 0x18, "joint-buffer ABI drift");
static_assert(sizeof(idConstantBuffer) == 0x10,
              "constant-buffer ABI drift");
static_assert(sizeof(idPixelPackBuffer) == 0x10,
              "pixel-pack-buffer ABI drift");
static_assert(sizeof(idPixelUnpackBuffer) == 0x10,
              "pixel-unpack-buffer ABI drift");
