#include "declloadingscreen.h"

idDeclInfoTemplate<idDeclLoadingScreen> idDeclLoadingScreen::resourceList(
    "loadingscreen", "idDeclLoadingScreen", "tdef", false);

idDeclLoadingScreen::idDeclLoadingScreen() : tauntImage(nullptr), loadIndex(-1) {
  resourceListPtr = &resourceList;
  mapName.Set("");
}

idDeclLoadingScreen::~idDeclLoadingScreen() = default;

idDeclInfo* idDeclLoadingScreen::GetDeclInfo() const {
  return &resourceList;
}

void idDeclLoadingScreen::LoadAllDecls() {
  const int count = resourceList.declSources.Num();
  for (int i = 0; i < count; ++i) {
    const char* name = resourceList.declSources[i]->name.str;
    if (name != nullptr)
      resourceList.FindWithInheritance(name, false, true);
  }
}

const idDeclLoadingScreen* idDeclLoadingScreen::GetLoadingScreenForMapName(
    const char* requestedMapName, const idStaticList<idStrStatic<128>, 256>& mapLayers) {
  if (resourceList.resourceList.Num() == 0)
    LoadAllDecls();

  for (int i = 0; i < resourceList.resourceList.Num(); ++i) {
    const auto* loadingScreen =
        dynamic_cast<const idDeclLoadingScreen*>(resourceList.resourceList[i]);
    if (loadingScreen == nullptr)
      continue;
    if (requestedMapName != nullptr) {
      if (loadingScreen->mapName.str != requestedMapName &&
          idStr::Cmp(loadingScreen->mapName.str, requestedMapName) != 0)
        continue;
    } else if (loadingScreen->mapName.str[0] != '\0') {
      continue;
    }
    if (loadingScreen->layers.Num() > 0 && !ContainsOne(mapLayers, loadingScreen->layers))
      continue;
    return loadingScreen;
  }
  return dynamic_cast<const idDeclLoadingScreen*>(
      resourceList.FindWithInheritance("load_general", true, true));
}
