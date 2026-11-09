#include "decl.h"

#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/hashing/md.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/networking/bitmsg.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

idDeclManager* declManager = nullptr;
void (*resourceLoadCallback)(idResource*) = nullptr;

namespace {
constexpr int DECL_LEXER_FLAGS = 156216;

bool ReadFileBytes(const char* path, std::vector<std::uint8_t>& bytes) {
  return fileSystem != nullptr && path != nullptr && fileSystem->ReadFile(path, bytes);
}

std::uint32_t FileTimestamp(const char* path) {
  if (fileSystem == nullptr || path == nullptr)
    return 0;
  std::unique_ptr<idFile> file(fileSystem->OpenFileRead(path));
  return file ? file->Timestamp() : 0;
}

int CountLines(const std::vector<std::uint8_t>& bytes) {
  return static_cast<int>(std::count(bytes.begin(), bytes.end(), static_cast<std::uint8_t>('\n')));
}

idStr SourceControlPath(const char* relativePath) {
  if (auto* local = dynamic_cast<idFileSystemLocal*>(fileSystem))
    return local->BuildOSPath(relativePath);
  return idStr(relativePath);
}
} // namespace

const idResource* idResourceList::GetResourceByID(int id) const {
  if (id == 0)
    return nullptr;
  for (int index = 0; index < resourceList.Num(); ++index) {
    if (resourceList[index] != nullptr && resourceList[index]->resourceID == id)
      return resourceList[index];
  }
  return nullptr;
}

const idResource* idResourceList::ReadResourceFromMsg(idBitMsg* message) const {
  return message != nullptr ? GetResourceByID(message->ReadBits(32)) : nullptr;
}

void idResourceList::WriteResourceToMsg(const idResource* resource,
                                        idBitMsg* message) const {
  if (message != nullptr)
    message->WriteBits(resource != nullptr ? resource->resourceID : 0, 32);
}

idResource* idResourceList::Alloc(const char*) const {
  return nullptr;
}

int idDecl::GetTextLength() const { return textLength; }

int idResourceList::AllocSize() const {
  return 0;
}

idResource* idResourceList::Load(const char* resourceName, bool load, bool forceReload) {
  if (resourceName == nullptr || resourceName[0] == '\0') return nullptr;
  for (int index = 0; index < resourceList.Num(); ++index) {
    idResource* resource = resourceList[index];
    if (resource != nullptr && idStr::Icmp(resource->name.str, resourceName) == 0) {
      if (forceReload || (load && !resource->loadedResource)) {
        resource->LoadResource();
        resource->loadedResource = 1;
      }
      return resource;
    }
  }
  idResource* resource = Alloc(resourceName);
  if (resource != nullptr && load) {
    resource->LoadResource();
    resource->loadedResource = 1;
  }
  return resource;
}

void idResourceList::Remove(idResource* resource) {
  if (resource == nullptr)
    return;
  for (int index = 0; index < resourceList.Num(); ++index) {
    if (resourceList[index] != resource)
      continue;
    resourceList.RemoveIndex(index);
    for (int remaining = index; remaining < resourceList.Num(); ++remaining) {
      if (resourceList[remaining] != nullptr)
        resourceList[remaining]->resourceID = remaining;
    }
    resource->resourceID = -1;
    return;
  }
}

idResource::idResource()
    : resourceListPtr(nullptr), resourceError(nullptr), resourceID(-1), staleCount(0),
      loadedResource(0), staticResource(0), insideRecursiveLoad(0), buildForceLoad(0),
      loadDeferred(0) {}

idResource::~idResource() noexcept(false) {
  delete[] resourceError;
}

void idResource::LoadResource() {}
bool idResource::ReloadIfStale() { return false; }
void idResource::WriteResourceFile() {}
idResourceList* idResource::GetResourceList() const { return resourceListPtr; }
void idResource::Print() { idLib::Printf("%s", name.str); }
void idResource::List() const { idLib::Printf("%s", name.str); }

void idResource::SetName(const char* resourceName) {
  name.Set(resourceName);
}

const char* idResource::GetName() const {
  return name.str;
}

const char* idResource::GetResourceTypeName() const {
  return resourceListPtr != nullptr && resourceListPtr->resourceTypeName != nullptr
             ? resourceListPtr->resourceTypeName
             : "decl";
}

void idResource::SetResourceError(const char* format, ...) {
  delete[] resourceError;
  resourceError = nullptr;
  if (format == nullptr)
    return;

  char buffer[4096];
  std::va_list arguments;
  va_start(arguments, format);
  idStr::vsnPrintf(buffer, sizeof(buffer), format, arguments);
  va_end(arguments);
  resourceError = CopyString(buffer);
}

idDecl* idDeclInfo::AllocDecl() {
  return nullptr;
}

idDecl* idDeclInfo::FindResource(const char* declName, bool load) const {
  for (int i = 0; i < resourceList.Num(); ++i) {
    idDecl* decl = dynamic_cast<idDecl*>(resourceList[i]);
    if (decl != nullptr && idStr::Icmp(decl->name.str, declName) == 0) {
      if (load && !decl->loadedResource) {
        decl->LoadResource();
        decl->loadedResource = 1;
      }
      return decl;
    }
  }
  return nullptr;
}

bool idDeclManager::ShouldLogMissingDecl(const idDecl&) const {
  return false;
}

void idDeclManager::AddDependency(idDecl& decl, const idDecl* dependency) {
  if (dependency != nullptr && dependency->declSource != nullptr &&
      dependency->declSource->sourceFile != nullptr)
    AddDependency(decl, dependency->declSource->sourceFile->fileName.str);
}

void idDeclManager::GetDeclTextHeader(const idDeclInfo& info, const char* resourceName,
                                      idStr& header) const {
  header.Format("\n%s %s ", info.name, resourceName);
}

std::uint32_t idSourceControl::CheckOut(const idStr& file) {
  idList<idStr> files;
  files.Append(file);
  return CheckOut(files);
}

std::uint32_t idSourceControl::Import(const idStr& file, bool keepCheckedOut, bool submit,
                                      scFileType_t fileType) {
  idList<idStr> files;
  files.Append(file);
  return Import(files, keepCheckedOut, submit, fileType);
}

idDecl::idDecl()
    : textSource(nullptr), textLength(0), md5Checksum(0), programaticallyDefined(false),
      declSource(nullptr) {}

idDecl::~idDecl() noexcept(false) {
  delete[] textSource;
  textSource = nullptr;
}

int idDecl::GetLineNum() const {
  return declSource != nullptr ? declSource->sourceLine : 0;
}

std::uint64_t idDecl::Size() const {
  return static_cast<std::uint64_t>(textLength) + 80u;
}

bool idDecl::SourceFileExists() const {
  return declSource != nullptr && declSource->sourceFile != nullptr &&
         declSource->sourceFile->fileSize > 0;
}

bool idDecl::EverReloaded() const {
  return declSource->sourceFile->everReloaded;
}

idDeclSource* idDecl::DeclSource() const {
  return declSource;
}

idStr ProductionFilename(const idDecl* resource) {
  idStr result;
  result.Format("generated/decls/%s/%s.decl", resource->GetResourceTypeName(),
                resource->name.str);
  return result;
}

bool idDecl::IsImplicit() const {
  return idLib::production < PROD_PRODUCTION && !programaticallyDefined &&
         std::strstr(name.str, ".decl") == nullptr && declSource == nullptr;
}

const char* idDecl::GetFileName() const {
  return declSource != nullptr && declSource->sourceFile != nullptr
             ? declSource->sourceFile->fileName.str
             : name.str;
}

void idDecl::SetText(const char* text, int length) {
  delete[] textSource;
  textSource = nullptr;
  if (text == nullptr) {
    textLength = 0;
    md5Checksum = 0;
    return;
  }

  textLength = length != 0 ? length : idStr::Length(text);
  while (textLength > 0) {
    const char last = text[textLength - 1];
    if (last != '\n' && last != '\r' && last != ' ')
      break;
    --textLength;
  }
  textSource = new char[static_cast<std::size_t>(textLength) + 1];
  std::memmove(textSource, text, textLength);
  textSource[textLength] = '\0';
  md5Checksum = MD5_BlockChecksum(reinterpret_cast<const std::uint8_t*>(text), textLength);
}

void idDecl::MakeDefault() {
  FreeData();
  idDeclInfo* info = GetDeclInfo();
  declSource = info != nullptr ? info->FindDeclSource(name.str) : nullptr;
  SetText(DefaultDefinition(), 0);
  if (textLength == 0)
    Revert();

  idParser source;
  source.LoadMemory(textSource, textLength, "_defaultFile");
  source.SetFlags(DECL_LEXER_FLAGS);
  source.SkipUntilString("{");
  Parse(source);
  if (source.HadError() || source.HadWarning()) {
    idLib::FatalError("Parse of defaulted %s decl %s failed", info != nullptr ? info->name : "decl",
                      name.str);
  }
  if (resourceError == nullptr)
    SetResourceError("defaulted '%s'", name.str);
}

void idDecl::List() const {
  idLib::Printf("%s %s %i\n", name.str, GetFileName(), GetLineNum());
}

void idDecl::Revert(bool force) {
  if (declSource == nullptr) {
    MakeDefault();
    return;
  }
  idDeclFile* sourceFile = declSource->sourceFile;
  if (idLib::production == PROD_DEVELOPMENT && sourceFile != nullptr && sourceFile->fileSize > 0 &&
      sourceFile->SourceFileChanged(false)) {
    if (declManager != nullptr)
      declManager->Reload(false);
    return;
  }
  SetText(nullptr);
  ReParse(force || true);
}

void idDecl::DefineFromSource(idDeclSource* source) {
  SetText(nullptr);
  declSource = source;
}

void idDecl::DeleteUnusedText() {
  idDeclInfo* info = GetDeclInfo();
  if (info == nullptr || info->classname == nullptr ||
      idStr::Cmp(info->classname, "idDeclEntityDef") != 0) {
    SetText(nullptr);
  }
}

bool idDecl::ReloadIfStale() {
  if (declSource == nullptr || declSource->sourceFile == nullptr ||
      idLib::production != PROD_DEVELOPMENT || declSource->sourceFile->fileSize <= 0 ||
      !declSource->sourceFile->SourceFileChanged(false)) {
    return false;
  }
  declSource->sourceFile->LoadAndParse();
  idDeclInfo* info = GetDeclInfo();
  if (info != nullptr && info->onReload != nullptr)
    info->onReload(this);
  return true;
}

std::uint32_t idDecl::GetDeclTimestamp() {
  if (IsImplicit())
    return ~std::uint32_t{0};
  return FileTimestamp(GetFileName());
}

char* idDecl::GetText(bool force) {
  if (textLength == 0)
    Revert(force);
  return textSource;
}

void idDecl::ReParse(bool fromDisk) {
  SetResourceError(nullptr);
  FreeData();

  idDeclFile* sourceFile = declSource != nullptr ? declSource->sourceFile : nullptr;
  if (!programaticallyDefined && sourceFile != nullptr && fromDisk) {
    if (idLib::production == PROD_DEVELOPMENT && sourceFile->fileSize > 0 &&
        sourceFile->SourceFileChanged(false) && declManager != nullptr) {
      declManager->Reload(false);
    }
    std::unique_ptr<idFile> file(fileSystem != nullptr ? fileSystem->OpenFileRead(sourceFile->fileName.str)
                                                       : nullptr);
    if (file != nullptr && declSource->sourceTextLength > 0) {
      std::vector<char> bytes(static_cast<std::size_t>(declSource->sourceTextLength));
      file->Seek(declSource->sourceTextOffset, FS_SEEK_SET);
      const std::size_t read = file->Read(bytes.data(), bytes.size());
      if (read == bytes.size())
        SetText(bytes.data(), static_cast<int>(read));
    }
  }

  bool implicitText = false;
  if (!programaticallyDefined && textSource == nullptr) {
    implicitText = SetImplicitText();
    if (!implicitText && textSource == nullptr) {
      MakeDefault();
      return;
    }
  }

  idParser source;
  source.SetFlags(DECL_LEXER_FLAGS);
  source.LoadMemory(textSource, textLength, GetFileName());
  source.SetSourceLine(GetLineNum());
  if (!programaticallyDefined && sourceFile != nullptr) {
    for (int i = sourceFile->includeFiles.Num() - 1; i >= 0; --i)
      source.AddInclude(sourceFile->includeFiles[i].data);
  }

  if (!programaticallyDefined && idLib::production == PROD_BUILDING &&
      (sourceFile != nullptr || implicitText)) {
    const idStr productionFile = ProductionFilename(this);
    idStr section;
    source.ParseBracedSectionExact(section, true);
    if (fileSystem != nullptr)
      fileSystem->WriteFile(productionFile.data, section.data, section.Length());
    std::vector<std::uint8_t> generated;
    if (ReadFileBytes(productionFile.data, generated)) {
      SetText(reinterpret_cast<const char*>(generated.data()), static_cast<int>(generated.size()));
      source.FreeSource();
      source.LoadMemory(textSource, textLength, GetFileName());
    }
  }

  source.SkipUntilString("{");
  Parse(source);
  if (declManager != nullptr) {
    for (int i = 0; i < source.GetNumDependencies(); ++i)
      declManager->AddDependency(*this, source.GetDependency(i).fileName.data);
  }
  if (source.HadError() || source.HadWarning())
    MakeDefault();
}

void idDecl::LoadResource() {
  if (programaticallyDefined)
    return;
  SetResourceError(nullptr);

  if (idLib::production >= PROD_PRODUCTION) {
    FreeData();
    const idStr fileName = std::strstr(name.str, ".decl") != nullptr ? idStr(name.str)
                                                                      : ProductionFilename(this);
    std::vector<std::uint8_t> bytes;
    if (!ReadFileBytes(fileName.data, bytes) || bytes.empty()) {
      SetResourceError("couldn't read production file for decl '%s'", fileName.data);
      MakeDefault();
      return;
    }
    SetText(reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()));
    idParser source;
    source.SetFlags(DECL_LEXER_FLAGS);
    source.LoadMemory(textSource, textLength, fileName.data);
    source.ExpectTokenString("{");
    Parse(source);
    DeleteUnusedText();
    if (source.HadError() || source.HadWarning())
      MakeDefault();
    return;
  }

  if (std::strstr(name.str, ".decl") == nullptr) {
    idDeclInfo* info = GetDeclInfo();
    idDeclSource* source = info != nullptr ? info->FindDeclSource(name.str) : nullptr;
    if (source == nullptr && declManager != nullptr && declManager->ShouldLogMissingDecl(*this))
      LogMissingDecl();
    DefineFromSource(source);
    ReParse(true);
    return;
  }

  std::vector<std::uint8_t> bytes;
  if (ReadFileBytes(name.str, bytes) && !bytes.empty()) {
    SetText(reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()));
    declSource = nullptr;
    ReParse(true);
    return;
  }
  if (std::strstr(name.str, "automaterial") == nullptr)
    idLib::FatalError("Missing decl %s - Is the file checked in?", name.str);
  SetResourceError("Explicit decl file not found '%s'", name.str);
  MakeDefault();
}

bool idDecl::ReplaceSourceFileText(bool useSourceControl) {
  if (std::strstr(name.str, ".decl") != nullptr) {
    return fileSystem != nullptr &&
           fileSystem->WriteFile(name.str, GetText(false), static_cast<std::size_t>(textLength));
  }
  if (declSource == nullptr) {
    idLib::Warning("Can't save decl %s without a declSource.", name.str);
    return false;
  }
  idDeclFile* sourceFile = declSource->sourceFile;
  const char* sourceName = sourceFile != nullptr ? sourceFile->fileName.str : name.str;
  if (sourceFile == nullptr || sourceName == nullptr) {
    idLib::Warning("Decl %s has an invalid sourceFile name.", name.str);
    return false;
  }

  const idStr sourceControlFile = SourceControlPath(sourceName);
  if (idLib::sourceControl != nullptr && useSourceControl && !sourceControlFile.IsEmpty() &&
      idLib::sourceControl->GetFileStatus(sourceControlFile) == SCF_CHECKEDIN) {
    idLib::sourceControl->CheckOut(sourceControlFile);
  }

  std::vector<std::uint8_t> oldBytes;
  if (sourceFile->fileSize != 0) {
    if (!ReadFileBytes(sourceName, oldBytes) || static_cast<int>(oldBytes.size()) != sourceFile->fileSize ||
        FileTimestamp(sourceName) != sourceFile->timestamp ||
        MD5_BlockChecksum(oldBytes.data(), oldBytes.size()) != sourceFile->checksum) {
      idLib::Warning("The file %s has been modified outside of the engine.", sourceName);
      return false;
    }
  }

  const bool append = declSource->sourceTextOffset >= sourceFile->fileSize;
  idStr header;
  if (append && declManager != nullptr)
    declManager->GetDeclTextHeader(*GetDeclInfo(), name.str, header);

  const int oldSize = sourceFile->fileSize;
  const int delta = append ? header.Length() + textLength : textLength - declSource->sourceTextLength;
  const int newSize = oldSize + delta;
  if (newSize < 0)
    return false;
  std::vector<std::uint8_t> result(static_cast<std::size_t>(newSize), 0);

  if (append) {
    std::copy(oldBytes.begin(), oldBytes.end(), result.begin());
    if (header.Length() > 0)
      std::memcpy(result.data() + oldSize, header.data, header.Length());
    declSource->sourceTextOffset = oldSize + header.Length();
  } else {
    const int offset = declSource->sourceTextOffset;
    std::copy_n(oldBytes.begin(), offset, result.begin());
    const int oldTail = offset + declSource->sourceTextLength;
    std::copy(oldBytes.begin() + oldTail, oldBytes.end(), result.begin() + offset + textLength);
  }
  if (textLength > 0)
    std::memcpy(result.data() + declSource->sourceTextOffset, textSource, textLength);

  if (fileSystem == nullptr || !fileSystem->WriteFile(sourceName, result.data(), result.size())) {
    idLib::Warning("Couldn't open %s for writing.", sourceName);
    return false;
  }

  sourceFile->fileSize = newSize;
  sourceFile->checksum = MD5_BlockChecksum(result.data(), result.size());
  sourceFile->timestamp = FileTimestamp(sourceName);
  sourceFile->numLines = CountLines(result);
  for (idDeclSource* source = sourceFile->firstDeclSourceInFile; source != nullptr;
       source = source->nextInFile) {
    if (source != declSource && source->sourceTextOffset > declSource->sourceTextOffset)
      source->sourceTextOffset += delta;
  }
  declSource->sourceTextLength = textLength;
  if (resourceLoadCallback != nullptr)
    resourceLoadCallback(this);

  if (idLib::sourceControl != nullptr && useSourceControl && !sourceControlFile.IsEmpty() &&
      idLib::sourceControl->GetFileStatus(sourceControlFile) == SCF_NOTIMPORTED) {
    idLib::sourceControl->Import(sourceControlFile, true, false, SCT_NONE);
  }
  return true;
}

void idDecl::WriteResourceFile() {
  ReplaceSourceFileText(true);
}

bool idDecl::RebuildTextSource() { return false; }
bool idDecl::SetImplicitText() { return false; }
const char* idDecl::DefaultDefinition() const { return "{ }"; }
void idDecl::LogMissingDecl() {}
void idDecl::FreeData() {}
bool idDecl::IsInventoryDecl() const { return false; }
