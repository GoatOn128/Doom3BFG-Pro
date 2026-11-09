#pragma once

#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <cstddef>
#include <cstdint>

class idFile;

class idMemoryMappedFile {
public:
  static constexpr int PAGE_SIZE = 0x10000;
  static constexpr int PAGE_SHIFT = 16;
  static constexpr std::int64_t PAGE_MASK = ~std::int64_t(PAGE_SIZE - 1);

  enum pageState_t : int {
    PGST_UNCOMITTED = 0,
    PGST_READING = 1,
    PGST_FULL = 2,
  };

  struct mmPage_t {
    void* dest;
    volatile bool completion;
    pageState_t state;
  };

  struct fileRange_t {
    std::int64_t fileOffset;
    std::int64_t length;
  };

  idMemoryMappedFile(idFile& file_, std::int64_t fileOffset,
                     std::int64_t length);
  ~idMemoryMappedFile();

  void CommitPage(int pageNum);
  bool SetBatchState(const fileRange_t* ranges, int numRanges);
  const std::uint8_t* PointerForFileBlock(std::int64_t fileOffset,
                                          std::int64_t length);

  const std::uint8_t* PointerForFileOffset(std::int64_t fileOffset) {
    return PointerForFileBlock(fileOffset, 1);
  }
  int PageNumForFileOffset(std::int64_t fileOffset) const {
    return static_cast<int>((fileOffset - alignedFileOffset) >> PAGE_SHIFT);
  }

  idFile* file;
  std::int64_t alignedFileOffset;
  std::uint64_t alignedLength;
  std::uint8_t* virtualBase;
  idHeapArray<mmPage_t> pages;
};

extern idMemoryMappedFile* currentMemoryMappedFile;
extern idCVar mmf_debug;

static_assert(sizeof(idMemoryMappedFile::mmPage_t) == 0x10,
              "idMemoryMappedFile::mmPage_t ABI drift");
static_assert(offsetof(idMemoryMappedFile::mmPage_t, completion) == 0x08,
              "mmPage_t::completion ABI drift");
static_assert(offsetof(idMemoryMappedFile::mmPage_t, state) == 0x0C,
              "mmPage_t::state ABI drift");
static_assert(sizeof(idMemoryMappedFile::fileRange_t) == 0x10,
              "idMemoryMappedFile::fileRange_t ABI drift");
static_assert(sizeof(idMemoryMappedFile) == 0x30,
              "idMemoryMappedFile ABI drift");
static_assert(offsetof(idMemoryMappedFile, virtualBase) == 0x18,
              "idMemoryMappedFile::virtualBase ABI drift");
static_assert(offsetof(idMemoryMappedFile, pages) == 0x20,
              "idMemoryMappedFile::pages ABI drift");

