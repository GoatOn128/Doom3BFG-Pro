#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"
#include "tungsten/game/ai/ai2_actions.h"

#include <string>
#include <vector>

enum sysEventType_t { SE_NONE, SE_KEY, SE_CHAR, SE_MOUSE };

struct sysEvent_t {
  sysEventType_t evType{SE_NONE};
  int evValue{0};
  int evValue2{0};
};

enum guiKey_t {
  GUI_KEY_UP = 1001,
  GUI_KEY_DOWN,
  GUI_KEY_LEFT,
  GUI_KEY_RIGHT,
  GUI_KEY_ENTER,
};

class idDeviceContext {
public:
  void Clear() { lines.clear(); }
  void AddLine(const char* value) {
    lines.emplace_back(value != nullptr ? value : "");
  }
  void AddLine(const idStr& value) { AddLine(value.data); }
  const std::vector<std::string>& GetLines() const { return lines; }

private:
  std::vector<std::string> lines;
};

struct idGuiPageStatus {
  int selectedRow{-1};
  int selectedColumn{-1};
  int selectedTab{-1};
  int tabNum{0};
};

class idGuiPage {
public:
  explicit idGuiPage(const char* name) : pageName(name) {}
  virtual ~idGuiPage() = default;

  void SetIsActive(bool active);
  bool GetIsActive() const { return isActive; }
  virtual bool IsEnabled() const { return true; }
  bool HandleEvent(const sysEvent_t* event) {
    return isActive && event != nullptr && InternalHandleEvent(event);
  }
  void Render(idEntity* debugEntity, idDeviceContext* context) {
    if (isActive && context != nullptr) InternalRender(debugEntity, context);
  }
  const idStr& GetPageName() const { return pageName; }

protected:
  virtual bool InternalHandleEvent(const sysEvent_t* event) {
    (void)event;
    return false;
  }
  virtual void InternalRender(idEntity* debugEntity,
                              idDeviceContext* context) = 0;
  bool HandleDefaultPageControls(const sysEvent_t* event,
                                 idGuiPageStatus* status);

  idStr pageName;
  bool isActive{true};
};

class idGuiPage_AI_List final : public idGuiPage {
public:
  struct aiGuiInfo_t {
    idAI2* ai{nullptr};
    int index{0};
  };

  idGuiPage_AI_List();
  void GetAiList(idList<aiGuiInfo_t, 5>* output, int filter) const;
  idAI2* FindAiByIndex(int index) const;
  idEntity* GetFilteredAndNearestDebugEntity() const;
  void SetAIList(const idList<idAI2*, 5>* value) { sourceAIs = value; }
  void SetReferenceOrigin(const idVec3& value) { referenceOrigin = value; }
  int GetSelectedAiIndex() const { return selectedAiIndex; }
  int GetFilter() const { return aiFilter; }

protected:
  bool InternalHandleEvent(const sysEvent_t* event) override;
  void InternalRender(idEntity* debugEntity,
                      idDeviceContext* context) override;

private:
  const idList<idAI2*, 5>* sourceAIs{nullptr};
  int selectedAiIndex{0};
  int scrollOffset{0};
  int aiFilter{3};
  idVec3 referenceOrigin{};
};

class idGuiPage_AI_Senses final : public idGuiPage {
public:
  idGuiPage_AI_Senses();
protected:
  void InternalRender(idEntity* debugEntity,
                      idDeviceContext* context) override;
};

class idGuiPage_AI_FSM_log final : public idGuiPage {
public:
  idGuiPage_AI_FSM_log();
protected:
  void InternalRender(idEntity* debugEntity,
                      idDeviceContext* context) override;
};

class idGuiPage_AI_Navigation final : public idGuiPage {
public:
  idGuiPage_AI_Navigation();
protected:
  void InternalRender(idEntity* debugEntity,
                      idDeviceContext* context) override;
};

class idGuiPageDamageGroups final : public idGuiPage {
public:
  idGuiPageDamageGroups();
  int GetSelectedRow() const { return selectedRow; }
  int GetIncreaseDamageLevel() const { return increaseDamageLevel; }
protected:
  bool InternalHandleEvent(const sysEvent_t* event) override;
  void InternalRender(idEntity* debugEntity,
                      idDeviceContext* context) override;
private:
  int selectedRow{0};
  int increaseDamageLevel{-1};
};

class idGuiPage_AI_StateMachine final : public idGuiPage {
public:
  idGuiPage_AI_StateMachine();
  const idGuiPageStatus& GetStatus() const { return pageStatus; }
protected:
  bool InternalHandleEvent(const sysEvent_t* event) override;
  void InternalRender(idEntity* debugEntity,
                      idDeviceContext* context) override;
private:
  idGuiPageStatus pageStatus;
};

class idGuiPageEntityList final : public idGuiPage {
public:
  struct uniqueWarnings_t {
    struct warning_t {
      idStr warning;
      int count{0};
    };
  };

  struct entityInfo_t {
    idEntity* ent{nullptr};
    idList<idStr, 5> warnings;
    ~entityInfo_t();
    idStr AllWarnings(const char* delimiter) const;
  };

  idGuiPageEntityList();
  ~idGuiPageEntityList() override;
  bool IsEnabled() const override;
  void SetEntities(const idList<entityInfo_t, 5>& value) { entityList = value; }
  void SetDebugPlayer(idPlayer* value) { debugPlayer = value; }
  int GetSelectedIndex() const { return selectedIndex; }
protected:
  bool InternalHandleEvent(const sysEvent_t* event) override;
  void InternalRender(idEntity* debugEntity,
                      idDeviceContext* context) override;
private:
  int selectedIndex{0};
  idList<entityInfo_t, 5> entityList;
  idPlayer* debugPlayer{nullptr};
};

template <typename T>
void idListArrayDelete(T* values, int count);
template <typename T>
T* idListArrayNew(int count, unsigned int tag, bool zeroBuffer);
template <typename T>
T* idListArrayResize(T* values, int oldCount, int newCount,
                     unsigned int tag, bool zeroBuffer);

class idAIGUI {
public:
  idAIGUI();
  ~idAIGUI();
  void Init();
  void Shutdown();
  idAI2* GetDebugAi();
  bool RenderActivePages(idDeviceContext* context);
  idDeviceContext* DrawDebugGUI(const idEntity* entity);
  void RenderAIGuiBase(idDeviceContext* context);
  bool SendEventToActivePages(const sysEvent_t* event);
  bool HandleEvent(const sysEvent_t* event);

  void RegisterAI(idAI2* ai) { ais.Append(ai); }
  void SetDebugPlayer(idPlayer* value) { debugPlayer = value; }
  void SetDebugEntity(idAI2* value) { debugAI = value; }
  bool IsInitialized() const { return initialized; }
  int NumPages() const { return pages.Num(); }
  int NumDebugPages() const { return debugPages.Num(); }
  idGuiPage* GetPage(int index) const {
    return index >= 0 && index < pages.Num() ? pages[index] : nullptr;
  }
  idGuiPageEntityList* GetEntityListPage() const;
  const idDeviceContext& GetContext() const { return context; }

  static int mouseX;
  static int mouseY;
  static bool usingMouse;

private:
  idList<idGuiPage*, 5> pages;
  idList<idGuiPage*, 5> debugPages;
  idList<idAI2*, 5> ais;
  idPlayer* debugPlayer{nullptr};
  idAI2* debugAI{nullptr};
  idDeviceContext context;
  bool initialized{false};
};

extern idCVar g_showEntityWarnings;
extern idCVar ai_showBlendTreeWeb;
extern idCVar g_debugSyncEntity;
extern idCVar aigui;
extern idCVar aigui_show;
extern idCVar aigui_keyboardUse;
