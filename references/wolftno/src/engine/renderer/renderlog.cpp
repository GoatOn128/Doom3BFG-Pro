#include "engine/renderer/renderlog.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/metrics/recoverymetrics.h"
#include "shared/idlib/sys/sys_time.h"

idCVar r_logLevel("r_logLevel", "2", CVAR_INTEGER,
                  "1 = blocks only, 2 = everything", 1.0f, 2.0f, nullptr);
idCVar r_logFlush(
    "r_logFlush", "0", CVAR_BOOL,
    "continuously flush renderlog (potentially slow) will use less ram as well");
idEventRecorderLight renderEventRecorder;

void RenderEventRecorderLightStartEventCopyString_Impl(const char* tag) {
  if (renderEventRecorder.enabled)
    renderEventRecorder.StartEventCopyStringInternal(tag);
}

void RenderEventRecorderLightStartEvent_Impl(const char* tag) {
  if (renderEventRecorder.enabled) {
    const idEventRecorderLight::event ev = {Sys_GetClockTicks(), tag};
    renderEventRecorder.list.Append(ev);
  }
}

void RenderEventRecorderLightStopEvent_Impl() {
  if (renderEventRecorder.enabled) {
    const idEventRecorderLight::event ev = {Sys_GetClockTicks(), nullptr};
    renderEventRecorder.list.Append(ev);
  }
}
