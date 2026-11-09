#include "engine/framework/memorymappedfile.h"

#include "engine/framework/streamfilecache.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstring>

idMemoryMappedFile* currentMemoryMappedFile = nullptr;
idCVar mmf_debug("mmf_debug", "0", CVAR_BOOL, "print during operation");

void idMemoryMappedFile::CommitPage(int pageNum) {
  mmPage_t& page = pages.buffer[pageNum];
  if (page.state == PGST_FULL) return;

  std::uint8_t* destination = virtualBase + (pageNum << PAGE_SHIFT);
  while (::VirtualAlloc(destination, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE) ==
         nullptr) {
    if (!streamFileCache->OutOfMemoryCallback()) {
      idLib::FatalError(
          "idMemoryMappedFile::CommitBlock: streamFileCache couldn't free more memory");
    }
  }

  page.state = PGST_READING;
  page.dest = destination;
  page.completion = false;
  const std::int64_t offset =
      alignedFileOffset + (static_cast<std::int64_t>(pageNum) << PAGE_SHIFT);
  streamFileCache->UncachedBackgroundRead(file, offset, PAGE_SIZE, &page.dest,
                                          TAG_FILE, &page.completion);
  if (mmf_debug.valueInteger != 0) {
    idLib::Printf(
        "idMemoryMappedFile: CommitPage( %i ), UncachedBackgroundRead( file 0x%08x -> mem 0x%08x )\n",
        pageNum, static_cast<unsigned>(offset),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(page.dest)));
  }
}

const std::uint8_t* idMemoryMappedFile::PointerForFileBlock(
    std::int64_t fileOffset, std::int64_t length) {
  int firstPage =
      static_cast<int>((fileOffset - alignedFileOffset) >> PAGE_SHIFT);
  const int lastPage = static_cast<int>(
      (fileOffset - alignedFileOffset + length - 1) >> PAGE_SHIFT);

  if (firstPage > lastPage) {
    return virtualBase + (fileOffset - alignedFileOffset);
  }
  for (int pageNum = firstPage; pageNum <= lastPage;
       ++pageNum, ++firstPage) {
    mmPage_t& page = pages.buffer[pageNum];
    if (page.state == PGST_READING && page.completion) {
      page.state = PGST_FULL;
    }
    if (page.state != PGST_FULL) return nullptr;
  }
  return virtualBase + (fileOffset - alignedFileOffset);
}

idMemoryMappedFile::~idMemoryMappedFile() {
  for (unsigned pageNum = 0; pageNum < pages.num; ++pageNum) {
    mmPage_t& page = pages.buffer[pageNum];
    if (page.state == PGST_FULL) {
      if (mmf_debug.valueInteger != 0) {
        idLib::Printf("idMemoryMappedFile: DecommitPage( %i )\n", pageNum);
      }
      if (page.state != PGST_UNCOMITTED) {
        if (page.state == PGST_READING) {
          do {
            Sys_Sleep(1);
          } while (page.state == PGST_READING);
        }
        if (!::VirtualFree(virtualBase + (pageNum << PAGE_SHIFT), PAGE_SIZE,
                           MEM_DECOMMIT)) {
          idLib::FatalError("VirtualFree MEM_DECOMMIT failed");
        }
        page.state = PGST_UNCOMITTED;
      }
    }
  }
  ::VirtualFree(virtualBase, 0, MEM_RELEASE);
  currentMemoryMappedFile = nullptr;
}

idMemoryMappedFile::idMemoryMappedFile(idFile& file_, std::int64_t fileOffset,
                                       std::int64_t length)
    : file(&file_),
      alignedFileOffset(fileOffset & PAGE_MASK),
      alignedLength(static_cast<std::uint64_t>(
          ((fileOffset + length + PAGE_SIZE - 1) & PAGE_MASK) -
          (fileOffset & PAGE_MASK))),
      virtualBase(static_cast<std::uint8_t*>(
          ::VirtualAlloc(nullptr, alignedLength, MEM_RESERVE, PAGE_READWRITE))),
      pages() {
  pages.Alloc(static_cast<unsigned>(alignedLength >> PAGE_SHIFT));
  if (virtualBase == nullptr) {
    idLib::FatalError("idMemoryMappedFile: failed to virtual alloc %i bytes",
                      static_cast<unsigned>(alignedLength));
  }
  std::memset(pages.buffer, 0, sizeof(mmPage_t) * pages.num);
  currentMemoryMappedFile = this;
  if (mmf_debug.valueInteger != 0) {
    idLib::Printf(
        "idMemoryMappedFile: reserve VirtualAlloc( mem 0x%08x to 0x%08x )\n",
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(virtualBase)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(virtualBase)) +
            static_cast<unsigned>(alignedLength));
  }
}

bool idMemoryMappedFile::SetBatchState(const fileRange_t* ranges,
                                       int numRanges) {
  idHeapArray<bool> pageNeeded;
  pageNeeded.Alloc(pages.num);
  std::memset(pageNeeded.buffer, 0, pageNeeded.num);

  if (numRanges > 0) {
    for (int rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex) {
      const int firstPage = static_cast<int>(
          (ranges[rangeIndex].fileOffset - alignedFileOffset) >> PAGE_SHIFT);
      const int lastPage = static_cast<int>(
          (ranges[rangeIndex].fileOffset + ranges[rangeIndex].length -
           alignedFileOffset - 1) >>
          PAGE_SHIFT);
      for (int pageNum = firstPage; pageNum <= lastPage; ++pageNum) {
        pageNeeded.buffer[pageNum] = true;
      }
    }
  }

  for (unsigned pageNum = 0; pageNum < pages.num; ++pageNum) {
    mmPage_t& page = pages.buffer[pageNum];
    if (!pageNeeded.buffer[pageNum] && page.state == PGST_FULL) {
      if (mmf_debug.valueInteger != 0) {
        idLib::Printf("idMemoryMappedFile: DecommitPage( %i )\n", pageNum);
      }
      if (page.state != PGST_UNCOMITTED) {
        if (page.state == PGST_READING) {
          do {
            Sys_Sleep(1);
          } while (page.state == PGST_READING);
        }
        if (!::VirtualFree(virtualBase + (pageNum << PAGE_SHIFT), PAGE_SIZE,
                           MEM_DECOMMIT)) {
          idLib::FatalError("VirtualFree MEM_DECOMMIT failed");
        }
        page.state = PGST_UNCOMITTED;
      }
    }
  }

  for (unsigned pageNum = 0; pageNum < pages.num; ++pageNum) {
    if (pageNeeded.buffer[pageNum] &&
        pages.buffer[pageNum].state == PGST_UNCOMITTED) {
      CommitPage(static_cast<int>(pageNum));
    }
  }
  streamFileCache->SignalWork();
  return false;
}

