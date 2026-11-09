#include "declentitydef.h"

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"

#include <cstring>

namespace {
bool SameColor(const idColor& left, const idColor& right) {
  return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

bool SameVec3(const idVec3& left, const idVec3& right) {
  return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool ParseEditorVector(idParser& source, int count, float* values) {
  if (!source.ExpectTokenString("("))
    return false;
  for (int i = 0; i < count; ++i) {
    bool error = false;
    values[i] = source.ParseFloat(&error);
    if (error)
      return false;
    if (i + 1 < count)
      source.CheckTokenString(",");
  }
  return source.ExpectTokenString(")");
}
} // namespace

idEntityDefEditorVars::idEntityDefEditorVars()
    : color{1.0f, 1.0f, 1.0f, 1.0f}, size{}, originOffset{}, animFrame(0),
      rotatable(false), showOrientation(false), showFixedBox(false),
      showInRenderMode(false), placeable(true), instantiable(true), noClip(false) {}

bool idEntityDefEditorVars::Parse(idParser& source) {
  if (!source.CheckTokenString("editorVars"))
    return false;
  if (idLib::production > PROD_DEVELOPMENT) {
    source.SkipBracedSection(true);
    return true;
  }
  if (!source.ExpectTokenString("{"))
    return true;

  bool deprecated = false;
  idToken key;
  while (!source.CheckTokenString("}")) {
    if (!source.ExpectAnyToken(key) || !source.ExpectTokenString("="))
      break;
    if (key.Icmp("rotatable") == 0)
      rotatable = source.ParseBool();
    else if (key.Icmp("showOrientation") == 0)
      showOrientation = source.ParseBool();
    else if (key.Icmp("noClip") == 0)
      noClip = source.ParseBool();
    else if (key.Icmp("showFixedBox") == 0)
      showFixedBox = source.ParseBool();
    else if (key.Icmp("showInRenderMode") == 0)
      showInRenderMode = source.ParseBool();
    else if (key.Icmp("placeable") == 0)
      placeable = source.ParseBool();
    else if (key.Icmp("instantiable") == 0)
      instantiable = source.ParseBool();
    else if (key.Icmp("color") == 0)
      ParseEditorVector(source, 4, &color.r);
    else if (key.Icmp("size") == 0)
      ParseEditorVector(source, 3, &size.x);
    else if (key.Icmp("originOffset") == 0)
      ParseEditorVector(source, 3, &originOffset.x);
    else if (key.Icmp("material") == 0) {
      idToken value;
      if (source.ExpectTokenType(TT_STRING, 0, value))
        material.Set(value.data);
    } else if (key.Icmp("animName") == 0) {
      idToken value;
      if (source.ExpectTokenType(TT_STRING, 0, value))
        animName.Set(value.data);
    } else if (key.Icmp("animFrame") == 0)
      animFrame = source.ParseInt();
    else if (key.Icmp("deprecated") == 0)
      deprecated = source.ParseBool();
    else if (key.Icmp("editorName") == 0) {
      idToken value;
      if (source.ExpectTokenType(TT_STRING, 0, value))
        editorName = value;
    }
    source.SkipUntilString(";");
  }
  if (deprecated)
    placeable = false;
  return true;
}

bool idEntityDefEditorVars::Write(const idDeclEntityDef* parentDef, idFile* file) const {
  if (idLib::production > PROD_DEVELOPMENT || file == nullptr)
    return true;
  const idEntityDefEditorVars defaults;
  const idEntityDefEditorVars& base = parentDef != nullptr ? parentDef->editorVars : defaults;
  file->WriteFloatString("\teditorVars {\n");
  if (parentDef == nullptr || rotatable != base.rotatable)
    file->WriteFloatString("\t\trotatable = %s;\n", rotatable ? "true" : "false");
  if (parentDef == nullptr || showOrientation != base.showOrientation)
    file->WriteFloatString("\t\tshowOrientation = %s;\n", showOrientation ? "true" : "false");
  if (parentDef == nullptr || showFixedBox != base.showFixedBox)
    file->WriteFloatString("\t\tshowFixedBox = %s;\n", showFixedBox ? "true" : "false");
  if (parentDef == nullptr || showInRenderMode != base.showInRenderMode)
    file->WriteFloatString("\t\tshowInRenderMode = %s;\n", showInRenderMode ? "true" : "false");
  file->WriteFloatString("\t\tplaceable = %s;\n", placeable ? "true" : "false");
  if (parentDef == nullptr || !SameColor(color, base.color))
    file->WriteFloatString("\t\tcolor = ( %f, %f, %f, %f );\n", color.r, color.g, color.b,
                           color.a);
  if (parentDef == nullptr || !SameVec3(size, base.size))
    file->WriteFloatString("\t\tsize = ( %f, %f, %f );\n", size.x, size.y, size.z);
  if (parentDef == nullptr || !SameVec3(originOffset, base.originOffset))
    file->WriteFloatString("\t\toriginOffset = ( %f, %f, %f );\n", originOffset.x,
                           originOffset.y, originOffset.z);
  if (parentDef == nullptr || material.str != base.material.str)
    file->WriteFloatString("\t\tmaterial = \"%s\";\n", material.str);
  if (parentDef == nullptr || animName.str != base.animName.str)
    file->WriteFloatString("\t\tanimName = \"%s\";\n", animName.str);
  if (parentDef == nullptr || animFrame != base.animFrame)
    file->WriteFloatString("\t\tanimFrame = %d;\n", animFrame);
  if (parentDef == nullptr || editorName != base.editorName)
    file->WriteFloatString("\t\teditorName = \"%s\";\n", editorName.data);
  file->WriteFloatString("\t}\n");
  return true;
}

idDeclEntityDef::idDeclEntityDef()
    : stateOffset(0), stateLineNumberOffset(0), expandInheritance(true), listedParent(this) {
  resourceListPtr = &resourceList;
}

idDeclEntityDef::~idDeclEntityDef() = default;

const char* idDeclEntityDef::DefaultDefinition() const {
  return "{\n\tinherit = \"\";\n\tclass = \"idEntity\";\n\tedit = {\n\t}\n}\n";
}

void idDeclEntityDef::FreeData() {
  inherit.Set("");
  className.Set("");
  stateOffset = 0;
  stateLineNumberOffset = 0;
}

idDeclInfo* idDeclEntityDef::GetDeclInfo() const { return &resourceList; }

const char* idDeclEntityDef::GetEntityState() const {
  return textSource != nullptr && stateOffset >= 0 && stateOffset <= textLength
             ? textSource + stateOffset
             : "";
}

int idDeclEntityDef::GetEntityStateLength() const {
  return stateOffset <= textLength ? textLength - stateOffset : 0;
}

int idDeclEntityDef::GetEntityStateLineNumber() const {
  return GetLineNum() + stateLineNumberOffset;
}

const char* idDeclEntityDef::GetEntityStateWithInheritance() const {
  return entityStateWithInheritanceText.data;
}

int idDeclEntityDef::GetEntityStateWithInheritanceLength() const {
  return entityStateWithInheritanceText.Length();
}

const idDeclEntityDef* idDeclEntityDef::GetListedParent() const { return listedParent; }

bool idDeclEntityDef::IsParent(const idDeclEntityDef* parentDef) const {
  if (parentDef == this)
    return true;
  return listedParent != nullptr && listedParent != this && listedParent->IsParent(parentDef);
}

void idDeclEntityDef::SetInherit(const char* value) { inherit.Set(value); }
void idDeclEntityDef::SetClass(const char* value) { className.Set(value); }
const char* idDeclEntityDef::GetClass() const { return className.str; }

void idDeclEntityDef::BuildText(const char* inheritName, const char* entityClass) {
  idFile_String file;
  file.WriteFloatString("{\n");
  file.WriteFloatString("\tinherit = \"%s\";\n", inheritName);
  file.WriteFloatString("\tclass = \"%s\";\n", entityClass);
  file.WriteFloatString("\tedit = {\n\t}\n}");
  const idStr built = file.ToString();
  SetText(built.data);
  ReParse(false);
}

void idDeclEntityDef::GetTextWithInheritance(idStr& result, bool stripWhiteSpace) const {
  idFile_String file;
  file.WriteFloatString("{\n");
  if (inherit.str[0] != '\0')
    file.WriteFloatString("inherit = \"%s\";\n", inherit.str);
  file.WriteFloatString("class = \"%s\";\n", className.str);
  file.WriteFloatString("expandInheritance = false;\n");
  file.Write(entityStateWithInheritanceText.data,
             static_cast<std::size_t>(entityStateWithInheritanceText.Length()));
  result = file.ToString();
  result.Replace("\n", "\n\t");
  result += "\n}";
  if (stripWhiteSpace) {
    result.Replace("\t", " ");
    result.Replace("\r", "");
    result.Replace("\n", "");
    result.Replace("   ", " ");
    result.Replace("  ", " ");
  }
}

void idDeclEntityDef::SetupEntityStateWithInheritance_r(idTypeInfoTree& tree) {
  if (inherit.str[0] != '\0') {
    auto* parent = dynamic_cast<idDeclEntityDef*>(
        resourceList.FindWithInheritance(inherit.str, false, false));
    if (parent != nullptr && parent != this) {
      parent->SetupEntityStateWithInheritance_r(tree);
      listedParent = parent->listedParent != nullptr ? parent->listedParent : parent;
    } else {
      idLib::Warning("Unknown entityDef '%s' inherited by '%s'", inherit.str, name.str);
    }
  }
  tree.ParseOverwrite(GetEntityState());
}

void idDeclEntityDef::SetupEntityStateWithInheritance() {
  listedParent = this;
  if (expandInheritance) {
    idTypeInfoTree tree;
    SetupEntityStateWithInheritance_r(tree);
    idFile_String file;
    tree.Write(&file);
    entityStateWithInheritanceText = file.ToString();
  } else {
    entityStateWithInheritanceText.CopyRange(GetEntityState(), 0, GetEntityStateLength());
    if (inherit.str[0] != '\0')
      listedParent = dynamic_cast<const idDeclEntityDef*>(
          resourceList.FindWithInheritance(inherit.str, false, false));
  }
}

void idDeclEntityDef::Parse(idParser& source) {
  if (source.PeekTokenString("}"))
    return;
  stateOffset = source.GetCurrentSourceOffset();
  inherit.Set("");
  idToken token;
  if (source.CheckTokenString("inherit")) {
    source.ExpectTokenString("=");
    if (source.ExpectTokenType(TT_STRING, 0, token))
      inherit.Set(token.data);
    source.ExpectTokenString(";");
    stateOffset = source.GetCurrentSourceOffset();
  }
  className.Set("");
  if (source.CheckTokenString("class")) {
    source.ExpectTokenString("=");
    if (source.ExpectTokenType(TT_STRING, 0, token))
      className.Set(token.data);
    source.ExpectTokenString(";");
    stateOffset = source.GetCurrentSourceOffset();
  }

  if (inherit.str[0] != '\0') {
    auto* parent = dynamic_cast<idDeclEntityDef*>(
        resourceList.FindWithInheritance(inherit.str, false, true));
    if (parent != nullptr) {
      if (declManager != nullptr)
        declManager->AddDependency(*this, parent);
      const bool ownPlaceable = editorVars.placeable;
      editorVars = parent->editorVars;
      editorVars.placeable = ownPlaceable;
      if (className.str[0] == '\0')
        className.Set(parent->GetClass());
      else if (declTypeInfoTools != nullptr &&
               !declTypeInfoTools->IsSubclassOf(className.str, parent->GetClass()))
        source.Error("Class %s does not derive from %s", className.str, parent->GetClass());
    } else {
      source.Warning("Unknown entityDef '%s' inherited by '%s'", inherit.str, name.str);
      if (className.str[0] == '\0')
        className.Set("idEntity");
    }
  }
  if (className.str[0] == '\0')
    source.Error("No class specified");
  if (source.CheckTokenString("expandInheritance")) {
    source.ExpectTokenString("=");
    expandInheritance = source.ParseBool();
    source.ExpectTokenString(";");
    stateOffset = source.GetCurrentSourceOffset();
  }
  if (editorVars.Parse(source))
    stateOffset = source.GetCurrentSourceOffset();
  stateLineNumberOffset = source.GetCurrentSourceLine() - GetLineNum();
  SetupEntityStateWithInheritance();
}

void idDeclEntityDef::SetEntityState(const char* state, bool writeEditorVars) {
  const char* stateText = state != nullptr ? state : "";
  int indent = 0;
  while (stateText[indent] == '\t')
    ++indent;
  idFile_String file;
  file.WriteFloatString("{\n");
  if (inherit.str[0] != '\0') {
    for (int i = 0; i < indent; ++i)
      file.WriteFloatString("\t");
    file.WriteFloatString("inherit = \"%s\";\n", inherit.str);
  }
  for (int i = 0; i < indent; ++i)
    file.WriteFloatString("\t");
  file.WriteFloatString("class = \"%s\";\n", className.str);
  if (writeEditorVars) {
    const auto* parent = dynamic_cast<const idDeclEntityDef*>(
        resourceList.FindWithInheritance(inherit.str, false, true));
    editorVars.Write(parent, &file);
  }
  file.Write(stateText, std::strlen(stateText));
  file.WriteFloatString("\n");
  for (int i = 1; i < indent; ++i)
    file.WriteFloatString("\t");
  file.WriteFloatString("}");
  const idStr built = file.ToString();
  SetText(built.data);
  ReParse(false);
}

void idDeclEntityDef::SetEntityState(const idTypeInfoTree* tree, bool, bool writeEditorVars) {
  if (tree == nullptr)
    return;
  idFile_String file;
  tree->Write(&file);
  const idStr state = file.ToString();
  SetEntityState(state.data, writeEditorVars);
}

void idDeclEntityDef::SetInheritedClass(bool writeEditorVars) {
  if (inherit.str[0] == '\0')
    return;
  auto* parent = dynamic_cast<idDeclEntityDef*>(
      resourceList.FindWithInheritance(inherit.str, false, true));
  if (parent == nullptr || idStr::Cmp(className.str, parent->GetClass()) == 0)
    return;
  className.Set(parent->GetClass());
  idStr state(GetEntityState());
  SetEntityState(state.data, writeEditorVars);
}

bool idDeclEntityDef::RebuildTextSource() {
  return false;
}
