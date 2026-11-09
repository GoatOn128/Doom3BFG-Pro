#include "engine/guis/swf/swf_resources.h"

#include "engine/guis/swf/swf.h"
#include "engine/guis/swf/swf_bitstream.h"
#include "engine/guis/swf/swf_spriteinstance.h"

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/text/str.h"

#include <vector>

namespace {
void WriteBig16(idFile* file, std::uint16_t value) {
  const std::uint8_t bytes[2] = {static_cast<std::uint8_t>(value >> 8),
                                 static_cast<std::uint8_t>(value)};
  file->Write(bytes, sizeof(bytes));
}

void WriteBig32(idFile* file, std::uint32_t value) {
  const std::uint8_t bytes[4] = {static_cast<std::uint8_t>(value >> 24),
                                 static_cast<std::uint8_t>(value >> 16),
                                 static_cast<std::uint8_t>(value >> 8),
                                 static_cast<std::uint8_t>(value)};
  file->Write(bytes, sizeof(bytes));
}

std::uint16_t ReadBig16(idFile* file) {
  std::uint8_t bytes[2]{};
  file->Read(bytes, sizeof(bytes));
  return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
}

std::uint32_t ReadBig32(idFile* file) {
  std::uint8_t bytes[4]{};
  file->Read(bytes, sizeof(bytes));
  return (static_cast<std::uint32_t>(bytes[0]) << 24) |
         (static_cast<std::uint32_t>(bytes[1]) << 16) |
         (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

void WriteAtomicString(idFile* file, const idAtomicString& string) {
  const int length = string.str == nullptr ? 0 : idStr::Length(string.str);
  file->Write(&length, sizeof(length));
  if (length > 0)
    file->Write(string.str, static_cast<std::size_t>(length));
}

void ReadAtomicString(idFile* file, idAtomicString& string) {
  int length = 0;
  file->Read(&length, sizeof(length));
  if (length <= 0) {
    string.Set("");
    return;
  }
  std::vector<char> buffer(static_cast<std::size_t>(length) + 1, '\0');
  file->Read(buffer.data(), static_cast<std::size_t>(length));
  string.Set(buffer.data());
}
} // namespace

int idSWFSprite::FindFrameByLabel(const char* label) const {
  for (int i = 0; i < frameLabels.Num(); ++i) {
    if (idStr::Icmp(frameLabels[i].frameLabel.str, label) == 0)
      return static_cast<int>(frameLabels[i].frameNum);
  }
  return -1;
}

void idSWFSprite::RunFrameCommands(const idSWFSpriteInstance* spriteInstance,
                                   int desiredCurrentFrame, int desiredTargetFrame) {
  const int target = std::min(desiredTargetFrame, frameOffsets.Num() - 1);
  const int current = std::min(desiredCurrentFrame, frameOffsets.Num() - 1);
  if (target < 0 || current < 0 || target >= frameOffsets.Num() || current >= frameOffsets.Num())
    return;
  const std::uint32_t firstCommand = frameOffsets[current];
  const std::uint32_t firstAction = frameOffsets[target - 1];
  const std::uint32_t commandEnd = frameOffsets[target];
  for (std::uint32_t commandIndex = firstCommand; commandIndex < commandEnd; ++commandIndex) {
    const swfSpriteCommand_t& command = commands[static_cast<int>(commandIndex)];
    if (command.tag != Tag_DoAction || commandIndex >= firstAction)
      const_cast<idSWFSpriteInstance*>(spriteInstance)->HandleSWFCommand(command);
  }
}

idSWFSprite::idSWFSprite()
    : frameCount(0), frameOffsets(), frameLabels(), commands(), doInitActions() {
  frameOffsets.memTag = 72;
  frameLabels.memTag = 72;
  commands.memTag = 72;
  doInitActions.memTag = 72;
}

idSWFSprite::~idSWFSprite() {
  for (int i = 0; i < commands.Num(); ++i)
    delete[] commands[i].data;
  for (int i = 0; i < doInitActions.Num(); ++i)
    delete[] doInitActions[i].data;
}

void idSWFSprite::Write(idFile* file) {
  WriteBig16(file, frameCount);
  WriteBig32(file, static_cast<std::uint32_t>(frameOffsets.Num()));
  for (int i = 0; i < frameOffsets.Num(); ++i)
    WriteBig32(file, frameOffsets[i]);

  WriteBig32(file, static_cast<std::uint32_t>(frameLabels.Num()));
  for (int i = 0; i < frameLabels.Num(); ++i) {
    WriteBig32(file, frameLabels[i].frameNum);
    WriteAtomicString(file, frameLabels[i].frameLabel);
  }

  WriteBig32(file, static_cast<std::uint32_t>(commands.Num()));
  for (int i = 0; i < commands.Num(); ++i) {
    WriteBig32(file, static_cast<std::uint32_t>(commands[i].tag));
    WriteBig32(file, static_cast<std::uint32_t>(commands[i].dataLength));
    file->Write(commands[i].data, static_cast<std::size_t>(commands[i].dataLength));
  }

  WriteBig32(file, static_cast<std::uint32_t>(doInitActions.Num()));
  for (int i = 0; i < doInitActions.Num(); ++i) {
    WriteBig32(file, static_cast<std::uint32_t>(doInitActions[i].dataLength));
    file->Write(doInitActions[i].data,
                static_cast<std::size_t>(doInitActions[i].dataLength));
  }
}

void idSWFSprite::Read(idFile* file) {
  for (int i = 0; i < commands.Num(); ++i)
    delete[] commands[i].data;
  for (int i = 0; i < doInitActions.Num(); ++i)
    delete[] doInitActions[i].data;

  frameCount = ReadBig16(file);
  frameOffsets.SetNum(static_cast<int>(ReadBig32(file)));
  for (int i = 0; i < frameOffsets.Num(); ++i)
    frameOffsets[i] = ReadBig32(file);

  frameLabels.SetNum(static_cast<int>(ReadBig32(file)));
  for (int i = 0; i < frameLabels.Num(); ++i) {
    frameLabels[i].frameNum = ReadBig32(file);
    ReadAtomicString(file, frameLabels[i].frameLabel);
  }

  commands.SetNum(static_cast<int>(ReadBig32(file)));
  for (int i = 0; i < commands.Num(); ++i) {
    commands[i].tag = static_cast<swfTag_t>(ReadBig32(file));
    commands[i].dataLength = static_cast<int>(ReadBig32(file));
    commands[i].data = new std::uint8_t[commands[i].dataLength];
    file->Read(commands[i].data, static_cast<std::size_t>(commands[i].dataLength));
  }

  doInitActions.SetNum(static_cast<int>(ReadBig32(file)));
  for (int i = 0; i < doInitActions.Num(); ++i) {
    doInitActions[i].tag = Tag_DoInitAction;
    doInitActions[i].dataLength = static_cast<int>(ReadBig32(file));
    doInitActions[i].data = new std::uint8_t[doInitActions[i].dataLength];
    file->Read(doInitActions[i].data,
               static_cast<std::size_t>(doInitActions[i].dataLength));
  }
}

void idSWFSprite::Load(idSWFBitStream& bitstream, idSWF* swf) {
  frameCount = bitstream.ReadU16();
  frameOffsets.SetNum(frameCount + 1);
  frameOffsets[0] = 0;
  int currentFrame = 1;

  for (;;) {
    const std::uint16_t tagAndLength = bitstream.ReadU16();
    const swfTag_t tag = static_cast<swfTag_t>(tagAndLength >> 6);
    int tagLength = tagAndLength & 0x3f;
    if (tagLength == 0x3f)
      tagLength = static_cast<int>(bitstream.ReadU32());
    const std::uint8_t* tagData = bitstream.ReadData(tagLength);
    idSWFBitStream tagStream(tagData, static_cast<std::uint32_t>(tagLength), false);

    if (swf != nullptr) {
      switch (tag) {
        case Tag_DefineShape: swf->DefineShape(tagStream); break;
        case Tag_DefineBits: swf->DefineBits(tagStream); break;
        case Tag_JPEGTables: swf->JPEGTables(tagStream); break;
        case Tag_DefineText: swf->DefineText(tagStream); break;
        case Tag_DefineSound: swf->DefineSound(tagStream); break;
        case Tag_DefineBitsLossless: swf->DefineBitsLossless(tagStream); break;
        case Tag_DefineBitsJPEG2: swf->DefineBitsJPEG2(tagStream); break;
        case Tag_DefineShape2: swf->DefineShape2(tagStream); break;
        case Tag_DefineShape3: swf->DefineShape3(tagStream); break;
        case Tag_DefineText2: swf->DefineText2(tagStream); break;
        case Tag_DefineBitsJPEG3: swf->DefineBitsJPEG3(tagStream); break;
        case Tag_DefineBitsLossless2: swf->DefineBitsLossless2(tagStream); break;
        case Tag_DefineEditText: swf->DefineEditText(tagStream); break;
        case Tag_DefineSprite: {
          const int characterID = tagStream.ReadU16();
          idSWFDictionaryEntry* entry = swf->AddDictionaryEntry(characterID, SWF_DICT_SPRITE);
          if (entry != nullptr)
            entry->sprite->Load(tagStream, nullptr);
          break;
        }
        case Tag_DefineFont2: swf->DefineFont2(tagStream); break;
        case Tag_DefineFont3: swf->DefineFont3(tagStream); break;
        case Tag_DefineShape4: swf->DefineShape4(tagStream); break;
        default: break;
      }
    }

    if (tag == Tag_End)
      return;
    switch (tag) {
      case Tag_ShowFrame:
        if (currentFrame < frameOffsets.Num())
          frameOffsets[currentFrame] = static_cast<std::uint32_t>(commands.Num());
        ++currentFrame;
        break;
      case Tag_DoAction:
      case Tag_PlaceObject2:
      case Tag_RemoveObject2:
      case Tag_PlaceObject3: {
        swfSpriteCommand_t* command = commands.Alloc();
        command->tag = tag;
        command->dataLength = tagLength;
        command->data = new std::uint8_t[tagLength];
        std::memcpy(command->data, tagData, static_cast<std::size_t>(tagLength));
        break;
      }
      case Tag_FrameLabel: {
        swfFrameLabel_t* label = frameLabels.Alloc();
        label->frameNum = static_cast<std::uint32_t>(currentFrame);
        label->frameLabel.Set(tagStream.ReadString());
        break;
      }
      case Tag_DoInitAction: {
        tagStream.ReadU16();
        swfSpriteCommand_t* command = doInitActions.Alloc();
        command->tag = Tag_DoInitAction;
        command->dataLength = tagLength - 2;
        command->data = new std::uint8_t[command->dataLength];
        const std::uint8_t* data = tagStream.ReadData(command->dataLength);
        std::memcpy(command->data, data, static_cast<std::size_t>(command->dataLength));
        break;
      }
      default: break;
    }
  }
}

void idSWF::DefineSound(idSWFBitStream&) {}
