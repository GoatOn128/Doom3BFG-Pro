#include "engine/decls/decl.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/hashing/hashing.h"
#include "shared/idlib/lib.h"

#include <cstdarg>
#include <cstdint>
#include <cstring>

idCVar resource_showLocalized("resource_showLocalized", "0", CVAR_BOOL,
                              "1 = show all localized resource names");
extern idCVar sys_lang;

void ResourceLoadTrackerBeginLoad(idResource* resource);
void ResourceLoadTrackerEndLoad(idResource* resource);

char idResource::resourceErrorMessage[64]{};

namespace {

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value >> 24) | ((value >> 8) & 0x0000FF00u) |
         ((value << 8) & 0x00FF0000u) | (value << 24);
}

std::uint64_t ByteSwap64(std::uint64_t value) {
  value = ((value & 0x00FF00FF00FF00FFull) << 8) |
          ((value >> 8) & 0x00FF00FF00FF00FFull);
  value = ((value & 0x0000FFFF0000FFFFull) << 16) |
          ((value >> 16) & 0x0000FFFF0000FFFFull);
  return (value << 32) | (value >> 32);
}

template <typename T> std::size_t WriteBig(idFile* file, T value) {
  if constexpr (sizeof(T) == 2)
    value = static_cast<T>(ByteSwap16(static_cast<std::uint16_t>(value)));
  else if constexpr (sizeof(T) == 4)
    value = static_cast<T>(ByteSwap32(static_cast<std::uint32_t>(value)));
  else if constexpr (sizeof(T) == 8)
    value = static_cast<T>(ByteSwap64(static_cast<std::uint64_t>(value)));
  return file->Write(&value, sizeof(value));
}

template <typename T> void ReadBig(idFile* file, T& value) {
  file->Read(&value, sizeof(value));
  if constexpr (sizeof(T) == 2)
    value = static_cast<T>(ByteSwap16(static_cast<std::uint16_t>(value)));
  else if constexpr (sizeof(T) == 4)
    value = static_cast<T>(ByteSwap32(static_cast<std::uint32_t>(value)));
  else if constexpr (sizeof(T) == 8)
    value = static_cast<T>(ByteSwap64(static_cast<std::uint64_t>(value)));
}

} // namespace

const char* idResource::GetResourceTypeName() const {
  return GetResourceList()->GetResourceTypeName();
}

void idResource::LoadResource() {
  idLib::FatalError("idResource::Load() for %s not implemented.\n",
                    GetResourceTypeName());
}

bool idResource::ReloadIfStale() { return false; }

void idResource::WriteResourceFile() const {
  idLib::FatalError("resource type '%s' has not implemented WriteResourceFile()",
                    GetResourceTypeName());
}

idResource::idResource()
    : name("unnamed"), resourceListPtr(nullptr), resourceError(nullptr),
      resourceID(0), staleCount(0), loadedResource(0), staticResource(0),
      insideRecursiveLoad(0), buildForceLoad(0), loadDeferred(0) {}

idResource::~idResource() noexcept(false) {
  if (staticResource)
    idLib::FatalError("Resource deleted with staticResource set: %s", name.str);
  if (resourceID != RESOURCE_ID_INVALID)
    resourceListPtr->Remove(this);
}

const char* idResource::GetName() const { return name.str; }

void idResource::SetName(const char* resourceName) {
  idStr canonical(resourceName);
  canonical.MakeNameCanonical();
  if (canonical.Cmp(name.str) != 0)
    name.Set(canonical.data);
}

void idResource::SetResourceError(const char* format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  if (format != nullptr) {
    char message[1024];
    idStr::vsnPrintf(message, sizeof(message), format, arguments);
    message[sizeof(message) - 1] = '\0';
    idAtomicString atomicMessage;
    atomicMessage.Set(message);
    resourceError = atomicMessage.str;
    idLib::Warning(resourceError);
  } else {
    resourceError = nullptr;
  }
  va_end(arguments);
}

void idResource::Print() const {
  if (resourceError != nullptr)
    idLib::Printf("%s:%s:%s\n", GetResourceTypeName(), name.str,
                  resourceError);
  else
    idLib::Printf("%s:%s\n", GetResourceTypeName(), name.str);
}

void idResource::List() const {}

void idResource::Load() {
  GetResourceTypeName();
  ResourceLoadTrackerBeginLoad(this);
  LoadResource();
  ResourceLoadTrackerEndLoad(this);
  GetResourceTypeName();
}

const char* idResource::GetLocalizedName(const char* sourceLanguage,
                                         const char* destLanguage,
                                         const char* inName, char* outName,
                                         const int sizeOfOutName) {
  const int location = idStr::Find(inName, sourceLanguage, true, 0, -1);
  if (location <= 0)
    return inName;
  if (inName[location - 1] != '/' && inName[location - 1] != '\\')
    return inName;
  if (inName[location + 7] != '/' && inName[location + 7] != '\\')
    return inName;
  const int suffixLength = idStr::Length(inName) - (location + 7);
  const int destinationLength = idStr::Length(destLanguage);
  if (location + destinationLength + suffixLength >= sizeOfOutName)
    return inName;
  idStr::CopyCountAndZero(outName, sizeOfOutName, inName, location);
  idStr::CopyCountAndZero(outName + location, sizeOfOutName - location,
                         destLanguage, destinationLength);
  idStr::CopyCountAndZero(outName + location + destinationLength,
                         sizeOfOutName - destinationLength - location,
                         inName + location + 7, suffixLength);
  if (resource_showLocalized.valueInteger != 0)
    idLib::Printf("Localized: '%s'\n", outName);
  return outName;
}

const char* idResource::GetLocalizedName(const char* inName, char* outName,
                                         const int sizeOfOutName) {
  return GetLocalizedName("english", sys_lang.GetString(), inName, outName,
                          sizeOfOutName);
}

idResource::resourceError_t idResource::ReadBinaryHeader(
    idFile* file, const unsigned int expectedResourceId,
    const unsigned short expectedVersionHi,
    const unsigned short expectedVersionLo, resourceHeader_t& header,
    char* const sourceFileName, char* const uniqueIdName) {
  ReadBig(file, header.headerVersionHi);
  if (header.headerVersionHi != 1)
    return RESOURCE_ERROR_HEADER_VERSION_MISMATCH;
  ReadBig(file, header.headerVersionLo);
  if (header.headerVersionLo != 1)
    return RESOURCE_ERROR_HEADER_VERSION_MISMATCH;
  ReadBig(file, header.resourceId);
  if (header.resourceId != expectedResourceId)
    return RESOURCE_ERROR_WRONG_TYPE;
  ReadBig(file, header.versionHi);
  if (header.versionHi != expectedVersionHi)
    return RESOURCE_ERROR_RESOURCE_VERSION_MISMATCH;
  ReadBig(file, header.versionLo);
  if (header.versionLo != expectedVersionLo)
    return RESOURCE_ERROR_RESOURCE_VERSION_MISMATCH;
  ReadBig(file, header.sourceTimestamp);
  ReadBig(file, header.sourceFileNameLen);
  if (header.sourceFileNameLen >= 256)
    return RESOURCE_ERROR_INVALID_NAME;
  ReadBig(file, header.uniqueIdNameLen);
  if (header.uniqueIdNameLen >= 256)
    return RESOURCE_ERROR_INVALID_NAME;
  file->Read(&header.platform, 1);
  file->Read(header.pad, 3);
  ReadBig(file, header.uniqueId);
  ReadBig(file, header.hash);
  ReadBig(file, header.dataOffset);
  ReadBig(file, header.totalSize);
  file->Read(sourceFileName, header.sourceFileNameLen);
  sourceFileName[header.sourceFileNameLen] = '\0';
  file->Read(uniqueIdName, header.uniqueIdNameLen);
  uniqueIdName[header.uniqueIdNameLen] = '\0';
  unsigned char alignment[16];
  file->Read(alignment,
             static_cast<unsigned int>(header.dataOffset) -
                 header.uniqueIdNameLen - header.sourceFileNameLen - 56u);
  return RESOURCE_ERROR_NONE;
}

bool idResource::WriteBinaryHeader(idFile* file,
                                   const resourceHeader_t& header,
                                   const void* data,
                                   const std::uint64_t dataLength,
                                   const char* uniqueIdName,
                                   const char* sourceFileName) {
  header.sourceFileNameLen =
      static_cast<std::uint16_t>(idStr::Length(sourceFileName));
  header.uniqueIdNameLen =
      static_cast<std::uint16_t>(idStr::Length(uniqueIdName));
  const std::uint32_t unaligned =
      header.uniqueIdNameLen + header.sourceFileNameLen + 56u;
  header.dataOffset = (unaligned + 15u) & 0xFFFFFFF0u;
  const int alignment = static_cast<int>(header.dataOffset) - unaligned;

  std::uint64_t bytes = 0;
  bytes += WriteBig(file, header.headerVersionHi);
  bytes += WriteBig(file, header.headerVersionLo);
  bytes += WriteBig(file, header.resourceId);
  bytes += WriteBig(file, header.versionHi);
  bytes += WriteBig(file, header.versionLo);
  bytes += WriteBig(file, header.sourceTimestamp);
  bytes += WriteBig(file, header.sourceFileNameLen);
  bytes += WriteBig(file, header.uniqueIdNameLen);
  header.platform = 0;
  bytes += file->Write(&header.platform, 1);
  for (int index = 0; index < 3; ++index)
    bytes += file->Write(&header.pad[index], 1);
  bytes += WriteBig(file, header.uniqueId);
  header.hash = MurMur64_HashData(
      reinterpret_cast<int*>(const_cast<void*>(data)),
      static_cast<std::uint32_t>(dataLength), 0);
  bytes += WriteBig(file, header.hash);
  bytes += WriteBig(file, header.dataOffset);
  header.totalSize = static_cast<std::uint32_t>(dataLength + header.dataOffset);
  bytes += WriteBig(file, header.totalSize);
  if (bytes != 56)
    return false;

  bytes += file->Write(sourceFileName, header.sourceFileNameLen);
  bytes += file->Write(uniqueIdName, header.uniqueIdNameLen);
  static const unsigned char zeroBytes[16]{};
  for (int index = 0; index < alignment; ++index)
    bytes += file->Write(&zeroBytes[index], 1);
  return bytes == header.dataOffset;
}

bool idResource::WriteBinary(const char* uniqueIdName,
                             const char* sourceFileName,
                             const char* fileExtension,
                             const resourceHeader_t& header, const void* data,
                             const std::uint64_t dataLength) {
  char binaryFileName[256];
  fileSystem->FixLongFilename("generated", fileExtension, sourceFileName,
                              binaryFileName, sizeof(binaryFileName), -2);
  idFile_Memory file;
  WriteBinaryHeader(&file, header, data, dataLength, uniqueIdName,
                    sourceFileName);
  file.Write(data, static_cast<std::size_t>(dataLength));
  return file.AtomicWrite(binaryFileName, FSPATH_BASE);
}
