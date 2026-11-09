#include "tungsten/game/ai/aigui.h"

#include <algorithm>
#include <limits>

idCVar g_showEntityWarnings("g_showEntityWarnings", "0", 1,
                            "show incorrectly configured entities");
idCVar ai_showBlendTreeWeb("ai_showBlendTreeWeb", "0", 1,
                           "show active anim-web nodes");
idCVar g_debugSyncEntity("g_debugSyncEntity", "0", 1,
                         "debug synchronized entities");
idCVar aigui("aigui", "0", 2, "AI debug GUI page");
idCVar aigui_show("aigui_show", "0", 2, "AI debug GUI detail level");
idCVar aigui_keyboardUse("aigui_keyboardUse", "2", 2,
                         "AI debug GUI keyboard controls");

int idAIGUI::mouseX = 0;
int idAIGUI::mouseY = 0;
bool idAIGUI::usingMouse = false;

namespace {

idAI2* AsAI(idEntity* entity) { return dynamic_cast<idAI2*>(entity); }

bool IsPressed(const sysEvent_t* event) {
  return event != nullptr && event->evType == SE_KEY && event->evValue2 != 0;
}

}  // namespace

// DIA: idGuiPage_AI_List::idGuiPage_AI_List
idGuiPage_AI_List::idGuiPage_AI_List() : idGuiPage("AI List") {}

// DIA: idGuiPage_AI_Senses::idGuiPage_AI_Senses
idGuiPage_AI_Senses::idGuiPage_AI_Senses() : idGuiPage("Senses") {}

// DIA: idGuiPage_AI_FSM_log::idGuiPage_AI_FSM_log
idGuiPage_AI_FSM_log::idGuiPage_AI_FSM_log() : idGuiPage("FSM Log") {}

// DIA: idGuiPage_AI_Navigation::idGuiPage_AI_Navigation
idGuiPage_AI_Navigation::idGuiPage_AI_Navigation()
    : idGuiPage("AI Navigation") {}

// DIA: idGuiPage_AI_Navigation::InternalRender
void idGuiPage_AI_Navigation::InternalRender(idEntity* debugEntity,
                                             idDeviceContext* context) {
  idAI2* ai = AsAI(debugEntity);
  context->AddLine(ai != nullptr ? "navigation: AI active"
                                 : "navigation: no AI selected");
}

// DIA: idGuiPageDamageGroups::idGuiPageDamageGroups
idGuiPageDamageGroups::idGuiPageDamageGroups()
    : idGuiPage("Damage/Injury") {}

// DIA: idGuiPage::SetIsActive
void idGuiPage::SetIsActive(bool active) { isActive = active; }

bool idGuiPage::HandleDefaultPageControls(const sysEvent_t* event,
                                          idGuiPageStatus* status) {
  if (!IsPressed(event) || status == nullptr) return false;
  switch (event->evValue) {
    case GUI_KEY_UP: --status->selectedRow; return true;
    case GUI_KEY_DOWN: ++status->selectedRow; return true;
    case GUI_KEY_LEFT: --status->selectedTab; return true;
    case GUI_KEY_RIGHT: ++status->selectedTab; return true;
    default: return false;
  }
}

// DIA: idGuiPage_AI_StateMachine::idGuiPage_AI_StateMachine
idGuiPage_AI_StateMachine::idGuiPage_AI_StateMachine()
    : idGuiPage("AI State Machine") {}

// DIA: idGuiPage_AI_StateMachine::InternalHandleEvent
bool idGuiPage_AI_StateMachine::InternalHandleEvent(const sysEvent_t* event) {
  return HandleDefaultPageControls(event, &pageStatus);
}

// DIA: idGuiPage_AI_FSM_log::InternalRender
void idGuiPage_AI_FSM_log::InternalRender(idEntity* debugEntity,
                                          idDeviceContext* context) {
  idAI2* ai = AsAI(debugEntity);
  if (ai == nullptr) {
    context->AddLine("fsm log: no AI selected");
    return;
  }
  idStr line("fsm log: ");
  line += ai->GetCoreState().lastCall;
  context->AddLine(line);
}

// DIA: `idGuiPage_AI_FSM_log::InternalRender'::`1'::dtor$0

// DIA: idGuiPageDamageGroups::InternalHandleEvent
bool idGuiPageDamageGroups::InternalHandleEvent(const sysEvent_t* event) {
  if (!IsPressed(event)) return false;
  if (event->evValue == GUI_KEY_UP) {
    selectedRow = std::max(0, selectedRow - 1);
    return true;
  }
  if (event->evValue == GUI_KEY_DOWN) {
    ++selectedRow;
    return true;
  }
  if (event->evValue == GUI_KEY_LEFT) {
    increaseDamageLevel = -1;
    return true;
  }
  if (event->evValue == GUI_KEY_RIGHT) {
    increaseDamageLevel = 1;
    return true;
  }
  return false;
}

// DIA: idAIGUI::RenderAIGuiBase
void idAIGUI::RenderAIGuiBase(idDeviceContext* output) {
  if (output == nullptr || aigui.GetInteger() == 0) return;
  for (int index = 0; index < pages.Num(); ++index) {
    if (pages[index] == nullptr) continue;
    idStr line(index == aigui.GetInteger() ? "> " : "  ");
    line += pages[index]->GetPageName();
    output->AddLine(line);
  }
}

// DIA: idAIGUI::SendEventToActivePages
bool idAIGUI::SendEventToActivePages(const sysEvent_t* event) {
  for (int index = 0; index < debugPages.Num(); ++index) {
    if (debugPages[index] != nullptr && debugPages[index]->IsEnabled()) {
      return debugPages[index]->HandleEvent(event);
    }
  }
  return false;
}

// DIA: idGuiPage_AI_Senses::InternalRender
void idGuiPage_AI_Senses::InternalRender(idEntity* debugEntity,
                                         idDeviceContext* context) {
  idAI2* ai = AsAI(debugEntity);
  if (ai == nullptr) {
    context->AddLine("senses: no AI selected");
  } else {
    context->AddLine(ai->HasFireTemporaryHold() ? "senses: hold fire"
                                                : "senses: tracking");
  }
}

// DIA: idGuiPageDamageGroups::InternalRender
void idGuiPageDamageGroups::InternalRender(idEntity* debugEntity,
                                           idDeviceContext* context) {
  idAI2* ai = AsAI(debugEntity);
  if (ai == nullptr) {
    context->AddLine("damage: no AI selected");
    return;
  }
  idStr line("damage health: ");
  line += static_cast<int>(ai->GetBaseHealth());
  context->AddLine(line);
}

// DIA: idAIGUI::HandleEvent
bool idAIGUI::HandleEvent(const sysEvent_t* event) {
  const bool debugHandled = SendEventToActivePages(event);
  if (aigui.GetInteger() == 0 || event == nullptr) return debugHandled;
  if (event->evType == SE_MOUSE) {
    mouseX += event->evValue;
    mouseY += event->evValue2;
    usingMouse = true;
  }
  if (IsPressed(event) && aigui_keyboardUse.GetInteger() > 1 &&
      event->evValue >= '0' && event->evValue <= '9') {
    aigui.SetInteger(event->evValue - '0');
    return true;
  }
  const int page = aigui.GetInteger();
  if (aigui_keyboardUse.GetInteger() <= 0 || page <= 0 ||
      page >= pages.Num() || pages[page] == nullptr) {
    return debugHandled;
  }
  return pages[page]->HandleEvent(event) || debugHandled;
}

// DIA: idGuiPage_AI_StateMachine::InternalRender
void idGuiPage_AI_StateMachine::InternalRender(idEntity* debugEntity,
                                               idDeviceContext* context) {
  idAI2* ai = AsAI(debugEntity);
  if (ai == nullptr) {
    context->AddLine("state machine: no AI selected");
    return;
  }
  idStr line("state machine call: ");
  line += ai->GetCoreState().lastCall;
  context->AddLine(line);
  context->AddLine(ai->IsDeadState() ? "state: dead" : "state: alive");
}

// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$0
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$1
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$2
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$3
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$4
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$5
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$6
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$7
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$8
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$9
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$10
// DIA: `idGuiPage_AI_StateMachine::InternalRender'::`1'::dtor$11

// DIA: idAIGUI::idAIGUI
idAIGUI::idAIGUI() = default;

// DIA: idAIGUI::~idAIGUI
idAIGUI::~idAIGUI() { Shutdown(); }

// DIA: idAIGUI::Shutdown
void idAIGUI::Shutdown() {
  for (int index = 0; index < pages.Num(); ++index) delete pages[index];
  for (int index = 0; index < debugPages.Num(); ++index) {
    delete debugPages[index];
  }
  pages.Clear();
  debugPages.Clear();
  initialized = false;
}

// DIA: idAIGUI::GetDebugAi
idAI2* idAIGUI::GetDebugAi() {
  if (aigui.GetInteger() != 0 && debugPlayer != nullptr && pages.Num() > 1) {
    auto* listPage = dynamic_cast<idGuiPage_AI_List*>(pages[1]);
    if (listPage != nullptr) {
      idAI2* selected =
          dynamic_cast<idAI2*>(listPage->GetFilteredAndNearestDebugEntity());
      if (selected != nullptr) debugAI = selected;
    }
  }
  return debugAI;
}

// DIA: idAIGUI::RenderActivePages
bool idAIGUI::RenderActivePages(idDeviceContext* output) {
  for (int index = 0; index < debugPages.Num(); ++index) {
    idGuiPage* page = debugPages[index];
    if (page != nullptr && page->IsEnabled()) {
      page->Render(GetDebugAi(), output);
      return true;
    }
  }
  return false;
}

// DIA: idGuiPageEntityList::entityInfo_t::~entityInfo_t
idGuiPageEntityList::entityInfo_t::~entityInfo_t() = default;

// DIA: idAIGUI::DrawDebugGUI
idDeviceContext* idAIGUI::DrawDebugGUI(const idEntity* entity) {
  (void)entity;
  idAI2* ai = GetDebugAi();
  if (ai == nullptr) return nullptr;
  context.Clear();
  RenderActivePages(&context);
  if (aigui.GetInteger() != 0) {
    RenderAIGuiBase(&context);
    const int page = aigui.GetInteger();
    if (page >= 0 && page < pages.Num() && pages[page] != nullptr) {
      pages[page]->Render(ai, &context);
    }
  }
  mouseX = std::max(0, mouseX);
  mouseY = std::max(0, mouseY);
  return &context;
}

// DIA: `idAIGUI::DrawDebugGUI'::`1'::dtor$0

// DIA: idGuiPage_AI_List::GetAiList
void idGuiPage_AI_List::GetAiList(idList<aiGuiInfo_t, 5>* output,
                                  int filter) const {
  if (output == nullptr) return;
  output->Clear();
  if (sourceAIs == nullptr) return;
  for (int index = 0; index < sourceAIs->Num(); ++index) {
    idAI2* ai = (*sourceAIs)[index];
    if (ai == nullptr) continue;
    const int requiredBit = ai->IsDeadState() ? 2 : 1;
    if ((filter & requiredBit) != 0) output->Append({ai, index});
  }
}

// DIA: idGuiPage_AI_List::FindAiByIndex
idAI2* idGuiPage_AI_List::FindAiByIndex(int index) const {
  if (sourceAIs == nullptr || index < 0 || index >= sourceAIs->Num()) {
    return nullptr;
  }
  return (*sourceAIs)[index];
}

// DIA: idListArrayDelete<idGuiPageEntityList::entityInfo_t>
template <typename T>
void idListArrayDelete(T* values, int count) {
  (void)count;
  delete[] values;
}
template void idListArrayDelete<idGuiPageEntityList::entityInfo_t>(
    idGuiPageEntityList::entityInfo_t*, int);

// DIA: idGuiPage_AI_List::GetFilteredAndNearestDebugEntity
idEntity* idGuiPage_AI_List::GetFilteredAndNearestDebugEntity() const {
  idList<aiGuiInfo_t, 5> available;
  GetAiList(&available, aiFilter);
  if (selectedAiIndex >= 0 && selectedAiIndex < available.Num()) {
    return available[selectedAiIndex].ai;
  }
  idAI2* nearest = nullptr;
  float nearestDistance = std::numeric_limits<float>::max();
  for (int index = 0; index < available.Num(); ++index) {
    idAI2* ai = available[index].ai;
    const idVec3 origin = ai->GetPhysics() != nullptr
                              ? ai->GetPhysics()->GetOrigin()
                              : idVec3{};
    const float distance = (origin - referenceOrigin).LengthSqr();
    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearest = ai;
    }
  }
  return nearest;
}

// DIA: idGuiPage_AI_List::InternalHandleEvent
bool idGuiPage_AI_List::InternalHandleEvent(const sysEvent_t* event) {
  if (!IsPressed(event)) return false;
  switch (event->evValue) {
    case GUI_KEY_UP: selectedAiIndex = std::max(0, selectedAiIndex - 1); return true;
    case GUI_KEY_DOWN: ++selectedAiIndex; return true;
    case GUI_KEY_LEFT: aiFilter = aiFilter <= 1 ? 3 : aiFilter - 1; return true;
    case GUI_KEY_RIGHT: aiFilter = aiFilter >= 3 ? 1 : aiFilter + 1; return true;
    default: return false;
  }
}

// DIA: idGuiPage_AI_List::InternalRender
void idGuiPage_AI_List::InternalRender(idEntity* debugEntity,
                                       idDeviceContext* context) {
  (void)debugEntity;
  idList<aiGuiInfo_t, 5> available;
  GetAiList(&available, aiFilter);
  selectedAiIndex = available.Num() == 0
                        ? 0
                        : std::clamp(selectedAiIndex, 0, available.Num() - 1);
  for (int index = 0; index < available.Num(); ++index) {
    idStr line(index == selectedAiIndex ? "> " : "  ");
    line += available[index].ai->GetName();
    context->AddLine(line);
  }
}

// DIA: `idGuiPage_AI_List::InternalRender'::`1'::dtor$0
// DIA: `idGuiPage_AI_List::InternalRender'::`1'::dtor$1
// DIA: `idGuiPage_AI_List::InternalRender'::`1'::dtor$2

// DIA: idGuiPageEntityList::idGuiPageEntityList
idGuiPageEntityList::idGuiPageEntityList() : idGuiPage("Action List") {}

// DIA: idPlayer::SetNoclip
void idPlayer::SetNoclip(bool value) { noclip = value; }

// DIA: idGuiPageEntityList::IsEnabled
bool idGuiPageEntityList::IsEnabled() const {
  return g_showEntityWarnings.GetInteger() != 0;
}

// DIA: idGuiPageEntityList::entityInfo_t::AllWarnings
idStr idGuiPageEntityList::entityInfo_t::AllWarnings(
    const char* delimiter) const {
  idStr result;
  for (int index = 0; index < warnings.Num(); ++index) {
    result += warnings[index];
    result += delimiter != nullptr ? delimiter : "";
  }
  return result;
}

// DIA: `idGuiPageEntityList::entityInfo_t::AllWarnings'::`1'::dtor$0

// DIA: idGuiPageEntityList::InternalHandleEvent
bool idGuiPageEntityList::InternalHandleEvent(const sysEvent_t* event) {
  if (!IsPressed(event)) return false;
  if (event->evValue == GUI_KEY_UP) {
    selectedIndex = std::max(0, selectedIndex - 1);
    return true;
  }
  if (event->evValue == GUI_KEY_DOWN) {
    selectedIndex = std::min(entityList.Num() - 1, selectedIndex + 1);
    return true;
  }
  if (event->evValue == GUI_KEY_ENTER) {
    if (debugPlayer != nullptr && selectedIndex >= 0 &&
        selectedIndex < entityList.Num() &&
        entityList[selectedIndex].ent != nullptr) {
      debugPlayer->SetNoclip(true);
    }
    return true;
  }
  return false;
}

// DIA: idListArrayResize<idGuiPageEntityList::uniqueWarnings_t::warning_t>
template <typename T>
T* idListArrayResize(T* values, int oldCount, int newCount,
                     unsigned int tag, bool zeroBuffer) {
  (void)tag;
  (void)zeroBuffer;
  if (newCount <= 0) {
    delete[] values;
    return nullptr;
  }
  T* replacement = new T[newCount]{};
  for (int index = 0; index < std::min(oldCount, newCount); ++index) {
    replacement[index] = values[index];
  }
  delete[] values;
  return replacement;
}
template idGuiPageEntityList::uniqueWarnings_t::warning_t*
idListArrayResize<idGuiPageEntityList::uniqueWarnings_t::warning_t>(
    idGuiPageEntityList::uniqueWarnings_t::warning_t*, int, int,
    unsigned int, bool);

// DIA: idListArrayNew<idGuiPageEntityList::entityInfo_t>
template <typename T>
T* idListArrayNew(int count, unsigned int tag, bool zeroBuffer) {
  (void)tag;
  (void)zeroBuffer;
  return count > 0 ? new T[count]{} : nullptr;
}
template idGuiPageEntityList::entityInfo_t*
idListArrayNew<idGuiPageEntityList::entityInfo_t>(int, unsigned int, bool);

// DIA: `idListArrayNew<idGuiPageEntityList::entityInfo_t>'::`1'::dtor$0

// DIA: idListArrayResize<idGuiPageEntityList::entityInfo_t>
template idGuiPageEntityList::entityInfo_t*
idListArrayResize<idGuiPageEntityList::entityInfo_t>(
    idGuiPageEntityList::entityInfo_t*, int, int, unsigned int, bool);

// DIA: idList<idGuiPageEntityList::entityInfo_t,5>::Resize
template bool idList<idGuiPageEntityList::entityInfo_t, 5>::Resize(int);

// DIA: idList<idGuiPageEntityList::entityInfo_t,5>::Append
template int idList<idGuiPageEntityList::entityInfo_t, 5>::Append(
    const idGuiPageEntityList::entityInfo_t&);

// DIA: idGuiPageEntityList::InternalRender
void idGuiPageEntityList::InternalRender(idEntity* debugEntity,
                                         idDeviceContext* context) {
  (void)debugEntity;
  if (entityList.Num() == 0) {
    context->AddLine("entity warnings: none");
    return;
  }
  selectedIndex = std::clamp(selectedIndex, 0, entityList.Num() - 1);
  for (int index = 0; index < entityList.Num(); ++index) {
    idStr line(index == selectedIndex ? "> " : "  ");
    line += entityList[index].ent != nullptr
                ? entityList[index].ent->GetName()
                : "<missing entity>";
    line += ": ";
    line += entityList[index].AllWarnings(", ");
    context->AddLine(line);
  }
}

// DIA: `idGuiPageEntityList::InternalRender'::`1'::dtor$0
// DIA: `idGuiPageEntityList::InternalRender'::`1'::dtor$1
// DIA: `idGuiPageEntityList::InternalRender'::`1'::dtor$2
// DIA: `idGuiPageEntityList::InternalRender'::`1'::dtor$3
// DIA: `idGuiPageEntityList::InternalRender'::`1'::dtor$4
// DIA: `idGuiPageEntityList::InternalRender'::`1'::dtor$5
// DIA: `idGuiPageEntityList::InternalRender'::`1'::dtor$6

// DIA: idAIGUI::Init
void idAIGUI::Init() {
  if (initialized) return;
  initialized = true;
  pages.Append(nullptr);
  auto* listPage = new idGuiPage_AI_List();
  listPage->SetAIList(&ais);
  pages.Append(listPage);
  pages.Append(new idGuiPage_AI_StateMachine());
  pages.Append(new idGuiPage_AI_Senses());
  pages.Append(new idGuiPage_AI_FSM_log());
  pages.Append(new idGuiPage_AI_Navigation());
  pages.Append(new idGuiPageDamageGroups());
  auto* entityPage = new idGuiPageEntityList();
  entityPage->SetDebugPlayer(debugPlayer);
  debugPages.Append(entityPage);
}

// DIA: `idAIGUI::Init'::`1'::dtor$0
// DIA: `idAIGUI::Init'::`1'::dtor$1
// DIA: `idAIGUI::Init'::`1'::dtor$2
// DIA: `idAIGUI::Init'::`1'::dtor$3
// DIA: `idAIGUI::Init'::`1'::dtor$4
// DIA: `idAIGUI::Init'::`1'::dtor$5
// DIA: `idAIGUI::Init'::`1'::dtor$6

// DIA: idGuiPageEntityList::`scalar deleting destructor'
idGuiPageEntityList::~idGuiPageEntityList() = default;

// DIA: `idGuiPageEntityList::`scalar deleting destructor''::`1'::dtor$0

// DIA: `dynamic initializer for 'g_showEntityWarnings''
// DIA: `dynamic initializer for 'ai_showBlendTreeWeb''
// DIA: `dynamic initializer for 'g_debugSyncEntity''
// DIA: `dynamic initializer for 'aigui''
// DIA: `dynamic initializer for 'aigui_show''
// DIA: `dynamic initializer for 'aigui_keyboardUse''
// DIA: `dynamic atexit destructor for 'g_showEntityWarnings''
// DIA: `dynamic atexit destructor for 'ai_showBlendTreeWeb''
// DIA: `dynamic atexit destructor for 'g_debugSyncEntity''
// DIA: `dynamic atexit destructor for 'aigui''
// DIA: `dynamic atexit destructor for 'aigui_show''
// DIA: `dynamic atexit destructor for 'aigui_keyboardUse''

idGuiPageEntityList* idAIGUI::GetEntityListPage() const {
  return debugPages.Num() > 0
             ? dynamic_cast<idGuiPageEntityList*>(debugPages[0])
             : nullptr;
}
