#include "shared/idlib/filesystem/cassystemlocal.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

idCasSystemLocal casSystemLocal;
idCasSystem* casSystem = &casSystemLocal;
idStr idContentAddress::ToString() const {
  static constexpr char hex[] = "0123456789abcdef";
  char text[33];
  for (int i = 0; i < 16; ++i) {
    text[i * 2] = hex[digest[i] >> 4];
    text[i * 2 + 1] = hex[digest[i] & 15];
  }
  text[32] = 0;
  return idStr(text);
}
bool idContentAddress::IsValid() const {
  return std::any_of(digest.begin(), digest.end(), [](std::uint8_t b) { return b != 0; });
}
idContentAddress idContentAddress::FromData(const void* data, std::size_t size) {
  MD5_CTX context{};
  MD5_Init(&context);
  MD5_Update(&context, static_cast<const std::uint8_t*>(data), size);
  idContentAddress result;
  MD5_Final(&context, result.digest.data());
  return result;
}
void idCasSystemLocal::Init(const char* root) {
  rootPath = root && *root ? root : "cas";
  std::error_code error;
  std::filesystem::create_directories(rootPath.data, error);
}
idStr idCasSystemLocal::PathFor(const idContentAddress& a) const {
  const idStr hex = a.ToString();
  std::filesystem::path path = rootPath.data;
  path /= std::string(hex.data, 2);
  path /= hex.data;
  return idStr(path.string().c_str());
}
idContentAddress idCasSystemLocal::Store(const void* data, std::size_t size) {
  const idContentAddress address = idContentAddress::FromData(data, size);
  const std::filesystem::path path = PathFor(address).data;
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (!std::filesystem::exists(path)) {
    std::ofstream file(path, std::ios::binary);
    file.write(static_cast<const char*>(data), size);
  }
  return address;
}
idFile* idCasSystemLocal::OpenCasFileRead(const idContentAddress& a) {
  const idStr path = PathFor(a);
  auto* file = new idFile_Permanent(a.ToString().data, path.data, FS_READ, false);
  if (!file->IsOpen()) {
    delete file;
    return nullptr;
  }
  return file;
}
bool idCasSystemLocal::Contains(const idContentAddress& a) const {
  return std::filesystem::is_regular_file(PathFor(a).data);
}
bool idCasSystemLocal::MigrateFile(const idContentAddress& a, const char* destination) const {
  const std::filesystem::path source = PathFor(a).data,
                              target = std::filesystem::path(destination) / a.ToString().data;
  std::error_code error;
  std::filesystem::create_directories(target.parent_path(), error);
  std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing,
                             error);
  return !error;
}
