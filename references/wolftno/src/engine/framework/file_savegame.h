#pragma once

#include "shared/idlib/filesystem/file.h"

enum saveGameFileType_t {
  SAVEGAMEFILE_NONE = 0,
  SAVEGAMEFILE_TEXT = 1 << 0,
  SAVEGAMEFILE_BINARY = 1 << 1,
  SAVEGAMEFILE_COMPRESSED = 1 << 2,
  SAVEGAMEFILE_PIPELINED = 1 << 3,
  SAVEGAMEFILE_THUMB = 1 << 4,
  SAVEGAMEFILE_BKGRND_IMAGE = 1 << 5,
  SAVEGAMEFILE_AUTO_DELETE = 1 << 6,
  SAVEGAMEFILE_OPTIONAL = 1 << 7
};

class idFile_SaveGame : public idFile_Memory {
public:
  idFile_SaveGame();
  explicit idFile_SaveGame(const char* name);
  idFile_SaveGame(const char* name, int fileType);
  ~idFile_SaveGame() override = default;

  void Clear(bool freeMemory = true);
  void SetNameAndType(const char* newName, int newType);
  bool operator==(const idFile_SaveGame& other) const;
  bool operator==(const char* otherName) const;

  int type{SAVEGAMEFILE_NONE};
  bool error{};
};
