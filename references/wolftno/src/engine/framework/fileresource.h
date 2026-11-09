#pragma once

// NEEDS_WORK(engine/framework): declarations recovered for renderer clients.
// The implementation belongs to the framework folder and is intentionally
// deferred until that folder is reconstructed.

#include "engine/decls/decl.h"

#include <cstddef>
#include <cstdint>

class idFile_Memory;

class idFileResource : public idResource {
public:
  idFileResource() = default;
  ~idFileResource() override;

  void FreeData();
  static bool FileExists(const char* name);
  idResourceList* GetResourceList() const override;
  void LoadResource() override;
  idFile_Memory* GetFileReadOnly();
  idFile_Memory* GetFile() const;
  int GetLength() const { return static_cast<int>(length); }
  void* GetData() const { return data; }
  bool ReloadIfStale() override;

  static idTypedResourceList<idFileResource> resourceList;

  std::uint32_t timestamp = static_cast<std::uint32_t>(-1);
  std::uint64_t length = 0;
  void* data = nullptr;
};

template <>
idFileResource*
idTypedResourceList<idFileResource>::Alloc(const char* resourceName) const;

static_assert(offsetof(idFileResource, length) == 0x38,
              "file-resource length offset drift");
static_assert(offsetof(idFileResource, data) == 0x40,
              "file-resource data offset drift");
static_assert(sizeof(idFileResource) == 0x48,
              "file-resource ABI drift");
