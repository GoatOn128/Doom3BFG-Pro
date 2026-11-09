#include "tungsten/game/animation/ponytail.h"

#include <cstring>

namespace {
int nextAttachmentHandle = 1;
}

// DIA: idPonytail::~idPonytail
idPonytail::~idPonytail() { Shutdown(); }
// DIA: `idPonytail::~idPonytail'::`1'::dtor$0

// DIA: idPonytail::idPonytail
idPonytail::idPonytail() = default;

// DIA: idPonytail::Update
void idPonytail::Update() {
  if (initialized && !usingParentAnimStack) ++blendCount;
}

// DIA: idPonytail::Shutdown
void idPonytail::Shutdown() {
  attachmentHandle = -1;
  initialized = false;
  usingParentAnimStack = false;
  parent = nullptr;
}

// DIA: idPonytail::Init
bool idPonytail::Init(idEntity* newParent) {
  Shutdown();
  if (newParent == nullptr || declPonytail == nullptr ||
      declPonytail->modelDefName.empty()) return false;

  parent = newParent;
  const char* parentModel = newParent->PeekEntityDefName();
  usingParentAnimStack = parentModel != nullptr &&
      std::strcmp(parentModel, declPonytail->modelDefName.c_str()) == 0;
  if (!usingParentAnimStack) attachmentHandle = nextAttachmentHandle++;
  initialized = true;
  return true;
}
