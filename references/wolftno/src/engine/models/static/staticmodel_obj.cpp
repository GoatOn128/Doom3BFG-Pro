#include "engine/models/static/staticmodel_obj.h"

#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace {

constexpr int kMaxFaceVerts = 8;

int HexNibble(char value) {
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  return 0;
}

bool ReadTextFile(const std::string& fileName, std::string& text) {
  std::ifstream stream(fileName, std::ios::binary);
  if (!stream)
    return false;
  text.assign(std::istreambuf_iterator<char>(stream),
              std::istreambuf_iterator<char>());
  return true;
}

void ParseMaterialLibrary(idOBJModel& model, const idStr& path,
                          const idStr& materialFile, const char* sourceName) {
  std::string text;
  std::string combined = path.data;
  if (!combined.empty() && combined.back() != '/' && combined.back() != '\\')
    combined.push_back('/');
  combined += materialFile.data;
  if (!ReadTextFile(combined, text) &&
      !ReadTextFile(materialFile.data, text)) {
    idLib::Warning("%s: Could not find %s", sourceName, materialFile.data);
    return;
  }
  if (!ParseMtlFile(text.c_str(), model.materials))
    idLib::Warning("%s: Could not parse %s", sourceName, materialFile.data);
}

} // namespace

template <typename T>
T* idListArrayNew(int count, unsigned int tag, bool zeroBuffer) {
  if (count <= 0)
    return nullptr;
  idMem* memory = idMem::Singleton();
  T* result = static_cast<T*>(memory->AllocWithLocation(
      zeroBuffer ? "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(27) : tag"
                 : "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag",
      sizeof(T) * static_cast<std::size_t>(count), static_cast<memTag_t>(tag),
      zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP));
  if (result == nullptr)
    return nullptr;
  for (int index = 0; index < count; ++index)
    ::new (static_cast<void*>(result + index)) T;
  return result;
}

template <typename T>
T* idListArrayResize(T* oldValues, int oldCount, int newCount,
                     unsigned int tag, bool zeroBuffer) {
  T* replacement = idListArrayNew<T>(newCount, tag, zeroBuffer);
  if (replacement != nullptr) {
    for (int index = 0; index < std::min(oldCount, newCount); ++index)
      replacement[index] = oldValues[index];
  }
  if (oldValues != nullptr) {
    for (int index = 0; index < oldCount; ++index)
      oldValues[index].~T();
    idMem::Singleton()->Free(oldValues, ALIGN_16);
  }
  return replacement;
}

int ParseFaceLine(const char* line, int attributes[8][3]) {
  idStr normalized(line != nullptr ? line : "");
  for (int index = 0; index < normalized.Length(); ++index) {
    if (normalized[index] == '/')
      normalized[index] = '\\';
  }

  idLexer lineSource(LEXFL_NOFATALERRORS);
  lineSource.LoadMemory(normalized.data, normalized.Length(), "obj face line");
  idToken token;
  int count = 0;
  while (lineSource.ReadToken(token)) {
    if (count >= kMaxFaceVerts) {
      idLib::Printf("exceeded MAX_FACE_VERTS\n");
      return -1;
    }

    const auto signedValue = [&lineSource](idToken& value) {
      bool negative = value == "-";
      if (negative)
        lineSource.ReadToken(value);
      const int number = value.type == TT_NUMBER
                             ? static_cast<int>(value.intvalue)
                             : 0;
      return negative ? -number : number;
    };

    attributes[count][0] = signedValue(token);
    if (!lineSource.ReadToken(token)) {
      attributes[count][1] = 0;
      attributes[count][2] = 0;
      ++count;
      break;
    }

    if (token != "\\") {
      attributes[count][1] = 0;
      attributes[count][2] = 0;
      lineSource.UnreadToken();
      ++count;
      continue;
    }

    lineSource.ReadToken(token);
    if (token == "\\") {
      attributes[count][1] = 0;
    } else {
      attributes[count][1] = signedValue(token);
      lineSource.ReadToken(token);
    }

    if (token != "\\") {
      attributes[count][2] = 0;
      if (lineSource.EndOfFile()) {
        ++count;
        break;
      }
      lineSource.UnreadToken();
    } else {
      lineSource.ReadToken(token);
      attributes[count][2] = signedValue(token);
    }
    ++count;
  }
  return count;
}

idOBJMesh::idOBJMesh()
    : materialName(), groupName(), hasTexCoords(false), hasNormals(false),
      padding52{}, f() {}

idOBJModel::idOBJModel()
    : materials(), v(), mrgb(), vt(), vn(), meshes() {}

void OBJ_Parse(idOBJModel* model, idLexer& source, const idStr& path) {
  idOBJMesh* mesh = model->meshes.Alloc();
  mesh->hasTexCoords = true;
  mesh->hasNormals = true;
  idToken token;
  idToken token2;
  (void)token2;
  while (source.ReadToken(token)) {
    if (token[0] == 'v') {
      idStr values;
      source.ParseRestOfLine(values);
      idVec3 parsed{};
      sscanf_s(values.data, "%f %f %f", &parsed.x, &parsed.y, &parsed.z);
      if (token == "v") {
        idVec3* value = model->v.Alloc();
        *value = {parsed.x, -parsed.z, parsed.y};
      } else if (token == "vt") {
        idVec2* value = model->vt.Alloc();
        *value = {parsed.x, 1.0f - parsed.y};
      } else if (token == "vn") {
        idVec3* value = model->vn.Alloc();
        *value = {parsed.x, -parsed.z, parsed.y};
      } else {
        source.Warning("Unknown v token");
      }
      continue;
    }
    if (token == "f") {
      idStr line;
      source.ParseRestOfLine(line);
      int attributes[kMaxFaceVerts][3]{};
      const int vertexCount = ParseFaceLine(line.data, attributes);
      for (int index = 0; index < vertexCount; ++index) {
        if (attributes[index][0] == 0)
          source.Error("Bad vertex index");
        if (attributes[index][0] < 0)
          attributes[index][0] += model->v.Num() + 1;
        if (attributes[index][1] != 0) {
          mesh->hasTexCoords = true;
          if (attributes[index][1] < 0)
            attributes[index][1] += model->vt.Num() + 1;
        } else {
          mesh->hasTexCoords = false;
        }
        if (attributes[index][2] != 0) {
          mesh->hasNormals = true;
          if (attributes[index][2] < 0)
            attributes[index][2] += model->vn.Num() + 1;
        } else {
          mesh->hasNormals = false;
        }
      }
      if (vertexCount < 3) {
        source.Warning("Face with < 3 verts");
        continue;
      }
      for (int index = 1; index < vertexCount - 1; ++index) {
        idOBJFace face{};
        const int triangle[3]{0, index + 1, index};
        for (int corner = 0; corner < 3; ++corner) {
          face.v[corner] = attributes[triangle[corner]][0];
          face.vt[corner] = attributes[triangle[corner]][1];
          face.vn[corner] = attributes[triangle[corner]][2];
        }
        mesh->f.Append(face);
      }
      continue;
    }
    if (token == "g" || token == "s") {
      if (mesh->f.Num() > 0) {
        const idStr previousMaterial(mesh->materialName);
        mesh = model->meshes.Alloc();
        mesh->materialName = previousMaterial;
        mesh->hasTexCoords = true;
        mesh->hasNormals = true;
      }
      source.SkipRestOfLine();
      continue;
    }
    if (token == "mtllib") {
      idStr materialFile;
      source.ParseRestOfLine(materialFile);
      if (materialFile.IsEmpty()) {
        source.Warning("Missing token after mtllib");
      } else if (model->materials.Num() != 0) {
        idLib::Error("%s: multiple mtllib statements\n", "obj");
      } else {
        ParseMaterialLibrary(*model, path, materialFile,
                             "obj");
      }
      continue;
    }
    if (token == "usemtl") {
      if (mesh->f.Num() > 0) {
        mesh = model->meshes.Alloc();
        mesh->hasTexCoords = true;
        mesh->hasNormals = true;
      }
      idStr material;
      source.ParseRestOfLine(material);
      if (material.IsEmpty())
        source.Warning("Missing token after usemtl");
      else
        mesh->materialName = material;
      continue;
    }
    if (token == "#") {
      if (source.PeekTokenString("MRGB")) {
        source.ReadToken(token);
        idStr encoded;
        source.ParseCompleteLine(encoded);
        const char* cursor = encoded.data;
        while (*cursor == ' ')
          ++cursor;
        const int count = idStr::Length(cursor) / 8;
        for (int index = 0; index < count; ++index) {
          mrgb_t value{};
          for (int channel = 0; channel < 4; ++channel) {
            value.mrgb[channel] = static_cast<std::uint8_t>(
                HexNibble(cursor[0]) * 16 + HexNibble(cursor[1]));
            cursor += 2;
          }
          model->mrgb.Append(value);
        }
      } else {
        source.SkipRestOfLine();
      }
      continue;
    }
    source.Warning("Unknown token %s", token.data);
    source.SkipRestOfLine();
  }

  for (int index = model->meshes.Num() - 1; index >= 0; --index) {
    if (model->meshes[index].f.Num() == 0)
      model->meshes.RemoveIndex(index);
  }
  if (model->vt.Num() == 0) {
    bool referenced = false;
    for (int index = 0; index < model->meshes.Num(); ++index) {
      referenced |= model->meshes[index].hasTexCoords;
      model->meshes[index].hasTexCoords = false;
    }
    if (referenced)
      source.Warning("meshes use texture coords that do not exist");
  }
}

bool idOBJModel::Load(const char* fileName) {
  idLexer source(8724);
  if (!source.LoadFile(fileName, false))
    return false;
  idStr path(fileName);
  path.StripFilename();
  OBJ_Parse(this, source, path);
  return true;
}

idOBJModel* OBJ_Load(const char* fileName) {
  idMem* memory = idMem::Singleton();
  void* allocation = memory->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
      sizeof(idOBJModel), TAG_NEW, false, ALIGN_16, HEAP_DEFAULTHEAP);
  if (allocation == nullptr)
    return nullptr;
  idOBJModel* model = ::new (allocation) idOBJModel;
  if (model->Load(fileName))
    return model;
  model->~idOBJModel();
  memory->Free(model, ALIGN_16);
  return nullptr;
}

template idOBJFace* idListArrayResize<idOBJFace>(idOBJFace*, int, int,
                                                 unsigned int, bool);
template idOBJMesh* idListArrayNew<idOBJMesh>(int, unsigned int, bool);
template idOBJMesh* idListArrayResize<idOBJMesh>(idOBJMesh*, int, int,
                                                 unsigned int, bool);
template idList<idOBJFace, 5>&
idList<idOBJFace, 5>::operator=(const idList<idOBJFace, 5>&);
template bool idList<idOBJFace, 5>::Resize(int);
template bool idList<idOBJMesh, 5>::RemoveIndex(int);
template bool idList<idOBJMesh, 5>::Resize(int);
template idOBJMesh* idList<idOBJMesh, 5>::Alloc();
