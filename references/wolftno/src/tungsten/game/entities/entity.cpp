#include "tungsten/game/ai/ai2_core_support.h"

#include <unordered_map>

namespace {

struct crosshairObservation_t {
  bool result{false};
  int callCount{0};
  const idEntity* activator{nullptr};
  const idFocusTrace* focusTrace{nullptr};
  int usable{0};
  idCrosshairInfo* info{nullptr};
};

std::unordered_map<const idEntity*, crosshairObservation_t>
    crosshairObservations;

}  // namespace

// Header-emitted entity support used by AI.
// DIA: idEntity::GetColor
void idEntity::GetColor(idVec4& color) const { color = entityColor; }

const idVec3* idEntity::GetPresentableLeftDir() {
  return &presentableAxis[1];
}

const idVec3* idEntity::GetPresentableUpDir() {
  return &presentableAxis[2];
}

idEntity* idEntity::GetTarget(int index) {
  return index >= 0 && index < targets.Num() ? targets[index] : nullptr;
}

idFXManager* idEntity::GetFXManagerForAI() {
  if (aiFXManager == nullptr) {
    aiFXManager = std::make_shared<idFXManager>();
  }
  return aiFXManager.get();
}

bool idEntity::ModifyCrosshairInfo(const idEntity* activator,
                                   const idFocusTrace* focusTrace,
                                   usableState_t usable,
                                   idCrosshairInfo* info) {
  crosshairObservation_t& observation = crosshairObservations[this];
  ++observation.callCount;
  observation.activator = activator;
  observation.focusTrace = focusTrace;
  observation.usable = static_cast<int>(usable);
  observation.info = info;
  return observation.result;
}

void idEntity::ConfigureModifyCrosshairInfoResultForTest(bool value) {
  crosshairObservations[this].result = value;
}

int idEntity::GetModifyCrosshairInfoCountForTest() const {
  return crosshairObservations[this].callCount;
}

const idEntity* idEntity::GetLastCrosshairActivatorForTest() const {
  return crosshairObservations[this].activator;
}

const idFocusTrace* idEntity::GetLastCrosshairFocusTraceForTest() const {
  return crosshairObservations[this].focusTrace;
}

int idEntity::GetLastCrosshairUsableForTest() const {
  return crosshairObservations[this].usable;
}

idCrosshairInfo* idEntity::GetLastCrosshairInfoForTest() const {
  return crosshairObservations[this].info;
}
