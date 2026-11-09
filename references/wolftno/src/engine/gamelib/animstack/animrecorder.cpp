#define WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animrecorder.h"

#include "engine/gamelib/animstack/animstack.h"
#include "engine/models/serializer.h"
#include "shared/idlib/filesystem/compressor.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI

#include <cstring>
#include <intrin.h>

namespace {

struct SerializerView {
  bool writing;
  std::uint8_t padding[7];
  idBitMsg* msg;
};

struct BitMsgView {
  std::uint8_t* writeData;
  const std::uint8_t* readData;
  int maxSize;
  int curSize;
  int writeBit;
  int readCount;
  int readBit;
  bool allowOverflow;
  bool overflowed;
  std::uint8_t padding[2];
  std::uint64_t tempValue;
};

static_assert(sizeof(SerializerView) == sizeof(idSerializer));
static_assert(sizeof(BitMsgView) == sizeof(idBitMsg));

} // namespace

idAnimRecorder* idAnimRecorder::instance = nullptr;
const char* idAnimRecorder::inputFileName = "AnimRecording.log";
const char* idAnimRecorder::outputDataFileName =
    "AnimRecordingData.log";
idAnimRecorder* (*idAnimRecorder::allocFunction)() =
    &idAnimRecorder::AllocInstance;

idAnimRecorder::idAnimRecorder()
    : cachedGameTime(0), cachedMessageSize(0), dataStart(0),
      compressor(nullptr), file(nullptr), activeSerializer(nullptr),
      activeAnimStack(nullptr), recordingFileMode(RECORDING_FILE_CLOSED),
      isPaused(false) {}

void idAnimRecorder::KillInstance() {
  if (instance != nullptr) {
    delete instance;
    instance = nullptr;
  }
}

void idAnimRecorder::ValidateBlock(std::uint8_t expectedBlockType) {
  std::uint8_t blockType = BLOCK_INVALID;
  const std::size_t read = compressor->Read(&blockType, 1);
  if (read != 1 || blockType != expectedBlockType) {
    activeSerializer = nullptr;
    idLib::Error(
        "idAnimRecorder::ValidateBlock - Read block type %u, expected block type %u",
        blockType, expectedBlockType);
    __debugbreak();
  }
}

idAnimRecorder* idAnimRecorder::AllocInstance() {
  void* const memory = idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
      sizeof(idAnimRecorder), TAG_NEW, false, ALIGN_16, HEAP_DEFAULTHEAP);
  return memory != nullptr ? ::new (memory) idAnimRecorder : nullptr;
}

void idAnimRecorder::CloseFile() {
  if (recordingFileMode == RECORDING_FILE_WRITE) {
    WriteHeaderBlock();
  }

  if (compressor != nullptr) {
    delete compressor;
    compressor = nullptr;
  }
  if (file != nullptr) {
    delete file;
    file = nullptr;
  }

  if (recordingFileMode == RECORDING_FILE_WRITE) {
    bool removeDataFile = false;
    {
      idFileSystem* appendFileSystem = fileSystem;
      const auto appendSlots =
          *reinterpret_cast<const void* const* const*>(appendFileSystem);
      using OpenAppend =
          idFile* (*)(idFileSystem*, const char*, fsPath_t);
      idFileLocal mergedFile(reinterpret_cast<OpenAppend>(appendSlots[37])(
          appendFileSystem, inputFileName, FSPATH_BASE));

      idFileSystem* readFileSystem = fileSystem;
      const auto readSlots =
          *reinterpret_cast<const void* const* const*>(readFileSystem);
      using OpenRead =
          idFile* (*)(idFileSystem*, const char*, bool, bool);
      idFileLocal dataFile(reinterpret_cast<OpenRead>(readSlots[31])(
          readFileSystem, outputDataFileName, true, false));
      if (mergedFile.file != nullptr && dataFile.file != nullptr) {
        removeDataFile = true;
        idArray<std::uint8_t, 16384> tempBuffer;
        std::size_t size =
            dataFile.file->Read(&tempBuffer, sizeof(tempBuffer));
        while (size != 0) {
          if (mergedFile.file->Write(&tempBuffer, size) != size) {
            activeSerializer = nullptr;
            idLib::Error(
                "idAnimRecorder::CloseFile - Error writing merged file");
            break;
          }
          size = dataFile.file->Read(&tempBuffer, sizeof(tempBuffer));
        }
      } else {
        activeSerializer = nullptr;
        idLib::Error(
            "idAnimRecorder::CloseFile - Couldn't merge data with header");
      }
    }
    if (removeDataFile) {
      idFileSystem* removeFileSystem = fileSystem;
      const auto removeSlots =
          *reinterpret_cast<const void* const* const*>(removeFileSystem);
      using RemoveFile =
          void (*)(idFileSystem*, const char*, fsPath_t);
      reinterpret_cast<RemoveFile>(removeSlots[28])(
          removeFileSystem, outputDataFileName, FSPATH_BASE);
    }
  }

  recordingFileMode = RECORDING_FILE_CLOSED;
  isPaused = false;
  dataStart = 0;
  cachedGameTime = 0;
  cachedMessageSize = 0;
  std::memset(&buffer, 0, sizeof(buffer));
  activeSerializer = nullptr;
}

void idAnimRecorder::OpenFile(bool writing) {
  CloseFile();
  idFileSystem* currentFileSystem = fileSystem;
  const auto slots =
      *reinterpret_cast<const void* const* const*>(currentFileSystem);
  if (writing) {
    using OpenWrite =
        idFile* (*)(idFileSystem*, const char*, fsPath_t);
    file = reinterpret_cast<OpenWrite>(slots[34])(
        currentFileSystem, outputDataFileName, FSPATH_BASE);
  } else {
    using OpenRead =
        idFile* (*)(idFileSystem*, const char*, bool, bool);
    file = reinterpret_cast<OpenRead>(slots[31])(
        currentFileSystem, inputFileName, true, false);
  }
  if (file != nullptr) {
    if (writing) {
      compressor = idCompressor::AllocLZW();
      compressor->Init(file, true, 8);
      recordingFileMode = RECORDING_FILE_WRITE;
    } else if (ReadHeaderBlock()) {
      compressor = idCompressor::AllocLZW();
      compressor->Init(file, false, 8);
      recordingFileMode = RECORDING_FILE_READ;
    } else {
      Stop();
    }
  } else {
    CloseFile();
    idLib::Warning("idAnimRecorder::OpenFile - Can't open file for %s",
                   writing ? "writing" : "reading");
  }
}

void idAnimRecorder::Stop() {
  CloseFile();
  activeSerializer = nullptr;
  activeAnimStack = nullptr;
}

void idAnimRecorder::ValidateFile(bool writing) {
  if (recordingFileMode != static_cast<recordingFileMode_t>(writing + 1)) {
    OpenFile(writing);
  } else if (!writing) {
    idFile* const currentFile = file;
    const std::int64_t position = currentFile->Tell();
    if (currentFile->Length() <= position) {
      file->Seek(dataStart, FS_SEEK_SET);
      compressor->Init(file, false, 8);
    }
  }
}

void idAnimRecorder::WriteHeaderBlock() {
  idFileSystem* currentFileSystem = fileSystem;
  const auto slots =
      *reinterpret_cast<const void* const* const*>(currentFileSystem);
  using OpenWrite =
      idFile* (*)(idFileSystem*, const char*, fsPath_t);
  idFileLocal headerFile(reinterpret_cast<OpenWrite>(slots[34])(
      currentFileSystem, inputFileName, FSPATH_BASE));
  if (headerFile.file == nullptr) {
    activeSerializer = nullptr;
    idLib::Error(
        "idAnimRecorder::WriteHeaderBlock - Couldn't open file");
    __debugbreak();
  }

  int value = BLOCK_HEADER;
  headerFile.file->Write(&value, 1);
  value = static_cast<int>(
      _byteswap_ulong(static_cast<unsigned long>(version)));
  headerFile.file->Write(&value, sizeof(value));
  value = static_cast<int>(_byteswap_ulong(
      static_cast<unsigned long>(BLOCK_HEADER + version)));
  headerFile.file->Write(&value, sizeof(value));
}

bool idAnimRecorder::ReadHeaderBlock() {
  std::uint8_t blockType = BLOCK_INVALID;
  file->Read(&blockType, 1);
  if (blockType != BLOCK_HEADER) {
    idLib::Warning(
        "idAnimRecorder::ReadHeaderBlock - Wrong block type");
    return false;
  }

  int readVersion = -1;
  file->Read(&readVersion, sizeof(readVersion));
  readVersion = static_cast<int>(
      _byteswap_ulong(static_cast<unsigned long>(readVersion)));
  if (readVersion != version) {
    idLib::Warning(
        "idAnimRecorder::ReadHeaderBlock - File is version %i. Expected version is %i.",
        readVersion, version);
    return false;
  }

  int blockEndMarker = -1;
  file->Read(&blockEndMarker, sizeof(blockEndMarker));
  blockEndMarker = static_cast<int>(
      _byteswap_ulong(static_cast<unsigned long>(blockEndMarker)));
  const bool valid = blockEndMarker == BLOCK_HEADER + version;
  dataStart = static_cast<int>(file->Tell());
  if (!valid) {
    idLib::Warning(
        "idAnimRecorder::ReadHeaderBlock - Wrong end block marker");
  }
  return valid;
}

void idAnimRecorder::ReadFrames(int numFrames, bool validateFile) {
  for (int frame = 0; frame < numFrames; ++frame) {
    if (validateFile) {
      ValidateFile(false);
    }

    cachedMessageSize = -1;
    std::uint8_t blockType = BLOCK_INVALID;
    const std::size_t blockRead = compressor->Read(&blockType, 1);
    if (blockRead != 1 || blockType != BLOCK_FRAME) {
      activeSerializer = nullptr;
      idLib::Error(
          "idAnimRecorder::ValidateBlock - Read block type %u, expected block type %u",
          blockType, BLOCK_FRAME);
      __debugbreak();
    }
    compressor->Read(&cachedGameTime, sizeof(cachedGameTime));
    compressor->Read(&cachedMessageSize, sizeof(cachedMessageSize));
    compressor->Read(&buffer, static_cast<std::size_t>(cachedMessageSize));

    idCompressor* const endCompressor = compressor;
    const int expectedEndMarker = cachedMessageSize + BLOCK_FRAME;
    int blockEndMarker = -1;
    const std::size_t markerRead =
        endCompressor->Read(&blockEndMarker, sizeof(blockEndMarker));
    if (markerRead != sizeof(blockEndMarker) ||
        blockEndMarker != expectedEndMarker) {
      activeSerializer = nullptr;
      idLib::Error("idAnimRecorder::ValidateInt - Read %i, expected %i",
                   blockEndMarker, expectedEndMarker);
      __debugbreak();
    }
  }
}

void idAnimRecorder::Serialize(int& gameTime,
                               idGameTimeManager& gameTimeManager,
                               bool writing) {
  SerializerView serializer;
  BitMsgView message;
  serializer.msg = reinterpret_cast<idBitMsg*>(&message);
  message.writeData = nullptr;
  message.readData = nullptr;
  activeSerializer = reinterpret_cast<idSerializer*>(&serializer);
  std::memset(&message.maxSize, 0, 22);
  message.tempValue = 0;
  serializer.writing = writing;

  if (writing) {
    if (recordingFileMode != RECORDING_FILE_WRITE) {
      OpenFile(true);
    }

    const int currentTime = gameTime;
    idAnimStack* const stack = activeAnimStack;
    message.writeData = buffer.ptr;
    message.readData = buffer.ptr;
    std::uint8_t blockType = BLOCK_FRAME;
    message.maxSize = bufferSize;
    message.curSize = 0;
    message.writeBit = 0;
    message.readCount = 0;
    message.readBit = 0;
    message.allowOverflow = false;
    message.overflowed = false;
    message.tempValue = 0;
    stack->SerializeSnapshot(*reinterpret_cast<idSerializer*>(&serializer),
                             nullptr, currentTime, &gameTimeManager, nullptr);

    const int messageSize =
        message.curSize + (message.writeBit != 0 ? 1 : 0);
    compressor->Write(&blockType, sizeof(blockType));
    compressor->Write(&gameTime, sizeof(gameTime));
    compressor->Write(&messageSize, sizeof(messageSize));
    compressor->Write(buffer.ptr, static_cast<std::size_t>(messageSize));
    const int blockEndMarker = messageSize + BLOCK_FRAME;
    compressor->Write(&blockEndMarker, sizeof(blockEndMarker));
  } else {
    if (!isPaused) {
      ReadFrames(1, true);
    }
    const int currentTime = cachedGameTime;
    gameTime = currentTime;
    message.readData = buffer.ptr;
    int messageSize = cachedMessageSize;
    message.maxSize = bufferSize;
    if (messageSize > bufferSize) {
      messageSize = bufferSize;
    }
    idAnimStack* const stack = activeAnimStack;
    message.writeData = nullptr;
    message.curSize = messageSize;
    message.writeBit = 0;
    message.readCount = 0;
    message.readBit = 0;
    message.allowOverflow = false;
    message.overflowed = false;
    message.tempValue = 0;
    stack->SerializeSnapshot(*reinterpret_cast<idSerializer*>(&serializer),
                             nullptr, currentTime, &gameTimeManager, nullptr);
  }

  activeSerializer = nullptr;
}
