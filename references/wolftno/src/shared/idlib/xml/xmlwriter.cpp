#include "xmlwriter.h"

#include <filesystem>
#include <fstream>
#include <string>

struct idXMLWriter::stackXMLTag_t {
  idStr tagName;
  stackXMLTag_t* next = nullptr;
};

namespace {
struct XMLFileState {
  std::ofstream stream;
};

std::string EscapeXML(const char* text, bool attribute) {
  std::string result;
  if (text == nullptr)
    return result;
  for (const char* scan = text; *scan != '\0'; ++scan) {
    switch (*scan) {
    case '&':
      result += "&amp;";
      break;
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '"':
      result += attribute ? "&quot;" : "\"";
      break;
    case '\'':
      result += attribute ? "&apos;" : "'";
      break;
    default:
      result += *scan;
      break;
    }
  }
  return result;
}

void Indent(std::ofstream& stream, int tabs) {
  for (int index = 0; index < tabs; ++index)
    stream << '\t';
}
} // namespace

idXMLWriter::idXMLWriter()
    : xmlFile(nullptr), tabLevel(0), hasRoot(false), openTags{nullptr, nullptr} {}

idXMLWriter::~idXMLWriter() {
  CloseFile();
}

bool idXMLWriter::CloseFile() {
  if (xmlFile == nullptr)
    return false;
  CloseDocument();
  auto* state = static_cast<XMLFileState*>(xmlFile);
  state->stream.flush();
  const bool good = state->stream.good();
  state->stream.close();
  delete state;
  xmlFile = nullptr;
  return good;
}

bool idXMLWriter::CloseElement() {
  if (xmlFile == nullptr || openTags.last == nullptr)
    return false;
  stackXMLTag_t* closing = openTags.last;
  stackXMLTag_t* previous = nullptr;
  for (stackXMLTag_t* node = openTags.first; node != closing; node = node->next)
    previous = node;
  openTags.last = previous;
  if (previous != nullptr)
    previous->next = nullptr;
  else
    openTags.first = nullptr;
  --tabLevel;
  auto& stream = static_cast<XMLFileState*>(xmlFile)->stream;
  Indent(stream, tabLevel);
  stream << "</" << closing->tagName.data << ">\n";
  delete closing;
  return stream.good();
}

bool idXMLWriter::CloseDocument() {
  while (openTags.last != nullptr) {
    if (!CloseElement())
      return false;
  }
  hasRoot = false;
  return true;
}

bool idXMLWriter::OpenFile(const char* relativePath, bool overwrite, bool savePath) {
  if (xmlFile != nullptr || relativePath == nullptr)
    return false;
  std::filesystem::path path(relativePath);
  if (savePath && path.has_parent_path()) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
      return false;
  }
  auto* state = new XMLFileState;
  state->stream.open(path, std::ios::binary | (overwrite ? std::ios::trunc : std::ios::app));
  if (!state->stream) {
    delete state;
    return false;
  }
  xmlFile = state;
  tabLevel = 0;
  hasRoot = false;
  state->stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
  return true;
}

bool idXMLWriter::OpenElement(const char* name) {
  idList<idXMLAttribute, 44> attributes;
  return OpenElement(name, attributes);
}

bool idXMLWriter::OpenElement(const char* name, const idList<idXMLAttribute, 44>& attributes) {
  if (xmlFile == nullptr || name == nullptr || *name == '\0' ||
      (hasRoot && openTags.last == nullptr))
    return false;
  auto& stream = static_cast<XMLFileState*>(xmlFile)->stream;
  Indent(stream, tabLevel);
  stream << '<' << name;
  for (int index = 0; index < attributes.Num(); ++index)
    stream << ' ' << attributes[index].name.data << "=\""
           << EscapeXML(attributes[index].value.data, true) << '"';
  stream << ">\n";
  auto* tag = new stackXMLTag_t;
  tag->tagName = name;
  if (openTags.last != nullptr)
    openTags.last->next = tag;
  else
    openTags.first = tag;
  openTags.last = tag;
  ++tabLevel;
  hasRoot = true;
  return stream.good();
}

bool idXMLWriter::WriteElement(const char* name, const char* value) {
  idList<idXMLAttribute, 44> attributes;
  return WriteElement(name, value, attributes);
}

bool idXMLWriter::WriteElement(const char* name, const char* value,
                               const idList<idXMLAttribute, 44>& attributes) {
  if (xmlFile == nullptr || name == nullptr || *name == '\0' ||
      (hasRoot && openTags.last == nullptr))
    return false;
  auto& stream = static_cast<XMLFileState*>(xmlFile)->stream;
  Indent(stream, tabLevel);
  stream << '<' << name;
  for (int index = 0; index < attributes.Num(); ++index)
    stream << ' ' << attributes[index].name.data << "=\""
           << EscapeXML(attributes[index].value.data, true) << '"';
  stream << '>' << EscapeXML(value, false) << "</" << name << ">\n";
  if (!hasRoot)
    hasRoot = true;
  return stream.good();
}
