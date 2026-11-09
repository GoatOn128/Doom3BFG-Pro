#pragma once

#include "shared/idlib/text/str.h"

class idParser;

class idVersionSelector {
public:
  struct idOption {
    idStrStatic<256> str;
    bool exists;
    bool used;
    bool build;
  };

  idVersionSelector(idParser& src, const char* context);
  ~idVersionSelector() = default;

  static const char* StripTags(char* buffer, int bufferSize, const char* name);
  static unsigned int GetFeatureBits();
  static void GetOptionString(int versionBits, idStrStatic<256>& str);
  static const char* SpecialRemap(const char* name);
  static const char* RemapResourceName(char* buffer, int bufferSize,
                                       const char* name);
  static const char* RemapVideoName(char* buffer, int bufferSize,
                                    const char* name);

  const idOption& GetOption(int versionBits) const {
    return options[versionBits & combinedMask];
  }
  const idOption& GetOptionRaw(int versionBits) const { return options[versionBits]; }
  const idOption& GetCurrentOption() const { return GetOption(GetFeatureBits()); }
  int GetNumOptions() const { return numOptions; }
  int GetNumBuildOptions() const { return numBuildOptions; }
  const idOption& GetBuildOption(int index) const {
    return options[buildOptions[index]];
  }

  idOption options[4];
  int buildOptions[4];
  int numOptions;
  int numBuildOptions;
  int combinedMask;

  void Assign(int versionBits, idStr str);
  void Finalize();
  int Parse(idParser& src, const char* context);
};

static_assert(sizeof(idVersionSelector::idOption) == 0x130,
              "idVersionSelector::idOption ABI drift");
static_assert(sizeof(idVersionSelector) == 0x4E0,
              "idVersionSelector ABI drift");
