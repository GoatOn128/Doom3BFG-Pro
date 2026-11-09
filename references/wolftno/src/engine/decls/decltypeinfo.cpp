#include "decl.h"

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"

namespace {
idTypeInfoTools defaultDeclTypeInfoTools;

bool CopyTypeInfoState(idDeclTypeInfo& destination, const idDeclTypeInfo& source) {
  idDeclInfo* info = destination.GetDeclInfo();
  if (declTypeInfoTools == nullptr || info == nullptr || info->classname == nullptr)
    return false;
  idFile_String memory("decl-parent-state");
  idTypeInfoFile writer(&memory);
  if (!declTypeInfoTools->WriteObject(writer, &source, info->classname))
    return false;
  memory.MakeReadOnly();
  idTypeInfoFile reader(&memory);
  return declTypeInfoTools->ReadObject(reader, &destination, info->classname);
}
} // namespace

idTypeInfoTools* declTypeInfoTools = &defaultDeclTypeInfoTools;
idDeclInfoTemplate<idDeclTypeInfo> idDeclTypeInfo::resourceList("typeinfo", "idDeclTypeInfo", "tdef", false);

idDeclTypeInfo::idDeclTypeInfo() : parent(nullptr), hasBeenParsedAtLeastOnce(false) {
  resourceListPtr = &resourceList;
}

idDeclTypeInfo::~idDeclTypeInfo() = default;

const char* idDeclTypeInfo::DefaultDefinition() const {
  return "{ }\n";
}

idDeclInfo* idDeclTypeInfo::GetDeclInfo() const {
  return &resourceList;
}

void idDeclTypeInfo::Parse(idParser& source) {
  parent = nullptr;
  idToken token;
  if (source.PeekTokenString("inherit")) {
    source.ExpectTokenString("inherit");
    source.ExpectTokenString("=");
    source.ExpectTokenType(TT_STRING, 0, token);
    if (!token.IsEmpty()) {
      idDeclInfo* info = GetDeclInfo();
      parent = info != nullptr ? dynamic_cast<idDeclTypeInfo*>(info->FindResource(token.data)) : nullptr;
      if (parent != nullptr && declManager != nullptr)
        declManager->AddDependency(*this, parent);
    }
    source.ExpectTokenString(";");
  }

  if (parent != nullptr || hasBeenParsedAtLeastOnce) {
    if (parent != nullptr) {
      if (parent->insideRecursiveLoad) {
        idLib::Error("Resource %s:%s is being used for parent inheritance for %s:%s before it is fully defined.",
                     parent->GetResourceTypeName(), parent->name.str, GetResourceTypeName(), name.str);
      }
      CopyTypeInfoState(*this, *parent);
      parent = dynamic_cast<idDeclTypeInfo*>(GetDeclInfo()->FindResource(token.data, false));
    } else {
      idDecl* emptyResource = GetDeclInfo()->AllocDecl();
      if (auto* empty = dynamic_cast<idDeclTypeInfo*>(emptyResource))
        CopyTypeInfoState(*this, *empty);
      delete emptyResource;
    }
  }

  idStr edit;
  source.ParseBracedSectionExact(edit, false);
  if (declTypeInfoTools != nullptr) {
    idFile_String memory("decl-edit-state");
    memory.Write(edit.data, static_cast<std::size_t>(edit.Length()));
    memory.MakeReadOnly();
    idTypeInfoFile file(&memory);
    idDeclInfo* info = GetDeclInfo();
    if (info != nullptr && info->classname != nullptr)
      declTypeInfoTools->ReadObject(file, this, info->classname);
  }
  hasBeenParsedAtLeastOnce = true;
}

void idDeclTypeInfo::SetState(idDeclTypeInfo* empty) {
  idDeclTypeInfo* basis = parent != nullptr ? parent : empty;
  idDeclInfo* info = GetDeclInfo();
  if (declTypeInfoTools == nullptr || info == nullptr || info->classname == nullptr)
    return;

  idFile_String current("decl-current-state");
  idTypeInfoFile currentWriter(&current);
  if (!declTypeInfoTools->WriteObject(currentWriter, this, info->classname))
    return;

  idStr text("{\n");
  if (parent != nullptr) {
    idStr inherit;
    inherit.Format("\tinherit = \"%s\";\n", parent->name.str);
    text.Append(inherit);
  }

  if (basis == nullptr) {
    text.Append(current.ToString());
  } else {
    idFile_String basisMemory("decl-basis-state");
    idTypeInfoFile basisWriter(&basisMemory);
    declTypeInfoTools->WriteObject(basisWriter, basis, info->classname);
    idTypeInfoTree currentTree;
    idTypeInfoTree basisTree;
    idTypeInfoTree difference;
    currentTree.Parse(current.ToString().data);
    basisTree.Parse(basisMemory.ToString().data);
    currentTree.Diff(basisTree, difference);
    idFile_String diffText("decl-diff-state");
    difference.Write(&diffText);
    text.Append(diffText.ToString());
  }
  text.Append("\n}");
  SetText(text.data);
}

bool idDeclTypeInfo::RebuildTextSource() {
  idDeclTypeInfo* basis = parent;
  bool ownsBasis = false;
  if (basis == nullptr) {
    basis = dynamic_cast<idDeclTypeInfo*>(GetDeclInfo()->AllocDecl());
    ownsBasis = true;
  }
  SetState(basis);
  if (ownsBasis)
    delete basis;
  return true;
}
