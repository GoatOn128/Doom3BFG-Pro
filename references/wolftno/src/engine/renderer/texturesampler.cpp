#include "engine/renderer/texturesampler.h"

#include "shared/idlib/sys/sys_alloc.h"

#include <new>

namespace {

class ScopedSystemHeap {
public:
  ScopedSystemHeap() { idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP); }
  ~ScopedSystemHeap() { idMem::Singleton()->PopHeap(); }
};

void BaseDestroy(idTextureSamplerManager* manager) {
  manager->~idTextureSamplerManager();
}

void LocalDestroy(idTextureSamplerManager* manager) {
  reinterpret_cast<idTextureSamplerManagerLocal*>(manager)
      ->~idTextureSamplerManagerLocal();
}

void LocalInit(idTextureSamplerManager* manager) {
  reinterpret_cast<idTextureSamplerManagerLocal*>(manager)->Init();
}

void LocalShutdown(idTextureSamplerManager* manager) {
  reinterpret_cast<idTextureSamplerManagerLocal*>(manager)->Shutdown();
}

const idTextureSampler* LocalFindSamplerByName(
    idTextureSamplerManager* manager, const char* name, bool addUser) {
  return reinterpret_cast<idTextureSamplerManagerLocal*>(manager)
      ->FindSamplerByName(name, addUser);
}

const char* LocalGetSamplerName(idTextureSamplerManager* manager,
                                const idTextureSampler* sampler) {
  return reinterpret_cast<idTextureSamplerManagerLocal*>(manager)
      ->GetSamplerName(sampler);
}

const idTextureSampler* LocalCreateSampler(
    idTextureSamplerManager* manager, const samplerOptions_t* options,
    const char* name) {
  return reinterpret_cast<idTextureSamplerManagerLocal*>(manager)
      ->CreateSampler(options, name);
}

void LocalDeleteSampler(idTextureSamplerManager* manager,
                        const idTextureSampler* sampler) {
  reinterpret_cast<idTextureSamplerManagerLocal*>(manager)
      ->DeleteSampler(sampler);
}

const idTextureSamplerManagerVTable textureSamplerManagerBaseVTable{
    BaseDestroy, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

bool SameOptions(const samplerOptions_t& left,
                 const samplerOptions_t& right) {
  return left.filter == right.filter && left.repeat == right.repeat &&
         left.border.x == right.border.x && left.border.y == right.border.y &&
         left.border.z == right.border.z && left.border.w == right.border.w &&
         left.lodBias == right.lodBias &&
         left.lodMinClamp == right.lodMinClamp &&
         left.lodMaxClamp == right.lodMaxClamp && left.aniso == right.aniso &&
         left.forceBias == right.forceBias &&
         left.depthCompareMode == right.depthCompareMode;
}

} // namespace

extern const idTextureSamplerManagerVTable textureSamplerManagerLocalVTable{
    LocalDestroy, LocalInit, LocalShutdown, LocalFindSamplerByName,
    LocalGetSamplerName, LocalCreateSampler, LocalDeleteSampler};

idTextureSamplerManagerLocal textureSamplerManagerLocal;
idTextureSamplerManager* textureSamplerManager =
    &textureSamplerManagerLocal.manager;

idTextureSamplerManager::~idTextureSamplerManager() {
  vtable = &textureSamplerManagerBaseVTable;
}

idUniqueTextureSampler::idUniqueTextureSampler(
    const samplerOptions_t& options_, const char* name_)
    : name(), numUsers(0), options{}, sampler(nullptr) {
  options.filter = TF_DEFAULT;
  options.lodMaxClamp = 13.0F;

  ScopedSystemHeap everythingHereGoesInTheGlobalHeap;
  if (name_ != nullptr) {
    name = name_;
  } else {
    name.Format(
        "sampler_%d_%d_%2.0f_%2.0f_%2.0f_%2.0f_%2.0f_%2.0f_%2.0f_%2.0f_%d_%d",
        options.filter, options.repeat, options.border.x * 10.0F,
        options.border.y * 10.0F, options.border.z * 10.0F,
        options.border.w * 10.0F, options.lodBias * 10.0F,
        options.lodMinClamp, options.lodMaxClamp, options.aniso,
        options.forceBias ? 1 : 0, options.depthCompareMode ? 1 : 0);
  }
  numUsers = 0;
  options = options_;
  sampler = static_cast<idTextureSampler*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
          sizeof(idTextureSampler), TAG_NEW, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  if (sampler != nullptr)
    sampler->oglSamplerState = 0;
}

void idTextureSamplerManagerLocal::Init() {
  samplerOptions_t options{};
  options.filter = TF_NEAREST;
  options.repeat = TR_REPEAT;
  options.forceBias = true;
  manager.pointSamplerRepeating =
      CreateSampler(&options, "pointSamplerRepeating");

  options.repeat = TR_CLAMP;
  manager.pointSampler = CreateSampler(&options, "pointSampler");

  for (int index = 0; index < 16; ++index) {
    options.filter = TF_LINEAR_MIPMAP_NEAREST;
    options.lodBias = static_cast<float>(index);
    options.lodMinClamp = static_cast<float>(index);
    options.lodMaxClamp = static_cast<float>(index);
    manager.mipMapSamplers[index] =
        CreateSampler(&options, va("mipMapSampler%d", index));
  }
}

void idTextureSamplerManagerLocal::Shutdown() {
  for (const idTextureSampler*& sampler : manager.mipMapSamplers) {
    DeleteSampler(sampler);
    sampler = nullptr;
  }
}

const idTextureSampler* idTextureSamplerManagerLocal::FindSamplerByName(
    const char* name_, bool addUser) {
  for (int index = 0; index < uniqueTextureSamplers.num; ++index) {
    idUniqueTextureSampler& candidate = uniqueTextureSamplers.list[index];
    if (candidate.name == name_) {
      if (addUser)
        ++candidate.numUsers;
      return candidate.sampler;
    }
  }
  return nullptr;
}

const char* idTextureSamplerManagerLocal::GetSamplerName(
    const idTextureSampler* sampler_) {
  if (sampler_ == nullptr)
    return "defaultSampler";
  for (int index = 0; index < uniqueTextureSamplers.num; ++index) {
    const idUniqueTextureSampler& candidate =
        uniqueTextureSamplers.list[index];
    if (candidate.sampler == sampler_)
      return candidate.name.data;
  }
  return "";
}

idTextureSampler* idTextureSamplerManagerLocal::FindSamplerByOptions(
    const samplerOptions_t& options_, bool addUser) {
  for (int index = 0; index < uniqueTextureSamplers.num; ++index) {
    idUniqueTextureSampler& candidate = uniqueTextureSamplers.list[index];
    if (SameOptions(candidate.options, options_)) {
      if (addUser)
        ++candidate.numUsers;
      return candidate.sampler;
    }
  }
  return nullptr;
}

void idTextureSamplerManagerLocal::DeleteSampler(
    const idTextureSampler* sampler_) {
  if (sampler_ == nullptr)
    return;
  for (int index = 0; index < uniqueTextureSamplers.num; ++index) {
    idUniqueTextureSampler& candidate = uniqueTextureSamplers.list[index];
    if (candidate.sampler == sampler_) {
      --candidate.numUsers;
      return;
    }
  }
}

template <typename T>
void idListArrayDelete(T* pointer, int num) {
  if (pointer != nullptr) {
    for (int index = 0; index < num; ++index)
      pointer[index].~T();
    idMem::Singleton()->Free(pointer, ALIGN_16);
  }
}

idTextureSamplerManagerLocal::~idTextureSamplerManagerLocal() {
  if ((uniqueTextureSamplers.listStatic == 0 ||
       uniqueTextureSamplers.listStatic == 2) &&
      uniqueTextureSamplers.list != nullptr) {
    idListArrayDelete(uniqueTextureSamplers.list,
                      uniqueTextureSamplers.size);
    uniqueTextureSamplers.list = nullptr;
    uniqueTextureSamplers.size = 0;
  }
  uniqueTextureSamplers.num = 0;
}

template void idListArrayDelete<idUniqueTextureSampler>(
    idUniqueTextureSampler*, int);
