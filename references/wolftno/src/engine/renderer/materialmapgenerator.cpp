#include "engine/renderer/declmaterial.h"

#include "engine/renderer/autorenderparms.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cstdint>
#include <utility>

struct imageProperties_t {
  const idMaterial* mtr;
  int x;
  int y;
  int width;
  int height;
  int area;
};

static_assert(sizeof(imageProperties_t) == 32,
              "imageProperties_t ABI drift");

template <typename T> class idSort {
public:
  virtual ~idSort() = default;
  virtual void Sort(T* base, unsigned int num) const = 0;
};

template <typename T, typename Derived>
class idSort_Quick : public idSort<T> {
public:
  void Sort(T* base, unsigned int num) const override {
    if (num <= 1) {
      return;
    }

    constexpr std::int64_t MAX_LEVELS = 128;
    std::int64_t lo[MAX_LEVELS];
    std::int64_t hi[MAX_LEVELS];
    std::int64_t low = 0;
    std::int64_t high = num - 1;
    std::int64_t level = 0;

    for (;;) {
      std::int64_t i = low;
      std::int64_t j = high;
      if (j - i >= 4 && level < MAX_LEVELS - 1) {
        const std::int64_t pivotIndex = (i + j) / 2;
        std::swap(base[j], base[pivotIndex]);
        T& pivot = base[j--];

        do {
          while (static_cast<const Derived*>(this)->Compare(base[i], pivot) <
                 0) {
            if (++i >= j) {
              break;
            }
          }
          while (static_cast<const Derived*>(this)->Compare(base[j], pivot) >
                 0) {
            if (--j <= i) {
              break;
            }
          }
          if (i >= j) {
            break;
          }
          std::swap(base[i], base[j]);
        } while (++i < --j);

        while (static_cast<const Derived*>(this)->Compare(base[i], pivot) <=
                   0 &&
               i < high) {
          ++i;
        }
        while (static_cast<const Derived*>(this)->Compare(base[j], pivot) >=
                   0 &&
               low < j) {
          --j;
        }

        std::swap(pivot, base[i]);
        if (high - i <= j - low) {
          lo[level] = low;
          hi[level++] = j;
          low = i;
        } else {
          hi[level] = high;
          lo[level++] = i;
          high = j;
        }
      } else {
        for (; i < j; --j) {
          std::int64_t maximum = i;
          for (std::int64_t index = i + 1; index <= j; ++index) {
            if (static_cast<const Derived*>(this)->Compare(base[index],
                                                            base[maximum]) >
                0) {
              maximum = index;
            }
          }
          std::swap(base[maximum], base[j]);
        }
        if (--level < 0) {
          return;
        }
        low = lo[level];
        high = hi[level];
      }
    }
  }
};

class idSort_ImageProperties
    : public idSort_Quick<imageProperties_t, idSort_ImageProperties> {
public:
  ~idSort_ImageProperties() override;

  int Compare(const imageProperties_t& left,
              const imageProperties_t& right) const {
    return right.area - left.area;
  }
};

idSort_ImageProperties::~idSort_ImageProperties() = default;

namespace {

constexpr int MATERIAL_MAP_GUTTER = 16;
constexpr int MATERIAL_MAP_MAX_SIZE = 0x8000;

const idDeclRenderParm* RenderParm(
    const idCodeResource<idDeclRenderParm>& resource) {
  return resource.Get();
}

bool RectanglesOverlapWithGutter(const imageProperties_t& candidate,
                                 const imageProperties_t& placed) {
  return candidate.x + candidate.width + MATERIAL_MAP_GUTTER > placed.x &&
         candidate.y + candidate.height + MATERIAL_MAP_GUTTER > placed.y &&
         candidate.x < placed.x + placed.width + MATERIAL_MAP_GUTTER &&
         candidate.y < placed.y + placed.height + MATERIAL_MAP_GUTTER;
}

} // namespace

const idMaterial* idMaterial::PackMaterials(
    const char* name, const idList<const idMaterial*, 5>& materials,
    unsigned int skinFileTimeStamp) {
  if (materials.Num() <= 0) {
    return nullptr;
  }

  const int startTime = static_cast<int>(Sys_Milliseconds());

  idList<imageProperties_t, 46> imageProperties;
  imageProperties.SetNum(materials.Num());
  for (int index = 0; index < materials.Num(); ++index) {
    imageProperties_t& properties = imageProperties[index];
    properties.mtr = materials[index];
    properties.x = 0;
    properties.y = 0;
    properties.mtr->GetImageDimensions(properties.width, properties.height);
    properties.area = properties.width * properties.height;
  }

  idSort_ImageProperties sorter;
  sorter.Sort(imageProperties.list,
              static_cast<unsigned int>(imageProperties.Num()));

  imageProperties[0].x = 0;
  imageProperties[0].y = 0;
  int packedWidth = imageProperties[0].width;
  int packedHeight = imageProperties[0].height;

  for (int imageIndex = 1; imageIndex < imageProperties.Num(); ++imageIndex) {
    imageProperties_t& candidate = imageProperties[imageIndex];
    int bestX = 0;
    int bestY = 0;
    int bestWidth = MATERIAL_MAP_MAX_SIZE;
    int bestHeight = MATERIAL_MAP_MAX_SIZE;

    for (int placedIndex = 0; placedIndex < imageIndex; ++placedIndex) {
      const imageProperties_t& anchor = imageProperties[placedIndex];
      for (int anchorCorner = 0; anchorCorner < 4; ++anchorCorner) {
        const int anchorX =
            anchor.x + ((anchorCorner & 1) != 0 ? anchor.width : 0);
        const int anchorY =
            anchor.y + ((anchorCorner & 2) != 0 ? anchor.height : 0);

        for (int gutterCorner = 0; gutterCorner < 4; ++gutterCorner) {
          candidate.x =
              anchorX + ((gutterCorner & 1) != 0 ? MATERIAL_MAP_GUTTER : 0);
          candidate.y =
              anchorY + ((gutterCorner & 2) != 0 ? MATERIAL_MAP_GUTTER : 0);

          const int newWidth =
              std::max(packedWidth, candidate.x + candidate.width);
          const int newHeight =
              std::max(packedHeight, candidate.y + candidate.height);
          if (newWidth * newHeight >= bestWidth * bestHeight) {
            continue;
          }

          int overlapIndex = 0;
          for (; overlapIndex < imageIndex; ++overlapIndex) {
            if (RectanglesOverlapWithGutter(candidate,
                                            imageProperties[overlapIndex])) {
              break;
            }
          }
          if (overlapIndex == imageIndex) {
            bestX = candidate.x;
            bestY = candidate.y;
            bestWidth = newWidth;
            bestHeight = newHeight;
          }
        }
      }
    }

    candidate.x = bestX;
    candidate.y = bestY;
    packedWidth = bestWidth;
    packedHeight = bestHeight;
  }

  auto* materialMap = new idMaterialMap;
  materialMap->name = name;
  materialMap->width = packedWidth;
  materialMap->height = packedHeight;
  materialMap->skinFileTimeStamp = skinFileTimeStamp;

  bool hasDiffuseMap = false;
  bool hasSpecularMap = false;
  bool hasBumpMap = false;
  bool hasCoverMap = false;
  bool hasPowerMap = false;

  for (int index = 0; index < imageProperties.Num(); ++index) {
    const imageProperties_t& properties = imageProperties[index];
    const idMaterial* sourceMaterial = properties.mtr;
    idMaterialMapping mapping;
    mapping.materialName.Set(sourceMaterial->name.str);
    mapping.materialChecksum = sourceMaterial->md5Checksum;
    mapping.materialTimestamp = sourceMaterial->GetDeclTimestamp();
    mapping.x = properties.x;
    mapping.y = properties.y;
    mapping.width = properties.width;
    mapping.height = properties.height;
    mapping.scaleBias.x =
        static_cast<float>(mapping.width) / static_cast<float>(packedWidth);
    mapping.scaleBias.y =
        static_cast<float>(mapping.height) / static_cast<float>(packedHeight);
    mapping.scaleBias.z =
        static_cast<float>(mapping.x) / static_cast<float>(packedWidth);
    mapping.scaleBias.w =
        static_cast<float>(mapping.y) / static_cast<float>(packedHeight);

    mapping.bumpFile.fileName.Set(sourceMaterial->parmBlock.GetString(
        RenderParm(rp_bumpMap), nullptr));
    mapping.diffuseFile.fileName.Set(sourceMaterial->parmBlock.GetString(
        RenderParm(rp_diffuseMap), nullptr));
    mapping.specularFile.fileName.Set(sourceMaterial->parmBlock.GetString(
        RenderParm(rp_specularMap), nullptr));
    mapping.powerFile.fileName.Set(sourceMaterial->parmBlock.GetString(
        RenderParm(rp_powerMap), nullptr));
    mapping.coverFile.fileName.Set(sourceMaterial->parmBlock.GetString(
        RenderParm(rp_coverMap), nullptr));

    hasDiffuseMap |= sourceMaterial->parmBlock.SetsRenderParm(
        RenderParm(rp_diffuseMap));
    hasSpecularMap |= sourceMaterial->parmBlock.SetsRenderParm(
        RenderParm(rp_specularMap));
    hasBumpMap |=
        sourceMaterial->parmBlock.SetsRenderParm(RenderParm(rp_bumpMap));
    hasCoverMap |=
        sourceMaterial->parmBlock.SetsRenderParm(RenderParm(rp_coverMap));
    hasPowerMap |=
        sourceMaterial->parmBlock.SetsRenderParm(RenderParm(rp_powerMap));
    hasPowerMap |=
        sourceMaterial->parmBlock.SetsRenderParm(RenderParm(rp_powerMip));

    materialMap->mappings.Append(mapping);
  }

  idStr fileName(name);
  fileName.Replace("/", "");
  fileName.Replace("\\", "");
  const idStr packedFileBase(fileName);
  fileName.Format("generated/skins/m2/%s.m2", packedFileBase.data);

  idFile_Memory file;
  file.Printf("{\n");
  file.Printf("\tmaterialmap\n");
  file.Printf("\t{\n");
  file.Printf("\t\t\"%s\"\n", materialMap->name.data);
  file.Printf("\t\t%d %d %d\n", materialMap->width, materialMap->height,
              materialMap->skinFileTimeStamp);
  for (int index = 0; index < materialMap->mappings.Num(); ++index) {
    const idMaterialMapping& mapping = materialMap->mappings[index];
    file.Printf("\t\t{\n");
    file.Printf("\t\t\t\"%s\" %d %d\n", mapping.materialName.str,
                mapping.materialChecksum, mapping.materialTimestamp);
    file.Printf("\t\t\t%d %d %d %d\n", mapping.x, mapping.y,
                mapping.width, mapping.height);
    file.Printf("\t\t\t\"%s\" %d\n", mapping.specularFile.fileName.str,
                mapping.specularFile.timeStamp);
    file.Printf("\t\t\t\"%s\" %d\n", mapping.coverFile.fileName.str,
                mapping.coverFile.timeStamp);
    file.Printf("\t\t\t\"%s\" %d\n", mapping.diffuseFile.fileName.str,
                mapping.diffuseFile.timeStamp);
    file.Printf("\t\t\t\"%s\" %d\n", mapping.bumpFile.fileName.str,
                mapping.bumpFile.timeStamp);
    file.Printf("\t\t\t\"%s\" %d\n", mapping.powerFile.fileName.str,
                mapping.powerFile.timeStamp);
    file.Printf("\t\t}\n");
  }
  file.Printf("\t}\n");
  if (hasSpecularMap) {
    file.Printf(
        "\tspecularmap\t\t\tgenerated/skins/textures/%s_specular.tga\n",
        name);
  }
  if (hasCoverMap) {
    file.Printf("\tcovermap\t\t\tgenerated/skins/textures/%s_cover.tga\n",
                name);
  }
  if (hasDiffuseMap) {
    file.Printf(
        "\tdiffusemap\t\t\tgenerated/skins/textures/%s_diffuse.tga\n",
        name);
  }
  if (hasBumpMap) {
    file.Printf("\tbumpmap\t\t\t\tgenerated/skins/textures/%s_bump.tga\n",
                name);
  }
  if (hasPowerMap) {
    file.Printf("\tpowermap\t\t\tgenerated/skins/textures/%s_power.tga\n",
                name);
  }
  file.Printf("}\n");

  delete materialMap;

  idMaterial* result = static_cast<idMaterial*>(
      resourceList.FindWithInheritance(name, false, false));
  if (result != nullptr) {
    result->SetText(file.filePtr, 0);
    result->ReplaceSourceFileText(false);
    result->ReParse(true);
  } else {
    idDecl* created = declManager->CreateNewDecl(
        &resourceList, name, fileName.data, file.filePtr);
    created->ReplaceSourceFileText(false);
    resourceList.Remove(created);
    delete created;

    char remappedName[512];
    result = static_cast<idMaterial*>(resourceList.Load(
        resourceList.RemapResourceName(remappedName,
                                       static_cast<int>(sizeof(remappedName)),
                                       name),
        true, false));
  }

  const int endTime = static_cast<int>(Sys_Milliseconds());
  idLib::Printf("%1.1f seconds to pack material %s\n",
                static_cast<float>(endTime - startTime) * 0.001F, name);
  return result;
}
