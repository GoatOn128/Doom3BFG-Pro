#include "dict.h"

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/hashing/hashing.h"
#include "shared/idlib/text/lexer.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string Trim(std::string text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

std::string EscapeJSON(const char* text) {
  std::string result;
  for (const unsigned char value : std::string(text ? text : "")) {
    switch (value) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (value < 0x20) {
        char escaped[8];
        idStr::snPrintf(escaped, sizeof(escaped), "\\u%04x", value);
        result += escaped;
      } else
        result.push_back(static_cast<char>(value));
    }
  }
  return result;
}
} // namespace

idStr GetNameFromLocationInPath(const idStr& path, int& location) {
  const int length = path.Length();
  while (location < length && (path[location] == '/' || path[location] == '\\'))
    ++location;
  const int start = location;
  while (location < length && path[location] != '/' && path[location] != '\\')
    ++location;
  return path.Mid(start, location - start);
}

bool IsChild(const idStr& child, const idStr& parent) {
  if (parent.IsEmpty() || child.Length() <= parent.Length())
    return false;
  if (idStr::IcmpnPath(child.data, parent.data, parent.Length()) != 0)
    return false;
  return child[parent.Length()] == '/' || child[parent.Length()] == '\\';
}

idDict::idDict() : argHash(128, 16) {}
idDict::idDict(const idDict& other)
    : args(other.args), argHash(128, std::max(16, other.args.Num())) {
  RebuildHash();
}

idDict& idDict::operator=(const idDict& other) {
  if (this != &other) {
    args = other.args;
    RebuildHash();
  }
  return *this;
}

void idDict::RebuildHash() {
  argHash.InternalFree();
  argHash.InternalInit(128, std::max(16, args.Num()));
  if (args.Num() == 0)
    return;
  argHash.InternalAllocate(128, std::max(16, args.Num()));
  for (int index = 0; index < args.Num(); ++index)
    argHash.Add(idStr::IHash(args[index].key.data), index);
}

void idDict::Print() const {
  std::printf("Num args: %d\n", args.Num());
  for (int index = 0; index < args.Num(); ++index)
    std::printf("%s = %s\n", args[index].key.data, args[index].value.data);
}

int idDict::FindKeyIndex(const char* key) const {
  if (!key || !*key)
    return -1;
  for (int index = argHash.First(idStr::IHash(key)); index >= 0; index = argHash.Next(index))
    if (index < args.Num() && idStr::Icmp(args[index].key.data, key) == 0)
      return index;
  return -1;
}

const idKeyValue* idDict::FindKey(const char* key) const {
  const int index = FindKeyIndex(key);
  return index >= 0 ? &args[index] : nullptr;
}

bool idDict::Set(const char* key, const char* value) {
  if (!key || !*key)
    return false;
  if (!value)
    value = "";
  const int index = FindKeyIndex(key);
  if (index >= 0) {
    if (idStr::Icmp(args[index].value.data, value) == 0)
      return false;
    args[index].value = value;
    return true;
  }
  idKeyValue pair;
  pair.key = key;
  pair.value = value;
  const int newIndex = args.Append(pair);
  argHash.Add(idStr::IHash(key), newIndex);
  return true;
}

void idDict::SetInt(const char* key, int value) {
  Set(key, idStr(value).data);
}
void idDict::SetFloat(const char* key, float value) {
  Set(key, idStr(value).data);
}
void idDict::SetBool(const char* key, bool value) {
  Set(key, value ? "1" : "0");
}

const char* idDict::GetString(const char* key, const char* defaultValue) const {
  const idKeyValue* pair = FindKey(key);
  return pair ? pair->value.data : defaultValue;
}
bool idDict::GetString(const char* key, const char* defaultValue, const char** output) const {
  const idKeyValue* pair = FindKey(key);
  if (output)
    *output = pair ? pair->value.data : defaultValue;
  return pair != nullptr;
}
bool idDict::GetFloat(const char* key, float defaultValue, float* output) const {
  const idKeyValue* pair = FindKey(key);
  if (output)
    *output = pair ? std::strtof(pair->value.data, nullptr) : defaultValue;
  return pair != nullptr;
}
bool idDict::GetInt(const char* key, int defaultValue, int* output) const {
  const idKeyValue* pair = FindKey(key);
  if (output)
    *output = pair ? std::strtol(pair->value.data, nullptr, 10) : defaultValue;
  return pair != nullptr;
}
bool idDict::GetBool(const char* key, bool defaultValue, bool* output) const {
  const idKeyValue* pair = FindKey(key);
  if (output)
    *output = pair ? (idStr::Icmp(pair->value.data, "true") == 0 ||
                      std::strtol(pair->value.data, nullptr, 10) != 0)
                   : defaultValue;
  return pair != nullptr;
}

void idDict::Delete(const char* key) {
  const int index = FindKeyIndex(key);
  if (index < 0)
    return;
  for (int current = index; current + 1 < args.Num(); ++current)
    args[current] = std::move(args[current + 1]);
  args[args.Num() - 1] = idKeyValue{};
  --args.num;
  RebuildHash();
}

void idDict::Clear() {
  for (int index = 0; index < args.Num(); ++index)
    args[index] = idKeyValue{};
  args.Clear();
  argHash.InternalFree();
  argHash.InternalInit(128, 16);
}

std::uint32_t idDict::Checksum() const {
  std::vector<const idKeyValue*> sorted;
  sorted.reserve(args.Num());
  for (int index = 0; index < args.Num(); ++index)
    sorted.push_back(&args[index]);
  std::sort(sorted.begin(), sorted.end(), [](const idKeyValue* left, const idKeyValue* right) {
    return idStr::Icmp(left->key.data, right->key.data) < 0;
  });
  std::uint32_t checksum;
  CRC32_InitChecksum(&checksum);
  for (const idKeyValue* pair : sorted) {
    CRC32_UpdateChecksum(&checksum, pair->key.data, pair->key.Length());
    CRC32_UpdateChecksum(&checksum, pair->value.data, pair->value.Length());
  }
  CRC32_FinishChecksum(&checksum);
  return checksum;
}

void idDict::WriteToIniFile(idFile* file) const {
  if (!file)
    return;
  std::vector<const idKeyValue*> sorted;
  for (int index = 0; index < args.Num(); ++index)
    sorted.push_back(&args[index]);
  std::sort(sorted.begin(), sorted.end(), [](const idKeyValue* left, const idKeyValue* right) {
    return idStr::IcmpPath(left->key.data, right->key.data) < 0;
  });
  std::string section;
  for (const idKeyValue* pair : sorted) {
    std::string key(pair->key.data);
    const std::size_t slash = key.find_last_of("/\\");
    const std::string nextSection = slash == std::string::npos ? "" : key.substr(0, slash);
    const std::string leaf = slash == std::string::npos ? key : key.substr(slash + 1);
    if (nextSection != section) {
      if (!section.empty() || file->Tell() > 0)
        file->Printf("\n");
      if (!nextSection.empty())
        file->Printf("[%s]\n", nextSection.c_str());
      section = nextSection;
    }
    file->Printf("%s=%s\n", leaf.c_str(), pair->value.data);
  }
}

bool idDict::ReadFromIniFile(idFile* file) {
  if (!file || file->Length() < 0)
    return false;
  std::string contents(static_cast<std::size_t>(file->Length()), '\0');
  file->Seek(0, FS_SEEK_SET);
  contents.resize(file->Read(contents.data(), contents.size()));
  Clear();
  std::istringstream stream(contents);
  std::string line, section;
  while (std::getline(stream, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == ';' || line[0] == '#')
      continue;
    if (line.front() == '[' && line.back() == ']') {
      section = Trim(line.substr(1, line.size() - 2));
      continue;
    }
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos)
      continue;
    const std::string leaf = Trim(line.substr(0, equals));
    const std::string value = Trim(line.substr(equals + 1));
    const std::string key = section.empty() ? leaf : section + "/" + leaf;
    Set(key.c_str(), value.c_str());
  }
  return true;
}

idStr idDict::WriteToJSONString() const {
  idStr result;
  return WriteToJSONString(result);
}

idStr& idDict::WriteToJSONString(idStr& result) const {
  struct Node {
    std::map<std::string, Node> children;
    const idStr* value = nullptr;
  } root;
  for (int index = 0; index < args.Num(); ++index) {
    Node* node = &root;
    std::string path(args[index].key.data);
    std::size_t position = 0;
    while (position <= path.size()) {
      const std::size_t slash = path.find_first_of("/\\", position);
      const std::string part =
          path.substr(position, slash == std::string::npos ? std::string::npos : slash - position);
      if (!part.empty())
        node = &node->children[part];
      if (slash == std::string::npos)
        break;
      position = slash + 1;
    }
    node->value = &args[index].value;
  }
  std::function<void(const Node&, std::string&, int)> writeNode;
  writeNode = [&](const Node& node, std::string& output, int depth) {
    output += "{";
    bool first = true;
    for (const auto& entry : node.children) {
      if (!first)
        output += ",";
      output +=
          "\n" + std::string((depth + 1) * 2, ' ') + "\"" + EscapeJSON(entry.first.c_str()) + "\":";
      if (!entry.second.children.empty())
        writeNode(entry.second, output, depth + 1);
      else
        output += "\"" + EscapeJSON(entry.second.value ? entry.second.value->data : "") + "\"";
      first = false;
    }
    if (!node.children.empty())
      output += "\n" + std::string(depth * 2, ' ');
    output += "}";
  };
  std::string output;
  writeNode(root, output, 0);
  result = output.c_str();
  return result;
}

bool idDict::ReadJSONObject(const idStr& parentName, idLexer* parser) {
  idToken token;
  if (!parser->ExpectTokenString("{"))
    return false;
  if (!parser->ReadToken(token))
    return false;
  if (token == "}")
    return true;
  while (true) {
    if (token.type != TT_STRING && token.type != TT_NAME)
      return false;
    idStr key = parentName;
    if (!key.IsEmpty())
      key.Append("/");
    key.Append(token.data);
    if (!parser->ExpectTokenString(":"))
      return false;
    if (!parser->ReadToken(token))
      return false;
    if (token == "{") {
      parser->UnreadToken();
      if (!ReadJSONObject(key, parser))
        return false;
    } else if (token == "[") {
      int arrayIndex = 0;
      if (!parser->ReadToken(token))
        return false;
      while (token != "]") {
        idStr arrayKey;
        arrayKey.Format("%s/%d", key.data, arrayIndex++);
        if (token == "{") {
          parser->UnreadToken();
          if (!ReadJSONObject(arrayKey, parser))
            return false;
        } else
          Set(arrayKey.data, token.data);
        if (!parser->ReadToken(token))
          return false;
        if (token == "]")
          break;
        if (token != "," || !parser->ReadToken(token))
          return false;
      }
    } else
      Set(key.data, token.data);
    if (!parser->ReadToken(token))
      return false;
    if (token == "}")
      return true;
    if (token != "," || !parser->ReadToken(token))
      return false;
  }
}

bool idDict::ReadFromJSONString(const idStr& text) {
  idLexer parser;
  if (!parser.LoadMemory(text.data, text.Length(), "JSON reader"))
    return false;
  idDict parsed;
  idStr root;
  if (!parsed.ReadJSONObject(root, &parser) || parser.HadError())
    return false;
  *this = parsed;
  return true;
}

bool idDict::Parse(idLexer* parser, const char* sectionName) {
  if (!parser)
    return false;
  if (sectionName && !parser->ExpectTokenString(sectionName))
    return false;
  if (!parser->ExpectTokenString("{"))
    return false;
  bool unique = true;
  idToken key, value;
  while (parser->ReadToken(key)) {
    if (key == "}")
      return unique;
    if (key.type != TT_STRING) {
      parser->Error("Expected quoted string, but found '%s'", key.data);
      return false;
    }
    if (!parser->ReadToken(value)) {
      parser->Error("Unexpected end of file");
      return false;
    }
    if (FindKey(key.data)) {
      parser->Warning("'%s' already defined", key.data);
      unique = false;
    }
    Set(key.data, value.data);
  }
  parser->Error("Unexpected end of file");
  return false;
}
