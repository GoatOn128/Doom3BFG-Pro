#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/text/str.h"

class idGameSpawnInfo;
class idRenderModelGui;
class idSWF;
class idSWFSpriteInstance;

class idLoadScreenInfo {
public:
  idStr varName;
  idStr value;
};

class idLoadScreenInfo2 {
public:
  idStr varName;
  idStr value;
};

class idLoadScreenThread final : public idSysThread {
public:
  idLoadScreenThread()
      : idSysThread(),initialized(false),findLogo(true),logo(nullptr),
        current(0.0f),prevCount(0),stopRunning(false),isRunning(false) {}
  ~idLoadScreenThread() override=default;
  int Run() override;
  void RenderFrame();

  bool initialized;
  bool findLogo;
  std::byte padding[6]{};
  idSWFSpriteInstance* logo;
  float current;
  int prevCount;
  volatile bool stopRunning;
  volatile bool isRunning;
  std::byte trailingPadding[6]{};
};

class idLoadScreen {
public:
  idLoadScreen() : thread(nullptr),loadingGui(nullptr),guiModel(nullptr),spawn(nullptr) {}
  void Init();
  void Shutdown(bool deleteThread);
  void StartPreload() { if(thread!=nullptr) thread->SignalWork(); }
  void LaunchLoadScreen(const idList<idLoadScreenInfo2,5>&);
  void LevelLoadUpdate();
  void LevelLoadCompleted();
  bool LoadScreenIsRendering() const { return thread!=nullptr&&thread->isRunning; }
  void SetActiveGameSpawn(const idGameSpawnInfo*);

  idLoadScreenThread* thread;
  idSWF* loadingGui;
  idRenderModelGui* guiModel;
  const idGameSpawnInfo* spawn;
};

extern idLoadScreen loadScreen;

static_assert(sizeof(idLoadScreenInfo2)==80,"idLoadScreenInfo2 ABI drift");
static_assert(sizeof(idLoadScreenInfo)==80,"idLoadScreenInfo ABI drift");
static_assert(sizeof(idLoadScreenThread)==120,"idLoadScreenThread ABI drift");
static_assert(offsetof(idLoadScreenThread,initialized)==0x58,"load thread initialized ABI drift");
static_assert(offsetof(idLoadScreenThread,logo)==0x60,"load thread logo ABI drift");
static_assert(offsetof(idLoadScreenThread,stopRunning)==0x70,"load thread stop ABI drift");
static_assert(sizeof(idLoadScreen)==32,"idLoadScreen ABI drift");
