#include "shared/idlib/filesystem/zip.h"

bool UnzipFile(const char* archivePath, const char* entryName, std::vector<std::uint8_t>& output) {
  idZipArchive archive;
  return archive.Open(archivePath) && archive.Read(entryName, output);
}
