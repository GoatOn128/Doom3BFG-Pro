#pragma once

// NEEDS_WORK(engine/renderer): PDB-derived image-manager boundary used by SWF.

#include "engine/renderer/image.h"
#include "shared/idlib/containers/BitArray.h"

class idCmdArgs;
class idCVar;

class idImageManager {
public:
  idImage* AllocImage(const char* name);
  idImage* ScratchImage(const char* name, const idImageOpts& options);
  idImage* ImageFromFile(const char* name, const char* overrideOptions,
                         bool makeDefault);
  void CreateIntrinsicImages();
  void UnbindAll();
  void ChangeTextureFilter();
  void CheckCvars();
  void PurgeAllOverlays();
  void Init();

  static void WriteImage_f(const idCmdArgs& args);
  static void ListImages_f(const idCmdArgs& args);
  static void ListImagesCollated_f(const idCmdArgs& args);

  static idCVar image_useCompression;
  static idCVar image_filter;
  static idCVar image_anisotropy;
  static idCVar image_lodbias;

  idImage* defaultImage{};
  idImage* whiteImage{};
  idImage* borderedPointWhiteImage{};
  idImage* blackImage{};
  idImage* ps3ReplacementImage{};
  class idImageAtlas* transSortImageAtlas{};
  textureFilter_t textureMinFilter{};
  textureFilter_t textureMaxFilter{};
  textureFilter_t textureMipFilter{};
  float textureAnisotropy{};
  float textureLODBias{};
};

class idImageAtlas {
public:
  idImageAtlas(const char* atlasName, int sizeLog2);
  ~idImageAtlas();
  void ClearAtlas();

  int ATLAS_WIDTH_BITS{};
  int ATLAS_HEIGHT_BITS{};
  int ATLAS_MIP_LEVELS{};
  int ATLAS_TILE_WIDTH_BITS{};
  int ATLAS_TILE_HEIGHT_BITS{};
  int ATLAS_WIDTH{};
  int ATLAS_HEIGHT{};
  int ATLAS_TILE_WIDTH{};
  int ATLAS_TILE_HEIGHT{};
  int ATLAS_TILES_WIDE{};
  int ATLAS_TILES_HIGH{};
  int ATLAS_TILES{};
  idBitArray tileAlloc;
  idImage* atlas{};
};

static_assert(offsetof(idImageManager, transSortImageAtlas) == 40,
              "idImageManager transparency atlas offset drift");
static_assert(sizeof(idImageManager) == 72, "idImageManager ABI drift");
static_assert(offsetof(idImageAtlas, tileAlloc) == 0x30,
              "idImageAtlas tile allocator offset drift");
static_assert(offsetof(idImageAtlas, atlas) == 0x48,
              "idImageAtlas image offset drift");
static_assert(sizeof(idImageAtlas) == 0x50, "idImageAtlas ABI drift");

extern idImageManager* globalImages;
