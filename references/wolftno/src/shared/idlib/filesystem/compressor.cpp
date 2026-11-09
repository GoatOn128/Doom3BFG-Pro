#include "shared/idlib/filesystem/compressor.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>

namespace {
std::vector<std::uint8_t> ReadRemaining(idFile* file) {
  std::vector<std::uint8_t> data;
  if (!file)
    return data;
  const std::int64_t remaining = std::max<std::int64_t>(0, file->Length() - file->Tell());
  data.resize(static_cast<std::size_t>(remaining));
  data.resize(file->Read(data.data(), data.size()));
  return data;
}
void Put16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
}
std::uint16_t Get16(const std::vector<std::uint8_t>& in, std::size_t offset) {
  return static_cast<std::uint16_t>(in[offset] | (in[offset + 1] << 8));
}
} // namespace

void idCompressor_BitStream::Init(idFile* target, bool compress, int bits) {
  file = target;
  compressing = compress;
  wordLength = bits;
  finished = false;
  compressedSize = 0;
  position = 0;
  writeBit = readBit = 0;
  plain.clear();
  if (!compress)
    InitDecompress();
}
void idCompressor_BitStream::InitDecompress() {
  const auto encoded = ReadRemaining(file);
  compressedSize = static_cast<int>(encoded.size());
  Decode(encoded, plain);
  position = 0;
}
void idCompressor_BitStream::FinishCompress() {
  if (!compressing || finished)
    return;
  const auto encoded = Encode(plain);
  if (file && !encoded.empty())
    file->Write(encoded.data(), encoded.size());
  compressedSize = static_cast<int>(encoded.size());
  finished = true;
  if (file)
    file->Flush();
}
float idCompressor_BitStream::GetCompressionRatio() const {
  return plain.empty() ? 0.0f
                       : 100.0f * (1.0f - static_cast<float>(compressedSize) /
                                              static_cast<float>(plain.size()));
}
const char* idCompressor_BitStream::GetName() const {
  return file ? file->GetName() : "";
}
const char* idCompressor_BitStream::GetFullPath() const {
  return file ? file->GetFullPath() : "";
}
std::size_t idCompressor_BitStream::Read(void* data, std::size_t length) {
  const std::size_t count = std::min(length, plain.size() - std::min(position, plain.size()));
  if (count)
    std::memcpy(data, plain.data() + position, count);
  position += count;
  return count;
}
std::size_t idCompressor_BitStream::Write(const void* data, std::size_t length) {
  if (!compressing || finished)
    return 0;
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  plain.insert(plain.end(), bytes, bytes + length);
  position = plain.size();
  return length;
}
std::int64_t idCompressor_BitStream::Length() const {
  return static_cast<std::int64_t>(plain.size());
}
std::uint32_t idCompressor_BitStream::Timestamp() const {
  return file ? file->Timestamp() : 0;
}
std::int64_t idCompressor_BitStream::Tell() const {
  return static_cast<std::int64_t>(position);
}
int idCompressor_BitStream::Seek(std::int64_t offset, fsOrigin_t origin) {
  std::int64_t base = origin == FS_SEEK_SET   ? 0
                      : origin == FS_SEEK_CUR ? static_cast<std::int64_t>(position)
                                              : static_cast<std::int64_t>(plain.size());
  const std::int64_t next =
      std::clamp(base + offset, std::int64_t{0}, static_cast<std::int64_t>(plain.size()));
  position = static_cast<std::size_t>(next);
  readBit = 0;
  return 0;
}
void idCompressor_BitStream::ForceFlush() {
  FinishCompress();
  if (file)
    file->ForceFlush();
}
void idCompressor_BitStream::Flush() {
  if (file)
    file->Flush();
}
std::vector<std::uint8_t>
idCompressor_BitStream::Encode(const std::vector<std::uint8_t>& input) const {
  return input;
}
bool idCompressor_BitStream::Decode(const std::vector<std::uint8_t>& input,
                                    std::vector<std::uint8_t>& output) const {
  output = input;
  return true;
}
void idCompressor_BitStream::WriteBits(std::uint32_t value, int count) {
  for (int i = 0; i < count; ++i) {
    if (writeBit == 0)
      plain.push_back(0);
    if (value & (1u << i))
      plain.back() |= static_cast<std::uint8_t>(1u << writeBit);
    writeBit = (writeBit + 1) & 7;
  }
  position = plain.size();
}
std::uint32_t idCompressor_BitStream::ReadBits(int count) {
  std::uint32_t value = 0;
  for (int i = 0; i < count && position < plain.size(); ++i) {
    if (plain[position] & (1u << readBit))
      value |= 1u << i;
    if (++readBit == 8) {
      readBit = 0;
      ++position;
    }
  }
  return value;
}
void idCompressor_BitStream::WriteAlignedByte(std::uint8_t v) {
  if (writeBit)
    writeBit = 0;
  Write(&v, 1);
}
std::uint8_t idCompressor_BitStream::ReadAlignedByte() {
  if (readBit) {
    readBit = 0;
    ++position;
  }
  std::uint8_t v = 0;
  Read(&v, 1);
  return v;
}
void idCompressor_BitStream::WriteAlignedWord(std::uint16_t v) {
  WriteAlignedByte(static_cast<std::uint8_t>(v));
  WriteAlignedByte(static_cast<std::uint8_t>(v >> 8));
}
std::uint16_t idCompressor_BitStream::ReadAlignedWord() {
  const auto lo = ReadAlignedByte();
  return static_cast<std::uint16_t>(lo | (ReadAlignedByte() << 8));
}

void idCompressor_RunLength_ZeroBased::Init(idFile* f, bool c, int w) {
  idCompressor_BitStream::Init(f, c, w);
}
void idCompressor_RunLength_ZeroBased::FinishCompress() {
  idCompressor_BitStream::FinishCompress();
}
std::size_t idCompressor_RunLength_ZeroBased::Write(const void* d, std::size_t n) {
  return idCompressor_BitStream::Write(d, n);
}
std::size_t idCompressor_RunLength_ZeroBased::Read(void* d, std::size_t n) {
  return idCompressor_BitStream::Read(d, n);
}
int idCompressor_RunLength_ZeroBased::BitsToRead(int value) const {
  return BitsToWrite(value);
}
int idCompressor_RunLength_ZeroBased::BitsToWrite(int value) const {
  int bits = 0;
  do {
    ++bits;
    value >>= 1;
  } while (value);
  return bits;
}
void idCompressor_RunLength_ZeroBased::WriteRun(std::vector<std::uint8_t>& out, std::uint8_t value,
                                                unsigned count) const {
  while (count) {
    const unsigned part = std::min(count, 255u);
    out.push_back(value);
    out.push_back(static_cast<std::uint8_t>(part));
    count -= part;
  }
}
void idCompressor_RunLength_ZeroBased::CompressBlock(const std::uint8_t* data, std::size_t length,
                                                     std::vector<std::uint8_t>& out) const {
  for (std::size_t i = 0; i < length;) {
    if (data[i] == 0) {
      std::size_t run = 1;
      while (i + run < length && data[i + run] == 0 && run < 255)
        ++run;
      out.push_back(0);
      out.push_back(static_cast<std::uint8_t>(run));
      i += run;
    } else {
      const std::size_t start = i;
      while (i < length && data[i] != 0 && i - start < 255)
        ++i;
      out.push_back(1);
      out.push_back(static_cast<std::uint8_t>(i - start));
      out.insert(out.end(), data + start, data + i);
    }
  }
}
bool idCompressor_RunLength_ZeroBased::DecompressBlock(const std::uint8_t* data, std::size_t length,
                                                       std::vector<std::uint8_t>& out) const {
  for (std::size_t i = 0; i < length;) {
    if (i + 2 > length)
      return false;
    const auto tag = data[i++], count = data[i++];
    if (tag == 0)
      out.insert(out.end(), count, 0);
    else if (tag == 1) {
      if (i + count > length)
        return false;
      out.insert(out.end(), data + i, data + i + count);
      i += count;
    } else
      return false;
  }
  return true;
}
std::vector<std::uint8_t>
idCompressor_RunLength_ZeroBased::Encode(const std::vector<std::uint8_t>& input) const {
  std::vector<std::uint8_t> out;
  CompressBlock(input.data(), input.size(), out);
  return out;
}
bool idCompressor_RunLength_ZeroBased::Decode(const std::vector<std::uint8_t>& input,
                                              std::vector<std::uint8_t>& output) const {
  return DecompressBlock(input.data(), input.size(), output);
}

void idCompressor_Arithmetic::Init(idFile* f, bool c, int w) {
  idCompressor_BitStream::Init(f, c, w);
}
void idCompressor_Arithmetic::InitCode() {
  writeBit = readBit = 0;
}
void idCompressor_Arithmetic::RemoveSymbolFromStream(int) {}
int idCompressor_Arithmetic::GetByte() {
  return position < plain.size() ? plain[position++] : -1;
}
void idCompressor_Arithmetic::EncodeSymbol(int symbol) {
  const std::uint8_t byte = static_cast<std::uint8_t>(symbol);
  Write(&byte, 1);
}
void idCompressor_Arithmetic::WriteOverflowBits(std::uint32_t v, int b) {
  WriteBits(v, b);
}
std::size_t idCompressor_Arithmetic::Write(const void* d, std::size_t n) {
  return idCompressor_BitStream::Write(d, n);
}
std::size_t idCompressor_Arithmetic::Read(void* d, std::size_t n) {
  return idCompressor_BitStream::Read(d, n);
}
void idCompressor_Arithmetic::FinishCompress() {
  idCompressor_BitStream::FinishCompress();
}
std::vector<std::uint8_t>
idCompressor_Arithmetic::Encode(const std::vector<std::uint8_t>& input) const {
  idCompressor_RunLength_ZeroBased codec;
  std::vector<std::uint8_t> output;
  codec.CompressBlock(input.data(), input.size(), output);
  return output;
}
bool idCompressor_Arithmetic::Decode(const std::vector<std::uint8_t>& input,
                                     std::vector<std::uint8_t>& output) const {
  idCompressor_RunLength_ZeroBased codec;
  return codec.DecompressBlock(input.data(), input.size(), output);
}

void idCompressor_LZSS_ByteAligned::Init(idFile* f, bool c, int w) {
  idCompressor_BitStream::Init(f, c, w);
}
unsigned idCompressor_LZSS_ByteAligned::GetHashKey(const std::uint8_t* d, std::size_t n) const {
  return n < 3 ? 0u : (d[0] * 251u + d[1] * 31u + d[2]) & 4095u;
}
int idCompressor_LZSS_ByteAligned::FindByteMatch(const std::vector<std::uint8_t>& input,
                                                 std::size_t pos, int& distance) const {
  int best = 0;
  distance = 0;
  const std::size_t begin = pos > 4095 ? pos - 4095 : 0;
  for (std::size_t candidate = begin; candidate < pos; ++candidate) {
    int length = 0;
    while (length < 18 && pos + length < input.size() &&
           input[candidate + length] == input[pos + length])
      ++length;
    if (length > best && length >= 3) {
      best = length;
      distance = static_cast<int>(pos - candidate);
    }
  }
  return best;
}
void idCompressor_LZSS_ByteAligned::CompressBlock(const std::vector<std::uint8_t>& input,
                                                  std::vector<std::uint8_t>& out) const {
  for (std::size_t p = 0; p < input.size();) {
    const std::size_t flagPos = out.size();
    out.push_back(0);
    std::uint8_t flags = 0;
    for (int bit = 0; bit < 8 && p < input.size(); ++bit) {
      int distance = 0, length = FindByteMatch(input, p, distance);
      if (length >= 3) {
        flags |= static_cast<std::uint8_t>(1u << bit);
        const std::uint16_t token =
            static_cast<std::uint16_t>(((length - 3) << 12) | (distance - 1));
        Put16(out, token);
        p += length;
      } else
        out.push_back(input[p++]);
    }
    out[flagPos] = flags;
  }
}
bool idCompressor_LZSS_ByteAligned::DecompressBlock(const std::vector<std::uint8_t>& input,
                                                    std::vector<std::uint8_t>& out) const {
  for (std::size_t p = 0; p < input.size();) {
    const std::uint8_t flags = input[p++];
    for (int bit = 0; bit < 8 && p < input.size(); ++bit) {
      if (flags & (1u << bit)) {
        if (p + 2 > input.size())
          return false;
        const std::uint16_t token = Get16(input, p);
        p += 2;
        const int length = (token >> 12) + 3, distance = (token & 4095) + 1;
        if (distance > static_cast<int>(out.size()))
          return false;
        for (int i = 0; i < length; ++i)
          out.push_back(out[out.size() - distance]);
      } else
        out.push_back(input[p++]);
    }
  }
  return true;
}
std::size_t idCompressor_LZSS_ByteAligned::Write(const void* d, std::size_t n) {
  return idCompressor_BitStream::Write(d, n);
}
std::size_t idCompressor_LZSS_ByteAligned::Read(void* d, std::size_t n) {
  return idCompressor_BitStream::Read(d, n);
}
void idCompressor_LZSS_ByteAligned::FinishCompress() {
  idCompressor_BitStream::FinishCompress();
}
std::vector<std::uint8_t>
idCompressor_LZSS_ByteAligned::Encode(const std::vector<std::uint8_t>& input) const {
  std::vector<std::uint8_t> out;
  CompressBlock(input, out);
  return out;
}
bool idCompressor_LZSS_ByteAligned::Decode(const std::vector<std::uint8_t>& input,
                                           std::vector<std::uint8_t>& output) const {
  return DecompressBlock(input, output);
}

void idCompressor_LZW::Init(idFile* f, bool c, int w) {
  idCompressor_BitStream::Init(f, c, w);
}
std::size_t idCompressor_LZW::Write(const void* d, std::size_t n) {
  return idCompressor_BitStream::Write(d, n);
}
std::size_t idCompressor_LZW::Read(void* d, std::size_t n) {
  return idCompressor_BitStream::Read(d, n);
}
void idCompressor_LZW::WriteChain(std::vector<std::uint16_t>& codes, std::uint16_t code) const {
  codes.push_back(code);
}
std::vector<std::uint8_t> idCompressor_LZW::Encode(const std::vector<std::uint8_t>& input) const {
  if (input.empty())
    return {};
  std::unordered_map<std::string, std::uint16_t> table;
  for (int i = 0; i < 256; ++i)
    table[std::string(1, static_cast<char>(i))] = static_cast<std::uint16_t>(i);
  std::uint16_t next = 256;
  std::vector<std::uint16_t> codes;
  std::string current;
  for (std::uint8_t byte : input) {
    std::string combined = current + static_cast<char>(byte);
    if (table.count(combined))
      current = combined;
    else {
      codes.push_back(table[current]);
      if (next < 4096)
        table.emplace(combined, next++);
      current.assign(1, static_cast<char>(byte));
    }
  }
  if (!current.empty())
    codes.push_back(table[current]);
  std::vector<std::uint8_t> out;
  for (auto code : codes)
    Put16(out, code);
  return out;
}
bool idCompressor_LZW::DecompressBlock(const std::vector<std::uint8_t>& input,
                                       std::vector<std::uint8_t>& out) const {
  if (input.empty())
    return true;
  if (input.size() % 2)
    return false;
  std::vector<std::string> table(4096);
  for (int i = 0; i < 256; ++i)
    table[i] = std::string(1, static_cast<char>(i));
  std::uint16_t next = 256, old = Get16(input, 0);
  if (old >= 256)
    return false;
  std::string previous = table[old];
  out.insert(out.end(), previous.begin(), previous.end());
  for (std::size_t p = 2; p < input.size(); p += 2) {
    const std::uint16_t code = Get16(input, p);
    std::string entry;
    if (code < next && !table[code].empty())
      entry = table[code];
    else if (code == next)
      entry = previous + previous.front();
    else
      return false;
    out.insert(out.end(), entry.begin(), entry.end());
    if (next < 4096)
      table[next++] = previous + entry.front();
    previous = entry;
  }
  return true;
}
bool idCompressor_LZW::Decode(const std::vector<std::uint8_t>& input,
                              std::vector<std::uint8_t>& output) const {
  return DecompressBlock(input, output);
}
void idCompressor_LZW::FinishCompress() {
  idCompressor_BitStream::FinishCompress();
}

idCompressor* idCompressor::AllocNoCompression() {
  return new idCompressor_None;
}
idCompressor* idCompressor::AllocBitStream() {
  return new idCompressor_BitStream;
}
idCompressor* idCompressor::AllocRunLength_ZeroBased() {
  return new idCompressor_RunLength_ZeroBased;
}
idCompressor* idCompressor::AllocArithmetic() {
  return new idCompressor_Arithmetic;
}
idCompressor* idCompressor::AllocLZSS_ByteAligned() {
  return new idCompressor_LZSS_ByteAligned;
}
idCompressor* idCompressor::AllocLZW() {
  return new idCompressor_LZW;
}
