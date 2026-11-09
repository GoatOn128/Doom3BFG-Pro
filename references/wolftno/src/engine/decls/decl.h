#pragma once

#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/array.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/text/parser.h"
#include "shared/idlib/text/str.h"
#include "shared/idlib/typeinfo/typeinfo.h"

#include <cstdint>
#include <cstddef>

enum production_t {
  PROD_DEVELOPMENT = 0,
  PROD_BUILDING = 1,
  PROD_PRODUCTION = 2,
  PROD_LOADED = 3,
};

class idResource;
class idDecl;
class idDeclFile;
class idDeclInfo;
class idBitMsg;
class idFile;
class idSysMutex;
struct idDeclSource;
class idAutoComplete;
class idResourceManagerLocal;

class idResourceList {
public:
  explicit idResourceList(const char* typeName = "resource");
  virtual ~idResourceList() = default;
  virtual const idResource* GetResourceByID(int resourceID);
  virtual idResource* Alloc(const char* name) const = 0;
  virtual int AllocSize() const = 0;
  idResource* Load(const char* name, bool makeDefault = true,
                   bool skipStaleCheck = false);
  idResource* FindExisting(const char* name,
                           bool skipStaleCheck = false) const;
  void Add(idResource* resource);
  void Remove(idResource* resource);
  int Num() const { return resourceList.Num(); }
  idResource* Index(int index) const;
  void GetLoadedResources(idList<idResource*, 99>& resources) const;
  bool ReloadStaleResources() const;
  const char* GetResourceTypeName() const;
  idResourceList* GetNextList() const { return nextResourceList; }
  void ClearStaleCheckCountIfExisting(const char* name);
  int GetResourceIndexForID(int resourceID, const char* name) const;
  const idResource* GetResourceByIDInternal(int resourceID) const;
  const idResource* ReadResourceFromMsg(idBitMsg& message);
  void WriteResourceToMsg(const idResource* resource, idBitMsg& message);

  static void MarkAllStaticResources();
  static void FreeAllDynamicResources();
  static void ForceAllResourcesToReload() { ++staleCount; }
  static void BuildGameForceAllResourcesToReload();
  static idResourceList* ForTypeName(const char* typeName);
  static int GetLoadResourceRecursionLevel() {
    return loadResourceRecursionLevel;
  }
  static idResource* GetRecursiveLoads(const int index) {
    return recursiveLoads.ptr[index];
  }
  static idResourceList* GetNextResourceList();
  static bool ReloadResourceIfStale(idResource* resource);
  static void AllowStaleChecks() { ++staleCount; }
  static bool CanPerformStaleCheck(const idResource* resource);
  int GenerateResourceIDForName(const char* canonicalName) const;

  const char* resourceTypeName;
  idResourceList* nextResourceList;
  idList<idResource*> resourceList;
  idHashIndex resourceIDHash;

protected:
  static bool GetInGameLoading() { return inGameLoading; }

private:
  friend class idBaseCodeResource;
  friend class idResourceManagerLocal;
  void Add_Locked(idResource* resource);
  void Remove_Locked(idResource* resource);
  idResource* Lookup(const char* name) const;
  static idSysMutex* Mutex();
  static void SetInGameLoading(bool value) { inGameLoading = value; }

  static constexpr int HASH_SIZE = 256;
  static int loadResourceRecursionLevel;
  static idArray<idResource*, 32> recursiveLoads;
  static int staleCount;
  static bool inGameLoading;
};

class idResource {
public:
  enum resourceError_t {
    RESOURCE_ERROR_NONE = 0,
    RESOURCE_ERROR_FILE_NOT_FOUND = 1,
    RESOURCE_ERROR_WRONG_TYPE = 2,
    RESOURCE_ERROR_INVALID_NAME = 3,
    RESOURCE_ERROR_HEADER_VERSION_MISMATCH = 4,
    RESOURCE_ERROR_RESOURCE_VERSION_MISMATCH = 5,
    RESOURCE_ERROR_TRUNCATED = 6,
    RESOURCE_ERROR_FATAL = 7,
    RESOURCE_ERROR_MAX = 8
  };

  struct resourceHeader_t {
    static constexpr unsigned int RESOURCEID_UNKNOWN = 0;
    static constexpr unsigned short HEADER_VERSION_HI = 1;
    static constexpr unsigned short HEADER_VERSION_LO = 1;

    resourceHeader_t()
        : headerVersionHi(1), headerVersionLo(1), resourceId(0), versionHi(0),
          versionLo(0), sourceTimestamp(0), sourceFileNameLen(0),
          uniqueIdNameLen(0), platform(0), pad{}, uniqueId(0), hash(0),
          dataOffset(0), totalSize(0) {}
    resourceHeader_t(unsigned int id, unsigned short hi, unsigned short lo,
                     unsigned int timestamp, std::int64_t unique)
        : headerVersionHi(1), headerVersionLo(1), resourceId(id), versionHi(hi),
          versionLo(lo), sourceTimestamp(timestamp), sourceFileNameLen(0),
          uniqueIdNameLen(0), platform(0), pad{},
          uniqueId(static_cast<std::uint64_t>(unique)), hash(0), dataOffset(0),
          totalSize(0) {}

    std::uint16_t headerVersionHi;
    std::uint16_t headerVersionLo;
    std::uint32_t resourceId;
    std::uint16_t versionHi;
    std::uint16_t versionLo;
    std::uint32_t sourceTimestamp;
    mutable std::uint16_t sourceFileNameLen;
    mutable std::uint16_t uniqueIdNameLen;
    mutable std::uint8_t platform;
    std::uint8_t pad[3];
    std::uint64_t uniqueId;
    mutable std::uint64_t hash;
    mutable std::uint64_t dataOffset;
    mutable std::uint64_t totalSize;
  };

  static constexpr int RESOURCE_ID_INVALID = 0;
  static char resourceErrorMessage[64];

  idResource();
  virtual ~idResource() noexcept(false);
  virtual void LoadResource();
  virtual bool ReloadIfStale();
  virtual void WriteResourceFile();
  virtual idResourceList* GetResourceList() const = 0;
  virtual void Print();
  virtual void List() const;

  const idAtomicString& GetAtomicName() const { return name; }
  void SetName(const char* resourceName);
  const char* GetName() const;
  const char* GetResourceTypeName() const;
  void Load();
  const char* GetResourceError() const { return resourceError; }
  bool GetLoadedResource() const { return loadedResource; }
  void SetLoadedResource(bool value) { loadedResource = value; }
  bool GetLoadDeferred() const { return loadDeferred; }
  void SetLoadDeferred(bool value) { loadDeferred = value; }
  bool ResourceFullyDefined() const { return loadedResource && resourceError == nullptr; }
  int GetResourceID() const { return resourceID; }
  bool GetStaticResource() const { return staticResource; }
  void SetStaticResource() { staticResource = true; }
  void SetStaleCount(const int value) { staleCount = value; }
  int GetStaleCount() const { return staleCount; }
protected:
  void SetResourceError(const char* format, ...);
public:
  static const char* GetLocalizedName(const char* inName, char* outName,
                                      const int sizeOfOutName);
  static const char* GetLocalizedName(const char* sourceLanguage,
                                      const char* destLanguage,
                                      const char* inName, char* outName,
                                      const int sizeOfOutName);
  static resourceError_t ReadBinaryHeader(
      idFile* file, const unsigned int expectedResourceId,
      const unsigned short expectedVersionHi,
      const unsigned short expectedVersionLo, resourceHeader_t& header,
      char* const sourceFileName, char* const uniqueIdName);
  static bool WriteBinaryHeader(idFile* file, const resourceHeader_t& header,
                                const void* data, const std::uint64_t dataLength,
                                const char* uniqueIdName,
                                const char* sourceFileName);
  static bool WriteBinary(const char* uniqueIdName, const char* sourceFileName,
                          const char* fileExtension,
                          const resourceHeader_t& header, const void* data,
                          const std::uint64_t dataLength);

  idAtomicString name;
  idResourceList* resourceListPtr;
  const char* resourceError;
  int resourceID;
  mutable int staleCount;
  std::uint8_t loadedResource : 1;
  std::uint8_t staticResource : 1;
  std::uint8_t insideRecursiveLoad : 1;
  std::uint8_t buildForceLoad : 1;
  std::uint8_t loadDeferred : 1;
};

template <typename T> class idTypedResourceList : public idResourceList {
public:
  explicit idTypedResourceList(const char* typeName = "resource")
      : idResourceList(typeName) {}
  T* Alloc(const char* resourceName) const override {
    T* resource = new T;
    resource->resourceListPtr = const_cast<idTypedResourceList<T>*>(this);
    resource->SetName(resourceName);
    return resource;
  }
  int AllocSize() const override { return static_cast<int>(sizeof(T)); }
};

static_assert(sizeof(idResource::resourceHeader_t) == 0x38,
              "resource header ABI drift");
static_assert(sizeof(idResource) == 0x30, "idResource ABI drift");
static_assert(offsetof(idResource, name) == 0x08,
              "idResource name offset drift");
static_assert(sizeof(idResourceList) == 0x58, "idResourceList ABI drift");

struct idDeclSource {
  idAtomicString name;
  idDeclFile* sourceFile = nullptr;
  int sourceLine = 0;
  idDeclSource* nextInFile = nullptr;
  bool isStatic = false;
  int sourceTextOffset = 0;
  int sourceTextLength = 0;
  const char* GetFileName() const;
};

class idDeclFile {
public:
  idDeclFile();
  ~idDeclFile();
  bool SourceFileChanged(bool force) const;
  std::uint32_t LoadAndParse();
  void FreeDynamic();
  idDecl* AddDecl(idDeclInfo* type, const char* declName, int line, int textOffset,
                  int textLength, bool parseDecl = false);

  bool hadRedefinition = false;
  idAtomicString fileName;
  idDeclInfo* defaultType = nullptr;
  std::uint32_t timestamp = 0;
  std::uint32_t checksum = 0;
  int fileSize = 0;
  int numLines = 0;
  bool everReloaded = false;
  idDeclSource* firstDeclSourceInFile = nullptr;
  idList<int, 46> dependencies;
  int dependenciesDynamicStartIndex = 0;
  idList<idStr> includeFiles;
};

class idDeclInfo : public idResourceList {
public:
  idDeclInfo(const char* declName = "decl", const char* className = "idDecl",
             const char* extension = "decl", bool remapping = false);
  ~idDeclInfo() override;
  virtual idDecl* AllocDecl();
  virtual const idResource* GetResourceByID(int resourceID);
  idDeclSource* FindDeclSource(const char* declName) const;
  idDecl* FindResource(const char* declName, bool load = true) const;
  const char* RemapResourceName(char* buffer, int bufferSize, const char* resourceName) const;
  const idDecl* FindExisting(const char* resourceName, bool remap = true) const;
  idDecl* FindWithInheritance(const char* resourceName, bool makeDefault = true,
                              bool remap = true) const;
  const idResource* GetResourceByIDWithInheritance(int resourceID) const;
  void RemoveDeclSource(idDeclSource* source);
  void AddDeclSource(idDeclSource* source);
  void FreeDynamic();

  const char* name = "decl";
  const char* classname = "idDecl";
  const char* fileExtension = ".decl";
  bool usesRemapping = false;
  idAtomicString nameExtension;
  idAtomicString defaultFolder;
  idDeclInfo* parent = nullptr;
  void (*onReload)(const idDecl*) = nullptr;
  idList<idDeclSource*, 46> declSources;
  idHashIndex declSourceHash;
  int declSourceDynamicStartIndex = 0;
};

class idDeclManager {
public:
  virtual ~idDeclManager() = default;
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual void Reload(bool force) = 0;
  virtual void MergeDecls(const idStr& mapName, const idStr& mapFile) = 0;
  virtual void MarkStatic() = 0;
  virtual void FreeDynamic() = 0;
  virtual void RegisterDeclFolder(const char* folder, const char* extension,
                                  idDeclInfo* defaultType) = 0;
  virtual int GetNumDeclTypes() const = 0;
  virtual idDeclInfo* GetDeclType(const char* typeName) const = 0;
  virtual idDeclInfo* GetDeclType(int index) const = 0;
  virtual idDeclInfo* GetDeclTypeFromClassname(const char* className) const = 0;
  virtual idDecl* CreateNewDecl(idDeclInfo* type, const char* name, const char* fileName,
                                const char* sourceText) = 0;
  virtual void AddDependency(idDecl& decl, const char* fileName) = 0;
  virtual void AddDependency(idDecl& decl, const idDecl* dependency);
  virtual void GetDeclFilesDependentOnDeclFile(const idDeclFile& source,
                                                idList<idDeclFile*>& result) const = 0;
  virtual void ArgCompletion_DeclTypeName(idAutoComplete* completion) = 0;
  virtual void ArgCompletion_DeclName(idAutoComplete* completion, idDeclInfo* type) = 0;
  virtual void PrintMultiplayerDeclOverrides() = 0;
  void GetDeclTextHeader(const idDeclInfo& info, const char* name, idStr& header) const;
  virtual bool ShouldLogMissingDecl(const idDecl& decl) const;
  static void ArgCompletion_DeclTypeAndName(idAutoComplete* completion);
  template <typename T>
  static void ArgCompletion_Decl(idAutoComplete& completion);
};

enum scFileStatus_t {
  SCF_NOTMANAGED = 0,
  SCF_NOTIMPORTED = 1,
  SCF_CHECKEDOUT = 2,
  SCF_CHECKEDOUT_PLUS = 3,
  SCF_CHECKEDOUT_BYOTHER = 4,
  SCF_CHECKEDOUT_BYOTHER_EXCLUSIVE = 5,
  SCF_CHECKEDIN = 6
};
enum scFileType_t {
  SCT_TEXT = 0,
  SCT_BINARY = 1,
  SCT_SYMLINK = 2,
  SCT_APPLE = 3,
  SCT_RESOURCE = 4,
  SCT_UNICODE = 5,
  SCT_UTF16 = 6,
  SCT_NONE = 7
};

class idSourceControl {
public:
  class idSourceControlWorkspace {
  public:
    idStr name;
    idStr lastAccessTime;
    idStr owner;
    idStr host;
  };

  virtual ~idSourceControl() = default;
  virtual bool Init() = 0;
  virtual void Shutdown() = 0;
  virtual void SetSilentCheckOut(bool set) = 0;
  virtual bool GetSilentCheckOut() = 0;
  virtual void SetSilentCheckIn(bool set) = 0;
  virtual bool GetSilentCheckIn() = 0;
  virtual bool IsConnected() const = 0;
  virtual bool IsInitialized() const = 0;
  virtual int CheckOut(const idList<idStr>& files) = 0;
  virtual int UndoCheckOut(const idList<idStr>& files) = 0;
  virtual int Import(const idList<idStr>& files, bool keepCheckedOut,
                     bool submit, scFileType_t fileType) = 0;
  virtual int GetLatest(const idList<idStr>& files, bool force) = 0;
  virtual int CheckIn(const idList<idStr>& files, const idStr& comment,
                      bool keepCheckedOut) = 0;
  virtual int Delete(const idList<idStr>& files) = 0;
  virtual int UpdateFolder(const char* path) = 0;
  virtual scFileStatus_t GetFileStatus(const idStr& file) = 0;
  virtual bool GetFileVersion(const idStr& file, int& localVersion,
                              int& serverVersion) = 0;
  virtual int GetNumCheckOutUsers(const idStr& file) = 0;
  virtual void GetCheckOutUser(const idStr& file, int userIndex,
                               char* userName, const int maxLength) = 0;
  virtual void GetUsernameForFileVersion(const idStr& file, int fileVersion,
                                         idStr& username) = 0;
  virtual void GetCurrentWorkspace(idSourceControlWorkspace&) {}
  virtual void GetAvailableWorkspaces(
      idList<idSourceControlWorkspace>&) {}
  virtual void SetWorkspace(idSourceControlWorkspace&) {}
  virtual void SetWorkspace(const char*) {}
  virtual bool IsWorkspaceValid() { return false; }
  virtual idStr GetUsername() = 0;

  int CheckOut(const idStr& file);
  int UndoCheckOut(const idStr& file);
  int Import(const idStr& file, bool keepCheckedOut, bool submit,
             scFileType_t fileType);
  int GetLatest(const idStr& file, bool force);
  int CheckIn(const idStr& file, const idStr& comment, bool keepCheckedOut);
  int Delete(const idStr& file);
};

static_assert(sizeof(idSourceControl::idSourceControlWorkspace) == 0xA0,
              "source-control workspace ABI drift");
static_assert(sizeof(idSourceControl) == 8,
              "source-control interface ABI drift");

class idDecl : public idResource {
public:
  idDecl();
  ~idDecl() noexcept(false) override;

  idResourceList* GetResourceList() const override { return GetDeclInfo(); }
  void LoadResource() override;
  bool ReloadIfStale() override;
  void WriteResourceFile() override;
  void List() const override;
  virtual std::uint32_t GetDeclTimestamp();
  virtual void ReParse(bool fromDisk);
  virtual idDeclInfo* GetDeclInfo() const = 0;
  virtual bool RebuildTextSource();
  virtual bool SetImplicitText();
  virtual const char* DefaultDefinition() const;
  virtual void LogMissingDecl();
  virtual void Parse(idParser& source) = 0;
  virtual void FreeData();
  virtual std::uint64_t Size() const;
  virtual bool IsInventoryDecl() const;

  int GetLineNum() const;
  int GetTextLength() const;
  bool SourceFileExists() const;
  bool EverReloaded() const;
  idDeclSource* DeclSource() const;
  bool IsImplicit() const;
  const char* GetFileName() const;
  void SetText(const char* text, int length = 0);
  void MakeDefault();
  void Revert(bool force = false);
  void DefineFromSource(idDeclSource* source);
  void DeleteUnusedText();
  char* GetText(bool force = false);
  bool ReplaceSourceFileText(bool useSourceControl);

  char* textSource;
  int textLength;
  std::uint32_t md5Checksum;
  bool programaticallyDefined;
  idDeclSource* declSource;
};

template <typename T> class idDeclInfoTemplate : public idDeclInfo {
public:
  using idDeclInfo::idDeclInfo;
  T* Alloc(const char* resourceName) const override {
    T* decl = new T;
    decl->resourceListPtr = const_cast<idDeclInfoTemplate<T>*>(this);
    decl->SetName(resourceName);
    return decl;
  }
  int AllocSize() const override { return static_cast<int>(sizeof(T)); }
  idDecl* AllocDecl() override {
    T* decl = new T;
    decl->resourceListPtr = this;
    return decl;
  }
};

class idDeclTypeInfo : public idDecl {
public:
  idDeclTypeInfo();
  ~idDeclTypeInfo() override;
  const char* DefaultDefinition() const override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;
  bool RebuildTextSource() override;
  void SetState(idDeclTypeInfo* empty);

  idDeclTypeInfo* parent;
  bool hasBeenParsedAtLeastOnce;

  static idDeclInfoTemplate<idDeclTypeInfo> resourceList;
};

idStr ProductionFilename(const idDecl* resource);

extern idDeclManager* declManager;
extern void (*resourceLoadCallback)(idResource* resource);
extern idTypeInfoTools* declTypeInfoTools;

static_assert(sizeof(idResource) == 48, "idResource ABI drift");
static_assert(sizeof(idDecl) == 80, "idDecl ABI drift");
static_assert(sizeof(idDeclSource) == 48, "idDeclSource ABI drift");
