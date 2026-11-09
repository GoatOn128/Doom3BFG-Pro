#pragma once

class idRenderModelGui;

class idRenderVideoOverlay {
public:
  virtual ~idRenderVideoOverlay() = default;
  virtual void Start(const char* videoName, float duration) = 0;
  virtual void Render(idRenderModelGui* model, float time) = 0;
  virtual void End() = 0;
};

static_assert(sizeof(idRenderVideoOverlay) == 8,
              "video-overlay ABI drift");
