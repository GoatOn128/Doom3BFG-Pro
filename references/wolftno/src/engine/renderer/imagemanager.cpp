#include "engine/renderer/imagemanager.h"

#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/imagedata.h"
#include "engine/renderer/imagetools/image_files.h"
#include "engine/renderer/renderersystem.h"
#include "engine/renderer/renderlocal.h"
#include "engine/renderer/texturesampler.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <malloc.h>
#include <new>

struct imageClassificate_t {
  const char* rootPath;
  const char* description;
  int type;
  int maxWidth;
  int maxHeight;
};

struct sortedImage_t {
  idImage* image;
  int size;
};

struct filterName_t {
  const char* name;
  textureFilter_t minimize;
  textureFilter_t maximize;
  textureFilter_t mipimize;
};

const imageClassificate_t IC_Info[] = {
    {"models/characters", "Characters", 0, 512, 512},
    {"models/weapons", "Weapons", 1, 512, 512},
    {"models/monsters", "Monsters", 2, 512, 512},
    {"models/mapobjects", "Model Geometry", 3, 512, 512},
    {"models/items", "Items", 4, 512, 512},
    {"models", "Other model textures", 5, 512, 512},
    {"swf/", "SWF Guis", 6, 1024, 1024},
    {"textures/guis", "Other Guis", 7, 256, 256},
    {"textures", "World Geometry", 8, 256, 256},
    {"env/", "Environment Cubes", 9, 128, 128},
    {"", "Other", 10, 256, 256},
};

const char* imageFilter[] = {
    "GL_LINEAR_MIPMAP_NEAREST", "GL_LINEAR_MIPMAP_LINEAR", "GL_NEAREST",
    "GL_LINEAR", "GL_NEAREST_MIPMAP_NEAREST",
    "GL_NEAREST_MIPMAP_LINEAR", nullptr};

constexpr filterName_t textureFilters[] = {
    {"GL_LINEAR", TF_LINEAR, TF_LINEAR, TF_LINEAR},
    {"GL_LINEAR_MIPMAP_LINEAR", TF_LINEAR, TF_LINEAR, TF_LINEAR},
    {"GL_LINEAR_MIPMAP_NEAREST", TF_LINEAR, TF_LINEAR, TF_NEAREST},
    {"GL_NEAREST_MIPMAP_LINEAR", TF_NEAREST, TF_NEAREST, TF_LINEAR},
    {"GL_NEAREST_MIPMAP_NEAREST", TF_NEAREST, TF_NEAREST, TF_NEAREST},
    {"GL_NEAREST", TF_NEAREST, TF_NEAREST, TF_NEAREST},
};

idCVar image_transSortImageSize(
    "image_transSortImageSize", "8192", CVAR_ARCHIVE,
    "size of the trans-sort image atlas");

static int ClassifyImage(const char* name) {
  idStr imageName(name);
  for (const imageClassificate_t& classification : IC_Info) {
    if (imageName.Find(classification.rootPath, false, 0, -1) == 0) {
      return classification.type;
    }
  }
  return 10;
}

static int R_QsortImageSizes(const void* left, const void* right) {
  const auto* a = static_cast<const sortedImage_t*>(left);
  const auto* b = static_cast<const sortedImage_t*>(right);
  if (a->size > b->size) {
    return -1;
  }
  if (a->size < b->size) {
    return 1;
  }
  return idStr::Icmp(a->image->GetName(), b->image->GetName());
}

idImageManager imageManager{};
idImageManager* globalImages = &imageManager;

idCVar idImageManager::image_useCompression(
    "image_useCompression", "1", CVAR_BOOL | CVAR_ARCHIVE,
    "0 = force everything to high quality");
idCVar idImageManager::image_filter(
    "image_filter", imageFilter[1], CVAR_ARCHIVE,
    "changes texture filtering on mipmapped images", 0.0F, 0.0F,
    imageFilter, idCmdSystem::ArgCompletion_String<imageFilter>);
idCVar idImageManager::image_anisotropy(
    "image_anisotropy", "1", CVAR_ARCHIVE,
    "set the maximum texture anisotropy if available");
idCVar idImageManager::image_lodbias(
    "image_lodbias", "0", CVAR_ARCHIVE,
    "change lod bias on mipmapped images");

idImage* idImageManager::AllocImage(const char* name) {
  if (idStr::Length(name) >= 256) {
    idLib::Error("idImageManager::AllocImage: \"%s\" is too long\n", name);
  }

  void* storage = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idImage), TAG_IMAGE, false, ALIGN_16, HEAP_DEFAULTHEAP);
  idImage* image = storage != nullptr ? new (storage) idImage : nullptr;
  image->SetName(name);
  idImage::resourceList.Add(image);
  return image;
}

idImage* idImageManager::ImageFromFile(const char* name,
                                       const char* overrideOptions,
                                       bool makeDefault) {
  idStr canonicalName;
  idImage::ImageLineMakeCanonical(name, canonicalName, overrideOptions);
  return static_cast<idImage*>(
      idImage::resourceList.Load(canonicalName.data, makeDefault, false));
}

idImage* idImageManager::ScratchImage(const char* name,
                                      const idImageOpts& imageOptions) {
  if (name == nullptr || name[0] == '\0') {
    idLib::FatalError("idImageManager::ScratchImage called with empty name");
  }

  idStr scratchName(name);
  idImage* image = static_cast<idImage*>(
      idImage::resourceList.FindExisting(scratchName.data, false));
  if (image != nullptr) {
    image->PurgeImage();
    image->loadedResource = false;
  } else {
    image = AllocImage(scratchName.data);
  }

  if (imageOptions.startPurged) {
    std::memmove(&image->opts, &imageOptions, sizeof(image->opts));
  } else {
    image->AllocImage(imageOptions);
  }
  return image;
}

void idImageManager::UnbindAll() {
  if (!idLib::headless) {
    for (int unit = 15; unit >= 0; --unit) {
      idDeclRenderProg::BindTexture(0, whiteImage, nullptr);
    }
  }
}

void idImageManager::ChangeTextureFilter() {
  image_filter.flags &= ~CVAR_MODIFIED;
  image_anisotropy.flags &= ~CVAR_MODIFIED;
  image_lodbias.flags &= ~CVAR_MODIFIED;

  const char* filter = image_filter.valueString.data;
  int filterIndex = 0;
  while (filterIndex < static_cast<int>(std::size(textureFilters)) &&
         idStr::Icmp(textureFilters[filterIndex].name, filter) != 0) {
    ++filterIndex;
  }
  if (filterIndex == static_cast<int>(std::size(textureFilters))) {
    idLib::Warning("bad r_textureFilter: '%s'", filter);
    filterIndex = 2;
  }

  textureMinFilter = textureFilters[filterIndex].minimize;
  textureMaxFilter = textureFilters[filterIndex].maximize;
  textureMipFilter = textureFilters[filterIndex].mipimize;
  textureAnisotropy = std::max(image_anisotropy.valueFloat, 1.0F);
  textureLODBias = image_lodbias.valueFloat;

  for (int index = 0; index < idImage::resourceList.resourceList.Num();
       ++index) {
    idImage* image =
        static_cast<idImage*>(idImage::resourceList.Index(index));
    textureSamplerManager->DeleteSampler(image->textureSampler);
    image->textureSampler =
        textureSamplerManager->CreateSampler(&image->opts.sampler, nullptr);
  }
}

void idImageManager::WriteImage_f(const idCmdArgs& args) {
  if (args.argc < 3) {
    idLib::Printf("usage: writeImage <destFile> <imageProgram>\n");
    return;
  }

  idImage* image = globalImages->ImageFromFile(args.argv[2], nullptr, true);
  idImageData* imageData = image->GetImageData(0, 0);
  idLib::Printf("Writing %ix%i image to %s\n", imageData->width,
                imageData->height, args.argv[1]);
  R_WriteTGA(args.argv[1], imageData->data, imageData->width,
             imageData->height, 4, false, true, FSPATH_BASE);
  if (imageData->data != nullptr) {
    idMem::Singleton()->Free(imageData->data, ALIGN_16);
  }
  if (imageData->floatData != nullptr) {
    idMem::Singleton()->Free(imageData->floatData, ALIGN_16);
  }
  idMem::Singleton()->Free(imageData, ALIGN_16);
}

void idImageManager::CheckCvars() {
  if ((image_filter.flags & CVAR_MODIFIED) != 0 ||
      (image_anisotropy.flags & CVAR_MODIFIED) != 0 ||
      (image_lodbias.flags & CVAR_MODIFIED) != 0) {
    ChangeTextureFilter();
  }
}

void idImageManager::PurgeAllOverlays() {
  idLib::Printf("Purging Overlays\n");
  renderSystem->SyncRenderThread(true);
  for (int index = 0; index < idImage::resourceList.resourceList.Num();
       ++index) {
    idImage* image =
        static_cast<idImage*>(idImage::resourceList.Index(index));
    if (image->opts.overlayMemory) {
      image->PurgeImage();
    }
  }
}

void idImageManager::ListImages_f(const idCmdArgs& args) {
  bool uncompressed = false;
  bool active = false;
  bool scratch = false;
  bool files = false;
  bool sorted = false;
  bool touched = false;
  bool cubes = false;
  bool classify = false;
  bool oversized = false;
  bool invalid = false;

  for (int index = 1; index < args.argc; ++index) {
    const char* option = args.Argv(index);
    if (idStr::Icmp(option, "uncompressed") == 0) {
      uncompressed = true;
    } else if (idStr::Icmp(option, "active") == 0 ||
               idStr::Icmp(option, "unloaded") == 0) {
      active = true;
    } else if (idStr::Icmp(option, "scratch") == 0) {
      scratch = true;
    } else if (idStr::Icmp(option, "files") == 0) {
      files = true;
    } else if (idStr::Icmp(option, "sorted") == 0) {
      sorted = true;
    } else if (idStr::Icmp(option, "touched") == 0) {
      touched = true;
    } else if (idStr::Icmp(option, "cubes") == 0) {
      cubes = true;
    } else if (idStr::Icmp(option, "isolated") == 0 ||
               idStr::Icmp(option, "block") == 0 ||
               idStr::Icmp(option, "overlay") == 0) {
      // Accepted retail shipping-build options with no additional filter.
    } else if (idStr::Icmp(option, "classify") == 0) {
      classify = true;
      sorted = true;
    } else if (idStr::Icmp(option, "oversized") == 0) {
      oversized = true;
      classify = true;
      sorted = true;
    } else {
      invalid = true;
    }
  }

  if (invalid) {
    idLib::Printf(
        "usage: listImages [unloaded] [scratch] [files] [cubes] [covers] [isolated] [block] [ active | sorted | touched | classify | oversized ]\n");
    return;
  }

  constexpr const char* header =
      "       -w-- -h-- lv filt -fmt-- wrap  size- --name-------\n";
  idLib::Printf("\n%s", header);

  idList<idResource*, 99> loadedResources;
  idImage::resourceList.GetLoadedResources(loadedResources);
  const int arrayCount = idImage::resourceList.resourceList.Num();
  auto* sortedImages = static_cast<sortedImage_t*>(
      _alloca(sizeof(sortedImage_t) * static_cast<std::size_t>(arrayCount)));
  int count = 0;
  int totalSize = 0;
  for (int index = 0; index < loadedResources.Num(); ++index) {
    idImage* image = static_cast<idImage*>(loadedResources[index]);
    if ((cubes && image->opts.textureType != TT_CUBIC) ||
        (active && image->frameUsed < tr.frameCount - 1) ||
        (uncompressed && !image->IsCompressed()) ||
        (scratch && image->loadedResource) ||
        (files && !image->loadedResource)) {
      continue;
    }
    if (touched) {
      if (image->bindCount == 0) {
        continue;
      }
      image->bindCount = 0;
    }

    if (sorted) {
      sortedImages[count].image = image;
      sortedImages[count].size = image->StorageSize();
    } else {
      idLib::Printf("%4i:", index);
      image->Print();
    }
    totalSize += image->StorageSize();
    ++count;
  }

  if (sorted) {
    std::qsort(sortedImages, static_cast<std::size_t>(count),
               sizeof(sortedImage_t), R_QsortImageSizes);
    int partialSize = 0;
    for (int index = 0; index < count; ++index) {
      idLib::Printf("%4i:", index);
      sortedImages[index].image->Print();
      partialSize += sortedImages[index].image->StorageSize();
      if ((index + 1) % 10 == 0) {
        idLib::Printf("-------- %5.1f of %5.1f megs --------\n",
                      static_cast<double>(partialSize) / 1048576.0,
                      static_cast<double>(totalSize) / 1048576.0);
      }
    }
  }

  idLib::Printf("%s", header);
  idLib::Printf(" %i images (%i total)\n", count,
                idImage::resourceList.resourceList.Num());
  idLib::Printf(" %5.1f total megabytes of images\n\n\n",
                static_cast<double>(totalSize) / 1048576.0);

  if (!classify) {
    return;
  }

  idList<int, TAG_IMAGE> classifications[11];
  for (int index = 0; index < count; ++index) {
    classifications[ClassifyImage(sortedImages[index].image->GetName())]
        .Append(index);
  }

  for (int classification = 0; classification < 11; ++classification) {
    int classificationSize = 0;
    idList<int, TAG_IMAGE> oversizedImages;
    for (int listIndex = 0;
         listIndex < classifications[classification].Num(); ++listIndex) {
      const int imageIndex = classifications[classification][listIndex];
      idImage* image = sortedImages[imageIndex].image;
      classificationSize += image->StorageSize();
      if (oversized &&
          image->opts.width > IC_Info[classification].maxWidth &&
          image->opts.height > IC_Info[classification].maxHeight) {
        oversizedImages.Append(imageIndex);
      }
    }
    idLib::Printf(" Classification %s contains %i images using %5.1f megabytes\n",
                  IC_Info[classification].description,
                  classifications[classification].Num(),
                  static_cast<double>(classificationSize) / 1048576.0);
    if (oversized && oversizedImages.Num() != 0) {
      idLib::Printf("  The following images may be oversized\n");
      for (int index = 0; index < oversizedImages.Num(); ++index) {
        idLib::Printf("    ");
        sortedImages[oversizedImages[index]].image->Print();
        idLib::Printf("\n");
      }
    }
  }
}

void idImageManager::ListImagesCollated_f(const idCmdArgs& args) {
  if (args.argc != 2) {
    idLib::Printf("usage: listImagesCollated descField\n");
    return;
  }

  idList<idResource*, 99> loadedResources;
  idImage::resourceList.GetLoadedResources(loadedResources);
  const int arrayCount = idImage::resourceList.resourceList.Num();
  auto* sortedImages = static_cast<sortedImage_t*>(
      _alloca(sizeof(sortedImage_t) * static_cast<std::size_t>(arrayCount)));
  int count = 0;
  for (int index = 0; index < loadedResources.Num(); ++index) {
    idImage* image = static_cast<idImage*>(loadedResources[index]);
    sortedImages[count].image = image;
    sortedImages[count].size = image->StorageSize();
    ++count;
  }
  std::qsort(sortedImages, static_cast<std::size_t>(count),
             sizeof(sortedImage_t), R_QsortImageSizes);
  for (int index = 0; index < count; ++index) {
    idLib::Printf("%s,", args.Argv(1));
    sortedImages[index].image->PrintCollated();
  }
}

void idImageManager::Init() {
  ChangeTextureFilter();
  CreateIntrinsicImages();

  int atlasSize = image_transSortImageSize.valueInteger;
  atlasSize |= atlasSize >> 1;
  atlasSize |= atlasSize >> 2;
  atlasSize |= atlasSize >> 4;
  atlasSize |= atlasSize >> 8;
  atlasSize |= atlasSize >> 16;
  atlasSize = (atlasSize + 1) >> 1;
  atlasSize = std::max(atlasSize, 8);
  const int atlasSizeLog2 =
      static_cast<int>(1.442695F * std::log(static_cast<float>(atlasSize)) +
                       0.5F);

  void* storage = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idImageAtlas), TAG_IMAGE, false, ALIGN_16, HEAP_DEFAULTHEAP);
  transSortImageAtlas = storage != nullptr
                            ? new (storage) idImageAtlas(
                                  "_transSortImageAtlas", atlasSizeLog2)
                            : nullptr;

  cmdSystem->AddCommand("listImages",
                        reinterpret_cast<commandFunction_t>(ListImages_f),
                        "lists images", nullptr);
  cmdSystem->AddCommand(
      "listImagesCollated",
      reinterpret_cast<commandFunction_t>(ListImagesCollated_f),
      "lists images in a format good for spread sheet compilation", nullptr);
  cmdSystem->AddCommand("writeImage",
                        reinterpret_cast<commandFunction_t>(WriteImage_f),
                        "Writes a tga for a given image",
                        reinterpret_cast<argCompletion_t>(
                            idRenderSystemLocal::ArgCompletion_ImageName));
}
