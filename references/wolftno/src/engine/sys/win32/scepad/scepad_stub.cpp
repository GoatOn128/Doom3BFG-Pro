#include "engine/sys/win32/scepad/scepad_stub.h"

#include <cstring>

namespace {

// The compatibility DLL deliberately exposes no controllers. Returning an
// invalid handle keeps WolfNewOrder on its independently recovered XInput path.
constexpr int SCE_PAD_ERROR_NO_DEVICE = -1;

}  // namespace

int SCEPAD_STUB_CALL scePadInit() {
  return 0;
}

int SCEPAD_STUB_CALL scePadOpen(
    int userId, int type, int index, const ScePadOpenParam* parameters) {
  (void)userId;
  (void)type;
  (void)index;
  (void)parameters;
  return SCE_PAD_ERROR_NO_DEVICE;
}

int SCEPAD_STUB_CALL scePadClose(int handle) {
  (void)handle;
  return 0;
}

int SCEPAD_STUB_CALL scePadReadState(int handle, ScePadData* data) {
  (void)handle;
  if (data != nullptr) {
    std::memset(data, 0, sizeof(*data));
  }
  return SCE_PAD_ERROR_NO_DEVICE;
}

int SCEPAD_STUB_CALL scePadRead(int handle, ScePadData* data, int count) {
  (void)handle;
  if (data != nullptr && count > 0) {
    std::memset(data, 0, sizeof(*data) * static_cast<std::size_t>(count));
  }
  return SCE_PAD_ERROR_NO_DEVICE;
}

int SCEPAD_STUB_CALL scePadSetMotionSensorState(int handle, bool enabled) {
  (void)handle;
  (void)enabled;
  return SCE_PAD_ERROR_NO_DEVICE;
}

int SCEPAD_STUB_CALL scePadSetTiltCorrectionState(int handle, bool enabled) {
  (void)handle;
  (void)enabled;
  return SCE_PAD_ERROR_NO_DEVICE;
}

int SCEPAD_STUB_CALL scePadSetAngularVelocityDeadbandState(int handle, bool enabled) {
  (void)handle;
  (void)enabled;
  return SCE_PAD_ERROR_NO_DEVICE;
}

int SCEPAD_STUB_CALL scePadResetOrientation(int handle) {
  (void)handle;
  return SCE_PAD_ERROR_NO_DEVICE;
}

int SCEPAD_STUB_CALL scePadSetVibration(
    int handle, const ScePadVibrationParam* vibration) {
  (void)handle;
  (void)vibration;
  return SCE_PAD_ERROR_NO_DEVICE;
}

int SCEPAD_STUB_CALL scePadSetLightBar(
    int handle, const ScePadLightBarParam* lightBar) {
  (void)handle;
  (void)lightBar;
  return SCE_PAD_ERROR_NO_DEVICE;
}

