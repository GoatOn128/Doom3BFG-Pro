#include "shared/idlib/filesystem/file_inzip.h"

idFile_InZip::idFile_InZip(const idZipArchive& archive, const idZipEntry& entry)
    : idFile_Memory(entry.name.data) {
  std::vector<std::uint8_t> data;
  valid = archive.Read(entry, data);
  if (valid && Write(data.data(), data.size()) == data.size())
    MakeReadOnly();
  else
    valid = false;
}
