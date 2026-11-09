#pragma once

#include "decl.h"
#include "shared/idlib/color.h"

class idFile;

struct idEntityDefEditorVars {
  idEntityDefEditorVars();
  bool Parse(idParser& source);
  bool Write(const class idDeclEntityDef* parentDef, idFile* file) const;

  idStr editorName;
  idColor color;
  idVec3 size;
  idVec3 originOffset;
  idAtomicString material;
  idAtomicString animName;
  int animFrame;
  unsigned char rotatable : 1;
  unsigned char showOrientation : 1;
  unsigned char showFixedBox : 1;
  unsigned char showInRenderMode : 1;
  unsigned char placeable : 1;
  unsigned char instantiable : 1;
  unsigned char noClip : 1;
};

class idDeclEntityDef final : public idDecl {
public:
  idDeclEntityDef();
  ~idDeclEntityDef() override;

  const char* DefaultDefinition() const override;
  void FreeData() override;
  void Parse(idParser& source) override;
  bool RebuildTextSource() override;
  idDeclInfo* GetDeclInfo() const override;

  const char* GetEntityState() const;
  int GetEntityStateLength() const;
  int GetEntityStateLineNumber() const;
  const char* GetEntityStateWithInheritance() const;
  int GetEntityStateWithInheritanceLength() const;
  const idDeclEntityDef* GetListedParent() const;
  bool IsParent(const idDeclEntityDef* parentDef) const;
  const char* GetInherit() const { return inherit.str; }
  const char* GetClass() const;
  void SetInherit(const char* value);
  void SetClass(const char* value);
  void BuildText(const char* inheritName, const char* entityClass);
  void GetTextWithInheritance(idStr& result, bool stripWhiteSpace) const;
  void SetInheritedClass(bool writeEditorVars);
  void SetEntityState(const char* state, bool writeEditorVars);
  void SetEntityState(const idTypeInfoTree* tree, bool onlyDiff, bool writeEditorVars);
  void SetupEntityStateWithInheritance();

  idAtomicString inherit;
  idAtomicString className;
  idEntityDefEditorVars editorVars;
  int stateOffset;
  int stateLineNumberOffset;
  bool expandInheritance;
  idStr entityStateWithInheritanceText;
  const idDeclEntityDef* listedParent;

  static idDeclInfoTemplate<idDeclEntityDef> resourceList;

private:
  void SetupEntityStateWithInheritance_r(idTypeInfoTree& tree);
};

static_assert(sizeof(idEntityDefEditorVars) == 104,
              "idEntityDefEditorVars ABI drift");
static_assert(sizeof(idDeclEntityDef) == 264, "idDeclEntityDef ABI drift");
