#pragma once

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vecx.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct idTypeInfoSettings {
  idTypeInfoSettings();
  bool writeDefaults{}, writeGameState{}, skipUnknown{};
  int precision{6};
};

class idTypeInfoFile {
public:
  explicit idTypeInfoFile(idFile* file = nullptr, const idTypeInfoSettings& settings = {});
  ~idTypeInfoFile() = default;
  void SetFile(idFile* file);
  void WriteOpeningBrace();
  bool ReadOpeningBrace();
  bool CheckOpeningBrace();
  void WriteClosingBrace();
  bool ReadClosingBrace();
  bool CheckClosingBrace();
  bool PeekClosingBrace();
  void WriteSkipObject();
  bool CheckSkipObject();
  void WriteValueString(const char*);
  bool ReadValueString(idStr&);
  void WriteBool(bool);
  bool ReadBool(bool&);
  void WriteChar(char);
  bool ReadChar(char&);
  void WriteUnsignedChar(unsigned char);
  bool ReadUnsignedChar(unsigned char&);
  void WriteWChar(wchar_t);
  bool ReadWChar(wchar_t&);
  void WriteShort(short);
  bool ReadShort(short&);
  void WriteUnsignedShort(unsigned short);
  bool ReadUnsignedShort(unsigned short&);
  void WriteInt(int);
  bool ReadInt(int&);
  void WriteUnsignedInt(unsigned);
  bool ReadUnsignedInt(unsigned&);
  void WriteLong(long);
  bool ReadLong(long&);
  void WriteUnsignedLong(unsigned long);
  bool ReadUnsignedLong(unsigned long&);
  void WriteFloat(float);
  bool ReadFloat(float&);
  void WriteDouble(double);
  bool ReadDouble(double&);
  void WriteStr(const idStr&);
  bool ReadStr(idStr&);
  void WriteVec3(const idVec3&);
  bool ReadVec3(idVec3&);
  void WriteVecX(const idVecX&);
  bool ReadVecX(idVecX&);
  void WriteMatX(const idMatX&);
  bool ReadMatX(idMatX&);
  void WriteAngles(const idAngles&);
  bool ReadAngles(idAngles&);
  void WriteQuat(const idQuat&);
  bool ReadQuat(idQuat&);
  void WriteColor(const idVec4&);
  bool ReadColor(idVec4&);
  void WriteNullPointer();
  bool ReadNullPointer();
  bool CheckNullPointer();
  void WriteUnknown(const char*);
  bool ReadUnknown(idStr&);
  bool CheckUnknown();
  void WriteComment(const char*);
  bool ReadComment(idStr&);
  void WriteType(const char*, const char*);
  bool ReadType(idStr&, idStr&);
  bool CheckType(const char*, const char*);
  bool ExpectType(const char*, const char*);
  void WriteArrayElementType(const char*, int);
  bool ReadArrayElementType(idStr&, int&);
  bool CheckArrayElementType(const char*, int);
  bool ExpectArrayElementType(const char*, int);
  void WriteMemory(const void*, std::size_t);
  bool ReadMemory(void*, std::size_t);
  void WriteMemoryFile(idFile_Memory&);
  bool ReadMemoryFile(idFile_Memory&);
  const char* GetTypeInfoString() const { return scratch.data; }
  void Error(const char*, ...);
  void Warning(const char*, ...);
  bool HadError() const { return hadError; }
  void OutputTabs();

private:
  bool Next(idStr&);
  idFile* file{};
  idTypeInfoSettings settings;
  idStr input;
  std::size_t cursor{};
  int tabs{};
  bool hadError{};
  idStr scratch;
};

struct idTypeInfoNode {
  idStr name, type, value;
  idTypeInfoNode* parent{};
  std::vector<std::unique_ptr<idTypeInfoNode>> children;
};
class idTypeInfoPath {
public:
  idList<idStr> elements;
};
class idTypeInfoTree {
public:
  idTypeInfoTree();
  ~idTypeInfoTree() = default;
  void Clear();
  idTypeInfoNode* CreatePath(const char*);
  const idTypeInfoNode* FindPath(const char*) const;
  idTypeInfoNode* FindPath(const char*);
  bool Set(const char*, const char*, const char* type = "string");
  const char* Get(const char*, const char* fallback = "") const;
  bool GetBool(const char*, bool = false) const;
  idVec3 GetVec3(const char* path, const idVec3& fallback = idVec3{}) const;
  void SetVec3(const char*, const idVec3&);
  idMat3 GetMat3(const char* path, const idMat3& fallback = mat3_identity) const;
  void SetMat3(const char*, const idMat3&);
  bool GetStrType(const char*, idStr&) const;
  bool SetStrType(const char*, const char*);
  bool Parse(const char*);
  bool ParseOverwrite(const char*);
  void Write(idFile*) const;
  void Diff(const idTypeInfoTree&, idTypeInfoTree&) const;
  void ForceRootDifferent();
  idStr GetPath(const idTypeInfoNode*) const;
  void GetTypes(idList<idStr>&) const;
  void ExtendListsWithDefaultsBasedOn(const idTypeInfoTree&);
  idTypeInfoNode root;

private:
  void WriteNode(const idTypeInfoNode*, const idStr&, idFile*) const;
  void DiffNode(const idTypeInfoNode*, const idTypeInfoTree&, idTypeInfoTree&, const idStr&) const;
  void CollectTypes(const idTypeInfoNode*, idList<idStr>&) const;
};

struct idEnumValueInfo {
  idStr name;
  int value;
};
struct idEnumInfo {
  idStr name;
  int flags{};
  std::vector<idEnumValueInfo> values;
};
struct idClassVariableInfo {
  idStr name, type;
  std::size_t offset{}, size{};
};
struct idClassInfo {
  idStr name, super;
  std::size_t size{};
  std::vector<idClassVariableInfo> variables;
};
class idTypeInfoTools {
public:
  idTypeInfoTools() = default;
  ~idTypeInfoTools() = default;
  void Init();
  void Shutdown();
  void AddEnum(const idEnumInfo&);
  void AddClass(const idClassInfo&);
  int FindEnumIndex(const char*) const;
  const idEnumInfo* FindEnumInfo(const char*) const;
  const idEnumValueInfo* FindEnumValueInfo(const idEnumInfo&, int) const;
  const idEnumValueInfo* FindEnumValueInfo(const idEnumInfo&, const char*,
                                           bool allowScopedName = true) const;
  const idEnumValueInfo* FindEnumValueInfo(const char* enumTypeName, int value,
                                           bool allowScopedName) const;
  const char* FindEnumValueName(const idEnumInfo&, int) const;
  const char* GetEnumName(const char* enumTypeName, int enumValue,
                          const char* defaultValue) const;
  bool GetEnumBitFlags(int value,const char* enumTypeName,idStr& result,
                       const char* separator) const;
  int FindClassIndex(const char*) const;
  const idClassInfo* FindClassInfo(const char*) const;
  const idClassVariableInfo* FindClassVariableInfo(const idClassInfo&, const char*) const;
  std::size_t SizeForType(const char*) const;
  bool IsSubclassOf(const char*, const char*) const;
  bool WriteObject(idTypeInfoFile&, const void*, const char*) const;
  bool ReadObject(idTypeInfoFile&, void*, const char*) const;
  void ClearWarnings();
  void AddWarning(const char*);

private:
  std::vector<idEnumInfo> enums;
  std::vector<idClassInfo> classes;
  std::vector<idStr> warnings;
};

class idTypeInfoObject {
public:
  idTypeInfoObject(const void* object = nullptr, const idClassInfo* type = nullptr)
      : object(object), type(type) {}
  idStr GetValueText(const char* member) const;
  const char* GetStrPtrType(const char* member) const;

private:
  const void* object;
  const idClassInfo* type;
};
