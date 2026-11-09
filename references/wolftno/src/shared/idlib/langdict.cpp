#include "langdict.h"

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/text/lexer.h"
#include "shared/idlib/text/str.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
constexpr const char* KEY_PREFIX = "#str_";
constexpr int KEY_PREFIX_LEN = 5;

std::string EscapeLanguageValue(const char* text) {
  std::string result;
  for (const char value : std::string(text ? text : "")) {
    if (value == '\\')
      result += "\\\\";
    else if (value == '"')
      result += "\\\"";
    else if (value == '\n')
      result += "\\n";
    else if (value == '\r')
      result += "\\r";
    else if (value == '\t')
      result += "\\t";
    else
      result.push_back(value);
  }
  return result;
}
} // namespace

namespace idLocalization {
idLangDict languageDict;
}

bool idLangDict::HasStrPrefix(const char* text) {
  return text && idStr::Icmpn(text, KEY_PREFIX, KEY_PREFIX_LEN) == 0;
}

std::uint32_t idLangDict::ComputeID(const char* key) {
  std::uint32_t hash = 2166136261u;
  if (!key)
    return hash;
  while (*key) {
    hash = 16777619u *
           (static_cast<std::uint32_t>(idStr::ToLower(static_cast<unsigned char>(*key))) ^ hash);
    ++key;
  }
  return hash == 0 ? 1 : hash;
}

int idLangDict::IndexFromKey(const char* key) const {
  if (!key || !*key)
    return -1;
  const std::uint32_t id = ComputeID(key);
  int low = 0, high = dictionary.Num();
  while (low < high) {
    const int middle = low + (high - low) / 2;
    if (dictionary[middle].id < id)
      low = middle + 1;
    else
      high = middle;
  }
  for (int index = low; index < dictionary.Num() && dictionary[index].id == id; ++index)
    if (idStr::Icmp(dictionary[index].key.str, key) == 0)
      return index;
  return -1;
}

const char* idLangDict::FindString(const char* key) const {
  const int index = IndexFromKey(key);
  return index >= 0 ? dictionary[index].value.str : nullptr;
}

const char* idLangDict::GetString(const char* key) const {
  const char* localized = FindString(key);
  return localized ? localized : key ? key : "";
}

bool idLangDict::Add(const char* key, const char* value) {
  if (!key || !*key)
    return false;
  const int oldIndex = IndexFromKey(key);
  if (oldIndex >= 0) {
    if (idStr::Cmp(dictionary[oldIndex].value.str, value ? value : "") == 0)
      return false;
    dictionary[oldIndex].value.Set(value ? value : "");
    return true;
  }
  langEntry_t entry;
  entry.id = ComputeID(key);
  entry.key.Set(key);
  entry.value.Set(value ? value : "");
  dictionary.Append(entry);
  std::sort(dictionary.list, dictionary.list + dictionary.num,
            [](const langEntry_t& left, const langEntry_t& right) {
              if (left.id != right.id)
                return left.id < right.id;
              return idStr::Icmp(left.key.str, right.key.str) < 0;
            });
  return true;
}

bool idLangDict::Load(const char* buffer, int bufferLength, const char* name) {
  if (!buffer || bufferLength < 0)
    return false;
  if (bufferLength >= 3 && static_cast<unsigned char>(buffer[0]) == 0xEF &&
      static_cast<unsigned char>(buffer[1]) == 0xBB &&
      static_cast<unsigned char>(buffer[2]) == 0xBF) {
    buffer += 3;
    bufferLength -= 3;
  }
  idLexer lexer;
  if (!lexer.LoadMemory(buffer, static_cast<std::size_t>(bufferLength),
                        name ? name : "language dictionary"))
    return false;
  if (!lexer.ExpectTokenString("{"))
    return false;
  idList<langEntry_t, 119> parsed;
  idToken key, value;
  while (lexer.ReadToken(key)) {
    if (key == "}") {
      dictionary = parsed;
      std::sort(dictionary.list, dictionary.list + dictionary.num,
                [](const langEntry_t& left, const langEntry_t& right) {
                  if (left.id != right.id)
                    return left.id < right.id;
                  return idStr::Icmp(left.key.str, right.key.str) < 0;
                });
      return true;
    }
    if (key.type != TT_STRING || !lexer.ReadToken(value) || value.type != TT_STRING) {
      lexer.Error("expected quoted language key and value");
      return false;
    }
    langEntry_t entry;
    entry.id = ComputeID(key.data);
    entry.key.Set(key.data);
    entry.value.Set(value.data);
    bool duplicate = false;
    for (int index = 0; index < parsed.Num(); ++index)
      if (idStr::Icmp(parsed[index].key.str, key.data) == 0) {
        parsed[index] = entry;
        duplicate = true;
        break;
      }
    if (!duplicate)
      parsed.Append(entry);
  }
  lexer.Error("file ended before closing language dictionary");
  return false;
}

bool idLangDict::Save(const char* fileName) const {
  if (!fileName || !*fileName)
    return false;
  idFile_Permanent file(fileName, fileName, FS_WRITE, true);
  if (!file.IsOpen())
    return false;
  const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
  if (file.Write(bom, sizeof(bom)) != sizeof(bom))
    return false;
  file.WriteFloatString("// string table\n//\n\n{\n");
  std::vector<const langEntry_t*> sorted;
  sorted.reserve(dictionary.Num());
  for (int index = 0; index < dictionary.Num(); ++index)
    sorted.push_back(&dictionary[index]);
  std::sort(sorted.begin(), sorted.end(), [](const langEntry_t* left, const langEntry_t* right) {
    return idStr::Icmp(left->key.str, right->key.str) < 0;
  });
  for (const langEntry_t* entry : sorted) {
    const std::string escaped = EscapeLanguageValue(entry->value.str);
    file.WriteFloatString("\t\"%s\"\t\"%s\"\n", entry->key.str, escaped.c_str());
  }
  file.WriteFloatString("\n}\n");
  file.ForceFlush();
  return true;
}

void idStrId::Set(const char* key) {
  const int index = idLocalization::languageDict.IndexFromKey(key);
  id = index >= 0 ? idLocalization::languageDict.dictionary[index].id : 0;
}

const char* idStrId::GetKey() const {
  for (int index = 0; index < idLocalization::languageDict.dictionary.Num(); ++index)
    if (idLocalization::languageDict.dictionary[index].id == id)
      return idLocalization::languageDict.dictionary[index].key.str;
  return "";
}

const char* idStrId::GetLocalizedString() const {
  for (int index = 0; index < idLocalization::languageDict.dictionary.Num(); ++index)
    if (idLocalization::languageDict.dictionary[index].id == id)
      return idLocalization::languageDict.dictionary[index].value.str;
  return "";
}
