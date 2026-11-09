#pragma once

#include "engine/renderer/sampleroptions.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

#include <cstddef>

class idTextureSampler {
public:
  unsigned int oglSamplerState;
};

struct idTextureSamplerManagerVTable {
  void (*destroy)(struct idTextureSamplerManager* manager);
  void (*Init)(struct idTextureSamplerManager* manager);
  void (*Shutdown)(struct idTextureSamplerManager* manager);
  const idTextureSampler* (*FindSamplerByName)(
      struct idTextureSamplerManager* manager, const char* name,
      bool createIfMissing);
  const char* (*GetSamplerName)(struct idTextureSamplerManager* manager,
                               const idTextureSampler* sampler);
  const idTextureSampler* (*CreateSampler)(
      struct idTextureSamplerManager* manager, const samplerOptions_t* options,
      const char* name);
  void (*DeleteSampler)(struct idTextureSamplerManager* manager,
                        const idTextureSampler* sampler);
};

struct idTextureSamplerManager {
  const idTextureSamplerManagerVTable* vtable;
  const idTextureSampler* pointSampler;
  const idTextureSampler* pointSamplerRepeating;
  const idTextureSampler* mipMapSamplers[16];

  void Init() {
    if (vtable != nullptr && vtable->Init != nullptr)
      vtable->Init(this);
  }
  void Shutdown() {
    if (vtable != nullptr && vtable->Shutdown != nullptr)
      vtable->Shutdown(this);
  }

  const idTextureSampler* FindSamplerByName(const char* name,
                                            bool createIfMissing) {
    return vtable != nullptr && vtable->FindSamplerByName != nullptr
               ? vtable->FindSamplerByName(this, name, createIfMissing)
               : nullptr;
  }
  const char* GetSamplerName(const idTextureSampler* sampler) {
    return vtable != nullptr && vtable->GetSamplerName != nullptr
               ? vtable->GetSamplerName(this, sampler)
               : nullptr;
  }
  const idTextureSampler* CreateSampler(const samplerOptions_t* options,
                                        const char* name) {
    return vtable != nullptr && vtable->CreateSampler != nullptr
               ? vtable->CreateSampler(this, options, name)
               : nullptr;
  }
  void DeleteSampler(const idTextureSampler* sampler) {
    if (vtable != nullptr && vtable->DeleteSampler != nullptr) {
      vtable->DeleteSampler(this, sampler);
    }
  }

  ~idTextureSamplerManager();
};

extern idTextureSamplerManager* textureSamplerManager;

class idUniqueTextureSampler {
public:
  idUniqueTextureSampler()
      : name(), numUsers(0), options{}, sampler(nullptr) {
    options.filter = TF_DEFAULT;
    options.lodMaxClamp = 13.0F;
  }
  idUniqueTextureSampler(const samplerOptions_t& options_, const char* name_);

  idStr name;
  int numUsers;
  samplerOptions_t options;
  idTextureSampler* sampler;
};

template <typename T>
T* idListArrayNew(int num, unsigned int tag, bool zeroBuffer);
template <typename T>
T* idListArrayResize(T* oldPointer, int oldNum, int newNum,
                     unsigned int tag, bool zeroBuffer);
template <typename T>
void idListArrayDelete(T* pointer, int num);

extern const idTextureSamplerManagerVTable textureSamplerManagerLocalVTable;

class idTextureSamplerManagerLocal {
public:
  idTextureSamplerManagerLocal()
      : manager{&textureSamplerManagerLocalVTable}, uniqueTextureSamplers() {}
  ~idTextureSamplerManagerLocal();

  void Init();
  void Shutdown();
  const idTextureSampler* FindSamplerByName(const char* name, bool addUser);
  const char* GetSamplerName(const idTextureSampler* sampler);
  idTextureSampler* FindSamplerByOptions(const samplerOptions_t& options,
                                         bool addUser);
  const idTextureSampler* CreateSampler(const samplerOptions_t* options,
                                        const char* name);
  void DeleteSampler(const idTextureSampler* sampler);

  idTextureSamplerManager manager;
  idList<idUniqueTextureSampler, 5> uniqueTextureSamplers;
};

extern idTextureSamplerManagerLocal textureSamplerManagerLocal;

static_assert(offsetof(idTextureSamplerManager, mipMapSamplers) == 0x18,
              "texture-sampler manager mip table offset drift");
static_assert(sizeof(idTextureSamplerManager) == 0x98,
              "texture-sampler manager ABI drift");
static_assert(sizeof(idTextureSampler) == 4,
              "texture-sampler ABI drift");
static_assert(sizeof(samplerOptions_t) == 44,
              "sampler-options ABI drift");
static_assert(offsetof(idUniqueTextureSampler, numUsers) == 40,
              "unique sampler user-count offset drift");
static_assert(offsetof(idUniqueTextureSampler, options) == 44,
              "unique sampler options offset drift");
static_assert(offsetof(idUniqueTextureSampler, sampler) == 88,
              "unique sampler handle offset drift");
static_assert(sizeof(idUniqueTextureSampler) == 96,
              "unique sampler ABI drift");
static_assert(offsetof(idTextureSamplerManagerLocal, uniqueTextureSamplers) ==
                  0x98,
              "local sampler list offset drift");
static_assert(sizeof(idTextureSamplerManagerLocal) == 0xB0,
              "local sampler manager ABI drift");
