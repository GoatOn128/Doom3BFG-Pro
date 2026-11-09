#include "engine/framework/fileresource.h"

#include "engine/framework/resourcemanager.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <new>

idTypedResourceList<idFileResource> idFileResource::resourceList("file");

namespace {

void* AllocFileResourceMemory(std::size_t size, const char* location) {
  return idMem::Singleton()->AllocWithLocation(location, size,
                                                TAG_FILE_RESOURCE, false,
                                                ALIGN_16, HEAP_DEFAULTHEAP);
}

idFile_Memory* AllocMemoryFile(const char* name) {
  void* storage = AllocFileResourceMemory(
      sizeof(idFile_Memory),
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag");
  return storage != nullptr ? new (storage) idFile_Memory(name) : nullptr;
}

} // namespace

idFileResource::~idFileResource() { FreeData(); }

void idFileResource::FreeData() {
  if (data == nullptr)
    return;
  idMem::Singleton()->Free(data, ALIGN_16);
  timestamp = static_cast<std::uint32_t>(-1);
  data = nullptr;
  length = 0;
}

bool idFileResource::FileExists(const char* fileName) {
  idFile* file = idLib::production == PROD_LOADED
                     ? static_cast<idFile*>(resourceManager->LoadCacheFile(fileName))
                     : fileSystem->OpenFileRead(fileName, true, false);
  if (file == nullptr)
    return false;
  delete file;
  return true;
}

idResourceList* idFileResource::GetResourceList() const { return &resourceList; }

void idFileResource::LoadResource() {
  FreeData();

  idFile* file = idLib::production == PROD_LOADED
                     ? static_cast<idFile*>(resourceManager->LoadCacheFile(name.str))
                     : fileSystem->OpenFileRead(name.str, true, false);
  if (file == nullptr) {
    SetResourceError("FileResource '%s' File not found", name.str);
    return;
  }

  timestamp = file->Timestamp();
  length = static_cast<std::uint64_t>(file->Length());
  data = AllocFileResourceMemory(
      static_cast<std::size_t>(length),
      "w:\\build\\tech5\\engine\\framework\\FileResource.cpp(79) : "
      "TAG_FILE_RESOURCE");
  file->Read(data, static_cast<std::size_t>(length));
  delete file;
}

idFile_Memory* idFileResource::GetFileReadOnly() {
  idFile_Memory* memoryFile = nullptr;
  if (data == nullptr) {
    idFile* file = resourceManager->LoadCacheFile(name.str);
    if (file == nullptr)
      return nullptr;

    timestamp = file->Timestamp();
    length = static_cast<std::uint64_t>(file->Length());
    memoryFile = dynamic_cast<idFile_Memory*>(file);
    if (memoryFile == nullptr) {
      data = AllocFileResourceMemory(
          static_cast<std::size_t>(length),
          "w:\\build\\tech5\\engine\\framework\\FileResource.cpp(108) : "
          "TAG_FILE_RESOURCE");
      file->Read(data, static_cast<std::size_t>(length));
      delete file;
    }
  }

  if (data != nullptr) {
    memoryFile = AllocMemoryFile(name.str);
    memoryFile->SetReadOnlyData(static_cast<char*>(data), length);
  }
  if (memoryFile == nullptr)
    return nullptr;

  memoryFile->Seek(0, FS_SEEK_SET);
  data = nullptr;
  memoryFile->ownsData = true;
  return memoryFile;
}

idFile_Memory* idFileResource::GetFile() const {
  idFile_Memory* file = AllocMemoryFile(name.str);
  file->SetReadOnlyData(static_cast<char*>(data), length);
  return file;
}

bool idFileResource::ReloadIfStale() {
  const std::uint32_t fileTimestamp = fileSystem->GetTimestamp(name.str, true);
  return fileTimestamp == static_cast<std::uint32_t>(-1) ||
         fileTimestamp != timestamp;
}

template <>
idFileResource*
idTypedResourceList<idFileResource>::Alloc(const char* resourceName) const {
  void* storage = AllocFileResourceMemory(
      sizeof(idFileResource),
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag");
  idFileResource* resource =
      storage != nullptr ? new (storage) idFileResource : nullptr;
  resource->SetName(resourceName);
  return resource;
}
