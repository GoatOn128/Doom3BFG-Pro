#pragma once

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/containers/heaparray.h"

#include <cstdint>
#include <memory>
#include <vector>

struct idFileList {
  idStr basePath;
  idList<idStr> list;
};

class idFileSystem {
public:
  virtual ~idFileSystem() = default;
  virtual void Init(const char* root) = 0;
  virtual void Shutdown() = 0;
  virtual void ClearZipCache() {}
  void SetFilePreCallbackRetail(void* callback) {
    using Fn=void(*)(idFileSystem*,void*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[4])(this,callback);
  }
  void SetFileCacheCallbackRetail(void* loadCallback,void* statCallback) {
    using Fn=void(*)(idFileSystem*,void*,void*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[6])(this,loadCallback,statCallback);
  }
  idFile* OpenOSFileReadRetail(const char* rootPath,const char* fileName) {
    using Fn=idFile*(*)(idFileSystem*,const char*,const char*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<Fn>(slots[32])(this,rootPath,fileName);
  }
  bool OSFileExistsRetail(const char* rootPath,const char* fileName,
                          bool caseSensitive) {
    using Fn=bool(*)(idFileSystem*,const char*,const char*,bool);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<Fn>(slots[33])(
        this,rootPath,fileName,caseSensitive);
  }
  virtual idFile* OpenFileRead(const char* path) = 0;
  virtual idFile* OpenFileWrite(const char* path) = 0;
  virtual idFile* OpenFileAppend(const char* path, fsPath_t basePath) = 0;
  virtual bool ReadFile(const char* path, std::vector<std::uint8_t>& data) = 0;
  virtual bool WriteFile(const char* path, const void* data, std::size_t size) = 0;
  virtual bool FileExists(const char* path) const = 0;
  virtual std::uint32_t GetTimestamp(const char* path) const = 0;
  virtual bool RemoveFile(const char* path) = 0;
  virtual idFileList* ListFiles(const char* directory, const char* extension,
                                bool recursive = false) const = 0;
  virtual void FreeFileList(idFileList* list) const { delete list; }
  // NEEDS_WORK(shared/idlib/filesystem): renderer-facing portions of the
  // retail interface; the full vtable will be recovered with filesystem.
  virtual void FixLongFilename(const char* basePath, const char* extension,
                               const char* input, char* output,
                               int outputLength) const {
    const char* selectedInput = input != nullptr ? input : "";
    if (idStr::Icmpn(selectedInput, "../../", 6) == 0)
      selectedInput += 6;

    idStr fixedName;
    if (basePath != nullptr && basePath[0] != '\0') {
      fixedName = basePath;
      fixedName.AppendPath(selectedInput);
    } else {
      fixedName = selectedInput;
    }
    fixedName.SetFileExtension(extension != nullptr ? extension : "");

    idStr leafName;
    fixedName.ExtractFileName(leafName);
    if (leafName.Length() >= 38) {
      idStr filePath;
      fixedName.ExtractFilePath(filePath);
      leafName.StripFileExtension();
      fixedName = filePath;
      int runLength = 0;
      for (int index = 0; index < leafName.Length(); ++index) {
        if (runLength >= 30) {
          fixedName += '/';
          runLength = 0;
        }
        fixedName += leafName[index];
        ++runLength;
      }
      fixedName.SetFileExtension(extension != nullptr ? extension : "");
    }
    idStr::Copynz(output, fixedName.data, outputLength);
  }
  void FixLongFilename(const char* basePath, const char* extension,
                       const char* input, char* output, int outputLength,
                       int) const {
    FixLongFilename(basePath, extension, input, output, outputLength);
  }
  std::uint32_t GetTimestamp(const char* path, bool) const {
    return GetTimestamp(path);
  }
  bool FileExists(const char* path, bool) const { return FileExists(path); }
  bool RemoveFile(const char* path, fsPath_t) { return RemoveFile(path); }
  idFile* OpenFileRead(const char* path, bool, bool) {
    return OpenFileRead(path);
  }
  // NEEDS_WORK(shared/idlib/filesystem): retail exposes this renderer-facing
  // read/write opener.  Keep the exact call boundary here until the filesystem
  // compiland supplies its path policy and permanent-file implementation.
  virtual idFile* OpenFileReadWrite(const char* path, bool, fsPath_t) {
    return OpenFileWrite(path);
  }
  idFile* OpenFileWrite(const char* path, fsPath_t) {
    return OpenFileWrite(path);
  }
  idFile* OpenFileWriteRetail(const char* path, fsPath_t basePath) {
    using Fn=idFile*(*)(idFileSystem*,const char*,fsPath_t);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<Fn>(slots[34])(this,path,basePath);
  }
  idFile* OpenFileReadRetail(const char* path,bool allowCopyFiles,
                             bool gamedirOnly) {
    using Fn=idFile*(*)(idFileSystem*,const char*,bool,bool);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<Fn>(slots[31])(
        this,path,allowCopyFiles,gamedirOnly);
  }
  idFileList* ListFilesTreeRetail(const char* directory,const char* extension,
                                  bool sort) {
    using Fn=idFileList*(*)(idFileSystem*,const char*,const char*,bool);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<Fn>(slots[9])(this,directory,extension,sort);
  }
  void FreeFileListRetail(idFileList* files) {
    using Fn=void(*)(idFileSystem*,idFileList*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[10])(this,files);
  }
  void GetNumberedFilenameRetail(int& lastNumber,const char* baseName,
                                 const char* extension,idStr& output,
                                 bool reserve) {
    using Fn=void(*)(idFileSystem*,int&,const char*,const char*,idStr&,bool);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[23])(
        this,lastNumber,baseName,extension,output,reserve);
  }
  bool FileExistsRetail(const char* path,bool gamedirOnly) {
    using Fn=bool(*)(idFileSystem*,const char*,bool);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<Fn>(slots[17])(this,path,gamedirOnly);
  }
  void ClearZipCacheRetail() {
    using Fn=void(*)(idFileSystem*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[41])(this);
  }
  void InitRetail() {
    using Fn=void(*)(idFileSystem*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[1])(this);
  }
  std::uint32_t GetTimestampRetail(const char* path,bool searchAll) {
    using Fn=std::uint32_t(*)(idFileSystem*,const char*,bool);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<Fn>(slots[21])(this,path,searchAll);
  }
  std::uint64_t ReadFileRetail(const char* path,
                               idHeapArray<unsigned char>& data,
                               std::uint32_t* timestamp) {
    using Fn=std::uint64_t(*)(idFileSystem*,const char*,
                              idHeapArray<unsigned char>*,std::uint32_t*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<Fn>(slots[24])(this,path,&data,timestamp);
  }
  int ReadFileRetail(const char* path,void** buffer,std::uint32_t* timestamp) {
    using Fn=int(*)(idFileSystem*,const char*,void**,std::uint32_t*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<Fn>(slots[25])(this,path,buffer,timestamp);
  }
  void FreeFileRetail(void* buffer) {
    using Fn=void(*)(idFileSystem*,void*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[26])(this,buffer);
  }
  void RemoveFileRetail(const char* path,fsPath_t basePath) {
    using Fn=void(*)(idFileSystem*,const char*,fsPath_t);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[28])(this,path,basePath);
  }
  void CreateOSPathRetail(const char* path) {
    using Fn=void(*)(idFileSystem*,const char*);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[16])(this,path);
  }
  void RelativePathToOSPathRetail(const char* relativePath,char* output,
                                  int outputLength,fsPath_t basePath) {
    using Fn=void(*)(idFileSystem*,const char*,char*,int,fsPath_t);
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    reinterpret_cast<Fn>(slots[13])(
        this,relativePath,output,outputLength,basePath);
  }
  // NEEDS_WORK(shared/idlib/filesystem): this is the renderer-facing retail
  // boundary. Recover the platform/path-selection implementation with the
  // filesystem module; the fallback preserves already-native full paths.
  virtual void RelativePathToOSPath(const char* relativePath, char* output,
                                    int outputLength, fsPath_t) const {
    idStr::Copynz(output, relativePath != nullptr ? relativePath : "",
                  outputLength);
  }
  // NEEDS_WORK(shared/idlib/filesystem): exact search-path selection belongs
  // to the filesystem recovery pass.
  virtual const char* GetBasePathStr(fsPath_t) const { return ""; }
  // NEEDS_WORK(shared/idlib/filesystem): exact numbering/search policy belongs
  // to filesystem. This boundary preserves the retail signature for renderer
  // screenshot commands until that compiland is recovered.
  virtual void GetNumberedFilename(int* lastNumber, const char* baseName,
                                   const char* extension, idStr* output,
                                   bool) const {
    const int number = lastNumber != nullptr ? ++*lastNumber : 1;
    output->Format("%s%05d.%s", baseName != nullptr ? baseName : "", number,
                   extension != nullptr ? extension : "");
  }
};

class idFileSystemLocal final : public idFileSystem {
public:
  void Init(const char* root) override;
  void Shutdown() override;
  idFile* OpenFileRead(const char* path) override;
  idFile* OpenFileWrite(const char* path) override;
  idFile* OpenFileAppend(const char* path, fsPath_t basePath) override;
  bool ReadFile(const char* path, std::vector<std::uint8_t>& data) override;
  bool WriteFile(const char* path, const void* data, std::size_t size) override;
  bool FileExists(const char* path) const override;
  std::uint32_t GetTimestamp(const char* path) const override;
  bool RemoveFile(const char* path) override;
  bool RenameFile(const char* from, const char* to);
  idFileList* ListFiles(const char* directory, const char* extension,
                        bool recursive = false) const override;
  idStr BuildOSPath(const char* path) const;
  const char* GetBasePathStr(fsPath_t) const override {
    return rootPath.data;
  }

private:
  idStr rootPath;
};

extern idFileSystemLocal fileSystemLocal;
extern idFileSystem* fileSystem;
