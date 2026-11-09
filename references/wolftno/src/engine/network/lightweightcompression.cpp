#include "engine/network/lightweightcompression.h"

#include <cassert>
#include <cstring>

int idLZWCompressor::ReadBits(int bits) {
  int bitsToRead = bits - lzwData->tempBits;
  while (bitsToRead > 0) {
    if (bytesRead >= maxSize) {
      return -1;
    }
    lzwData->tempValue |= static_cast<std::uint64_t>(data[bytesRead++]) << lzwData->tempBits;
    lzwData->tempBits += 8;
    bitsToRead -= 8;
  }

  const int value = static_cast<int>(lzwData->tempValue) & ((1 << bits) - 1);
  lzwData->tempValue >>= bits;
  lzwData->tempBits -= bits;
  return value;
}

void idLZWCompressor::WriteBits(std::uint32_t value, int bits) {
  lzwData->tempValue |= static_cast<std::uint64_t>(value) << lzwData->tempBits;
  lzwData->tempBits += bits;
  while (lzwData->tempBits >= 8) {
    if (lzwData->bytesWritten >= maxSize) {
      overflowed = true;
      return;
    }
    data[lzwData->bytesWritten++] = static_cast<std::uint8_t>(lzwData->tempValue);
    lzwData->tempValue >>= 8;
    lzwData->tempBits -= 8;
  }
}

int idLZWCompressor::WriteChain(int code) {
  std::uint8_t chain[lzwCompressionData_t::LZW_DICT_SIZE];
  int count = 0;
  do {
    assert(count < lzwCompressionData_t::LZW_DICT_SIZE);
    assert(code >= 0 && code < lzwCompressionData_t::LZW_DICT_SIZE);
    chain[count++] = lzwData->dictionaryK[code];
    code = lzwData->dictionaryW[code];
  } while (code != 0xFFFF);

  const int firstCharacter = chain[count - 1];
  for (int i = count - 1; i >= 0; --i) {
    block[blockSize++] = chain[i];
  }
  return firstCharacter;
}

int idLZWCompressor::End() {
  if (lzwData->codeWord != -1) {
    WriteBits(static_cast<std::uint32_t>(lzwData->codeWord), lzwData->codeBits);
  }
  if (lzwData->tempBits > 0) {
    if (lzwData->bytesWritten >= maxSize) {
      overflowed = true;
      return -1;
    }
    data[lzwData->bytesWritten++] =
        static_cast<std::uint8_t>(lzwData->tempValue) & ((1 << lzwData->tempBits) - 1);
  }
  return lzwData->bytesWritten > 0 ? lzwData->bytesWritten : -1;
}

void idLZWCompressor::Save() {
  savedBytesWritten = lzwData->bytesWritten;
  savedCodeWord = lzwData->codeWord;
  saveCodeBits = lzwData->codeBits;
  savedTempValue = lzwData->tempValue;
  savedTempBits = lzwData->tempBits;
}

void idLZWCompressor::Restore() {
  lzwData->bytesWritten = savedBytesWritten;
  lzwData->codeWord = savedCodeWord;
  lzwData->codeBits = saveCodeBits;
  lzwData->tempValue = savedTempValue;
  lzwData->tempBits = savedTempBits;
}

void idZeroRunLengthCompressor::Start(std::uint8_t* destination,
                                      idLZWCompressor* compressor,
                                      int maximumSize) {
  dest = destination;
  comp = compressor;
  zeroCount = 0;
  compressed = 0;
  maxSize = maximumSize;
}

void idLZWCompressor::Start(std::uint8_t* destination, int maximumSize, bool append) {
  ClearHash();
  if (append) {
    const int originalNextCode = lzwData->nextCode;
    lzwData->nextCode = LZW_FIRST_CODE;
    for (int i = LZW_FIRST_CODE; i < originalNextCode; ++i) {
      AddToDict(lzwData->dictionaryW[i], lzwData->dictionaryK[i]);
    }
  } else {
    for (int i = 0; i < LZW_FIRST_CODE; ++i) {
      lzwData->dictionaryK[i] = static_cast<std::uint8_t>(i);
      lzwData->dictionaryW[i] = 0xFFFF;
    }
    lzwData->nextCode = LZW_FIRST_CODE;
    lzwData->codeBits = LZW_START_BITS;
    lzwData->codeWord = -1;
    lzwData->tempValue = 0;
    lzwData->tempBits = 0;
    lzwData->bytesWritten = 0;
  }

  data = destination;
  maxSize = maximumSize;
  oldCode = -1;
  blockSize = 0;
  blockIndex = 0;
  bytesRead = 0;
  overflowed = false;
  savedBytesWritten = 0;
  savedCodeWord = 0;
  saveCodeBits = 0;
  savedTempValue = 0;
  savedTempBits = 0;
}

bool idLZWCompressor::BumpBits() {
  if (lzwData->nextCode != (1 << lzwData->codeBits)) {
    return false;
  }
  if (++lzwData->codeBits <= lzwCompressionData_t::LZW_DICT_BITS) {
    return false;
  }
  lzwData->nextCode = LZW_FIRST_CODE;
  lzwData->codeBits = LZW_START_BITS;
  ClearHash();
  return true;
}

void idLZWCompressor::DecompressBlock() {
  blockSize = 0;
  blockIndex = 0;
  do {
    const int code = ReadBits(lzwData->codeBits);
    if (code == -1) {
      break;
    }
    if (oldCode == -1) {
      block[blockSize++] = static_cast<std::uint8_t>(code);
      oldCode = code;
      continue;
    }

    int firstCharacter;
    if (code < lzwData->nextCode) {
      firstCharacter = WriteChain(code);
    } else {
      firstCharacter = WriteChain(oldCode);
      block[blockSize++] = static_cast<std::uint8_t>(firstCharacter);
    }
    AddToDict(oldCode, firstCharacter);
    if (BumpBits()) {
      oldCode = -1;
    } else {
      oldCode = code;
    }
  } while (blockSize < LZW_BLOCK_SIZE - lzwCompressionData_t::LZW_DICT_SIZE);
}

int idLZWCompressor::ReadByte(bool ignoreOverflow) {
  if (blockIndex == blockSize) {
    DecompressBlock();
  }
  if (blockIndex == blockSize) {
    if (!ignoreOverflow) {
      overflowed = true;
    }
    return -1;
  }
  return block[blockIndex++];
}

void idLZWCompressor::WriteByte(std::uint8_t value) {
  const int code = Lookup(lzwData->codeWord, value);
  if (code >= 0) {
    lzwData->codeWord = code;
  } else {
    WriteBits(static_cast<std::uint32_t>(lzwData->codeWord), lzwData->codeBits);
    if (!BumpBits()) {
      AddToDict(lzwData->codeWord, value);
    }
    lzwData->codeWord = value;
  }
  if (lzwData->bytesWritten >=
      maxSize - (lzwData->tempBits + lzwData->codeBits + 7) / 8) {
    overflowed = true;
  }
}

bool idZeroRunLengthCompressor::WriteRun() {
  if (zeroCount > 0) {
    if (compressed + 2 > maxSize) {
      maxSize = -1;
      return false;
    }
    if (comp != nullptr) {
      comp->WriteByte(0);
      comp->WriteByte(static_cast<std::uint8_t>(zeroCount));
    } else {
      *dest++ = 0;
      *dest++ = static_cast<std::uint8_t>(zeroCount);
    }
    compressed += 2;
    zeroCount = 0;
  }
  return true;
}

bool idZeroRunLengthCompressor::WriteByte(std::uint8_t value) {
  if (value != 0 || zeroCount >= 255) {
    if (!WriteRun()) {
      maxSize = -1;
      return false;
    }
  }
  if (value != 0) {
    if (compressed + 1 > maxSize) {
      maxSize = -1;
      return false;
    }
    if (comp != nullptr) {
      comp->WriteByte(value);
    } else {
      *dest++ = value;
    }
    ++compressed;
  } else {
    ++zeroCount;
  }
  return true;
}

void idZeroRunLengthCompressor::WriteBytes(std::uint8_t* source, unsigned int count) {
  for (unsigned int i = 0; i < count; ++i) {
    WriteByte(source[i]);
  }
}

int idZeroRunLengthCompressor::End() {
  WriteRun();
  return maxSize == -1 ? -1 : compressed;
}

int idZeroRunLengthCompressor::ReadInternal() {
  ++compressed;
  return comp != nullptr ? comp->ReadByte() : *dest++;
}

std::uint8_t idZeroRunLengthCompressor::ReadByte() {
  if (zeroCount == 0) {
    const int value = ReadInternal();
    if (value != 0) {
      return static_cast<std::uint8_t>(value);
    }
    zeroCount = ReadInternal();
  }
  --zeroCount;
  return 0;
}

void idZeroRunLengthCompressor::ReadBytes(std::uint8_t* destination, unsigned int count) {
  for (unsigned int i = 0; i < count; ++i) {
    destination[i] = ReadByte();
  }
}

void idLZWCompressor::ClearHash() {
  std::memset(hash, 0xFF, sizeof(hash));
}

int idLZWCompressor::Lookup(int w, int k) const {
  if (w == -1) {
    return k;
  }
  for (int i = hash[HashIndex(w, k)]; i != 0xFFFF; i = nextHash[i]) {
    if (lzwData->dictionaryK[i] == k && lzwData->dictionaryW[i] == w) {
      return i;
    }
  }
  return -1;
}

int idLZWCompressor::AddToDict(int w, int k) {
  const int code = lzwData->nextCode;
  lzwData->dictionaryK[code] = static_cast<std::uint8_t>(k);
  lzwData->dictionaryW[code] = static_cast<std::uint16_t>(w);
  const int hashIndex = HashIndex(w, k);
  nextHash[code] = hash[hashIndex];
  hash[hashIndex] = static_cast<std::uint16_t>(code);
  return lzwData->nextCode++;
}
