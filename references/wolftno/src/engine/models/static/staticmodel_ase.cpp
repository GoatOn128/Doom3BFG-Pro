#include "engine/models/static/staticmodel_ase.h"

#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

namespace {

struct ase_t {
  const char* buffer;
  const char* curpos;
  int len;
  char token[1024];
  bool verbose;
  std::uint8_t recoveredPadding415[3];
  idASEModel* model;
  idASEObject* currentObject;
  idASEMesh* currentMesh;
  idASEMaterial* currentMaterial;
  int currentFace;
  int currentVertex;
};

static_assert(sizeof(ase_t) == 0x440, "ase_t ABI drift");
static_assert(offsetof(ase_t, token) == 0x14, "ase_t token offset drift");
static_assert(offsetof(ase_t, model) == 0x418, "ase_t model offset drift");
static_assert(offsetof(ase_t, currentFace) == 0x438,
              "ase_t face offset drift");

ase_t ase{};

void* ASE_Alloc(std::size_t size, bool zero = false) {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW", size,
      TAG_NEW, zero, ALIGN_16, HEAP_DEFAULTHEAP);
}

template <typename Type> Type* ASE_New() {
  void* storage = ASE_Alloc(sizeof(Type));
  return storage != nullptr ? ::new (storage) Type : nullptr;
}

template <typename Type> void ASE_Delete(Type* object) {
  if (object == nullptr)
    return;
  object->~Type();
  idMem::Singleton()->Free(object, ALIGN_16);
}

bool ASE_IsAbsolutePath(const char* path) {
  if (path == nullptr || path[0] == '\0')
    return false;
  return path[0] == '/' || path[0] == '\\' || std::strchr(path, ':') != nullptr;
}

idStr ASE_OSPathToRelativePath(const idStr& input) {
  if (!ASE_IsAbsolutePath(input.data))
    return input;

  static const char* const basePaths[] = {"base", "d3xp", "d3le"};
  const char* path = input.data;
  const char* selected = nullptr;
  for (const char* cursor = path; *cursor != '\0'; ++cursor) {
    if (*cursor != '/' && *cursor != '\\')
      continue;
    const char* component = cursor + 1;
    const char* end = component;
    while (*end != '\0' && *end != '/' && *end != '\\')
      ++end;
    const int length = static_cast<int>(end - component);
    for (const char* base : basePaths) {
      if (length != idStr::Length(base) || idStr::Icmpn(component, base, length) != 0)
        continue;
      const char* following = *end != '\0' ? end + 1 : end;
      const char* followingEnd = following;
      while (*followingEnd != '\0' && *followingEnd != '/' &&
             *followingEnd != '\\')
        ++followingEnd;
      bool hasAdjacentBase = false;
      for (const char* secondBase : basePaths) {
        const int secondLength = static_cast<int>(followingEnd - following);
        if (secondLength == idStr::Length(secondBase) &&
            idStr::Icmpn(following, secondBase, secondLength) == 0) {
          hasAdjacentBase = true;
          break;
        }
      }
      selected = hasAdjacentBase && *followingEnd != '\0' ? followingEnd + 1
                                                            : following;
      break;
    }
    if (selected != nullptr)
      break;
  }
  if (selected == nullptr) {
    idLib::Warning("OSPathToRelativePath failed on %s", input.data);
    selected = input.data;
  }
  return idStr(selected);
}

idVec3 ASE_TransformNormal(const idASEMesh& mesh, const idVec3& normal) {
  return {
      normal.x * mesh.transform[0].x + normal.y * mesh.transform[1].x +
          normal.z * mesh.transform[2].x,
      normal.x * mesh.transform[0].y + normal.y * mesh.transform[1].y +
          normal.z * mesh.transform[2].y,
      normal.x * mesh.transform[0].z + normal.y * mesh.transform[1].z +
          normal.z * mesh.transform[2].z,
  };
}

unsigned char ASE_ColorByte(float value) {
  const int converted = static_cast<int>(value * 255.0f);
  return static_cast<unsigned char>(std::clamp(converted, 0, 255));
}

} // namespace

template <>
template <>
void idList<idASEMesh*, 5>::DeleteContents<idASEMesh*>() {
  for (int index = 0; index < num; ++index) {
    ASE_Delete(list[index]);
    list[index] = nullptr;
  }
  ClearFree();
}

template <>
template <>
void idList<idASEObject*, 5>::DeleteContents<idASEObject*>() {
  for (int index = 0; index < num; ++index) {
    ASE_Delete(list[index]);
    list[index] = nullptr;
  }
  ClearFree();
}

idASEMesh::idASEMesh()
    : timeValue(0), numFaces(0), numTVFaces(0), numCVFaces(0), transform{},
      colorsParsed(false), normalsParsed(false), recoveredPadding42{},
      vertexes(), tvertexes(), cvertexes(), faces(nullptr) {}

idASEMesh::~idASEMesh() {
  idMem::Singleton()->Free(faces, ALIGN_16);
  faces = nullptr;
}

idASEObject::idASEObject()
    : name(), materialRef(0), recoveredPadding2C{}, mesh(), frames() {}

idASEObject::~idASEObject() noexcept(false) {
  frames.DeleteContents();
}

idASEModel::idASEModel() : materials(), objects() {}

idASEModel::~idASEModel() {
  for (int index = 0; index < materials.Num(); ++index) {
    ASE_Delete(materials[index]);
    materials[index] = nullptr;
  }
  materials.ClearFree();
  objects.DeleteContents();
}

int ASE_GetToken(bool restOfLine) {
  int index = 0;
  if (ase.buffer == nullptr || ase.curpos - ase.buffer == ase.len)
    return 0;

  while (ase.curpos - ase.buffer < ase.len && *ase.curpos <= 32)
    ++ase.curpos;

  while (ase.curpos - ase.buffer < ase.len) {
    ase.token[index] = *ase.curpos++;
    ++index;
    if ((ase.token[index - 1] <= 32 && !restOfLine) ||
        ase.token[index - 1] == '\n' || ase.token[index - 1] == '\r') {
      ase.token[index - 1] = '\0';
      break;
    }
  }
  ase.token[index] = '\0';
  return 1;
}

void ASE_ParseBracedBlock(void (*parser)(const char* token)) {
  int indent = 0;
  while (ASE_GetToken(false)) {
    if (std::strcmp(ase.token, "{") == 0) {
      ++indent;
    } else if (std::strcmp(ase.token, "}") == 0) {
      --indent;
      if (indent == 0)
        return;
      if (indent < 0)
        idLib::Error("Unexpected '}'");
    } else if (parser != nullptr) {
      parser(ase.token);
    }
  }
}

void ASE_SkipEnclosingBraces() {
  int indent = 0;
  while (ASE_GetToken(false)) {
    if (std::strcmp(ase.token, "{") == 0) {
      ++indent;
    } else if (std::strcmp(ase.token, "}") == 0) {
      --indent;
      if (indent == 0)
        return;
      if (indent < 0)
        idLib::Error("Unexpected '}'");
    }
  }
}

void ASE_SkipRestOfLine() { ASE_GetToken(true); }

void ASE_KeyMESH_FACE_LIST(const char* token) {
  idASEMesh* mesh = ase.currentMesh;
  if (std::strcmp(token, "*MESH_FACE") != 0)
    idLib::Error("Unknown token '%s' while parsing MESH_FACE_LIST", token);

  ASE_GetToken(false);
  ASE_GetToken(false);
  ASE_GetToken(false);
  mesh->faces[ase.currentFace].vertexNum[0] = std::atoi(ase.token);
  ASE_GetToken(false);
  ASE_GetToken(false);
  mesh->faces[ase.currentFace].vertexNum[2] = std::atoi(ase.token);
  ASE_GetToken(false);
  ASE_GetToken(false);
  mesh->faces[ase.currentFace].vertexNum[1] = std::atoi(ase.token);
  ASE_GetToken(true);
  ++ase.currentFace;
}

void ASE_KeyTFACE_LIST(const char* token) {
  idASEMesh* mesh = ase.currentMesh;
  if (std::strcmp(token, "*MESH_TFACE") != 0)
    idLib::Error("Unknown token '%s' in MESH_TFACE", token);

  ASE_GetToken(false);
  ASE_GetToken(false);
  const int first = std::atoi(ase.token);
  ASE_GetToken(false);
  const int third = std::atoi(ase.token);
  ASE_GetToken(false);
  const int second = std::atoi(ase.token);
  mesh->faces[ase.currentFace].tVertexNum[0] = first;
  mesh->faces[ase.currentFace].tVertexNum[1] = second;
  mesh->faces[ase.currentFace].tVertexNum[2] = third;
  ++ase.currentFace;
}

void ASE_KeyMAP_DIFFUSE(const char* token) {
  if (std::strcmp(token, "*BITMAP") == 0) {
    idStr materialName;
    ASE_GetToken(false);
    char* quote = std::strstr(ase.token + 1, "\"");
    if (quote != nullptr)
      *quote = '\0';
    materialName = ase.token + 1;
    materialName.BackSlashesToSlashes();
    const idStr relativePath = ASE_OSPathToRelativePath(materialName);
    ase.currentMaterial->name = relativePath;
  } else if (std::strcmp(token, "*UVW_U_OFFSET") == 0) {
    idASEMaterial* material = ase.model->materials[ase.model->materials.Num() - 1];
    ASE_GetToken(false);
    material->uOffset = static_cast<float>(std::atof(ase.token));
  } else if (std::strcmp(token, "*UVW_V_OFFSET") == 0) {
    idASEMaterial* material = ase.model->materials[ase.model->materials.Num() - 1];
    ASE_GetToken(false);
    material->vOffset = static_cast<float>(std::atof(ase.token));
  } else if (std::strcmp(token, "*UVW_U_TILING") == 0) {
    idASEMaterial* material = ase.model->materials[ase.model->materials.Num() - 1];
    ASE_GetToken(false);
    material->uTiling = static_cast<float>(std::atof(ase.token));
  } else if (std::strcmp(token, "*UVW_V_TILING") == 0) {
    idASEMaterial* material = ase.model->materials[ase.model->materials.Num() - 1];
    ASE_GetToken(false);
    material->vTiling = static_cast<float>(std::atof(ase.token));
  } else if (std::strcmp(token, "*UVW_ANGLE") == 0) {
    idASEMaterial* material = ase.model->materials[ase.model->materials.Num() - 1];
    ASE_GetToken(false);
    material->angle = static_cast<float>(std::atof(ase.token));
  }
}

void ASE_KeyMATERIAL(const char* token) {
  if (std::strcmp(token, "*MAP_DIFFUSE") == 0)
    ASE_ParseBracedBlock(ASE_KeyMAP_DIFFUSE);
}

void ASE_KeyNODE_TM(const char* token) {
  int row = -1;
  if (std::strcmp(token, "*TM_ROW0") == 0)
    row = 0;
  else if (std::strcmp(token, "*TM_ROW1") == 0)
    row = 1;
  else if (std::strcmp(token, "*TM_ROW2") == 0)
    row = 2;
  else if (std::strcmp(token, "*TM_ROW3") == 0)
    row = 3;
  if (row < 0)
    return;
  for (int component = 0; component < 3; ++component) {
    ASE_GetToken(false);
    ase.currentObject->mesh.transform[row][component] =
        static_cast<float>(std::atof(ase.token));
  }
}

void ASE_KeyMESH_VERTEX_LIST(const char* token) {
  idASEMesh* mesh = ase.currentMesh;
  if (std::strcmp(token, "*MESH_VERTEX") != 0)
    idLib::Error("Unknown token '%s' while parsing MESH_VERTEX_LIST", token);
  ASE_GetToken(false);
  ASE_GetToken(false);
  mesh->vertexes[ase.currentVertex].x = static_cast<float>(std::atof(ase.token));
  ASE_GetToken(false);
  mesh->vertexes[ase.currentVertex].y = static_cast<float>(std::atof(ase.token));
  ASE_GetToken(false);
  mesh->vertexes[ase.currentVertex].z = static_cast<float>(std::atof(ase.token));
  if (++ase.currentVertex > mesh->vertexes.Num())
    idLib::Error("ase.currentVertex >= pMesh->vertexes.Num");
}

void ASE_KeyCFACE_LIST(const char* token) {
  idASEMesh* mesh = ase.currentMesh;
  if (std::strcmp(token, "*MESH_CFACE") != 0)
    idLib::Error("Unknown token '%s' in MESH_CFACE", token);
  ASE_GetToken(false);
  static int remap[3] = {0, 2, 1};
  for (int index = 0; index < 3; ++index) {
    ASE_GetToken(false);
    const int colorIndex = std::atoi(ase.token);
    const idVec3& color = mesh->cvertexes[colorIndex];
    mesh->faces[ase.currentFace].vertexColors[remap[index]][0] = ASE_ColorByte(color.x);
    mesh->faces[ase.currentFace].vertexColors[remap[index]][1] = ASE_ColorByte(color.y);
    mesh->faces[ase.currentFace].vertexColors[remap[index]][2] = ASE_ColorByte(color.z);
  }
  ++ase.currentFace;
}

void ASE_KeyMESH_TVERTLIST(const char* token) {
  idASEMesh* mesh = ase.currentMesh;
  if (std::strcmp(token, "*MESH_TVERT") != 0)
    idLib::Error("Unknown token '%s' while parsing MESH_TVERTLIST", token);
  char u[80];
  char v[80];
  char w[80];
  ASE_GetToken(false);
  ASE_GetToken(false);
  idStr::Copynz(u, ase.token, sizeof(u));
  ASE_GetToken(false);
  idStr::Copynz(v, ase.token, sizeof(v));
  ASE_GetToken(false);
  idStr::Copynz(w, ase.token, sizeof(w));
  mesh->tvertexes[ase.currentVertex].x = static_cast<float>(std::atof(u));
  mesh->tvertexes[ase.currentVertex].y = 1.0f - static_cast<float>(std::atof(v));
  if (++ase.currentVertex > mesh->tvertexes.Num())
    idLib::Error("ase.currentVertex > pMesh->tvertexes.Num");
}

void ASE_KeyMESH_CVERTLIST(const char* token) {
  idASEMesh* mesh = ase.currentMesh;
  mesh->colorsParsed = true;
  if (std::strcmp(token, "*MESH_VERTCOL") != 0)
    idLib::Error("Unknown token '%s' while parsing MESH_CVERTLIST", token);
  ASE_GetToken(false);
  ASE_GetToken(false);
  mesh->cvertexes[ase.currentVertex].x = static_cast<float>(std::atof(token));
  ASE_GetToken(false);
  mesh->cvertexes[ase.currentVertex].y = static_cast<float>(std::atof(token));
  ASE_GetToken(false);
  mesh->cvertexes[ase.currentVertex].z = static_cast<float>(std::atof(token));
  if (++ase.currentVertex > mesh->cvertexes.Num())
    idLib::Error("ase.currentVertex > pMesh->cvertexes.Num");
}

void ASE_KeyMESH_NORMALS(const char* token) {
  idASEMesh* mesh = ase.currentMesh;
  mesh->normalsParsed = true;
  aseFace_t* face = &mesh->faces[ase.currentFace];
  if (std::strcmp(token, "*MESH_FACENORMAL") == 0) {
    ASE_GetToken(false);
    const int faceNumber = std::atoi(ase.token);
    if (faceNumber >= mesh->numFaces || faceNumber < 0)
      idLib::Error("MESH_NORMALS face index out of range: %i", faceNumber);
    if (faceNumber != ase.currentFace)
      idLib::Error("MESH_NORMALS face index != currentFace");
    idVec3 normal;
    ASE_GetToken(false);
    normal.x = static_cast<float>(std::atof(ase.token));
    ASE_GetToken(false);
    normal.y = static_cast<float>(std::atof(ase.token));
    ASE_GetToken(false);
    normal.z = static_cast<float>(std::atof(ase.token));
    face->faceNormal = ASE_TransformNormal(*mesh, normal);
    face->faceNormal.Normalize();
    ++ase.currentFace;
  } else if (std::strcmp(token, "*MESH_VERTEXNORMAL") == 0) {
    ASE_GetToken(false);
    const int vertexNumber = std::atoi(ase.token);
    if (vertexNumber >= mesh->vertexes.Num() || vertexNumber < 0)
      idLib::Error("MESH_NORMALS vertex index out of range: %i", vertexNumber);
    face = &mesh->faces[ase.currentFace - 1];
    int vertex = 0;
    while (vertex < 3 && vertexNumber != face->vertexNum[vertex])
      ++vertex;
    if (vertex == 3)
      idLib::Error("MESH_NORMALS vertex index doesn't match face");
    idVec3 normal;
    ASE_GetToken(false);
    normal.x = static_cast<float>(std::atof(ase.token));
    ASE_GetToken(false);
    normal.y = static_cast<float>(std::atof(ase.token));
    ASE_GetToken(false);
    normal.z = static_cast<float>(std::atof(ase.token));
    face->vertexNormals[vertex] = ASE_TransformNormal(*mesh, normal);
    face->vertexNormals[vertex].Normalize();
  }
}

void ASE_KeyMESH(const char* token) {
  idASEMesh* mesh = ase.currentMesh;
  if (std::strcmp(token, "*TIMEVALUE") == 0) {
    ASE_GetToken(false);
    mesh->timeValue = std::atoi(ase.token);
    if (ase.verbose)
      idLib::Printf(".....timevalue: %d\n", mesh->timeValue);
  } else if (std::strcmp(token, "*MESH_NUMVERTEX") == 0) {
    ASE_GetToken(false);
    mesh->vertexes.SetNum(std::atoi(ase.token));
    if (ase.verbose)
      idLib::Printf(".....num vertexes: %d\n", mesh->vertexes.Num());
  } else if (std::strcmp(token, "*MESH_NUMTVERTEX") == 0) {
    ASE_GetToken(false);
    mesh->tvertexes.SetNum(std::atoi(ase.token));
    if (ase.verbose)
      idLib::Printf(".....num tvertexes: %d\n", mesh->tvertexes.Num());
  } else if (std::strcmp(token, "*MESH_NUMCVERTEX") == 0) {
    ASE_GetToken(false);
    mesh->cvertexes.SetNum(std::atoi(ase.token));
    if (ase.verbose)
      idLib::Printf(".....num cvertexes: %d\n", mesh->cvertexes.Num());
  } else if (std::strcmp(token, "*MESH_NUMFACES") == 0) {
    ASE_GetToken(false);
    mesh->numFaces = std::atoi(ase.token);
    if (ase.verbose)
      idLib::Printf(".....num faces: %d\n", mesh->numFaces);
  } else if (std::strcmp(token, "*MESH_NUMTVFACES") == 0) {
    ASE_GetToken(false);
    mesh->numTVFaces = std::atoi(ase.token);
    if (ase.verbose)
      idLib::Printf(".....num tvfaces: %d\n", mesh->numTVFaces);
    if (mesh->numTVFaces != mesh->numFaces)
      idLib::Error("MESH_NUMTVFACES != MESH_NUMFACES");
  } else if (std::strcmp(token, "*MESH_NUMCVFACES") == 0) {
    ASE_GetToken(false);
    mesh->numCVFaces = std::atoi(ase.token);
    if (ase.verbose)
      idLib::Printf(".....num cvfaces: %d\n", mesh->numCVFaces);
    if (mesh->numTVFaces != mesh->numFaces)
      idLib::Error("MESH_NUMCVFACES != MESH_NUMFACES");
  } else if (std::strcmp(token, "*MESH_VERTEX_LIST") == 0) {
    ase.currentVertex = 0;
    if (ase.verbose)
      idLib::Printf(".....parsing MESH_VERTEX_LIST\n");
    ASE_ParseBracedBlock(ASE_KeyMESH_VERTEX_LIST);
  } else if (std::strcmp(token, "*MESH_TVERTLIST") == 0) {
    ase.currentVertex = 0;
    if (ase.verbose)
      idLib::Printf(".....parsing MESH_TVERTLIST\n");
    ASE_ParseBracedBlock(ASE_KeyMESH_TVERTLIST);
  } else if (std::strcmp(token, "*MESH_CVERTLIST") == 0) {
    ase.currentVertex = 0;
    if (ase.verbose)
      idLib::Printf(".....parsing MESH_CVERTLIST\n");
    ASE_ParseBracedBlock(ASE_KeyMESH_CVERTLIST);
  } else if (std::strcmp(token, "*MESH_FACE_LIST") == 0) {
    mesh->faces = static_cast<aseFace_t*>(
        ASE_Alloc(sizeof(aseFace_t) * static_cast<std::size_t>(mesh->numFaces), true));
    ase.currentFace = 0;
    if (ase.verbose)
      idLib::Printf(".....parsing MESH_FACE_LIST\n");
    ASE_ParseBracedBlock(ASE_KeyMESH_FACE_LIST);
  } else if (std::strcmp(token, "*MESH_TFACELIST") == 0) {
    if (mesh->faces == nullptr)
      idLib::Error("*MESH_TFACELIST before *MESH_FACE_LIST");
    ase.currentFace = 0;
    if (ase.verbose)
      idLib::Printf(".....parsing MESH_TFACE_LIST\n");
    ASE_ParseBracedBlock(ASE_KeyTFACE_LIST);
  } else if (std::strcmp(token, "*MESH_CFACELIST") == 0) {
    if (mesh->faces == nullptr)
      idLib::Error("*MESH_CFACELIST before *MESH_FACE_LIST");
    ase.currentFace = 0;
    if (ase.verbose)
      idLib::Printf(".....parsing MESH_CFACE_LIST\n");
    ASE_ParseBracedBlock(ASE_KeyCFACE_LIST);
  } else if (std::strcmp(token, "*MESH_NORMALS") == 0) {
    if (mesh->faces == nullptr)
      idLib::Warning("*MESH_NORMALS before *MESH_FACE_LIST");
    ase.currentFace = 0;
    if (ase.verbose)
      idLib::Printf(".....parsing MESH_NORMALS\n");
    ASE_ParseBracedBlock(ASE_KeyMESH_NORMALS);
  }
}

void ASE_KeyMATERIAL_LIST(const char* token) {
  if (std::strcmp(token, "*MATERIAL_COUNT") == 0) {
    ASE_GetToken(false);
    if (ase.verbose)
      idLib::Printf("..num materials: %s\n", ase.token);
  } else if (std::strcmp(token, "*MATERIAL") == 0) {
    if (ase.verbose)
      idLib::Printf("..material %d\n", ase.model->materials.Num());
    ase.currentMaterial = ASE_New<idASEMaterial>();
    if (ase.currentMaterial == nullptr)
      idLib::Error("ASE material allocation failed");
    ase.currentMaterial->uTiling = 1.0f;
    ase.currentMaterial->vTiling = 1.0f;
    ase.model->materials.Append(ase.currentMaterial);
    ASE_ParseBracedBlock(ASE_KeyMATERIAL);
  }
}

void ASE_KeyMESH_ANIMATION(const char* token) {
  if (std::strcmp(token, "*MESH") != 0)
    idLib::Error("Unknown token '%s' while parsing MESH_ANIMATION", token);
  if (ase.verbose)
    idLib::Printf("...found MESH\n", token);
  idASEMesh* mesh = ASE_New<idASEMesh>();
  if (mesh == nullptr)
    idLib::Error("ASE mesh allocation failed");
  ase.currentMesh = mesh;
  ase.currentObject->frames.Append(mesh);
  ASE_ParseBracedBlock(ASE_KeyMESH);
}

void ASE_KeyGEOMOBJECT(const char* token) {
  idASEObject* object = ase.currentObject;
  if (std::strcmp(token, "*NODE_NAME") == 0) {
    ASE_GetToken(true);
    if (ase.verbose)
      idLib::Printf(" %s\n", ase.token);
    object->name = ase.token;
  } else if (std::strcmp(token, "*NODE_PARENT") == 0) {
    ASE_SkipRestOfLine();
  } else if (std::strcmp(token, "*NODE_TM") == 0 ||
             std::strcmp(token, "*TM_ANIMATION") == 0) {
    ASE_ParseBracedBlock(ASE_KeyNODE_TM);
  } else if (std::strcmp(token, "*MESH") == 0) {
    ase.currentMesh = &object->mesh;
    std::memset(&object->mesh, 0, sizeof(object->mesh));
    ASE_ParseBracedBlock(ASE_KeyMESH);
  } else if (std::strcmp(token, "*MATERIAL_REF") == 0) {
    ASE_GetToken(false);
    object->materialRef = std::atoi(ase.token);
  } else if (std::strcmp(token, "*MESH_ANIMATION") == 0) {
    if (ase.verbose)
      idLib::Printf("..found MESH_ANIMATION\n");
    ASE_ParseBracedBlock(ASE_KeyMESH_ANIMATION);
  } else if (std::strcmp(token, "*PROP_MOTIONBLUR") == 0 ||
             std::strcmp(token, "*PROP_CASTSHADOW") == 0 ||
             std::strcmp(token, "*PROP_RECVSHADOW") == 0) {
    ASE_SkipRestOfLine();
  }
}

void ASE_ParseGeomObject() {
  if (ase.verbose)
    idLib::Printf("GEOMOBJECT");
  idASEObject* object = ASE_New<idASEObject>();
  if (object == nullptr)
    idLib::Error("ASE object allocation failed");
  ase.model->objects.Append(object);
  ase.currentObject = object;
  object->frames.ResizeForPort(32);
  ASE_ParseBracedBlock(ASE_KeyGEOMOBJECT);
}

void ASE_KeyGROUP(const char* token) {
  if (std::strcmp(token, "*GEOMOBJECT") == 0)
    ASE_ParseGeomObject();
}

idASEModel* ASE_Parse(const char* buffer, bool verbose) {
  std::memset(&ase, 0, sizeof(ase));
  ase.verbose = verbose;
  ase.buffer = buffer;
  ase.len = idStr::Length(buffer);
  ase.curpos = ase.buffer;
  ase.currentObject = nullptr;
  ase.model = ASE_New<idASEModel>();
  if (ase.model == nullptr)
    idLib::Error("ASE model allocation failed");
  ase.model->objects.ResizeForPort(32);
  ase.model->materials.ResizeForPort(32);

  while (ASE_GetToken(false)) {
    if (std::strcmp(ase.token, "*3DSMAX_ASCIIEXPORT") == 0 ||
        std::strcmp(ase.token, "*COMMENT") == 0) {
      ASE_SkipRestOfLine();
    } else if (std::strcmp(ase.token, "*SCENE") == 0) {
      ASE_SkipEnclosingBraces();
    } else if (std::strcmp(ase.token, "*GROUP") == 0) {
      ASE_GetToken(false);
      ASE_ParseBracedBlock(ASE_KeyGROUP);
    } else if (std::strcmp(ase.token, "*SHAPEOBJECT") == 0 ||
               std::strcmp(ase.token, "*CAMERAOBJECT") == 0) {
      ASE_SkipEnclosingBraces();
    } else if (std::strcmp(ase.token, "*MATERIAL_LIST") == 0) {
      if (ase.verbose)
        idLib::Printf("MATERIAL_LIST\n");
      ASE_ParseBracedBlock(ASE_KeyMATERIAL_LIST);
    } else if (std::strcmp(ase.token, "*GEOMOBJECT") == 0) {
      ASE_ParseGeomObject();
    } else if (ase.token[0] != '\0') {
      idLib::Printf("Unknown token '%s'\n", ase.token);
    }
  }
  return ase.model;
}

idASEModel* ASE_Load(const char* fileName) {
  if (fileSystem == nullptr || fileName == nullptr)
    return nullptr;
  std::vector<std::uint8_t> contents;
  if (!fileSystem->ReadFile(fileName, contents))
    return nullptr;
  contents.push_back(0);
  return ASE_Parse(reinterpret_cast<const char*>(contents.data()), false);
}

void ASE_Free(idASEModel* aseModel) { ASE_Delete(aseModel); }
