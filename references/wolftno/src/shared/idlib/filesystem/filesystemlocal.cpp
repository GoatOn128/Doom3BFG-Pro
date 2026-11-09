#include "shared/idlib/filesystem/filesystemlocal.h"

#include <filesystem>

idFileSystemLocal fileSystemLocal;
idFileSystem* fileSystem = &fileSystemLocal;
void idFileSystemLocal::Init(const char* root) {
  rootPath = root && *root ? root : ".";
}
void idFileSystemLocal::Shutdown() {}
idStr idFileSystemLocal::BuildOSPath(const char* path) const {
  std::filesystem::path value = rootPath.data;
  value /= path ? path : "";
  return idStr(value.lexically_normal().string().c_str());
}
idFile* idFileSystemLocal::OpenFileRead(const char* path) {
  const idStr os = BuildOSPath(path);
  auto* file = new idFile_Permanent(path, os.data, FS_READ, false);
  if (!file->IsOpen()) {
    delete file;
    return nullptr;
  }
  return file;
}
idFile* idFileSystemLocal::OpenFileWrite(const char* path) {
  const idStr os = BuildOSPath(path);
  std::error_code error;
  std::filesystem::create_directories(std::filesystem::path(os.data).parent_path(), error);
  auto* file = new idFile_Permanent(path, os.data, FS_WRITE, true);
  if (!file->IsOpen()) {
    delete file;
    return nullptr;
  }
  return file;
}
idFile* idFileSystemLocal::OpenFileAppend(const char* path, fsPath_t) {
  const idStr os = BuildOSPath(path);
  std::error_code error;
  std::filesystem::create_directories(std::filesystem::path(os.data).parent_path(), error);
  auto* file = new idFile_Permanent(path, os.data, FS_APPEND, true);
  if (!file->IsOpen()) {
    delete file;
    return nullptr;
  }
  return file;
}
bool idFileSystemLocal::ReadFile(const char* path, std::vector<std::uint8_t>& data) {
  std::unique_ptr<idFile> file(OpenFileRead(path));
  if (!file)
    return false;
  data.resize(static_cast<std::size_t>(file->Length()));
  return file->Read(data.data(), data.size()) == data.size();
}
bool idFileSystemLocal::WriteFile(const char* path, const void* data, std::size_t size) {
  std::unique_ptr<idFile> file(OpenFileWrite(path));
  return file && file->Write(data, size) == size;
}
bool idFileSystemLocal::FileExists(const char* path) const {
  return std::filesystem::is_regular_file(BuildOSPath(path).data);
}
std::uint32_t idFileSystemLocal::GetTimestamp(const char* path) const {
  std::uint32_t timestamp = 0;
  std::uint64_t size = 0;
  return idFile_Permanent::GetFilestat(BuildOSPath(path).data, &timestamp, &size) ? timestamp : 0;
}
bool idFileSystemLocal::RemoveFile(const char* path) {
  std::error_code error;
  return std::filesystem::remove(BuildOSPath(path).data, error);
}
bool idFileSystemLocal::RenameFile(const char* from, const char* to) {
  std::error_code error;
  std::filesystem::rename(BuildOSPath(from).data, BuildOSPath(to).data, error);
  return !error;
}
idFileList* idFileSystemLocal::ListFiles(const char* directory, const char* extension,
                                         bool recursive) const {
  auto* result = new idFileList;
  result->basePath = directory ? directory : "";
  const std::filesystem::path base = BuildOSPath(directory).data;
  std::error_code error;
  auto append = [&](const std::filesystem::directory_entry& entry) {
    if (!entry.is_regular_file())
      return;
    if (extension && *extension && entry.path().extension() != std::string(".") + extension)
      return;
    result->list.Append(
        idStr(std::filesystem::relative(entry.path(), base, error).generic_string().c_str()));
  };
  if (recursive) {
    for (const auto& e : std::filesystem::recursive_directory_iterator(base, error)) {
      if (error)
        break;
      append(e);
    }
  } else {
    for (const auto& e : std::filesystem::directory_iterator(base, error)) {
      if (error)
        break;
      append(e);
    }
  }
  return result;
}
