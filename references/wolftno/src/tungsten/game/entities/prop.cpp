#include "tungsten/game/ai/ai2_core_support.h"

// Header-emitted moveable-prop support used by AI.
void idProp_Moveable::SetOwner(idEntity* ownerEntity) {
  owner = ownerEntity;
}
