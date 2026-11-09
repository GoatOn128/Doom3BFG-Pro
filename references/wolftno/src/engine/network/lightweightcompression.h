#pragma once

#include <cstddef>
#include <cstdint>

struct lzwCompressionData_t {
  static constexpr int LZW_DICT_BITS = 12;
  static constexpr int LZW_DICT_SIZE = 1 << LZW_DICT_BITS;

  std::uint8_t dictionaryK[LZW_DICT_SIZE];
  std::uint16_t dictionaryW[LZW_DICT_SIZE];
  int nextCode;
  int codeBits;
  int codeWord;
  std::uint64_t tempValue;
  int tempBits;
  int bytesWritten;
};

class idLZWCompressor {
public:
  static constexpr int LZW_BLOCK_SIZE = 1 << 15;
  static constexpr int LZW_START_BITS = 9;
  static constexpr int LZW_FIRST_CODE = 1 << (LZW_START_BITS - 1);
  static constexpr int DICTIONARY_HASH_BITS = 10;
  static constexpr int MAX_DICTIONARY_HASH = 1 << DICTIONARY_HASH_BITS;
  static constexpr int HASH_MASK = MAX_DICTIONARY_HASH - 1;

  explicit idLZWCompressor(lzwCompressionData_t* data) : lzwData(data) {}

  void Start(std::uint8_t* data, int maxSize, bool append = false);
  int ReadBits(int bits);
  void WriteBits(std::uint32_t value, int bits);
  int WriteChain(int code);
  int End();
  void Save();
  void Restore();
  bool BumpBits();
  void DecompressBlock();
  int ReadByte(bool ignoreOverflow = false);
  void WriteByte(std::uint8_t value);

  int Length() const { return lzwData->bytesWritten; }
  int GetReadCount() const { return bytesRead; }
  bool IsOverflowed() const { return overflowed; }

  int Write(const void* source, int length) {
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    for (int i = 0; i < length && !overflowed; ++i) {
      WriteByte(bytes[i]);
    }
    return length;
  }

  int Read(void* destination, int length, bool ignoreOverflow = false) {
    auto* bytes = static_cast<std::uint8_t*>(destination);
    for (int i = 0; i < length; ++i) {
      const int value = ReadByte(ignoreOverflow);
      if (value == -1) {
        return i;
      }
      bytes[i] = static_cast<std::uint8_t>(value);
    }
    return length;
  }

  int WriteR(const void* source, int length) {
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    for (int i = 0; i < length && !overflowed; ++i) {
      WriteByte(bytes[length - i - 1]);
    }
    return length;
  }

  int ReadR(void* destination, int length, bool ignoreOverflow = false) {
    auto* bytes = static_cast<std::uint8_t*>(destination);
    for (int i = 0; i < length; ++i) {
      const int value = ReadByte(ignoreOverflow);
      if (value == -1) {
        return i;
      }
      bytes[length - i - 1] = static_cast<std::uint8_t>(value);
    }
    return length;
  }

  template <class T> std::size_t WriteAgnostic(const T& value) {
    return static_cast<std::size_t>(Write(&value, sizeof(value)));
  }

  template <class T> std::size_t ReadAgnostic(T& value, bool ignoreOverflow = false) {
    return static_cast<std::size_t>(Read(&value, sizeof(value), ignoreOverflow));
  }

private:
  static int HashIndex(int w, int k) { return (w ^ k) & HASH_MASK; }
  void ClearHash();
  int Lookup(int w, int k) const;
  int AddToDict(int w, int k);

  lzwCompressionData_t* lzwData;
  std::uint16_t hash[MAX_DICTIONARY_HASH];
  std::uint16_t nextHash[lzwCompressionData_t::LZW_DICT_SIZE];
  int oldCode{};
  std::uint8_t* data{};
  int maxSize{};
  bool overflowed{};
  int bytesRead{};
  std::uint8_t block[LZW_BLOCK_SIZE]{};
  int blockSize{};
  int blockIndex{};
  int savedBytesWritten{};
  int savedCodeWord{};
  int saveCodeBits{};
  std::uint64_t savedTempValue{};
  int savedTempBits{};
};

class idZeroRunLengthCompressor {
public:
  idZeroRunLengthCompressor() = default;

  void Start(std::uint8_t* destination, idLZWCompressor* compressor, int maxSize);
  bool WriteRun();
  bool WriteByte(std::uint8_t value);
  void WriteBytes(std::uint8_t* source, unsigned int count);
  int End();
  std::uint8_t ReadByte();
  void ReadBytes(std::uint8_t* destination, unsigned int count);
  int CompressedSize() const { return compressed; }

private:
  int ReadInternal();

  int zeroCount{};
  idLZWCompressor* comp{};
  std::uint8_t* destStart{};
  std::uint8_t* dest{};
  int compressed{};
  int maxSize{};
};

static_assert(sizeof(lzwCompressionData_t) == 12320, "Wolf retail LZW state ABI drift");
static_assert(sizeof(idLZWCompressor) == 43080, "Wolf retail LZW compressor ABI drift");
static_assert(sizeof(idZeroRunLengthCompressor) == 40, "Wolf retail zero-RLE ABI drift");
