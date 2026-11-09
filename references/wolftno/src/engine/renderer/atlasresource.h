#pragma once

// PDB-derived public boundary.  atlasresource.cpp owns the resource loading
// implementation when that renderer compiland is recovered.

#include "engine/decls/decl.h"
#include "shared/idlib/math/vector.h"

class idAtlasResource : public idResource {
public:
  idAtlasResource();
  ~idAtlasResource() override;
  void LoadResource() override;
  bool ReloadIfStale() override;
  idResourceList* GetResourceList() const override;
  void List() const override;

  unsigned binaryTimestamp;
  idVec4 scaleBias;
  int img_tiles_wide;
  int img_tiles_high;
  int img_x;
  int img_y;
  int img_w;
  int img_h;

  static idTypedResourceList<idAtlasResource> resourceList;
};

bool CheckForSpace(const class idBitArray& tileAlloc, int x, int y, int width,
                   int height);
bool AllocBlock(class idBitArray& tileAlloc, int* x, int* y, int width,
                int height);
void FreeBlock(class idBitArray& tileAlloc, int* x, int* y, int width,
               int height);

template <>
idAtlasResource*
idTypedResourceList<idAtlasResource>::Alloc(const char* name) const;

static_assert(offsetof(idAtlasResource, binaryTimestamp) == 0x30,
              "idAtlasResource timestamp offset drift");
static_assert(offsetof(idAtlasResource, scaleBias) == 0x34,
              "idAtlasResource scale/bias offset drift");
static_assert(sizeof(idAtlasResource) == 0x60, "idAtlasResource ABI drift");
