#include "declskins.h"

namespace {
const idMaterial* FindMaterial(const char* name) {
  idDeclInfo* materialType = declManager != nullptr ? declManager->GetDeclType("material") : nullptr;
  return materialType != nullptr
             ? reinterpret_cast<const idMaterial*>(
                   materialType->FindWithInheritance(name, true, true))
             : nullptr;
}

const char* MaterialName(const idMaterial* material) {
  return material != nullptr ? reinterpret_cast<const idResource*>(material)->name.str : "";
}
} // namespace

const char* idDeclSkins::DefaultDefinition() const {
  return "{\n}\n";
}

const idMaterial* idSkin::Remap(const idMaterial* material) const {
  for (int i = 0; i < mappings.Num(); ++i) {
    if (mappings[i].from == material)
      return mappings[i].to;
  }
  return material;
}

int idDeclSkins::IndexForSkin(const char* skinName) const {
  for (int i = 0; i < skins.Num(); ++i) {
    if (idStr::Icmp(skins[i].name.str, skinName) == 0)
      return i;
  }
  return -1;
}

bool idDeclSkins::RebuildTextSource() {
  idStr source("{\n");
  for (int i = 0; i < skins.Num(); ++i) {
    idStr line;
    line.Format("\t%s {\n", skins[i].name.str);
    source.Append(line);
    for (int j = 0; j < skins[i].mappings.Num(); ++j) {
      const idMaterial* from = skins[i].mappings[j].from;
      const idMaterial* to = skins[i].mappings[j].to;
      if (from == nullptr || to == nullptr)
        continue;
      line.Format("\t\t\"%s\" \"%s\"\n", MaterialName(from), MaterialName(to));
      source.Append(line);
    }
    source.Append("\t}\n");
  }
  source.Append("}\n");
  SetText(source.data, source.Length());
  return true;
}

void idDeclSkins::FreeData() {
  if (skins.listStatic == 0 || skins.listStatic == 2) {
    delete[] skins.list;
    skins.list = nullptr;
    skins.size = 0;
  }
  skins.num = 0;
}

void idDeclSkins::Parse(idParser& source) {
  idToken token;
  if (source.CheckTokenString("}"))
    return;

  for (;;) {
    idSkin skin;
    if (!source.ExpectAnyToken(token))
      return;
    skin.name.Set(token.data);
    if (!source.ExpectTokenString("{"))
      return;

    while (!source.CheckTokenString("}")) {
      if (!source.ReadToken(token)) {
        source.Warning("Unexpected end of file");
        return;
      }
      const idMaterial* from = FindMaterial(token.data);
      if (!source.ReadToken(token)) {
        source.Warning("Unexpected end of file");
        return;
      }
      const idMaterial* to = FindMaterial(token.data);
      skin.mappings.Append({from, to});
    }

    if (skin.mappings.Num() != 0)
      skins.Append(skin);
    if (source.CheckTokenString("}"))
      return;
  }
}
