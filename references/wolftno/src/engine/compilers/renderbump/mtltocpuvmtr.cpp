#include "engine/compilers/renderbump/mtltocpuvmtr.h"

#include "engine/renderer/declmaterial.h"
#include "shared/idlib/text/lexer.h"
#include "shared/idlib/text/token.h"

#include <cstdio>
#include <cstring>

idParsedMtl::idParsedMtl()
    : Ns(0.0f), Ni(0.0f), d(0.0f), Tr(0.0f), sharpness(0.0f), Tf{0.0f, 0.0f, 0.0f},
      illum(2), Ka{0.0f, 0.0f, 0.0f}, Kd{0.5f, 0.5f, 0.5f}, Ks{0.0f, 0.0f, 0.0f},
      Ke{0.0f, 0.0f, 0.0f}, map_aat(false) {}

idParsedMtl::~idParsedMtl() = default;

idParsedMtl& idParsedMtl::operator=(const idParsedMtl& other) {
  if (this == &other)
    return *this;
  mtlName = other.mtlName;
  Ns = other.Ns;
  Ni = other.Ni;
  d = other.d;
  Tr = other.Tr;
  sharpness = other.sharpness;
  Tf = other.Tf;
  illum = other.illum;
  Ka = other.Ka;
  Kd = other.Kd;
  Ks = other.Ks;
  Ke = other.Ke;
  map_Ka = other.map_Ka;
  map_Kd = other.map_Kd;
  map_Ks = other.map_Ks;
  map_Ke = other.map_Ke;
  map_bump = other.map_bump;
  map_opacity = other.map_opacity;
  map_d = other.map_d;
  map_Ns = other.map_Ns;
  map_refl = other.map_refl;
  decal = other.decal;
  disp = other.disp;
  map_aat = other.map_aat;
  return *this;
}

idStr ExtractFinalFilename(const char* str) {
  int start = static_cast<int>(std::strlen(str)) - 1;
  for (int i = start; i >= 0; --i) {
    if (static_cast<unsigned char>(str[i]) <= 32)
      break;
    --start;
  }
  return idStr(str + start + 1);
}

idStr StripBaseDirFromFilename(const char* str) {
  const char* base = std::strstr(str, "base/");
  if (base == nullptr)
    base = std::strstr(str, "base\\");
  return idStr(base != nullptr ? base + 5 : str);
}

idStr LocalFileNameToGameFileName(const char* localFileName, const char* currentDirectory) {
  idStr canonicalFilename(localFileName);
  canonicalFilename.MakeNameCanonical();
  if (canonicalFilename.Find(":", true, 0, -1) != -1 || canonicalFilename[0] == '/')
    return StripBaseDirFromFilename(canonicalFilename.data);

  idStr result(currentDirectory);
  result.StripTrailing('/');
  result.StripTrailing('\\');
  result += "/";
  result += canonicalFilename;
  return result;
}

idVec3 ParseVec3(idLexer& src) {
  idStr line;
  src.ParseRestOfLine(line);
  idVec3 result{0.0f, 0.0f, 0.0f};
  if (std::sscanf(line.data, "%f %f %f", &result.x, &result.y, &result.z) == 1) {
    result.y = result.x;
    result.z = result.x;
  }
  return result;
}

const idMaterial* DeclMaterialForParsedMtl(const idParsedMtl& mtl, idMtlOptions* mtlOptions) {
  char buffer[512];
  const char* materialName = idMaterial::resourceList.RemapResourceName(
      buffer, static_cast<int>(sizeof(buffer)), mtl.mtlName.data);
  const idMaterial* material =
      static_cast<const idMaterial*>(idMaterial::resourceList.Load(materialName, false, false));
  if (material != nullptr && !material->IsImplicit())
    return material;

  const idStr kdFileName = ExtractFinalFilename(mtl.map_Kd.data);
  if (!kdFileName.IsEmpty()) {
    idStr canonicalFilename(kdFileName);
    canonicalFilename.ToLower();
    canonicalFilename.BackSlashesToSlashes();
    const idStr localDirectory = mtlOptions->loadedFile.GetFilePath();
    const idStr gameFileName =
        LocalFileNameToGameFileName(canonicalFilename.data, localDirectory.data);
    const idStr localWithoutExtension = gameFileName.GetWithoutExtension();
    const char* generatedName = idMaterial::resourceList.RemapResourceName(
        buffer, static_cast<int>(sizeof(buffer)), localWithoutExtension.data);
    return static_cast<const idMaterial*>(
        idMaterial::resourceList.Load(generatedName, true, false));
  }

  return FindMaterialOrAutoMaterial(mtl.mtlName.data);
}

bool ParseMtlFile(const char* mtlText, idList<idParsedMtl, 5>& parsedMtls) {
  parsedMtls.ClearFree();
  idLexer src(8724);
  if (!src.LoadMemory(mtlText, std::strlen(mtlText), "parsed mtl file"))
    return false;

  idToken token;
  idStr junk;
  while (true) {
    while (true) {
      if (!src.ReadToken(token))
        return true;
      token.ToLower();
      if (token != "#")
        break;
      src.ParseCompleteLine(junk);
    }

    if (token != "newmtl") {
      src.ParseCompleteLine(junk);
      return false;
    }

    idParsedMtl material;
    src.ParseRestOfLine(material.mtlName);
    while (src.ReadToken(token)) {
      token.ToLower();
      if (token == "#") {
        src.ParseCompleteLine(junk);
        continue;
      }
      if (token == "newmtl") {
        src.UnreadToken();
        break;
      }

      idStr* map = nullptr;
      if (token == "map_ka") map = &material.map_Ka;
      else if (token == "map_kd") map = &material.map_Kd;
      else if (token == "map_ks") map = &material.map_Ks;
      else if (token == "map_ke") map = &material.map_Ke;
      else if (token == "map_bump" || token == "bump") map = &material.map_bump;
      else if (token == "map_opacity") map = &material.map_opacity;
      else if (token == "map_d") map = &material.map_d;
      else if (token == "map_ns") map = &material.map_Ns;
      else if (token == "map_refl" || token == "refl") map = &material.map_refl;
      if (map != nullptr) {
        src.ParseRestOfLine(*map);
        continue;
      }

      if (token == "decal") src.ParseRestOfLine(material.decal);
      else if (token == "disp") src.ParseRestOfLine(material.disp);
      else if (token == "map_aat") {
        idStr value;
        src.ParseRestOfLine(value);
        value.ToLower();
        material.map_aat = value == "on";
      } else if (token == "ns") material.Ns = src.ParseFloat();
      else if (token == "ni") material.Ni = src.ParseFloat();
      else if (token == "d") material.d = src.ParseFloat();
      else if (token == "tr") material.Tr = src.ParseFloat();
      else if (token == "illum") material.illum = src.ParseInt();
      else if (token == "tf") material.Tf = ParseVec3(src);
      else if (token == "ka") material.Ka = ParseVec3(src);
      else if (token == "kd") material.Kd = ParseVec3(src);
      else if (token == "ks") material.Ks = ParseVec3(src);
      else if (token == "ke") material.Ke = ParseVec3(src);
      else return false;
    }
    parsedMtls.Append(material);
    if (src.EndOfFile())
      return true;
  }
}
