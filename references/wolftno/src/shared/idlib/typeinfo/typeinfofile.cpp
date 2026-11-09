#include "shared/idlib/typeinfo/typeinfo.h"

#include <cstdarg>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

idTypeInfoSettings::idTypeInfoSettings() = default;
idTypeInfoFile::idTypeInfoFile(idFile* f, const idTypeInfoSettings& s) : file(f), settings(s) {
  SetFile(f);
}
void idTypeInfoFile::SetFile(idFile* f) {
  file = f;
  input.Clear();
  cursor = 0;
  if (file && file->Length() > 0) {
    std::vector<char> bytes(static_cast<std::size_t>(file->Length()) + 1);
    const auto old = file->Tell();
    file->Seek(0, FS_SEEK_SET);
    const auto count = file->Read(bytes.data(), bytes.size() - 1);
    bytes[count] = 0;
    input = bytes.data();
    file->Seek(old, FS_SEEK_SET);
  }
}
void idTypeInfoFile::OutputTabs() {
  if (file)
    for (int i = 0; i < tabs; ++i)
      file->Write("\t", 1);
}
void idTypeInfoFile::WriteOpeningBrace() {
  OutputTabs();
  if (file)
    file->Printf("{\n");
  ++tabs;
}
void idTypeInfoFile::WriteClosingBrace() {
  tabs = std::max(0, tabs - 1);
  OutputTabs();
  if (file)
    file->Printf("}\n");
}
void idTypeInfoFile::WriteSkipObject() {
  OutputTabs();
  if (file)
    file->Printf("skip\n");
}
bool idTypeInfoFile::Next(idStr& token) {
  while (cursor < static_cast<std::size_t>(input.len) &&
         std::isspace(static_cast<unsigned char>(input.data[cursor])))
    ++cursor;
  if (cursor >= static_cast<std::size_t>(input.len))
    return false;
  if (input.data[cursor] == '\"') {
    const std::size_t begin = ++cursor;
    while (cursor < static_cast<std::size_t>(input.len) && input.data[cursor] != '\"')
      ++cursor;
    token = idStr(input.data, static_cast<int>(begin), static_cast<int>(cursor));
    if (cursor < input.len)
      ++cursor;
    return true;
  }
  const std::size_t begin = cursor;
  while (cursor < static_cast<std::size_t>(input.len) &&
         !std::isspace(static_cast<unsigned char>(input.data[cursor])))
    ++cursor;
  token = idStr(input.data, static_cast<int>(begin), static_cast<int>(cursor));
  return true;
}
bool idTypeInfoFile::ReadOpeningBrace() {
  idStr t;
  return Next(t) && t == "{";
}
bool idTypeInfoFile::CheckOpeningBrace() {
  return ReadOpeningBrace();
}
bool idTypeInfoFile::ReadClosingBrace() {
  idStr t;
  return Next(t) && t == "}";
}
bool idTypeInfoFile::CheckClosingBrace() {
  return ReadClosingBrace();
}
bool idTypeInfoFile::PeekClosingBrace() {
  const auto saved = cursor;
  const bool result = ReadClosingBrace();
  cursor = saved;
  return result;
}
bool idTypeInfoFile::CheckSkipObject() {
  idStr t;
  const auto saved = cursor;
  if (Next(t) && t == "skip")
    return true;
  cursor = saved;
  return false;
}
void idTypeInfoFile::WriteValueString(const char* v) {
  OutputTabs();
  if (file)
    file->Printf("\"%s\"\n", v ? v : "");
}
bool idTypeInfoFile::ReadValueString(idStr& v) {
  return Next(v);
}
void idTypeInfoFile::WriteBool(bool v) {
  WriteValueString(v ? "1" : "0");
}
bool idTypeInfoFile::ReadBool(bool& v) {
  idStr s;
  if (!Next(s))
    return false;
  v = std::atoi(s.data) != 0;
  return true;
}
#define TI_SCALAR(Name, Type, Format, Parse)                                                       \
  void idTypeInfoFile::Write##Name(Type v) {                                                       \
    char b[128];                                                                                   \
    std::snprintf(b, sizeof(b), Format, v);                                                        \
    WriteValueString(b);                                                                           \
  }                                                                                                \
  bool idTypeInfoFile::Read##Name(Type& v) {                                                       \
    idStr s;                                                                                       \
    if (!Next(s))                                                                                  \
      return false;                                                                                \
    v = static_cast<Type>(Parse);                                                                  \
    return true;                                                                                   \
  }
TI_SCALAR(Char, char, "%d", std::strtol(s.data, nullptr, 0))
TI_SCALAR(UnsignedChar, unsigned char, "%u", std::strtoul(s.data, nullptr, 0))
    TI_SCALAR(WChar, wchar_t, "%u", std::strtoul(s.data, nullptr, 0))
        TI_SCALAR(Short, short, "%d", std::strtol(s.data, nullptr, 0))
            TI_SCALAR(UnsignedShort, unsigned short, "%u", std::strtoul(s.data, nullptr, 0))
                TI_SCALAR(Int, int, "%d", std::strtol(s.data, nullptr, 0))
                    TI_SCALAR(UnsignedInt, unsigned, "%u", std::strtoul(s.data, nullptr, 0))
                        TI_SCALAR(Long, long, "%ld", std::strtol(s.data, nullptr, 0))
                            TI_SCALAR(UnsignedLong, unsigned long, "%lu",
                                      std::strtoul(s.data, nullptr, 0))
                                TI_SCALAR(Float, float, "%.9g", std::strtof(s.data, nullptr))
                                    TI_SCALAR(Double, double, "%.17g", std::strtod(s.data, nullptr))
#undef TI_SCALAR
                                        void idTypeInfoFile::WriteStr(const idStr& v) {
  WriteValueString(v.data);
}
bool idTypeInfoFile::ReadStr(idStr& v) {
  return ReadValueString(v);
}
void idTypeInfoFile::WriteVec3(const idVec3& v) {
  char b[128];
  std::snprintf(b, sizeof(b), "%g,%g,%g", v.x, v.y, v.z);
  WriteValueString(b);
}
bool idTypeInfoFile::ReadVec3(idVec3& v) {
  idStr s;
  if (!Next(s))
    return false;
  return std::sscanf(s.data, "%f,%f,%f", &v.x, &v.y, &v.z) == 3;
}
void idTypeInfoFile::WriteVecX(const idVecX& v) {
  idStr s;
  for (int i = 0; i < v.size; ++i) {
    if (i)
      s.Append(",");
    s.Append(idStr(v[i]));
  }
  WriteValueString(s.data);
}
bool idTypeInfoFile::ReadVecX(idVecX& v) {
  idStr s;
  if (!Next(s))
    return false;
  int count = 1;
  for (int i = 0; i < s.len; ++i)
    if (s[i] == ',')
      ++count;
  v.SetSize(count);
  const char* p = s.data;
  for (int i = 0; i < count; ++i) {
    char* end = nullptr;
    v[i] = std::strtof(p, &end);
    p = end;
    if (*p == ',')
      ++p;
  }
  return true;
}
void idTypeInfoFile::WriteMatX(const idMatX& m) {
  WriteInt(m.numRows);
  WriteInt(m.numColumns);
  idVecX v(m.numRows * m.numColumns);
  std::memcpy(v.p, m.mat, sizeof(float) * v.size);
  WriteVecX(v);
}
bool idTypeInfoFile::ReadMatX(idMatX& m) {
  int r, c;
  if (!ReadInt(r) || !ReadInt(c))
    return false;
  idVecX v;
  if (!ReadVecX(v) || v.size != r * c)
    return false;
  m.SetSize(r, c);
  std::memcpy(m.mat, v.p, sizeof(float) * v.size);
  return true;
}
void idTypeInfoFile::WriteAngles(const idAngles& v) {
  WriteVec3({v.pitch, v.yaw, v.roll});
}
bool idTypeInfoFile::ReadAngles(idAngles& v) {
  return ReadVec3(*reinterpret_cast<idVec3*>(&v));
}
void idTypeInfoFile::WriteQuat(const idQuat& q) {
  char b[160];
  std::snprintf(b, sizeof(b), "%g,%g,%g,%g", q.x, q.y, q.z, q.w);
  WriteValueString(b);
}
bool idTypeInfoFile::ReadQuat(idQuat& q) {
  idStr s;
  return Next(s) && std::sscanf(s.data, "%f,%f,%f,%f", &q.x, &q.y, &q.z, &q.w) == 4;
}
void idTypeInfoFile::WriteColor(const idVec4& c) {
  WriteQuat(*reinterpret_cast<const idQuat*>(&c));
}
bool idTypeInfoFile::ReadColor(idVec4& c) {
  return ReadQuat(*reinterpret_cast<idQuat*>(&c));
}
void idTypeInfoFile::WriteNullPointer() {
  WriteValueString("null");
}
bool idTypeInfoFile::ReadNullPointer() {
  idStr s;
  return Next(s) && s == "null";
}
bool idTypeInfoFile::CheckNullPointer() {
  const auto save = cursor;
  const bool r = ReadNullPointer();
  cursor = save;
  return r;
}
void idTypeInfoFile::WriteUnknown(const char* s) {
  WriteValueString(s ? s : "unknown");
}
bool idTypeInfoFile::ReadUnknown(idStr& s) {
  return Next(s);
}
bool idTypeInfoFile::CheckUnknown() {
  idStr s;
  const auto save = cursor;
  const bool r = Next(s) && s == "unknown";
  cursor = save;
  return r;
}
void idTypeInfoFile::WriteComment(const char* s) {
  OutputTabs();
  if (file)
    file->Printf("// %s\n", s ? s : "");
}
bool idTypeInfoFile::ReadComment(idStr& s) {
  return Next(s);
}
void idTypeInfoFile::WriteType(const char* t, const char* n) {
  OutputTabs();
  if (file)
    file->Printf("%s %s\n", t ? t : "", n ? n : "");
}
bool idTypeInfoFile::ReadType(idStr& t, idStr& n) {
  return Next(t) && Next(n);
}
bool idTypeInfoFile::CheckType(const char* t, const char* n) {
  const auto save = cursor;
  idStr a, b;
  const bool r = ReadType(a, b) && a == t && b == n;
  cursor = save;
  return r;
}
bool idTypeInfoFile::ExpectType(const char* t, const char* n) {
  if (CheckType(t, n)) {
    idStr a, b;
    ReadType(a, b);
    return true;
  }
  Error("expected type %s %s", t, n);
  return false;
}
void idTypeInfoFile::WriteArrayElementType(const char* t, int i) {
  OutputTabs();
  if (file)
    file->Printf("%s [%d]\n", t ? t : "", i);
}
bool idTypeInfoFile::ReadArrayElementType(idStr& t, int& i) {
  idStr s;
  if (!Next(t) || !Next(s))
    return false;
  i = std::atoi(s.data + 1);
  return true;
}
bool idTypeInfoFile::CheckArrayElementType(const char* t, int i) {
  const auto save = cursor;
  idStr a;
  int b;
  const bool r = ReadArrayElementType(a, b) && a == t && b == i;
  cursor = save;
  return r;
}
bool idTypeInfoFile::ExpectArrayElementType(const char* t, int i) {
  if (CheckArrayElementType(t, i)) {
    idStr a;
    int b;
    ReadArrayElementType(a, b);
    return true;
  }
  return false;
}
void idTypeInfoFile::WriteMemory(const void* data, std::size_t size) {
  static const char hex[] = "0123456789abcdef";
  idStr s;
  s.EnsureAlloced(static_cast<int>(size * 2 + 1));
  for (std::size_t i = 0; i < size; ++i) {
    const auto b = static_cast<const unsigned char*>(data)[i];
    s.Append(hex[b >> 4]);
    s.Append(hex[b & 15]);
  }
  WriteValueString(s.data);
}
bool idTypeInfoFile::ReadMemory(void* data, std::size_t size) {
  idStr s;
  if (!Next(s) || s.len != static_cast<int>(size * 2))
    return false;
  for (std::size_t i = 0; i < size; ++i) {
    char b[3] = {s[static_cast<int>(i * 2)], s[static_cast<int>(i * 2 + 1)], 0};
    static_cast<unsigned char*>(data)[i] = static_cast<unsigned char>(std::strtoul(b, nullptr, 16));
  }
  return true;
}
void idTypeInfoFile::WriteMemoryFile(idFile_Memory& f) {
  WriteMemory(f.GetDataPtr(), static_cast<std::size_t>(f.Length()));
}
bool idTypeInfoFile::ReadMemoryFile(idFile_Memory& f) {
  idStr s;
  if (!Next(s))
    return false;
  std::vector<unsigned char> d(s.len / 2);
  cursor -= s.len + 2;
  if (!ReadMemory(d.data(), d.size()))
    return false;
  f.Clear();
  return f.Write(d.data(), d.size()) == d.size();
}
void idTypeInfoFile::Error(const char* format, ...) {
  hadError = true;
  char b[1024];
  va_list a;
  va_start(a, format);
  std::vsnprintf(b, sizeof(b), format, a);
  va_end(a);
  scratch = b;
}
void idTypeInfoFile::Warning(const char* format, ...) {
  char b[1024];
  va_list a;
  va_start(a, format);
  std::vsnprintf(b, sizeof(b), format, a);
  va_end(a);
  scratch = b;
}
