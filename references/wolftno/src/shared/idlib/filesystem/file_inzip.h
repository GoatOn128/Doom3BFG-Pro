#pragma once
#include "shared/idlib/filesystem/zip.h"
class idFile_InZip final : public idFile_Memory {
public:
  idFile_InZip(const idZipArchive& archive, const idZipEntry& entry);
  bool IsValid() const { return valid; }

private:
  bool valid{};
};
