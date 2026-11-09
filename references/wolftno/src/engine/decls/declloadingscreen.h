#pragma once

#include "decl.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/langdict.h"

class idMaterial;

class idDeclLoadingScreen final : public idDeclTypeInfo {
public:
  idDeclLoadingScreen();
  ~idDeclLoadingScreen() override;
  idDeclInfo* GetDeclInfo() const override;

  template <typename Haystack, typename Needles>
  static bool ContainsOne(const Haystack& haystack, const Needles& needles) {
    for (int needle = 0; needle < needles.Num(); ++needle) {
      for (int item = 0; item < haystack.Num(); ++item) {
        if (idStr::Cmp(haystack[item].data, needles[needle].str) == 0)
          return true;
      }
    }
    return false;
  }

  static void LoadAllDecls();
  static const idDeclLoadingScreen* GetLoadingScreenForMapName(
      const char* mapName, const idStaticList<idStrStatic<128>, 256>& layers);

  idList<idAtomicString> layers;
  idAtomicString mapName;
  idList<idStrId> tips;
  idMaterial* tauntImage;
  int loadIndex;

  static idDeclInfoTemplate<idDeclLoadingScreen> resourceList;
};

static_assert(sizeof(idDeclLoadingScreen) == 168, "idDeclLoadingScreen ABI drift");
