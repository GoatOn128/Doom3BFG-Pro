#pragma once

#include "shared/idlib/filesystem/file.h"

#include <cstdint>
#include <vector>

struct idZipEntry {
  idStr name;
  std::uint16_t method{};
  std::uint32_t crc{}, compressedSize{}, uncompressedSize{}, dataOffset{};
};
class idZipArchive {
public:
  bool Open(const char* path);
  bool Open(const void* data, std::size_t size);
  int NumEntries() const { return static_cast<int>(entries.size()); }
  const idZipEntry* Find(const char* name) const;
  bool Read(const idZipEntry& entry, std::vector<std::uint8_t>& output) const;
  bool Read(const char* name, std::vector<std::uint8_t>& output) const;
  const std::vector<idZipEntry>& Entries() const { return entries; }

private:
  std::vector<std::uint8_t> bytes;
  std::vector<idZipEntry> entries;
};
class idZipBuilder {
public:
  void AddFile(const char* name, const void* data, std::size_t size);
  bool Save(const char* path, bool compress = true) const;

private:
  struct file_t {
    idStr name;
    std::vector<std::uint8_t> data;
  };
  std::vector<file_t> files;
};
