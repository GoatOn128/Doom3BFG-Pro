#include "shared/idlib/filesystem/filesystemlocal.h"

#include <filesystem>
class idMtpStorage {
public:
  explicit idMtpStorage(const char* root) : rootPath(root) {}
  bool Put(const char* path, const void* data, std::size_t size) {
    std::filesystem::path target = std::filesystem::path(rootPath.data) / path;
    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    idFile_Permanent file(path, target.string().c_str(), FS_WRITE, true);
    return file.IsOpen() && file.Write(data, size) == size;
  }
  bool Get(const char* path, std::vector<std::uint8_t>& data) {
    std::filesystem::path target = std::filesystem::path(rootPath.data) / path;
    idFile_Permanent file(path, target.string().c_str(), FS_READ, false);
    if (!file.IsOpen())
      return false;
    data.resize(static_cast<std::size_t>(file.Length()));
    return file.Read(data.data(), data.size()) == data.size();
  }

private:
  idStr rootPath;
};
