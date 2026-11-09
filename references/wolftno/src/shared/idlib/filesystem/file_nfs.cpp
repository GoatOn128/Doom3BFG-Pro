#include "shared/idlib/filesystem/filesystemlocal.h"

#include <filesystem>

class idNfsClient {
public:
  enum NfsErr { NFS_OK, NFS_ERR_CON, NFS_ERR_NOENT, NFS_ERR_ACCESS, NFS_ERR_OTHER };
  explicit idNfsClient(const char* mount = ".") : mountPath(mount) {}
  idFile* Open(const char* path, fsMode_t mode) {
    idStr full((std::filesystem::path(mountPath.data) / path).string().c_str());
    return new idFile_Permanent(path, full.data, mode, mode != FS_READ);
  }
  bool Exists(const char* path) const {
    return std::filesystem::exists(std::filesystem::path(mountPath.data) / path);
  }

private:
  idStr mountPath;
};
