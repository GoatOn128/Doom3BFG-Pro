#include "declmanager.h"

#include "declsimple.h"
#include "decltable.h"
#include "shared/idlib/csystems/autocomplete.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/hashing/md.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

idDeclManagerLocal declManagerLocal;

namespace {
std::uint32_t TimestampForFile(const char* fileName) {
  std::unique_ptr<idFile> file(fileSystem != nullptr ? fileSystem->OpenFileRead(fileName) : nullptr);
  return file != nullptr ? file->Timestamp() : 0;
}

idDeclFile* FindLoadedFile(idDeclManagerLocal& manager, const char* fileName) {
  for (int i = 0; i < manager.loadedFiles.Num(); ++i) {
    if (idStr::IcmpPath(manager.loadedFiles[i]->fileName.str, fileName) == 0)
      return manager.loadedFiles[i];
  }
  return nullptr;
}

class idDeclMergeLexer final : public idLexer {
public:
  using idLexer::idLexer;

  int CurrentOffset() const { return static_cast<int>(script_p - buffer); }
  int CurrentLine() const { return line; }
};

struct idDeclMergeInfo {
  idStr declName;
  idDeclInfo* declType = nullptr;
  int startPos = 0;
  int length = 0;
  bool needsToWriteType = false;
};

bool MergeInfoLess(const idDeclMergeInfo& left, const idDeclMergeInfo& right) {
  return idStr::Icmp(left.declName.data, right.declName.data) < 0;
}

bool WriteMergeFile(const idStr& fileName, const std::string& contents) {
  if (fileSystem != nullptr && fileSystem->WriteFile(fileName.data, contents.data(), contents.size()))
    return true;
  idLib::Warning("Couldn't open %s for writing.", fileName.data);
  return false;
}

void PrintDeclTypes() {
  idLib::Printf("valid types: ");
  for (int i = 0; i < declManagerLocal.declTypes.Num(); ++i)
    idLib::Printf("%s\n", declManagerLocal.declTypes[i]->name);
  idLib::Printf("\n");
}

struct DeclTreeReplacement {
  int offset;
  int length;
  std::string includeText;
};
} // namespace

// NEEDS_WORK(engine/sound): the retail callback forwards to
// soundSystem->OnReloadSound.  The sound-system implementation is intentionally
// deferred; retaining this callback keeps the decl registration ABI complete.
void OnReloadSound(const idDecl*) {}

void ShowAvailableDeclLogs(idAutoComplete* completion) {
  if (completion == nullptr)
    return;
  idList<idStr, 44> types;
  declManagerLocal.GetLogDeclsList(types);
  for (int i = 0; i < types.Num(); ++i)
    completion->Append(types[i], -1);
}

void idDeclManager::ArgCompletion_DeclTypeAndName(idAutoComplete* completion) {
  if (declManager != nullptr)
    declManager->ArgCompletion_DeclTypeName(completion);
}

const char* idDeclSource::GetFileName() const {
  return sourceFile != nullptr ? sourceFile->fileName.str : "";
}

idDeclFile::idDeclFile() = default;
idDeclFile::~idDeclFile() = default;

bool idDeclFile::SourceFileChanged(bool compareChecksums) const {
  std::vector<std::uint8_t> bytes;
  if (fileSystem == nullptr || !fileSystem->ReadFile(fileName.str, bytes) ||
      static_cast<int>(bytes.size()) != fileSize)
    return true;
  if (!compareChecksums)
    return TimestampForFile(fileName.str) != timestamp;
  return MD5_BlockChecksum(bytes.data(), bytes.size()) != checksum;
}

idDeclInfo::idDeclInfo(const char* identifier, const char* className, const char* extension,
                       bool remapping)
    : name(identifier), classname(className), fileExtension(extension), usesRemapping(remapping) {
  resourceTypeName = identifier;
}

idDeclInfo::~idDeclInfo() = default;

idDeclSource* idDeclInfo::FindDeclSource(const char* declName) const {
  if (declName == nullptr)
    return nullptr;
  idStr canonical(declName);
  if (!idStr::IsNameCanonical(declName))
    canonical.MakeNameCanonical();
  const char* nameToFind = canonical.data;
  for (int index = declSourceHash.First(idStr::Hash(nameToFind)); index != -1;
       index = declSourceHash.Next(index)) {
    if (index >= 0 && index < declSources.Num() && declSources[index] != nullptr &&
        idStr::Cmp(nameToFind, declSources[index]->name.str) == 0)
      return declSources[index];
  }
  return nullptr;
}

void idDeclFile::FreeDynamic() {
  idDeclSource* source = firstDeclSourceInFile;
  idDeclSource* previous = nullptr;
  while (source != nullptr) {
    idDeclSource* next = source->nextInFile;
    if (!source->isStatic) {
      if (declManager != nullptr) {
        for (int i = 0; i < declManager->GetNumDeclTypes(); ++i) {
          idDeclInfo* type = declManager->GetDeclType(i);
          if (type != nullptr)
            type->RemoveDeclSource(source);
        }
      }
      if (previous != nullptr)
        previous->nextInFile = next;
      else
        firstDeclSourceInFile = next;
      delete source;
    } else {
      previous = source;
    }
    source = next;
  }
  dependencies.SetNum(std::min(dependenciesDynamicStartIndex, dependencies.Num()));
  includeFiles.Clear();
}

idDecl* idDeclFile::AddDecl(idDeclInfo* type, const char* declName, int line, int textOffset,
                            int sourceLength, bool parseDecl) {
  if (type == nullptr || declName == nullptr)
    return nullptr;
  idDeclSource* source = type->FindDeclSource(declName);
  if (source != nullptr && source->sourceFile != this) {
    hadRedefinition = true;
    idLib::Warning("Decl %s defined in files %s and %s.", declName,
                   source->sourceFile != nullptr ? source->sourceFile->fileName.str : "",
                   fileName.str);
    return nullptr;
  }
  if (source == nullptr) {
    source = new idDeclSource;
    source->name.Set(declName);
    source->sourceFile = this;
    source->nextInFile = firstDeclSourceInFile;
    source->isStatic = dependenciesDynamicStartIndex < 0;
    firstDeclSourceInFile = source;
    type->AddDeclSource(source);
  }
  source->sourceFile = this;
  source->sourceLine = line;
  source->sourceTextOffset = textOffset;
  source->sourceTextLength = sourceLength;

  idDecl* loaded = type->FindResource(declName, false);
  if (loaded == nullptr)
    return nullptr;
  loaded->DefineFromSource(source);
  if (parseDecl)
    loaded->ReParse(true);
  return loaded;
}

std::uint32_t idDeclFile::LoadAndParse() {
  if (fileName.str[0] == '\0')
    return 0;

  std::vector<std::uint8_t> bytes;
  if (fileSystem == nullptr || !fileSystem->ReadFile(fileName.str, bytes)) {
    idLib::FatalError("couldn't load %s", fileName.str);
  }

  dependencies.Clear();
  includeFiles.Clear();
  fileSize = static_cast<int>(bytes.size());
  timestamp = TimestampForFile(fileName.str);
  checksum = MD5_BlockChecksum(bytes.data(), bytes.size());
  numLines = static_cast<int>(std::count(bytes.begin(), bytes.end(), static_cast<std::uint8_t>('\n')));

  if (idStr::Icmpn(fileName.str, "declTree/", 9) == 0) {
    const int slash = idStr::Find(fileName.str, '/', 9);
    if (slash > 0 && declManager != nullptr) {
      idStr typeName;
      typeName.CopyRange(fileName.str, 9, slash);
      idDeclInfo* type = declManager->GetDeclType(typeName.data);
      if (type != nullptr) {
        idStr declName(fileName.str + slash + 1);
        declName.StripFileExtension();
        declName.MakeNameCanonical();
        AddDecl(type, declName.data, 0, 0, fileSize, true);
      } else {
        idLib::Warning("Unknown decl type %s", typeName.data);
      }
    }
    return checksum;
  }

  idDeclMergeLexer lexer(156216);
  if (!lexer.LoadMemory(reinterpret_cast<const char*>(bytes.data()), bytes.size(), fileName.str))
    idLib::Error("Couldn't parse %s", fileName.str);

  std::vector<idDecl*> newDecls;
  idToken token;
  while (lexer.ReadToken(token)) {
    if (token == "$") {
      lexer.Error("$ precompiled directives only allowed in decls bodies and include files");
      break;
    }
    if (token == "#") {
      if (!lexer.ExpectTokenString("include"))
        break;
      if (!lexer.ReadToken(token)) {
        lexer.Error("Could not read token after include");
        break;
      }
      includeFiles.Append(token);
      continue;
    }

    idDeclInfo* type = declManager != nullptr ? declManager->GetDeclType(token.data) : nullptr;
    if (type != nullptr) {
      if (!lexer.ReadToken(token)) {
        lexer.Error("Could not read decl name after type");
        break;
      }
    } else {
      type = defaultType;
      if (type == nullptr) {
        lexer.Warning("Unknown decl type %s", token.data);
        break;
      }
    }
    if (token == "{") {
      lexer.Error("Missing decl name");
      break;
    }

    if (type->defaultFolder.str[0] == '\0') {
      idStr path(fileName.str);
      path.StripFilename();
      type->defaultFolder.Set(path.data);
    }
    idStr canonicalName(token);
    canonicalName.MakeNameCanonical();
    lexer.SkipWhiteSpace(false);
    const int sourceOffset = lexer.CurrentOffset();
    const int sourceLine = lexer.CurrentLine();
    if (!lexer.ExpectTokenString("{"))
      break;
    const int braceLine = lexer.CurrentLine();
    if (!lexer.SkipBracedSection(false))
      idLib::Warning("Mismatched braces in decl '%s' file '%s' at line %i",
                     canonicalName.data, fileName.str, braceLine);
    const int sourceLength = lexer.CurrentOffset() - sourceOffset;
    if (idDecl* loaded =
            AddDecl(type, canonicalName.data, sourceLine, sourceOffset, sourceLength, false))
      newDecls.push_back(loaded);
  }
  for (idDecl* decl : newDecls)
    decl->ReParse(true);
  return checksum;
}

const char* idDeclInfo::RemapResourceName(char* buffer, int bufferSize,
                                          const char* resourceName) const {
  if (buffer != nullptr && bufferSize > 0)
    idStr::Copynz(buffer, resourceName, bufferSize);
  return usesRemapping && buffer != nullptr ? buffer : resourceName;
}

const idDecl* idDeclInfo::FindExisting(const char* resourceName, bool remap) const {
  char buffer[512];
  const char* canonical = remap ? RemapResourceName(buffer, sizeof(buffer), resourceName) : resourceName;
  return const_cast<idDeclInfo*>(this)->FindResource(canonical, false);
}

idDecl* idDeclInfo::FindWithInheritance(const char* resourceName, bool makeDefault, bool remap) const {
  if (resourceName == nullptr || *resourceName == '\0') {
    if (!makeDefault)
      return nullptr;
    resourceName = "_emptyName";
  }

  idStr canonical(resourceName);
  canonical.MakeNameCanonical();
  char buffer[512];
  const char* selectedName = canonical.data;
  if (remap && usesRemapping)
    selectedName = RemapResourceName(buffer, sizeof(buffer), canonical.data);

  if (idDecl* existing = const_cast<idDeclInfo*>(this)->FindResource(selectedName, false)) {
    if (makeDefault || existing->resourceError == nullptr)
      return existing;
    return nullptr;
  }

  idDeclInfo* selectedType = const_cast<idDeclInfo*>(this);
  if (declManager != nullptr && std::strstr(selectedName, ".decl") == nullptr) {
    for (int i = 0; i < declManager->GetNumDeclTypes(); ++i) {
      idDeclInfo* child = declManager->GetDeclType(i);
      if (child == nullptr || child == this)
        continue;
      idDeclInfo* ancestor = child->parent;
      while (ancestor != nullptr && ancestor != this)
        ancestor = ancestor->parent;
      if (ancestor != this)
        continue;
      char childBuffer[512];
      const char* childName = remap && child->usesRemapping
                                  ? child->RemapResourceName(childBuffer, sizeof(childBuffer), canonical.data)
                                  : selectedName;
      if (idDecl* existing = child->FindResource(childName, false)) {
        if (makeDefault || existing->resourceError == nullptr)
          return existing;
        return nullptr;
      }
    }
  }

  idDeclSource* matchingSource = nullptr;
  matchingSource = FindDeclSource(selectedName);
  if (matchingSource == nullptr && declManager != nullptr &&
      std::strstr(selectedName, ".decl") == nullptr) {
    for (int i = 0; i < declManager->GetNumDeclTypes() && matchingSource == nullptr; ++i) {
      idDeclInfo* child = declManager->GetDeclType(i);
      if (child == nullptr || child == this)
        continue;
      idDeclInfo* ancestor = child->parent;
      while (ancestor != nullptr && ancestor != this)
        ancestor = ancestor->parent;
      if (ancestor == this) {
        matchingSource = child->FindDeclSource(selectedName);
        if (matchingSource != nullptr)
          selectedType = child;
      }
    }
  }
  if (matchingSource == nullptr && !makeDefault)
    return nullptr;
  if (!makeDefault && std::strstr(selectedName, ".decl") != nullptr && fileSystem != nullptr &&
      !fileSystem->FileExists(selectedName))
    return nullptr;

  idResource* resource = selectedType->Alloc(selectedName);
  idDecl* decl = dynamic_cast<idDecl*>(resource);
  if (decl == nullptr) {
    delete resource;
    return nullptr;
  }
  decl->resourceID = selectedType->resourceList.Num();
  selectedType->resourceList.Append(decl);
  decl->declSource = matchingSource;
  decl->LoadResource();
  decl->loadedResource = 1;
  return decl;
}

const idResource* idDeclInfo::GetResourceByID(int resourceID) const {
  return GetResourceByIDWithInheritance(resourceID);
}

const idResource* idDeclInfo::GetResourceByIDWithInheritance(int resourceID) const {
  if (resourceID >= 0 && resourceID < resourceList.Num() && resourceList[resourceID] != nullptr)
    return resourceList[resourceID];
  if (declManager == nullptr)
    return nullptr;
  for (int i = 0; i < declManager->GetNumDeclTypes(); ++i) {
    idDeclInfo* child = declManager->GetDeclType(i);
    for (idDeclInfo* ancestor = child; ancestor != nullptr; ancestor = ancestor->parent) {
      if (ancestor == this && child != this && resourceID >= 0 && resourceID < child->resourceList.Num())
        return child->resourceList[resourceID];
    }
  }
  return nullptr;
}

void idDeclInfo::AddDeclSource(idDeclSource* source) {
  if (source == nullptr)
    return;
  const int index = declSources.Append(source);
  declSourceHash.Add(idStr::Hash(source->name.str), index);
}

void idDeclInfo::RemoveDeclSource(idDeclSource* source) {
  int index = -1;
  for (int i = 0; i < declSources.Num(); ++i) {
    if (declSources[i] == source) {
      index = i;
      break;
    }
  }
  if (index < 0)
    return;
  const int key = idStr::Hash(source->name.str);
  for (int i = index + 1; i < declSources.Num(); ++i)
    declSources[i - 1] = declSources[i];
  declSources.SetNum(declSources.Num() - 1);
  declSourceHash.RemoveIndex(key, index);
}

void idDeclInfo::FreeDynamic() {
  for (int i = resourceList.Num() - 1; i >= 0; --i) {
    idResource* resource = resourceList[i];
    if (resource != nullptr && !resource->staticResource) {
      delete resource;
      resourceList.SetNum(i);
    }
  }
  while (declSources.Num() > declSourceDynamicStartIndex) {
    idDeclSource* source = declSources[declSources.Num() - 1];
    RemoveDeclSource(source);
    delete source;
  }
}

idDeclManagerLocal::idDeclManagerLocal()
    : loadedFilesDynamicStartIndex(0), dependenciesDynamicStartIndex(0), checksum(0) {
  declManager = this;
}

idDeclManagerLocal::~idDeclManagerLocal() { Shutdown(); }

void idDeclManagerLocal::GetDeclTextHeader(idDeclInfo* type, const char* canonicalName,
                                           idStr& header) const {
  header.Format("\n%s %s ", type != nullptr ? type->name : "", canonicalName ? canonicalName : "");
}

void idDeclManagerLocal::GetLogDeclsList(idList<idStr, 44>& declList) const {
  for (int i = 0; i < registeredDeclLogTypes.Num(); ++i)
    declList.Append(registeredDeclLogTypes[i]);
}

void idDeclManagerLocal::RegisterDeclType(idDeclInfo* type) {
  if (type == nullptr || GetDeclType(type->name) != nullptr)
    return;
  declTypes.Append(type);
}

void idDeclManagerLocal::Init() {
  RegisterDeclType(&idDeclTypeInfo::resourceList);
  RegisterDeclType(&idDeclEntityDef::resourceList);
  RegisterDeclType(&idDeclTable::resourceList);
  RegisterDeclType(&idDeclRibbon::resourceList);
  RegisterDeclType(&idDeclFlare::resourceList);
  RegisterDeclType(&idDeclRollBones::resourceList);
  RegisterDeclType(&idDeclMapInfo::resourceList);
  RegisterDeclType(&idDeclWalkIK::resourceList);
  RegisterDeclType(&idDeclReachIK::resourceList);
  RegisterDeclType(&idDeclHandIK::resourceList);
  RegisterDeclType(&idDeclMaterialDef::resourceList);
  RegisterDeclType(&idDeclLoadingScreen::resourceList);
  RegisterDeclType(&idDeclVmtrPreload::resourceList);
  RegisterDeclType(&idDeclElectricBolt::resourceList);
  RegisterDeclType(&idDeclTrailBeam::resourceList);
  RegisterDeclType(&idDeclGlass::resourceList);
  RegisterDeclType(&idDeclProductionFilter::resourceList);
  RegisterDeclType(&idDeclModelLOD::resourceList);
  RegisterDeclType(&idDeclImpactParticle::resourceList);
  RegisterDeclType(&idDeclImpactSound::resourceList);
  RegisterDeclType(&idDeclStaticImage::resourceList);
  RegisterDeclType(&idDeclSkins::resourceList);
  RegisterDeclType(&idDeclSwfResource::resourceList);

  RegisterDeclFolder("ik", "walkik", &idDeclWalkIK::resourceList);
  RegisterDeclFolder("reachik", "reachik", &idDeclReachIK::resourceList);
  RegisterDeclFolder("rollbones", "rollbones", &idDeclRollBones::resourceList);
  RegisterDeclFolder("ribbons", "ribbon", &idDeclRibbon::resourceList);
  RegisterDeclFolder("flares", "flare", &idDeclFlare::resourceList);
  RegisterDeclFolder("ebolts", "ebolt", &idDeclElectricBolt::resourceList);
  RegisterDeclFolder("modellods", "modellod", &idDeclModelLOD::resourceList);
  RegisterDeclFolder("skins", "skin", &idDeclSkins::resourceList);
  registeredDeclLogTypes.Append(idStr("particle"));
}

void idDeclManagerLocal::Shutdown() {
  for (int i = 0; i < loadedFiles.Num(); ++i)
    delete loadedFiles[i];
  loadedFiles.Clear();
  for (int i = 0; i < declTypes.Num(); ++i) {
    idDeclInfo* type = declTypes[i];
    while (type->declSources.Num() > 0) {
      idDeclSource* source = type->declSources[type->declSources.Num() - 1];
      for (int j = 0; j < type->resourceList.Num(); ++j) {
        if (auto* decl = dynamic_cast<idDecl*>(type->resourceList[j]);
            decl != nullptr && decl->declSource == source)
          decl->declSource = nullptr;
      }
      type->RemoveDeclSource(source);
      delete source;
    }
  }
  declTypes.Clear();
  declFileDependencies.Clear();
  multiplayerDeclOverrides.Clear();
  registeredDeclLogTypes.Clear();
  loadedFilesDynamicStartIndex = dependenciesDynamicStartIndex = checksum = 0;
}

void idDeclManagerLocal::Reload(bool force) {
  for (int i = 0; i < loadedFiles.Num(); ++i) {
    idDeclFile* file = loadedFiles[i];
    if (file != nullptr && (force || file->SourceFileChanged(false)))
      file->LoadAndParse();
  }
}

void idDeclManagerLocal::MergeDecls(const idStr& declType, const idStr& path) {
  if (declType.IsEmpty() && path.IsEmpty()) {
    idLib::Printf("Will merge all decls of the given type to one file. Defaults to basepath, add "
                  "'-o' to use overlay path instead\n");
    return;
  }

  idDeclInfo* mergeType = GetDeclType(declType.data);
  if (mergeType == nullptr) {
    idLib::Printf("No decl type found for '%s'\n", declType.data);
    return;
  }

  std::vector<idDeclFile*> filesToMerge;
  std::string sourceText;
  for (int i = 0; i < loadedFiles.Num(); ++i) {
    idDeclFile* file = loadedFiles[i];
    if (file == nullptr || !idStr::CheckExtension(file->fileName.str, mergeType->fileExtension) ||
        idStr::Find(file->fileName.str, "decls", false) < 0 || fileSystem == nullptr ||
        !fileSystem->FileExists(file->fileName.str)) {
      continue;
    }
    std::vector<std::uint8_t> contents;
    if (!fileSystem->ReadFile(file->fileName.str, contents))
      continue;
    filesToMerge.push_back(file);
    sourceText.append(reinterpret_cast<const char*>(contents.data()), contents.size());
    sourceText.push_back('\n');
  }

  if (filesToMerge.empty()) {
    idLib::Printf("No decls found of given type '%s'\n", mergeType->name);
    return;
  }

  std::vector<idDeclMergeInfo> mergeDecls;
  std::vector<idDeclMergeInfo> leftoverDecls;
  std::vector<idDeclInfo*> leftoverTypes;
  idDeclMergeLexer lexer(156216);
  lexer.LoadMemory(sourceText.data(), sourceText.size(), "");
  idToken token;
  bool parseSucceeded = true;
  while (lexer.ReadToken(token)) {
    const int startPos = lexer.CurrentOffset() - token.Length();
    if (token == "$") {
      lexer.Error("$ precompiled directives only allowed in decls bodies and include files");
      parseSucceeded = false;
      break;
    }
    if (token == "#") {
      if (!lexer.ExpectTokenString("include") || !lexer.ReadToken(token)) {
        if (!lexer.HadError())
          lexer.Error("Could not read token after include");
        parseSucceeded = false;
        break;
      }
      continue;
    }

    idDeclInfo* parsedType = GetDeclType(token.data);
    idDeclMergeInfo info;
    info.startPos = startPos;
    if (parsedType != nullptr) {
      if (!lexer.ReadToken(token)) {
        lexer.Error("Could not read decl name after type");
        parseSucceeded = false;
        break;
      }
      info.declType = parsedType;
      info.declName = token;
    } else {
      info.declType = mergeType;
      info.declName = token;
      info.needsToWriteType = true;
    }

    lexer.SkipWhiteSpace(false);
    if (!lexer.ExpectTokenString("{") || !lexer.SkipBracedSection(false)) {
      if (!lexer.HadError())
        idLib::Warning("Mismatched braces in decl '%s'", info.declName.data);
      parseSucceeded = false;
      break;
    }
    info.length = lexer.CurrentOffset() - info.startPos;

    if (info.declType == mergeType) {
      mergeDecls.push_back(info);
      std::stable_sort(mergeDecls.begin(), mergeDecls.end(), MergeInfoLess);
    } else {
      info.declName.Format("%s %s", info.declType->name, token.data);
      if (std::find(leftoverTypes.begin(), leftoverTypes.end(), info.declType) ==
          leftoverTypes.end())
        leftoverTypes.push_back(info.declType);
      leftoverDecls.push_back(info);
      std::stable_sort(leftoverDecls.begin(), leftoverDecls.end(), MergeInfoLess);
    }
  }

  idStr mergedFileName;
  mergedFileName.Format("merged_%s_files", mergeType->name);
  mergedFileName.ToLower();
  mergedFileName.SetFileExtension(mergeType->fileExtension);
  if (parseSucceeded && !mergeDecls.empty()) {
    std::string output;
    for (const idDeclMergeInfo& info : mergeDecls) {
      if (info.needsToWriteType) {
        output.append(mergeType->name);
        output.push_back(' ');
      }
      output.append(sourceText.data() + info.startPos, static_cast<std::size_t>(info.length));
      output.push_back('\n');
    }
    if (!WriteMergeFile(mergedFileName, output))
      return;
  }

  idStr leftoverFileName;
  leftoverFileName.Format("merged_%s_leftovers", mergeType->name);
  leftoverFileName.ToLower();
  if (parseSucceeded) {
    for (idDeclInfo* type : leftoverTypes) {
      std::string output;
      for (const idDeclMergeInfo& info : leftoverDecls) {
        if (info.declType != type)
          continue;
        output.append(sourceText.data() + info.startPos, static_cast<std::size_t>(info.length));
        output.push_back('\n');
      }
      leftoverFileName.SetFileExtension(type->fileExtension);
      if (!WriteMergeFile(leftoverFileName, output))
        return;
    }
  }

  if (!parseSucceeded)
    return;
  idStr message;
  message.Format("Merged %d %s decls to '%s' successfully", static_cast<int>(mergeDecls.size()),
                 mergeType->name, mergedFileName.data);
  if (!leftoverDecls.empty()) {
    leftoverFileName.StripFileExtension();
    idStr suffix;
    suffix.Format(", %d decls not matching the given type can be found in '%s'",
                  static_cast<int>(leftoverDecls.size()), leftoverFileName.data);
    message.Append(suffix);
  }
  idLib::Printf("%s\n", message.data);
}

void idDeclManagerLocal::MarkStatic() {
  for (int i = 0; i < declTypes.Num(); ++i) {
    idDeclInfo* type = declTypes[i];
    type->declSourceDynamicStartIndex = type->declSources.Num();
    for (int j = 0; j < type->resourceList.Num(); ++j) {
      if (type->resourceList[j] != nullptr)
        type->resourceList[j]->staticResource = 1;
    }
    for (int j = 0; j < type->declSources.Num(); ++j)
      type->declSources[j]->isStatic = true;
  }
  dependenciesDynamicStartIndex = declFileDependencies.Num();
  loadedFilesDynamicStartIndex = loadedFiles.Num();
  for (int i = 0; i < loadedFiles.Num(); ++i)
    loadedFiles[i]->dependenciesDynamicStartIndex = loadedFiles[i]->dependencies.Num();
}

void idDeclManagerLocal::FreeDynamic() {
  for (int i = 0; i < declTypes.Num(); ++i)
    declTypes[i]->FreeDynamic();
  while (loadedFiles.Num() > loadedFilesDynamicStartIndex) {
    delete loadedFiles[loadedFiles.Num() - 1];
    loadedFiles.SetNum(loadedFiles.Num() - 1);
  }
  declFileDependencies.SetNum(
      std::min(dependenciesDynamicStartIndex, declFileDependencies.Num()));
}

void idDeclManagerLocal::RegisterDeclFolder(const char* folder, const char* extension,
                                            idDeclInfo* defaultType) {
  if (idLib::production >= PROD_PRODUCTION || fileSystem == nullptr || folder == nullptr)
    return;
  std::unique_ptr<idFileList> files(fileSystem->ListFiles(folder, extension, true));
  if (!files)
    return;
  for (int i = 0; i < files->list.Num(); ++i) {
    idStr path(folder);
    path.AppendPath(files->list[i].data);
    if (FindLoadedFile(*this, path.data) != nullptr)
      idLib::Error("Decl file %s loaded twice, RegisterDeclFolder called twice on the same folder?",
                   path.data);
    auto* file = new idDeclFile;
    file->fileName.Set(path.data);
    file->defaultType = defaultType;
    loadedFiles.Append(file);
    file->LoadAndParse();
  }
}

int idDeclManagerLocal::GetNumDeclTypes() const { return declTypes.Num(); }

idDeclInfo* idDeclManagerLocal::GetDeclType(const char* typeName) const {
  if (typeName == nullptr || *typeName == '\0')
    return nullptr;
  for (int i = 0; i < declTypes.Num(); ++i) {
    if (idStr::Icmp(declTypes[i]->name, typeName) == 0)
      return declTypes[i];
  }
  return nullptr;
}

idDeclInfo* idDeclManagerLocal::GetDeclType(int index) const {
  return index >= 0 && index < declTypes.Num() ? declTypes[index] : nullptr;
}

idDeclInfo* idDeclManagerLocal::GetDeclTypeFromClassname(const char* className) const {
  for (int i = 0; i < declTypes.Num(); ++i) {
    if (idStr::Icmp(declTypes[i]->classname, className) == 0)
      return declTypes[i];
  }
  return nullptr;
}

idDecl* idDeclManagerLocal::CreateNewDecl(idDeclInfo* type, const char* resourceName,
                                          const char* sourceFileName, const char* sourceText) {
  if (type == nullptr || resourceName == nullptr)
    return nullptr;
  if (idDecl* existing = type->FindWithInheritance(resourceName, false, false))
    return existing;

  idDeclFile* file = nullptr;
  if (sourceFileName != nullptr && *sourceFileName != '\0') {
    idStr canonicalFileName(sourceFileName);
    canonicalFileName.BackSlashesToSlashes();
    file = FindLoadedFile(*this, canonicalFileName.data);
    if (file != nullptr && file->timestamp != 0 && file->SourceFileChanged(true))
      return nullptr;
    if (file == nullptr) {
      file = new idDeclFile;
      file->fileName.Set(canonicalFileName.data);
      file->defaultType = type;
      loadedFiles.Append(file);
    }
  }

  idDecl* decl = type->AllocDecl();
  if (decl == nullptr)
    return nullptr;

  idStr canonicalName(resourceName);
  canonicalName.MakeNameCanonical();
  idStr header;
  GetDeclTextHeader(type, canonicalName.data, header);
  idStr text(sourceText);
  if (text.IsEmpty())
    text = decl->DefaultDefinition();

  idDeclSource* source = nullptr;
  if (file != nullptr) {
    int sourceOffset = file->fileSize + header.Length();
    for (idDeclSource* other = file->firstDeclSourceInFile; other != nullptr;
         other = other->nextInFile) {
      if (other->sourceTextOffset + other->sourceTextLength > sourceOffset)
        sourceOffset = other->sourceTextOffset + other->sourceTextLength + header.Length();
    }
    source = new idDeclSource;
    source->name.Set(canonicalName.data);
    source->sourceFile = file;
    source->sourceTextOffset = sourceOffset;
    source->sourceTextLength = text.Length();
    source->sourceLine = file->numLines;
    source->nextInFile = file->firstDeclSourceInFile;
    file->firstDeclSourceInFile = source;
    type->AddDeclSource(source);
  }

  decl->SetName(canonicalName.data);
  decl->SetText(text.data);
  decl->declSource = source;
  decl->ReParse(false);
  if (sourceText != nullptr && idStr::Length(sourceText) > 0)
    decl->programaticallyDefined = true;
  decl->resourceID = type->resourceList.Num();
  type->resourceList.Append(decl);
  return decl;
}

void idDeclManagerLocal::AddDependency(idDecl& decl, const char* fileName) {
  if (idLib::production >= PROD_PRODUCTION || decl.declSource == nullptr ||
      decl.declSource->sourceFile == nullptr || fileName == nullptr)
    return;
  idDeclFile* sourceFile = decl.declSource->sourceFile;
  if (idStr::Icmp(sourceFile->fileName.str, fileName) == 0)
    return;
  int dependencyIndex = -1;
  for (int i = 0; i < declFileDependencies.Num(); ++i) {
    if (idStr::IcmpPath(declFileDependencies[i].fileName.str, fileName) == 0) {
      dependencyIndex = i;
      break;
    }
  }
  if (dependencyIndex < 0) {
    dependencyIndex = declFileDependencies.Num();
    declFileDependency_t* dependency = declFileDependencies.Alloc();
    dependency->fileName.Set(fileName);
    dependency->timestamp = TimestampForFile(fileName);
    dependency->dirty = false;
  }
  for (int i = 0; i < sourceFile->dependencies.Num(); ++i) {
    if (sourceFile->dependencies[i] == dependencyIndex)
      return;
  }
  sourceFile->dependencies.Append(dependencyIndex);
}

void idDeclManagerLocal::AddDependency(idDecl& decl, const idDecl* dependency) {
  idDeclManager::AddDependency(decl, dependency);
}

void idDeclManagerLocal::GetDeclFilesDependentOnDeclFile(
    const idDeclFile& source, idList<idDeclFile*>& result) const {
  result.Clear();
  int sourceIndex = -1;
  for (int i = 0; i < declFileDependencies.Num(); ++i) {
    if (idStr::IcmpPath(declFileDependencies[i].fileName.str, source.fileName.str) == 0) {
      sourceIndex = i;
      break;
    }
  }
  if (sourceIndex < 0)
    return;
  for (int i = 0; i < loadedFiles.Num(); ++i) {
    idDeclFile* file = loadedFiles[i];
    for (int j = 0; j < file->dependencies.Num(); ++j) {
      if (file->dependencies[j] == sourceIndex) {
        result.Append(file);
        break;
      }
    }
  }
}

void idDeclManagerLocal::ArgCompletion_DeclTypeName(idAutoComplete* completion) {
  if (completion == nullptr)
    return;
  for (int i = 0; i < declTypes.Num(); ++i)
    completion->Append(idStr(declTypes[i]->name), 1);
}

void idDeclManagerLocal::ArgCompletion_DeclName(idAutoComplete* completion, idDeclInfo* type) {
  if (completion == nullptr || type == nullptr)
    return;
  for (int i = 0; i < type->resourceList.Num(); ++i) {
    if (type->resourceList[i] != nullptr)
      completion->Append(idStr(type->resourceList[i]->name.str), 1);
  }
}

void idDeclManagerLocal::PrintMultiplayerDeclOverrides() {
  idLib::Printf("----- Multiplayer decl overrides -----\n");
  for (int i = 0; i < multiplayerDeclOverrides.Num(); ++i)
    idLib::Printf("%s\n", multiplayerDeclOverrides[i].data);
  idLib::Printf("%d overrides\n", multiplayerDeclOverrides.Num());
  idLib::Printf("--------------------------------------\n");
}

void idDeclManagerLocal::ListDecls_f(const idCmdArgs*) {
  int totalDecls = 0;
  std::uint64_t structureBytes = 0;
  for (int typeIndex = 0; typeIndex < declManagerLocal.declTypes.Num(); ++typeIndex) {
    idDeclInfo* type = declManagerLocal.declTypes[typeIndex];
    const int count = type->resourceList.Num();
    std::uint64_t typeBytes = 0;
    for (int resourceIndex = 0; resourceIndex < count; ++resourceIndex) {
      if (const auto* decl = dynamic_cast<const idDecl*>(type->resourceList[resourceIndex]))
        typeBytes += decl->Size();
    }
    totalDecls += count;
    structureBytes += typeBytes;
    idLib::Printf("%4ik %4i %s\n", static_cast<int>(typeBytes >> 10), count, type->name);
  }

  int textBytes = 0;
  for (int i = 0; i < declManagerLocal.loadedFiles.Num(); ++i)
    textBytes += declManagerLocal.loadedFiles[i]->fileSize;
  idLib::Printf("%i total decls is %i decl files\n", totalDecls,
                declManagerLocal.loadedFiles.Num());
  idLib::Printf("%iKB in text, %iKB in structures\n", textBytes >> 10,
                static_cast<int>(structureBytes >> 10));
}

void idDeclManagerLocal::TouchDecl_f(const idCmdArgs* args) {
  if (args == nullptr || args->argc < 2) {
    idLib::Printf("usage: touchDecl <type> [name]\n");
    PrintDeclTypes();
    return;
  }
  idDeclInfo* type = declManagerLocal.GetDeclType(args->argv[1]);
  if (type == nullptr) {
    idLib::Printf("idDeclManagerLocal::TouchDecl_f Unknown decl type '%s'\n", args->argv[1]);
    idLib::Printf("usage: touchDecl <type> [name]\n");
    return;
  }
  if (args->argc == 3) {
    idDecl* decl = type->FindWithInheritance(args->argv[2], false, false);
    if (decl == nullptr) {
      idLib::Printf("idDeclManagerLocal::TouchDecl_f %s '%s' not found\n", args->argv[1],
                    args->argv[2]);
      return;
    }
    idLib::Printf("touched %s\n", decl->GetResourceTypeName());
    decl->LoadResource();
    return;
  }
  for (int i = 0; i < type->declSources.Num(); ++i) {
    if (idDecl* decl = type->FindWithInheritance(type->declSources[i]->name.str, false, false))
      decl->LoadResource();
  }
}

void idDeclManagerLocal::ReExportDecls_f(const idCmdArgs* args) {
  if (args == nullptr || (args->argc != 2 && args->argc != 3)) {
    idLib::Printf("usage: reexportDecls type <name>\n");
    PrintDeclTypes();
    return;
  }
  idDeclInfo* type = declManagerLocal.GetDeclType(args->argv[1]);
  if (type == nullptr) {
    idLib::Printf("idDeclManagerLocal::ReExportDecl_f Unknown decl type '%s'\n", args->argv[1]);
    return;
  }
  auto reExport = [](idDecl* decl) {
    if (decl == nullptr)
      return;
    idLib::Printf("Re-exporting decl %s\n", decl->GetResourceTypeName());
    if (decl->RebuildTextSource())
      decl->ReplaceSourceFileText(true);
  };
  if (args->argc == 3) {
    idDecl* decl = type->FindWithInheritance(args->argv[2], false, false);
    if (decl == nullptr)
      idLib::Printf("Could not find decl %s\n", args->argv[2]);
    else
      reExport(decl);
    return;
  }
  for (int i = 0; i < type->declSources.Num(); ++i)
    reExport(type->FindWithInheritance(type->declSources[i]->name.str, false, false));
}

void idDeclManagerLocal::MakeDeclTree_f(const idCmdArgs*) {
  if (fileSystem == nullptr)
    return;

  for (int fileIndex = 0; fileIndex < declManagerLocal.loadedFiles.Num(); ++fileIndex) {
    idDeclFile* file = declManagerLocal.loadedFiles[fileIndex];
    if (file == nullptr || idStr::Icmpn(file->fileName.str, "declTree", 8) == 0)
      continue;
    std::vector<std::uint8_t> source;
    if (!fileSystem->ReadFile(file->fileName.str, source))
      continue;
    std::string rewritten(reinterpret_cast<const char*>(source.data()), source.size());
    std::vector<DeclTreeReplacement> replacements;

    for (idDeclSource* declSource = file->firstDeclSourceInFile; declSource != nullptr;
         declSource = declSource->nextInFile) {
      idDeclInfo* owner = nullptr;
      for (int typeIndex = 0; typeIndex < declManagerLocal.declTypes.Num() && owner == nullptr;
           ++typeIndex) {
        idDeclInfo* candidate = declManagerLocal.declTypes[typeIndex];
        if (candidate->FindDeclSource(declSource->name.str) == declSource)
          owner = candidate;
      }
      if (owner == nullptr || declSource->sourceTextOffset < 0 || declSource->sourceTextLength <= 0 ||
          declSource->sourceTextOffset + declSource->sourceTextLength > static_cast<int>(source.size()))
        continue;

      idStr treePath;
      treePath.Format("declTree/%s/%s.decl", owner->name, declSource->name.str);
      const char* body = reinterpret_cast<const char*>(source.data()) + declSource->sourceTextOffset;
      if (!fileSystem->WriteFile(treePath.data, body,
                                 static_cast<std::size_t>(declSource->sourceTextLength))) {
        idLib::Warning("Couldn't open %s for write", treePath.data);
        continue;
      }
      DeclTreeReplacement replacement;
      replacement.offset = declSource->sourceTextOffset;
      replacement.length = declSource->sourceTextLength;
      replacement.includeText = "#include \"/" + std::string(treePath.data) + "\"\n";
      replacements.push_back(std::move(replacement));
    }

    std::sort(replacements.begin(), replacements.end(),
              [](const DeclTreeReplacement& a, const DeclTreeReplacement& b) {
                return a.offset > b.offset;
              });
    for (const DeclTreeReplacement& replacement : replacements)
      rewritten.replace(static_cast<std::size_t>(replacement.offset),
                        static_cast<std::size_t>(replacement.length), replacement.includeText);
    if (!replacements.empty() &&
        !fileSystem->WriteFile(file->fileName.str, rewritten.data(), rewritten.size()))
      idLib::Warning("Couldn't open %s for read/write", file->fileName.str);
  }
}

void idDeclManagerLocal::ReloadDecls_f(const idCmdArgs* args) {
  const bool force = args != nullptr && args->argc > 1 && idStr::Icmp(args->argv[1], "all") == 0;
  idLib::Printf(force ? "reloading all decl files:\n" : "reloading changed decl files:\n");
  declManagerLocal.Reload(force);
  // NEEDS_WORK(engine/framework): the retail build notifies common->Game()->OnReloadDecls().
}

void idDeclManagerLocal::MergeDecls_f(const idCmdArgs* args) {
  const char* type = args != nullptr && args->argc > 1 ? args->argv[1] : "";
  const char* path = args != nullptr && args->argc > 2 ? args->argv[2] : "";
  declManagerLocal.MergeDecls(idStr(type), idStr(path));
}
