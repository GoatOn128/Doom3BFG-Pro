#pragma once

#include "decl.h"

class idCmdArgs;

struct declFileDependency_t {
  idAtomicString fileName;
  std::uint32_t timestamp = 0;
  bool dirty = false;
};

class idDeclManagerLocal final : public idDeclManager {
public:
  idDeclManagerLocal();
  ~idDeclManagerLocal() override;

  void Init() override;
  void Shutdown() override;
  void Reload(bool force) override;
  void MergeDecls(const idStr& mapName, const idStr& mapFile) override;
  void MarkStatic() override;
  void FreeDynamic() override;
  void RegisterDeclFolder(const char* folder, const char* extension,
                          idDeclInfo* defaultType) override;
  int GetNumDeclTypes() const override;
  idDeclInfo* GetDeclType(const char* typeName) const override;
  idDeclInfo* GetDeclType(int index) const override;
  idDeclInfo* GetDeclTypeFromClassname(const char* className) const override;
  idDecl* CreateNewDecl(idDeclInfo* type, const char* name, const char* fileName,
                        const char* sourceText) override;
  void AddDependency(idDecl& decl, const char* fileName) override;
  void AddDependency(idDecl& decl, const idDecl* dependency) override;
  void GetDeclFilesDependentOnDeclFile(const idDeclFile& source,
                                        idList<idDeclFile*>& result) const override;
  void ArgCompletion_DeclTypeName(idAutoComplete* completion) override;
  void ArgCompletion_DeclName(idAutoComplete* completion, idDeclInfo* type) override;
  void PrintMultiplayerDeclOverrides() override;

  void RegisterDeclType(idDeclInfo* type);
  void GetDeclTextHeader(idDeclInfo* type, const char* canonicalName, idStr& header) const;
  void GetLogDeclsList(idList<idStr, 44>& declList) const;

  static void ListDecls_f(const idCmdArgs* args);
  static void TouchDecl_f(const idCmdArgs* args);
  static void ReExportDecls_f(const idCmdArgs* args);
  static void MakeDeclTree_f(const idCmdArgs* args);
  static void ReloadDecls_f(const idCmdArgs* args);
  static void MergeDecls_f(const idCmdArgs* args);

  idList<idDeclInfo*, 46> declTypes;
  idList<idDeclFile*, 46> loadedFiles;
  int loadedFilesDynamicStartIndex;
  idList<declFileDependency_t, 46> declFileDependencies;
  int dependenciesDynamicStartIndex;
  int checksum;
  idList<idStr> multiplayerDeclOverrides;
  idList<idStr> registeredDeclLogTypes;
};

extern idDeclManagerLocal declManagerLocal;

static_assert(sizeof(declFileDependency_t) == 16, "declFileDependency_t ABI drift");
