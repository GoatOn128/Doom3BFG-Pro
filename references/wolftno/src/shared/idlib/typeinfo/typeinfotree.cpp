#include "shared/idlib/typeinfo/typeinfo.h"

#include <algorithm>
#include <sstream>
#include <string>

namespace {
std::vector<std::string> Split(const char* path) {
  std::vector<std::string> out;
  std::string current;
  for (const char* p = path ? path : "";; ++p) {
    if (*p == '.' || *p == '/' || *p == 0) {
      if (!current.empty()) {
        out.push_back(current);
        current.clear();
      }
      if (!*p)
        break;
    } else
      current += *p;
  }
  return out;
}
std::string Trim(std::string s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}
} // namespace
idTypeInfoTree::idTypeInfoTree() {
  root.name = "root";
  root.type = "root";
}
void idTypeInfoTree::Clear() {
  root.children.clear();
  root.value.Clear();
}
idTypeInfoNode* idTypeInfoTree::CreatePath(const char* path) {
  idTypeInfoNode* node = &root;
  for (const auto& part : Split(path)) {
    auto it = std::find_if(node->children.begin(), node->children.end(),
                           [&](const auto& child) { return child->name == part.c_str(); });
    if (it == node->children.end()) {
      auto child = std::make_unique<idTypeInfoNode>();
      child->name = part.c_str();
      child->parent = node;
      node->children.push_back(std::move(child));
      node = node->children.back().get();
    } else
      node = it->get();
  }
  return node;
}
const idTypeInfoNode* idTypeInfoTree::FindPath(const char* path) const {
  const idTypeInfoNode* node = &root;
  for (const auto& part : Split(path)) {
    auto it = std::find_if(node->children.begin(), node->children.end(),
                           [&](const auto& child) { return child->name == part.c_str(); });
    if (it == node->children.end())
      return nullptr;
    node = it->get();
  }
  return node;
}
idTypeInfoNode* idTypeInfoTree::FindPath(const char* p) {
  return const_cast<idTypeInfoNode*>(static_cast<const idTypeInfoTree*>(this)->FindPath(p));
}
bool idTypeInfoTree::Set(const char* path, const char* value, const char* type) {
  auto* n = CreatePath(path);
  n->value = value ? value : "";
  n->type = type ? type : "string";
  return true;
}
const char* idTypeInfoTree::Get(const char* p, const char* f) const {
  const auto* n = FindPath(p);
  return n ? n->value.data : f;
}
bool idTypeInfoTree::GetBool(const char* p, bool f) const {
  const auto* n = FindPath(p);
  return n ? std::atoi(n->value.data) != 0 : f;
}
idVec3 idTypeInfoTree::GetVec3(const char* p, const idVec3& f) const {
  idVec3 v = f;
  const auto* n = FindPath(p);
  if (n)
    std::sscanf(n->value.data, "%f,%f,%f", &v.x, &v.y, &v.z);
  return v;
}
void idTypeInfoTree::SetVec3(const char* p, const idVec3& v) {
  char b[96];
  std::snprintf(b, sizeof(b), "%g,%g,%g", v.x, v.y, v.z);
  Set(p, b, "idVec3");
}
idMat3 idTypeInfoTree::GetMat3(const char* p, const idMat3& f) const {
  idMat3 m = f;
  const auto* n = FindPath(p);
  if (n)
    std::sscanf(n->value.data, "%f,%f,%f,%f,%f,%f,%f,%f,%f", &m[0].x, &m[0].y, &m[0].z, &m[1].x,
                &m[1].y, &m[1].z, &m[2].x, &m[2].y, &m[2].z);
  return m;
}
void idTypeInfoTree::SetMat3(const char* p, const idMat3& m) {
  char b[256];
  std::snprintf(b, sizeof(b), "%g,%g,%g,%g,%g,%g,%g,%g,%g", m[0].x, m[0].y, m[0].z, m[1].x, m[1].y,
                m[1].z, m[2].x, m[2].y, m[2].z);
  Set(p, b, "idMat3");
}
bool idTypeInfoTree::GetStrType(const char* p, idStr& value) const {
  value.Clear();
  const auto* n = FindPath(p);
  if (n == nullptr)
    return false;
  value = n->value;
  if (value.Length() >= 2 && value[0] == '"' && value[value.Length() - 1] == '"') {
    idStr unquoted(value.data, 1, value.Length() - 1);
    value = unquoted;
  }
  return true;
}
bool idTypeInfoTree::SetStrType(const char* p, const char* value) {
  idTypeInfoNode* node = CreatePath(p);
  if (node == nullptr)
    return false;
  node->type = "string";
  if (value == nullptr) {
    node->value = "NULL";
  } else {
    node->value = "\"";
    node->value.Append(*value ? value : "");
    node->value.Append("\"");
  }
  return true;
}
bool idTypeInfoTree::Parse(const char* text) {
  Clear();
  return ParseOverwrite(text);
}
bool idTypeInfoTree::ParseOverwrite(const char* text) {
  std::istringstream stream(text ? text : "");
  std::string line;
  while (std::getline(stream, line)) {
    const auto equal = line.find('=');
    if (equal == std::string::npos)
      continue;
    std::string left = Trim(line.substr(0, equal)), value = Trim(line.substr(equal + 1));
    std::string type = "string";
    const auto colon = left.find(':');
    if (colon != std::string::npos) {
      type = Trim(left.substr(colon + 1));
      left = Trim(left.substr(0, colon));
    }
    Set(left.c_str(), value.c_str(), type.c_str());
  }
  return true;
}
void idTypeInfoTree::WriteNode(const idTypeInfoNode* n, const idStr& base, idFile* f) const {
  for (const auto& child : n->children) {
    idStr path = base;
    if (!path.IsEmpty())
      path.Append(".");
    path.Append(child->name);
    if (!child->value.IsEmpty())
      f->Printf("%s:%s=%s\n", path.data, child->type.data, child->value.data);
    WriteNode(child.get(), path, f);
  }
}
void idTypeInfoTree::Write(idFile* f) const {
  if (f)
    WriteNode(&root, idStr(), f);
}
void idTypeInfoTree::DiffNode(const idTypeInfoNode* n, const idTypeInfoTree& other,
                              idTypeInfoTree& out, const idStr& base) const {
  for (const auto& child : n->children) {
    idStr path = base;
    if (!path.IsEmpty())
      path.Append(".");
    path.Append(child->name);
    const auto* right = other.FindPath(path.data);
    if (!right || right->value != child->value || right->type != child->type)
      out.Set(path.data, child->value.data, child->type.data);
    DiffNode(child.get(), other, out, path);
  }
}
void idTypeInfoTree::Diff(const idTypeInfoTree& o, idTypeInfoTree& out) const {
  out.Clear();
  DiffNode(&root, o, out, idStr());
}
void idTypeInfoTree::ForceRootDifferent() {
  root.value = "different";
}
idStr idTypeInfoTree::GetPath(const idTypeInfoNode* n) const {
  idStr path;
  while (n && n != &root) {
    if (path.IsEmpty())
      path = n->name;
    else {
      path.Insert(".", 0);
      path.Insert(n->name.data, 0);
    }
    n = n->parent;
  }
  return path;
}
void idTypeInfoTree::CollectTypes(const idTypeInfoNode* n, idList<idStr>& types) const {
  if (!n->type.IsEmpty()) {
    bool exists = false;
    for (int i = 0; i < types.Num(); ++i)
      if (types[i] == n->type) {
        exists = true;
        break;
      }
    if (!exists)
      types.Append(n->type);
  }
  for (const auto& c : n->children)
    CollectTypes(c.get(), types);
}
void idTypeInfoTree::GetTypes(idList<idStr>& types) const {
  CollectTypes(&root, types);
}
void idTypeInfoTree::ExtendListsWithDefaultsBasedOn(const idTypeInfoTree& defaults) {
  idTypeInfoTree missing;
  defaults.Diff(*this, missing);
  idFile_Memory file;
  missing.Write(&file);
  file.MakeReadOnly();
  std::vector<char> text(static_cast<std::size_t>(file.Length()) + 1);
  file.Read(text.data(), text.size() - 1);
  text.back() = 0;
  ParseOverwrite(text.data());
}
