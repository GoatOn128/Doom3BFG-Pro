#pragma once

#include <cstdint>

#if defined(_WIN32)
#define SCEPAD_STUB_EXPORT extern "C" __declspec(dllexport)
#define SCEPAD_STUB_CALL __fastcall
#else
#define SCEPAD_STUB_EXPORT extern "C"
#define SCEPAD_STUB_CALL
#endif

// Minimal ABI declarations for the subset of Sony's PC pad library imported by
// WolfNewOrder. The unavailable SDK-owned structures are intentionally opaque.
// ScePadData's size is known from the recovered caller, which copies 0x58 bytes.
struct ScePadOpenParam;

struct ScePadData {
  std::uint8_t bytes[0x58];
};

struct ScePadVibrationParam {
  std::uint8_t largeMotor;
  std::uint8_t smallMotor;
};

struct ScePadLightBarParam {
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
};

static_assert(sizeof(ScePadData) == 0x58, "Recovered ScePadData ABI changed");

SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadInit();
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadOpen(
    int userId, int type, int index, const ScePadOpenParam* parameters);
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadClose(int handle);
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadReadState(int handle, ScePadData* data);
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadRead(int handle, ScePadData* data, int count);
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadSetMotionSensorState(int handle, bool enabled);
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadSetTiltCorrectionState(int handle, bool enabled);
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadSetAngularVelocityDeadbandState(int handle, bool enabled);
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadResetOrientation(int handle);
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadSetVibration(
    int handle, const ScePadVibrationParam* vibration);
SCEPAD_STUB_EXPORT int SCEPAD_STUB_CALL scePadSetLightBar(
    int handle, const ScePadLightBarParam* lightBar);

