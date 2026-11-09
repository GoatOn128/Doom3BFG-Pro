#pragma once

// NEEDS_WORK(engine/renderer): this is the PDB-derived public boundary needed
// by GUI recovery. Renderer owns the complete image layout and implementation.

#include "engine/decls/decl.h"
#include "engine/renderer/sampleroptions.h"

#include <cstdint>

enum textureType_t : int { TT_2D, TT_3D, TT_CUBIC };
enum textureFormat_t : int { FMT_NONE, FMT_RGBA32F, FMT_RGBA16F, FMT_RGBA8,
  FMT_ARGB8, FMT_ALPHA, FMT_L8A8, FMT_RG8, FMT_LUM8, FMT_INT8, FMT_DXT1,
  FMT_DXT5, FMT_DEPTH, FMT_DEPTH_STENCIL, FMT_X32F, FMT_Y16F_X16F,
  FMT_X16, FMT_Y16_X16, FMT_RGB565 };
enum textureColor_t : int { CFM_DEFAULT, CFM_RGBA_HQ_DXT5, CFM_NORMAL_HQ_DXT5,
  CFM_YCOCG_DXT5, CFM_YCOCGA_DXT5, CFM_YCOCGA_HQ_DXT5, CFM_RGB_HQ_DXT1 };
enum textureBindType_t : int { TB_SHADER, TB_RENDER_TARGET, TB_DEPTH_STENCIL_TARGET };
enum textureUsage_t : int { TU_DEFAULT, TU_STATIC, TU_DYNAMIC, TU_STAGING };
enum textureOptions_t : int { TO_NONE, TO_GENERATE_MIPS };

class idImageOpts {
public:
  textureType_t textureType{};
  textureFormat_t format{};
  textureColor_t colorFormat{};
  int width{};
  int height{};
  int depth{};
  int numLevels{};
  int numSamples{};
  bool packedTail{};
  bool readback{};
  bool linear{};
  bool partiallyResident{};
  bool cubeFilter{};
  bool overlayMemory{};
  bool startPurged{};
  std::uint8_t structPad{};
  textureBindType_t textureBindType{};
  textureUsage_t textureUsage{};
  textureOptions_t textureOptions{};
  samplerOptions_t sampler{};
  std::uint32_t structPad2{};
};

enum imageState_t : int {
  IS_PURGED = 0,
  IS_BACKGROUND_READING = 1,
  IS_READY = 2,
  IMAGE_STATE_UNLOADED = IS_PURGED,
  IMAGE_STATE_LOADING = IS_BACKGROUND_READING,
  IMAGE_STATE_LOADED = IS_READY,
};

class idImage : public idResource {
public:
  enum tiling_t : int {
    INTEL_LINEAR = 0,
    INTEL_X_TILED_4KB = 1,
    INTEL_Y_TILED_4KB = 2,
  };

  idImage();
  ~idImage() override;

  static const char* SkipImageOpts(const char* imageName);
  static void ParseImageOpts(class idParser& parser, idImageOpts& options);
  static void WriteImageOptsStr(const idImageOpts& options, idStr& result);
  static void MakeGeneratedName(char* binaryFileName, int maxLength,
                                const char* imageName);
  static void ParsePastImageLineAndMakeCanonical(class idParser& parser,
                                                 idStr& result,
                                                 const char* overrideOptions);
  static void ImageLineMakeCanonical(const char* source, idStr& result,
                                     const char* overrideOptions = nullptr);
  static void WriteBinaryImage(const char* imageName, idImageOpts options,
                               const std::uint8_t* imageData,
                               std::uint32_t sourceFileTime);
  void LoadResource() override;
  bool ReloadIfStale() override;
  idResourceList* GetResourceList() const override;
  void Print() override;
  void PrintCollated() const;
  void LoadFromBinaryImage(const class idBinaryImage& binaryImage);
  struct idImageData* GetImageData(int mipLevel, int z) const;
  imageState_t ImageState() const;
  int BitsForInternalFormat() const;
  int StorageSize() const;
  bool IsCompressed() const;
  void ModifySampler(const samplerOptions_t& options);
  void DeriveNumLevels();
  void AllocImage(const idImageOpts& imageOptions);
  void MakeDefault();
  void PurgeImage();
  bool MapTexture();
  void UnmapTexture();
  bool IsMapped() const;
  void SetPixel(int mipLevel, int x, int y, const void* data);
  bool Resize(int width, int height, int depth);
  void BackgroundRead(class idFile* file, std::uint64_t offset);
  void SubImageUpload(int mipLevel, int x, int y, int z, int width,
                      int height, const void* pixels, int pixelPitch) const;

  unsigned sourceFileTime{};
  unsigned binaryFileTime{};
  void* backgroundReadData{};
  volatile bool backgroundReadComplete{};
  int frameUsed{};
  int bindCount{};
  idImageOpts opts{};
  bool optsHasBeenSet{};
  bool defaulted{};
  bool allocationFailed{};
  int physicalMemoryBytes{};
  void* imageBuffer{};
  const class idTextureSampler* textureSampler{};
  unsigned oglTexture{~0u};
  unsigned internalFormat{};
  unsigned dataFormat{};
  unsigned dataType{};
  unsigned target{};
  unsigned bindTarget{};
  unsigned uploadTarget{};
  void* buffers[14]{};
  int pitches[14]{};
  tiling_t tiling[14]{};

  static idTypedResourceList<idImage> resourceList;
};

int BitsForFormat(textureFormat_t format);
void ComputeNumLevels(idImageOpts* options, const char* imageName);

template <>
idImage* idTypedResourceList<idImage>::Alloc(const char* name) const;

static_assert(sizeof(samplerOptions_t) == 44, "samplerOptions_t ABI drift");
static_assert(sizeof(idImageOpts) == 100, "idImageOpts ABI drift");
static_assert(offsetof(idImage, opts) == 76, "idImage options offset drift");
static_assert(offsetof(idImage, oglTexture) == 200, "idImage texture offset drift");
static_assert(offsetof(idImage, dataFormat) == 208, "idImage data format offset drift");
static_assert(offsetof(idImage, dataType) == 212, "idImage data type offset drift");
static_assert(offsetof(idImage, buffers) == 0xE8,
              "idImage mapped-buffer offset drift");
static_assert(offsetof(idImage, pitches) == 0x158,
              "idImage mapped-pitch offset drift");
static_assert(offsetof(idImage, tiling) == 0x190,
              "idImage mapped-tiling offset drift");
static_assert(sizeof(idImage) == 0x1C8, "idImage ABI drift");
