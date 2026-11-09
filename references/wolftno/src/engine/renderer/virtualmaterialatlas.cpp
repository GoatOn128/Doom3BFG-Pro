#include "engine/renderer/virtualtexture.h"

#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/imagetools/image_program.h"
#include "engine/renderer/materialmap.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/cmdargs.h"
#include "shared/idlib/text/lexer.h"

#include <new>

#if defined(WOLF_RENDERER_VIRTUALMATERIALATLAS_VALIDATION)
const idDeclRenderParm* WolfVirtualMaterialAtlasFindRenderParm(
    const char* name);
#endif

namespace {

const idDeclRenderParm* FindRenderParm(const char* name) {
#if defined(WOLF_RENDERER_VIRTUALMATERIALATLAS_VALIDATION)
  return WolfVirtualMaterialAtlasFindRenderParm(name);
#else
  return static_cast<const idDeclRenderParm*>(
      idDeclRenderParm::resourceList.FindWithInheritance(name, true, true));
#endif
}

idVirtualMaterial* AllocateVirtualMaterial() {
  void* const memory = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idVirtualMaterial), TAG_VIRTUALTEXTURE, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  return memory != nullptr ? ::new (memory) idVirtualMaterial() : nullptr;
}

} // namespace

void idVMTRData::SetupVirtualMaterial(idVirtualMaterial* vmtr) {
  const float virtualTexelsWide = static_cast<float>(vt->VIRTUAL_TEXELS_WIDE);
  vmtr->scaleAndBias.x =
      static_cast<float>(vmtr->allocWidth) / virtualTexelsWide;
  vmtr->scaleAndBias.y =
      static_cast<float>(vmtr->allocHeight) / virtualTexelsWide;
  vmtr->scaleAndBias.z =
      static_cast<float>(vmtr->allocX) / virtualTexelsWide;
  vmtr->scaleAndBias.w =
      static_cast<float>(vmtr->allocY) / virtualTexelsWide;
  if (vmtr->material != nullptr) {
    vmtr->material->UpdateVirtualMapping();
  }
}

void idVirtualTextureSystem::PrintVirtualMaterial_f(const idCmdArgs& args) {
  if (args.argc != 2) {
    idLib::Printf("USAGE: printVirtualMaterial <materialName>\n");
    return;
  }

  for (int pageSource = 1; pageSource < PAGESOURCE_TOTAL; ++pageSource) {
    idVirtualTexture& virtualTexture = virtualTextureSystem.vts[pageSource];
    if (virtualTexture.flavor != ID_VTF_VMTR) {
      continue;
    }

    for (int index = 0; index < virtualTexture.vmtrData->vmtrs.num; ++index) {
      idVirtualMaterial* const vmtr =
          virtualTexture.vmtrData->vmtrs.list[index];
      if (idStr::Icmp(args.Argv(1), vmtr->materialName.str) != 0) {
        continue;
      }

      idLib::Printf("alloc blocks: %3i,%3i : %2i x %2i\n", vmtr->allocX,
                    vmtr->allocY, vmtr->allocWidth, vmtr->allocHeight);
      const int pagesWide = virtualTexture.header.pagesWide;
      idLib::Printf(
          "pages: %i,%i to %i,%i\n",
          vmtr->allocX * pagesWide / virtualTexture.VIRTUAL_TEXELS_WIDE,
          vmtr->allocY * pagesWide / virtualTexture.VIRTUAL_TEXELS_WIDE,
          pagesWide * (vmtr->allocX + vmtr->allocWidth) /
              virtualTexture.VIRTUAL_TEXELS_WIDE,
          pagesWide * (vmtr->allocY + vmtr->allocHeight) /
              virtualTexture.VIRTUAL_TEXELS_WIDE);
      idLib::Printf("map: %f %f %f %f\n", vmtr->scaleAndBias.x,
                    vmtr->scaleAndBias.y, vmtr->scaleAndBias.z,
                    vmtr->scaleAndBias.w);
      idLib::Printf(vmtr->material != nullptr ? "loaded\n" : "not loaded\n");
      return;
    }
  }

  idLib::Printf("'%s' not found\n", args.argc <= 1 ? "" : args.argv[1]);
}

bool idVirtualImageFile::NeedsReload() {
  idFileProps fileProps{};
  fileProps.timestamp = static_cast<unsigned int>(-1);
  fileProps.writable = false;
  R_LoadImageProgram(fileName.str, nullptr, nullptr, nullptr, &fileProps);
  return (timeStamp == static_cast<unsigned int>(-1) &&
          fileProps.timestamp != static_cast<unsigned int>(-1)) ||
         fileProps.timestamp > timeStamp;
}

idVirtualMaterial* idVMTRData::VirtualMaterialForMaterial(
    const idMaterial* mtr) {
  if (mtr == nullptr) {
    return nullptr;
  }
  if (mtr->devVmtr != nullptr) {
    return mtr->devVmtr;
  }

  const idDeclRenderParm* const bumpMap = FindRenderParm("bumpMap");
  const idDeclRenderParm* const diffuseMap = FindRenderParm("diffuseMap");
  const idDeclRenderParm* const virtualTransMap =
      FindRenderParm("virtualTransMap");
  if (!mtr->parmBlock.SetsRenderParm(bumpMap) &&
      !mtr->parmBlock.SetsRenderParm(diffuseMap) &&
      !mtr->parmBlock.SetsRenderParm(virtualTransMap)) {
    return nullptr;
  }

  const int hashKey = idStr::Hash(mtr->name.str);
  for (int index = vmtrHash.First(hashKey); index != -1;
       index = vmtrHash.Next(index)) {
    idVirtualMaterial* const vmtr = vmtrs.list[index];
    const char* const materialName = mtr->name.str;
    if ((materialName == nullptr &&
         vmtr->materialName.str == idAtomicString::nullAtomicString) ||
        (materialName != nullptr &&
         (vmtr->materialName.str == materialName ||
          idStr::Cmp(vmtr->materialName.str, materialName) == 0))) {
      vmtr->material = mtr;
      return vmtr;
    }
  }

  idLib::Warning(
      "Cannot create new vmtr for %s on the console, operation unsupported",
      mtr->name.str);
  return nullptr;
}

idVirtualMaterial* idVirtualTextureSystem::VirtualMaterialForMaterial(
    const idMaterial* mtr) {
  idVMTRData* const vmtrData = vts[mtr->pageSource].vmtrData;
  return vmtrData != nullptr ? vmtrData->VirtualMaterialForMaterial(mtr)
                             : nullptr;
}

void idVMTRData::ReadVmtrFile() {
  vmtrs.ClearFree();
  vmtrHash.Clear();

  idLexer source(0);
  if (!source.LoadFile(vmtrHeaderPath.data, false)) {
    idLib::Printf("Bad version on read %s, all vmtrs will be re-created\n",
                  vmtrHeaderPath.data);
    return;
  }

  source.SetFlags(0x66238);
  if (source.ParseInt() != VMTR_FILE_VERSION) {
    idLib::Printf("Bad version on read %s, all vmtrs will be re-created\n",
                  vmtrHeaderPath.data);
    return;
  }

  const int numVmtrs = source.ParseInt();
  idToken token;
  int hashIndex = 0;
  for (int index = 0; index < numVmtrs; ++index) {
    if (!source.ReadToken(token)) {
      idLib::Warning("Failed to read all the vmtrs.");
      break;
    }
    source.UnreadToken();

    idVirtualMaterial* const vmtr = AllocateVirtualMaterial();
    vmtr->allocX = source.ParseInt();
    vmtr->allocY = source.ParseInt();
    vmtr->allocWidth = source.ParseInt();
    vmtr->allocHeight = source.ParseInt();
    vmtr->flags = source.ParseInt();
    vmtr->cpuVmtrTimeStamp = source.ParseInt();
    vmtr->materialChecksum = source.ParseInt();
    source.ReadToken(token);
    vmtr->materialName.Set(token.data);
    SetupVirtualMaterial(vmtr);
    vmtrs.Append(vmtr);
    vmtrHash.Add(idStr::Hash(vmtrs.list[index]->materialName.str),
                 hashIndex++);
  }

  idLib::Printf("Read %i vmtrs from %s\n", vmtrs.num, vmtrHeaderPath.data);
}
