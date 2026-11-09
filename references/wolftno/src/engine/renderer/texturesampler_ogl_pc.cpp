#include "engine/renderer/texturesampler.h"

#include "engine/renderer/gl_api.h"
#include "engine/renderer/glconfig.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstring>
#include <new>

extern bool forceIdListsToReallocateEveryAppend;
class idImageManager;
extern idImageManager* globalImages;

struct textureSamplerImageSettings_t {
  void* images[6];
  textureFilter_t textureMinFilter;
  textureFilter_t textureMaxFilter;
  textureFilter_t textureMipFilter;
  float textureAnisotropy;
  float textureLODBias;
};

static_assert(offsetof(textureSamplerImageSettings_t, textureMinFilter) ==
                  0x30,
              "sampler image settings minimum-filter offset drift");
static_assert(offsetof(textureSamplerImageSettings_t, textureLODBias) == 0x40,
              "sampler image settings LOD-bias offset drift");

const textureSamplerImageSettings_t* ImageSettings() {
  return reinterpret_cast<const textureSamplerImageSettings_t*>(globalImages);
}

class idScopedGlobalHeap {
public:
  idScopedGlobalHeap() { idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP); }
  ~idScopedGlobalHeap() { idMem::Singleton()->PopHeap(); }
};

constexpr unsigned int GL_TEXTURE_MIN_FILTER_VALUE = 0x2801;
constexpr unsigned int GL_TEXTURE_MAG_FILTER_VALUE = 0x2800;
constexpr unsigned int GL_TEXTURE_WRAP_S_VALUE = 0x2802;
constexpr unsigned int GL_TEXTURE_WRAP_T_VALUE = 0x2803;
constexpr unsigned int GL_TEXTURE_BORDER_COLOR_VALUE = 0x1004;
constexpr unsigned int GL_TEXTURE_MIN_LOD_VALUE = 0x813A;
constexpr unsigned int GL_TEXTURE_MAX_LOD_VALUE = 0x813B;
constexpr unsigned int GL_TEXTURE_LOD_BIAS_VALUE = 0x8501;
constexpr unsigned int GL_TEXTURE_MAX_ANISOTROPY_VALUE = 0x84FE;
constexpr unsigned int GL_TEXTURE_COMPARE_MODE_VALUE = 0x884C;

template <typename T>
T* idListArrayNew(int num, unsigned int tag, bool zeroBuffer) {
  if (num <= 0)
    return nullptr;
  const char* location =
      zeroBuffer
          ? "w:\\build\\tech5\\shared\\idlib\\containers/List.h(27) : tag"
          : "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag";
  T* result = static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(T) * static_cast<std::size_t>(num),
      static_cast<memTag_t>(tag), zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP));
  if (result == nullptr)
    return nullptr;
  int constructed = 0;
  try {
    for (; constructed < num; ++constructed)
      ::new (static_cast<void*>(&result[constructed])) T;
  } catch (...) {
    while (constructed > 0)
      result[--constructed].~T();
    idMem::Singleton()->Free(result, ALIGN_16);
    throw;
  }
  return result;
}

template <typename T>
T* idListArrayResize(T* oldPointer, int oldNum, int newNum,
                     unsigned int tag, bool zeroBuffer) {
  T* replacement =
      newNum > 0 ? idListArrayNew<T>(newNum, tag, zeroBuffer) : nullptr;
  if (newNum > 0 && replacement == nullptr)
    return nullptr;
  const int copyCount = (std::min)(oldNum, newNum);
  for (int index = 0; index < copyCount; ++index)
    replacement[index] = oldPointer[index];
  if (oldPointer != nullptr) {
    for (int index = 0; index < oldNum; ++index)
      oldPointer[index].~T();
    idMem::Singleton()->Free(oldPointer, ALIGN_16);
  }
  return replacement;
}

const idTextureSampler* idTextureSamplerManagerLocal::CreateSampler(
    const samplerOptions_t* options, const char* name) {
  idScopedGlobalHeap everythingHereGoesInTheGlobalHeap;
  if (idTextureSampler* existing = FindSamplerByOptions(*options, true))
    return existing;

  idScopedGlobalHeap globalHeap;
  idUniqueTextureSampler newSampler(*options, name);
  uniqueTextureSamplers.Append(newSampler);
  idTextureSampler* const sampler = newSampler.sampler;
  if (idLib::headless)
    return sampler;

  _glewGenSamplers(1, &sampler->oglSamplerState);

  int minFilter = 9729;
  int magFilter = 9729;
  switch (options->filter) {
    case TF_LINEAR:
      minFilter = 9729;
      magFilter = 9729;
      break;
    case TF_NEAREST:
      minFilter = 9728;
      magFilter = 9728;
      break;
    case TF_NEAREST_MIPMAP_NEAREST:
      minFilter = 9984;
      magFilter = 9728;
      break;
    case TF_LINEAR_MIPMAP_NEAREST:
      minFilter = 9985;
      magFilter = 9729;
      break;
    case TF_TRILINEAR:
    case TF_DEFAULT:
      if (ImageSettings()->textureMipFilter != TF_LINEAR) {
        minFilter = 9985 -
                    (ImageSettings()->textureMinFilter != TF_LINEAR ? 1 : 0);
      } else {
        minFilter = 9987 -
                    (ImageSettings()->textureMinFilter != TF_LINEAR ? 1 : 0);
      }
      magFilter =
          9729 - (ImageSettings()->textureMaxFilter != TF_LINEAR ? 1 : 0);
      break;
    default:
      idLib::FatalError("CreateSampler: bad texture filter %d",
                        options->filter);
  }
  _glewSamplerParameteri(sampler->oglSamplerState,
                         GL_TEXTURE_MIN_FILTER_VALUE, minFilter);
  _glewSamplerParameteri(sampler->oglSamplerState,
                         GL_TEXTURE_MAG_FILTER_VALUE, magFilter);

  if (glConfig.anisotropicAvailable) {
    float anisotropy = 1.0F;
    if (options->filter == TF_DEFAULT &&
        ImageSettings()->textureAnisotropy >= 1.0F) {
      anisotropy = ImageSettings()->textureAnisotropy;
    } else if (options->aniso >= 1.0F) {
      anisotropy = options->aniso;
    }
    _glewSamplerParameterf(sampler->oglSamplerState,
                           GL_TEXTURE_MAX_ANISOTROPY_VALUE, anisotropy);
  }

  const float lodBias = options->forceBias
                            ? options->lodBias
                            : ImageSettings()->textureLODBias + options->lodBias;
  _glewSamplerParameterf(sampler->oglSamplerState,
                         GL_TEXTURE_LOD_BIAS_VALUE, lodBias);
  _glewSamplerParameterf(sampler->oglSamplerState,
                         GL_TEXTURE_MIN_LOD_VALUE, options->lodMinClamp);
  _glewSamplerParameterf(sampler->oglSamplerState,
                         GL_TEXTURE_MAX_LOD_VALUE, options->lodMaxClamp);
  _glewSamplerParameterfv(sampler->oglSamplerState,
                          GL_TEXTURE_BORDER_COLOR_VALUE, &options->border.x);

  int wrapS = 10497;
  int wrapT = 10497;
  switch (options->repeat) {
    case TR_REPEAT:
      break;
    case TR_CLAMP:
      wrapS = wrapT = 33071;
      break;
    case TR_CLAMP_S:
      wrapS = 33071;
      break;
    case TR_CLAMP_T:
      wrapT = 33071;
      break;
    case TR_CLAMP_TO_BORDER:
      wrapS = wrapT = 33069;
      break;
    case TR_MIRROR:
      wrapS = wrapT = 33648;
      break;
    default:
      idLib::FatalError("CreateSampler: bad texture repeat");
  }
  _glewSamplerParameteri(sampler->oglSamplerState, GL_TEXTURE_WRAP_S_VALUE,
                         wrapS);
  _glewSamplerParameteri(sampler->oglSamplerState, GL_TEXTURE_WRAP_T_VALUE,
                         wrapT);
  _glewSamplerParameteri(sampler->oglSamplerState,
                         GL_TEXTURE_COMPARE_MODE_VALUE,
                         options->depthCompareMode ? 34894 : 0);
  return sampler;
}

template idUniqueTextureSampler* idListArrayNew<idUniqueTextureSampler>(
    int, unsigned int, bool);
template idUniqueTextureSampler* idListArrayResize<idUniqueTextureSampler>(
    idUniqueTextureSampler*, int, int, unsigned int, bool);
template bool idList<idUniqueTextureSampler, 5>::Resize(int);
template int idList<idUniqueTextureSampler, 5>::Append(
    const idUniqueTextureSampler&);
