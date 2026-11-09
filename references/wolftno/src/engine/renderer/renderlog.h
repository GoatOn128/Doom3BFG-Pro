#pragma once

class idCVar;
class idEventRecorderLight;

class idRenderLog {
public:
  void OpenBlock(const char* format, ...);
};

extern idRenderLog renderLog;
extern idCVar r_logLevel;
extern idCVar r_logFlush;
extern idEventRecorderLight renderEventRecorder;

void RenderEventRecorderLightStartEventCopyString_Impl(const char* tag);
void RenderEventRecorderLightStartEvent_Impl(const char* tag);
void RenderEventRecorderLightStopEvent_Impl();

static_assert(sizeof(idRenderLog) == 1, "render-log ABI drift");
