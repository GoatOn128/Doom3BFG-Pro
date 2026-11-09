#pragma once

#include "shared/idlib/filesystem/file.h"

#include <cstdint>
#include <vector>

class idCompressor : public idFile {
public:
  ~idCompressor() override = default;
  virtual void Init(idFile* file, bool compress, int wordLength) = 0;
  virtual void FinishCompress() = 0;
  virtual float GetCompressionRatio() const = 0;
  static idCompressor* AllocNoCompression();
  static idCompressor* AllocBitStream();
  static idCompressor* AllocRunLength_ZeroBased();
  static idCompressor* AllocArithmetic();
  static idCompressor* AllocLZSS_ByteAligned();
  static idCompressor* AllocLZW();
};

class idCompressor_BitStream : public idCompressor {
public:
  ~idCompressor_BitStream() override = default;
  void Init(idFile* file, bool compress, int wordLength) override;
  void InitDecompress();
  void FinishCompress() override;
  float GetCompressionRatio() const override;
  const char* GetName() const override;
  const char* GetFullPath() const override;
  std::size_t Read(void* data, std::size_t length) override;
  std::size_t Write(const void* data, std::size_t length) override;
  std::int64_t Length() const override;
  std::uint32_t Timestamp() const override;
  std::int64_t Tell() const override;
  int Seek(std::int64_t offset, fsOrigin_t origin) override;
  void ForceFlush() override;
  void Flush() override;
  bool IsWritable() const override { return compressing; }
  int GetCompressedSize() const { return compressedSize; }
  int GetUncompressedSize() const { return static_cast<int>(plain.size()); }
  void WriteBits(std::uint32_t value, int bitCount);
  std::uint32_t ReadBits(int bitCount);
  void WriteAlignedByte(std::uint8_t value);
  std::uint8_t ReadAlignedByte();
  void WriteAlignedWord(std::uint16_t value);
  std::uint16_t ReadAlignedWord();

protected:
  virtual std::vector<std::uint8_t> Encode(const std::vector<std::uint8_t>& input) const;
  virtual bool Decode(const std::vector<std::uint8_t>& input,
                      std::vector<std::uint8_t>& output) const;
  idFile* file{};
  bool compressing{};
  bool finished{};
  int wordLength{};
  int compressedSize{};
  std::size_t position{};
  int writeBit{};
  int readBit{};
  std::vector<std::uint8_t> plain;
};

class idCompressor_None final : public idCompressor_BitStream {};

class idCompressor_RunLength_ZeroBased final : public idCompressor_BitStream {
public:
  void Init(idFile* file, bool compress, int wordLength) override;
  void FinishCompress() override;
  std::size_t Write(const void* data, std::size_t length) override;
  std::size_t Read(void* data, std::size_t length) override;
  int BitsToRead(int value) const;
  int BitsToWrite(int value) const;
  void WriteRun(std::vector<std::uint8_t>& output, std::uint8_t value, unsigned count) const;
  void CompressBlock(const std::uint8_t* data, std::size_t length,
                     std::vector<std::uint8_t>& output) const;
  bool DecompressBlock(const std::uint8_t* data, std::size_t length,
                       std::vector<std::uint8_t>& output) const;

protected:
  std::vector<std::uint8_t> Encode(const std::vector<std::uint8_t>& input) const override;
  bool Decode(const std::vector<std::uint8_t>& input,
              std::vector<std::uint8_t>& output) const override;
};

class idCompressor_Arithmetic final : public idCompressor_BitStream {
public:
  void Init(idFile* file, bool compress, int wordLength) override;
  void InitCode();
  void RemoveSymbolFromStream(int symbol);
  int GetByte();
  void EncodeSymbol(int symbol);
  void WriteOverflowBits(std::uint32_t value, int bits);
  std::size_t Write(const void* data, std::size_t length) override;
  std::size_t Read(void* data, std::size_t length) override;
  void FinishCompress() override;

protected:
  std::vector<std::uint8_t> Encode(const std::vector<std::uint8_t>& input) const override;
  bool Decode(const std::vector<std::uint8_t>& input,
              std::vector<std::uint8_t>& output) const override;
};

class idCompressor_LZSS_ByteAligned final : public idCompressor_BitStream {
public:
  void Init(idFile* file, bool compress, int wordLength) override;
  unsigned GetHashKey(const std::uint8_t* data, std::size_t remaining) const;
  int FindByteMatch(const std::vector<std::uint8_t>& input, std::size_t position,
                    int& distance) const;
  void CompressBlock(const std::vector<std::uint8_t>& input,
                     std::vector<std::uint8_t>& output) const;
  bool DecompressBlock(const std::vector<std::uint8_t>& input,
                       std::vector<std::uint8_t>& output) const;
  std::size_t Write(const void* data, std::size_t length) override;
  std::size_t Read(void* data, std::size_t length) override;
  void FinishCompress() override;

protected:
  std::vector<std::uint8_t> Encode(const std::vector<std::uint8_t>& input) const override;
  bool Decode(const std::vector<std::uint8_t>& input,
              std::vector<std::uint8_t>& output) const override;
};

class idCompressor_LZW final : public idCompressor_BitStream {
public:
  void Init(idFile* file, bool compress, int wordLength) override;
  std::size_t Write(const void* data, std::size_t length) override;
  std::size_t Read(void* data, std::size_t length) override;
  void WriteChain(std::vector<std::uint16_t>& codes, std::uint16_t code) const;
  bool DecompressBlock(const std::vector<std::uint8_t>& input,
                       std::vector<std::uint8_t>& output) const;
  void FinishCompress() override;

protected:
  std::vector<std::uint8_t> Encode(const std::vector<std::uint8_t>& input) const override;
  bool Decode(const std::vector<std::uint8_t>& input,
              std::vector<std::uint8_t>& output) const override;
};
