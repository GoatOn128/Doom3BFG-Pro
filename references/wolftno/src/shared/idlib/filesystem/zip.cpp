#include "shared/idlib/filesystem/zip.h"
#include "shared/idlib/hashing/hashing.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <zlib.h>

namespace {
std::uint16_t U16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | p[1] << 8);
}
std::uint32_t U32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(U16(p) | (static_cast<std::uint32_t>(U16(p + 2)) << 16));
}
void P16(std::vector<std::uint8_t>& o, std::uint16_t v) {
  o.push_back(v & 255);
  o.push_back(v >> 8);
}
void P32(std::vector<std::uint8_t>& o, std::uint32_t v) {
  P16(o, v & 65535);
  P16(o, v >> 16);
}
bool Deflate(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>& out) {
  z_stream s{};
  if (deflateInit2(&s, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    return false;
  out.resize(compressBound(static_cast<uLong>(in.size())));
  s.next_in = const_cast<Bytef*>(in.data());
  s.avail_in = static_cast<uInt>(in.size());
  s.next_out = out.data();
  s.avail_out = static_cast<uInt>(out.size());
  const int status = deflate(&s, Z_FINISH);
  out.resize(s.total_out);
  deflateEnd(&s);
  return status == Z_STREAM_END;
}
bool Inflate(const std::uint8_t* in, std::size_t inSize, std::vector<std::uint8_t>& out,
             std::size_t outSize) {
  out.resize(outSize);
  z_stream s{};
  if (inflateInit2(&s, -MAX_WBITS) != Z_OK)
    return false;
  s.next_in = const_cast<Bytef*>(in);
  s.avail_in = static_cast<uInt>(inSize);
  s.next_out = out.data();
  s.avail_out = static_cast<uInt>(out.size());
  const int status = inflate(&s, Z_FINISH);
  inflateEnd(&s);
  return status == Z_STREAM_END;
}
} // namespace
bool idZipArchive::Open(const char* path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return false;
  bytes.assign(std::istreambuf_iterator<char>(file), {});
  return Open(bytes.data(), bytes.size());
}
bool idZipArchive::Open(const void* data, std::size_t size) {
  if (data != bytes.data())
    bytes.assign(static_cast<const std::uint8_t*>(data),
                 static_cast<const std::uint8_t*>(data) + size);
  entries.clear();
  if (bytes.size() < 22)
    return false;
  std::size_t end = bytes.size() - 22;
  while (end > 0 && U32(bytes.data() + end) != 0x06054b50u)
    --end;
  if (U32(bytes.data() + end) != 0x06054b50u)
    return false;
  const unsigned count = U16(bytes.data() + end + 10);
  std::size_t p = U32(bytes.data() + end + 16);
  for (unsigned i = 0; i < count; ++i) {
    if (p + 46 > bytes.size() || U32(bytes.data() + p) != 0x02014b50u)
      return false;
    idZipEntry e;
    e.method = U16(bytes.data() + p + 10);
    e.crc = U32(bytes.data() + p + 16);
    e.compressedSize = U32(bytes.data() + p + 20);
    e.uncompressedSize = U32(bytes.data() + p + 24);
    const unsigned nameLen = U16(bytes.data() + p + 28), extra = U16(bytes.data() + p + 30),
                   comment = U16(bytes.data() + p + 32);
    e.name = idStr(reinterpret_cast<const char*>(bytes.data() + p + 46), 0, nameLen);
    const std::size_t local = U32(bytes.data() + p + 42);
    if (local + 30 > bytes.size())
      return false;
    e.dataOffset = static_cast<std::uint32_t>(local + 30 + U16(bytes.data() + local + 26) +
                                              U16(bytes.data() + local + 28));
    entries.push_back(e);
    p += 46 + nameLen + extra + comment;
  }
  return true;
}
const idZipEntry* idZipArchive::Find(const char* name) const {
  for (const auto& e : entries)
    if (idStr::IcmpPath(e.name.data, name) == 0)
      return &e;
  return nullptr;
}
bool idZipArchive::Read(const char* name, std::vector<std::uint8_t>& out) const {
  const auto* entry = Find(name);
  return entry && Read(*entry, out);
}
bool idZipArchive::Read(const idZipEntry& e, std::vector<std::uint8_t>& out) const {
  if (static_cast<std::size_t>(e.dataOffset) + e.compressedSize > bytes.size())
    return false;
  const auto* data = bytes.data() + e.dataOffset;
  if (e.method == 0)
    out.assign(data, data + e.compressedSize);
  else if (e.method == 8) {
    if (!Inflate(data, e.compressedSize, out, e.uncompressedSize))
      return false;
  } else
    return false;
  return out.size() == e.uncompressedSize &&
         CRC32_BlockChecksum(reinterpret_cast<char*>(out.data()), static_cast<int>(out.size())) ==
             e.crc;
}
void idZipBuilder::AddFile(const char* name, const void* data, std::size_t size) {
  file_t file;
  file.name = name;
  file.data.assign(static_cast<const std::uint8_t*>(data),
                   static_cast<const std::uint8_t*>(data) + size);
  files.push_back(std::move(file));
}
bool idZipBuilder::Save(const char* path, bool compress) const {
  struct central_t {
    const file_t* file;
    std::uint32_t crc, offset;
    std::uint16_t method;
    std::vector<std::uint8_t> encoded;
  };
  std::vector<central_t> central;
  std::vector<std::uint8_t> out;
  for (const auto& file : files) {
    central_t c{
        &file,
        CRC32_BlockChecksum(reinterpret_cast<char*>(const_cast<std::uint8_t*>(file.data.data())),
                            static_cast<int>(file.data.size())),
        static_cast<std::uint32_t>(out.size()), 0, file.data};
    if (compress && !file.data.empty()) {
      std::vector<std::uint8_t> packed;
      if (Deflate(file.data, packed) && packed.size() < file.data.size()) {
        c.method = 8;
        c.encoded.swap(packed);
      }
    }
    P32(out, 0x04034b50);
    P16(out, 20);
    P16(out, 0);
    P16(out, c.method);
    P16(out, 0);
    P16(out, 0);
    P32(out, c.crc);
    P32(out, static_cast<std::uint32_t>(c.encoded.size()));
    P32(out, static_cast<std::uint32_t>(file.data.size()));
    P16(out, static_cast<std::uint16_t>(file.name.len));
    P16(out, 0);
    out.insert(out.end(), file.name.data, file.name.data + file.name.len);
    out.insert(out.end(), c.encoded.begin(), c.encoded.end());
    central.push_back(std::move(c));
  }
  const std::uint32_t directoryOffset = static_cast<std::uint32_t>(out.size());
  for (const auto& c : central) {
    P32(out, 0x02014b50);
    P16(out, 20);
    P16(out, 20);
    P16(out, 0);
    P16(out, c.method);
    P16(out, 0);
    P16(out, 0);
    P32(out, c.crc);
    P32(out, static_cast<std::uint32_t>(c.encoded.size()));
    P32(out, static_cast<std::uint32_t>(c.file->data.size()));
    P16(out, static_cast<std::uint16_t>(c.file->name.len));
    P16(out, 0);
    P16(out, 0);
    P16(out, 0);
    P16(out, 0);
    P32(out, 0);
    P32(out, c.offset);
    out.insert(out.end(), c.file->name.data, c.file->name.data + c.file->name.len);
  }
  const std::uint32_t directorySize = static_cast<std::uint32_t>(out.size()) - directoryOffset;
  P32(out, 0x06054b50);
  P16(out, 0);
  P16(out, 0);
  P16(out, static_cast<std::uint16_t>(central.size()));
  P16(out, static_cast<std::uint16_t>(central.size()));
  P32(out, directorySize);
  P32(out, directoryOffset);
  P16(out, 0);
  std::ofstream file(path, std::ios::binary);
  return file &&
         static_cast<bool>(file.write(reinterpret_cast<const char*>(out.data()), out.size()));
}
