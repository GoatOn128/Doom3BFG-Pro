#include "engine/guis/swf/swf_spriteinstance.h"

#include "engine/guis/swf/swf.h"
#include "engine/guis/swf/swf_bitstream.h"
#include "engine/guis/swf/swf_easing.h"
#include "engine/guis/swf/swf_scriptfunctionas2.h"
#include "engine/guis/swf/swf_textinstance.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

idSWFSpriteInstance::idSWFSpriteInstance(idSWF* swf_, idSWFSprite* sprite_)
    : name(), scriptObject(nullptr), swf(swf_), sprite(sprite_), parent(nullptr), depth(0),
      isPlaying(true), isVisible(true), childrenRunning(true), firstRun(false), ignoreMouse(false),
      ignoreSnapToPixel(false), recoveredFlagsPadding(0), currentFrame(0),
      frameCount(sprite_ != nullptr ? sprite_->frameCount : 0), recoveredFramePadding(0), z(0.0f),
      materialOverride(nullptr), materialWidth(0), materialHeight(0), xOffset(0.0f), yOffset(0.0f),
      recoveredDisplayPadding(0), displayList(), onEnterFrame(), followBorder(-1),
      recoveredTweenPadding(0), tweens(), actions(), actionScript(nullptr) {
  displayList.memTag = tweens.memTag = actions.memTag = 72;
}

idSWFSpriteInstance::~idSWFSpriteInstance() {
  FreeDisplayList();
  if (scriptObject != nullptr) {
    scriptObject->sprite = nullptr;
    scriptObject->Clear();
    scriptObject->Release();
  }
  if (actionScript != nullptr)
    actionScript->Release();
}

void idSWFSpriteInstance::Init(idSWFSpriteInstance* parent_, const char* name_, int depth_) {
  parent = parent_;
  depth = depth_;
  name.Set(name_ != nullptr ? name_ : "");
  frameCount = sprite != nullptr ? sprite->frameCount : 0;
  scriptObject = new idSWFScriptSprite();
  scriptObject->sprite = this;
  static idSWFScriptObject_SpriteInstancePrototype spritePrototype;
  scriptObject->SetPrototype(&spritePrototype);
  firstRun = true;
  Play();
  actionScript = new idSWFScriptFunctionAS2();
  idStaticList<idSWFScriptObject*, 4> scope;
  scope.Append(swf->globals);
  scope.Append(scriptObject);
  actionScript->SetScope(scope);
  actionScript->defaultSprite = this;
  if (sprite != nullptr) {
    for (int i = 0; i < sprite->doInitActions.Num(); ++i) {
      actionScript->data = sprite->doInitActions[i].data;
      actionScript->length = static_cast<std::uint32_t>(sprite->doInitActions[i].dataLength);
      idSWFParmList parameters;
      actionScript->Call(scriptObject, parameters);
    }
  }
}

void idSWFSpriteInstance::Set(const char* property, idSWFScriptVar& value) {
  if (scriptObject != nullptr) scriptObject->Set(property, value);
}
idSWFScriptVar idSWFSpriteInstance::Get(const char* property) {
  return scriptObject != nullptr ? scriptObject->Get(property) : idSWFScriptVar();
}
idSWFScriptVar idSWFSpriteInstance::Get(const char* n1, const char* n2, const char* n3,
                                         const char* n4, const char* n5, const char* n6) {
  const char* names[] = {n1, n2, n3, n4, n5, n6};
  idSWFScriptVar value = Get(n1);
  for (int i = 1; i < 6 && names[i] != nullptr; ++i) {
    if (value.type != idSWFScriptVar::SWF_VAR_OBJECT || value.GetObject() == nullptr)
      return idSWFScriptVar();
    value = value.GetObject()->Get(names[i]);
  }
  return value;
}

void idSWFSpriteInstance::Play() {
  for (idSWFSpriteInstance* p = parent; p != nullptr; p = p->parent) p->childrenRunning = true;
  isPlaying = true;
}
void idSWFSpriteInstance::Stop() { isPlaying = false; }
void idSWFSpriteInstance::SetVisible(bool value) {
  isVisible = value;
  if (value) for (idSWFSpriteInstance* p = parent; p != nullptr; p = p->parent) p->childrenRunning = true;
}

int idSWFSpriteInstance::FindFrameByLabel(const char* label) {
  const int frame = std::atoi(label);
  return frame == 0 && sprite != nullptr ? sprite->FindFrameByLabel(label) : frame;
}
std::uint32_t idSWFSpriteInstance::FindFrame(const char* label) {
  const int frame = FindFrameByLabel(label);
  return frame < 0 ? currentFrame : static_cast<std::uint32_t>(frame);
}
bool idSWFSpriteInstance::FrameExists(const char* label) {
  const int frame = FindFrameByLabel(label);
  return sprite != nullptr && frame >= 0 && frame <= sprite->frameCount;
}
bool idSWFSpriteInstance::IsBetweenFrames(const char* first, const char* second) {
  const int a = FindFrameByLabel(first), b = FindFrameByLabel(second);
  return currentFrame >= a && currentFrame <= b;
}

swfDisplayEntry_t* idSWFSpriteInstance::FindDisplayEntry(int displayDepth) {
  int low = 0, high = displayList.Num();
  while (low < high) {
    const int middle = low + (high - low) / 2;
    if (displayList[middle].depth < displayDepth) low = middle + 1; else high = middle;
  }
  return low < displayList.Num() && displayList[low].depth == displayDepth ? &displayList[low] : nullptr;
}

idSWFSpriteInstance* idSWFSpriteInstance::FindChildSprite(const char* targetName) {
  for (int i = 0; i < displayList.Num(); ++i) {
    idSWFSpriteInstance* child = displayList[i].spriteInstance;
    if (child != nullptr && idStr::Icmp(child->name.str, targetName) == 0) return child;
  }
  return nullptr;
}

idVec2 idSWFSpriteInstance::GetPos() {
  if (parent == nullptr) return idVec2{0.0f, 0.0f};
  swfDisplayEntry_t* entry = parent->FindDisplayEntry(depth);
  return entry != nullptr && entry->spriteInstance == this ? idVec2{entry->matrix.tx, entry->matrix.ty}
                                                            : idVec2{0.0f, 0.0f};
}
idVec2 idSWFSpriteInstance::GetAbsolutePos() {
  const idVec2 local = GetPos();
  if (parent == nullptr) return local;
  const idVec2 absolute = parent->GetAbsolutePos();
  return idVec2{local.x + absolute.x, local.y + absolute.y};
}
void idSWFSpriteInstance::SetPosX(float value) {
  if (parent != nullptr) { swfDisplayEntry_t* e = parent->FindDisplayEntry(depth); if (e && e->spriteInstance == this) e->matrix.tx = value; }
}
void idSWFSpriteInstance::SetPosY(float value) {
  if (parent != nullptr) { swfDisplayEntry_t* e = parent->FindDisplayEntry(depth); if (e && e->spriteInstance == this) e->matrix.ty = value; }
}
void idSWFSpriteInstance::SetPos(float x, float y) { SetPosX(x); SetPosY(y); }
void idSWFSpriteInstance::SetAlpha(float value) {
  if (parent != nullptr) { swfDisplayEntry_t* e = parent->FindDisplayEntry(depth); if (e && e->spriteInstance == this) e->cxf.mul.w = value; }
}
void idSWFSpriteInstance::SetColor(idColor& value) {
  if (parent != nullptr) { swfDisplayEntry_t* e = parent->FindDisplayEntry(depth); if (e && e->spriteInstance == this) e->cxf.mul = idVec4{value.r,value.g,value.b,value.a}; }
}
void idSWFSpriteInstance::SetColorRGB(idColor& value) {
  if (parent != nullptr) { swfDisplayEntry_t* e = parent->FindDisplayEntry(depth); if (e && e->spriteInstance == this) { e->cxf.mul.x=value.r; e->cxf.mul.y=value.g; e->cxf.mul.z=value.b; } }
}
void idSWFSpriteInstance::SetScale(float x, float y) {
  if (parent == nullptr) return;
  swfDisplayEntry_t* e = parent->FindDisplayEntry(depth);
  if (e == nullptr || e->spriteInstance != this) return;
  const float xLength = std::hypot(e->matrix.xx, e->matrix.yx);
  const float xScale = x / 100.0f;
  if (xLength == 0.0f) {
    e->matrix.xx = xScale;
    e->matrix.yx = 0.0f;
  } else {
    e->matrix.xx = std::fabs(e->matrix.xx / xLength) * xScale;
    e->matrix.yx = std::fabs(e->matrix.yx / xLength) * xScale;
  }
  const float yLength = std::hypot(e->matrix.xy, e->matrix.yy);
  const float yScale = y / 100.0f;
  if (yLength == 0.0f) {
    e->matrix.yy = yScale;
    e->matrix.xy = 0.0f;
  } else {
    e->matrix.yy = std::fabs(e->matrix.yy / yLength) * yScale;
    e->matrix.xy = std::fabs(e->matrix.xy / yLength) * yScale;
  }
}

swfRect_t idSWFSpriteInstance::GetBounds() {
  swfRect_t result;
  result.tl = idVec2{1.0e30f, 1.0e30f};
  result.br = idVec2{-1.0e30f, -1.0e30f};
  for (int i = 0; i < displayList.Num(); ++i) {
    idSWFDictionaryEntry* entry = swf->FindDictionaryEntry(displayList[i].characterID);
    if (entry == nullptr || entry->type != SWF_DICT_SHAPE || entry->shape == nullptr)
      continue;
    result.tl.x = std::min(result.tl.x, entry->shape->startBounds.tl.x);
    result.tl.y = std::min(result.tl.y, entry->shape->startBounds.tl.y);
    result.br.x = std::max(result.br.x, entry->shape->startBounds.br.x);
    result.br.y = std::max(result.br.y, entry->shape->startBounds.br.y);
  }
  return result;
}

idVec2 idSWFSpriteInstance::GetSize() {
  const swfRect_t bounds = GetBounds();
  return idVec2{bounds.br.x - bounds.tl.x, bounds.br.y - bounds.tl.y};
}

void idSWFSpriteInstance::SetMaterial(const char* materialName, int width, int height) {
  materialOverride = materialName != nullptr && materialName[0] != '\0'
                         ? const_cast<idMaterial*>(FindMaterialOrAutoMaterial(materialName))
                         : nullptr;
  if (materialOverride != nullptr) {
    materialWidth = static_cast<std::uint16_t>(materialOverride->GetWidth());
    materialHeight = static_cast<std::uint16_t>(materialOverride->GetHeight());
  } else {
    materialWidth = 0;
    materialHeight = 0;
  }
  if (width >= 0)
    materialWidth = static_cast<std::uint16_t>(width);
  if (height >= 0)
    materialHeight = static_cast<std::uint16_t>(height);
}

void idSWFSpriteInstance::RemoveTweens(const char* variable) {
  if (variable == nullptr) {
    tweens.Clear();
    return;
  }
  for (int i = 0; i < tweens.Num(); ++i) {
    const char* tweenVariable = tweens[i].variable.str;
    if (tweenVariable == variable || idStr::Cmp(tweenVariable, variable) == 0) {
      tweens.RemoveIndex(i--);
    }
  }
}

idStr idSWFSpriteInstance::GetTargetPath() {
  idList<idStr, 5> names;
  for (idSWFSpriteInstance* current = this; current != nullptr; current = current->parent) {
    names.Insert(idStr(current->name.str), 0);
  }
  idStr result;
  for (int i = 0; i < names.Num(); ++i) {
    if (i != 0)
      result.Append("/");
    result.Append(names[i]);
  }
  return result;
}

idSWFSpriteInstance* idSWFSpriteInstance::ResolveTarget(const char* targetName) {
  if (targetName == nullptr || targetName[0] == '\0')
    return this;

  idSWFSpriteInstance* target = this;
  const char* componentStart = targetName;
  if (componentStart[0] == '/') {
    while (target->parent != nullptr)
      target = target->parent;
    ++componentStart;
  }

  while (*componentStart != '\0') {
    const char* separator = std::strchr(componentStart, '/');
    const int componentLength = separator != nullptr
                                    ? static_cast<int>(separator - componentStart)
                                    : static_cast<int>(std::strlen(componentStart));
    idStr component;
    component.Append(componentStart, componentLength);
    if (idStr::Cmp(component.data, "..") == 0) {
      target = target->parent;
    } else {
      target = target->FindChildSprite(component.data);
    }
    if (target == nullptr) {
      idLib::Warning("SWF %s: Could not resolve %s, %s not found", swf->filename.str,
                     targetName, component.data);
      return this;
    }
    if (separator == nullptr)
      break;
    componentStart = separator + 1;
  }
  return target;
}

idSWFSpriteInstance* idSWFSpriteInstance::DuplicateMovieClip(int displayDepth,
                                                              idStr& objectName) {
  if (parent == nullptr) {
    idLib::Warning("SWF %s: DuplicateMovieClip attempted for root movie clip",
                   swf->filename.str);
    return nullptr;
  }
  swfDisplayEntry_t* source = parent->FindDisplayEntry(depth);
  if (source == nullptr || source->spriteInstance != this) {
    idLib::Warning(
        "SWF %s: DuplicateMovieClip Couldn't find our display entry in our parent's display list",
        swf->filename.str);
    return nullptr;
  }
  swfDisplayEntry_t* duplicate =
      parent->AddDisplayEntry(displayDepth + 0x4000, source->characterID, objectName);
  if (duplicate == nullptr)
    return nullptr;
  duplicate->matrix = source->matrix;
  duplicate->cxf = source->cxf;
  duplicate->spriteInstance->RunTo(1);
  return duplicate->spriteInstance;
}

void idSWFSpriteInstance::Tween(const char* variable, float from, float to, int duration,
                                float (*function)(float, float, float, float), int pauseTime,
                                bool snapBack) {
  if (duration <= 0)
    return;
  int index = -1;
  for (int i = 0; i < tweens.Num(); ++i) {
    const char* tweenVariable = tweens[i].variable.str;
    if ((variable == nullptr && (tweenVariable == nullptr || tweenVariable[0] == '\0')) ||
        (variable != nullptr &&
         (tweenVariable == variable || idStr::Cmp(tweenVariable, variable) == 0))) {
      index = i;
      break;
    }
  }
  if (index < 0) {
    tween_t* allocated = tweens.Alloc();
    index = tweens.Num() - 1;
    allocated->variable.Set(variable);
  }
  tween_t& tween = tweens[index];
  tween.function = function;
  tween.startValue = from;
  tween.stopValue = to;
  tween.duration = duration;
  tween.startTime = swf->lastRenderTime;
  tween.pauseTime = pauseTime;
  tween.snapBack = snapBack;
}

void idSWFSpriteInstance::FreeDisplayList() {
  for (int i = 0; i < displayList.Num(); ++i) {
    if (displayList[i].spriteInstance != nullptr) swf->AddSpriteOrphan(displayList[i].spriteInstance);
    if (displayList[i].textInstance != nullptr) swf->AddTextOrphan(displayList[i].textInstance);
  }
  displayList.Clear();
  currentFrame = 0;
}
void idSWFSpriteInstance::ClearQueuedActions() { actions.Clear(); }
void idSWFSpriteInstance::RemoveDisplayEntry(int displayDepth) {
  for (int i=0;i<displayList.Num();++i) if (displayList[i].depth==displayDepth) {
    if (displayList[i].spriteInstance) swf->AddSpriteOrphan(displayList[i].spriteInstance);
    if (displayList[i].textInstance) swf->AddTextOrphan(displayList[i].textInstance);
    displayList.RemoveIndex(i); return;
  }
}
void idSWFSpriteInstance::SwapDepths(int a, int b) {
  for (int i = 0; i < displayList.Num(); ++i) {
    if (displayList[i].depth == a)
      displayList[i].depth = static_cast<std::uint16_t>(b);
    else if (displayList[i].depth == b)
      displayList[i].depth = static_cast<std::uint16_t>(a);
    if (displayList[i].spriteInstance != nullptr)
      displayList[i].spriteInstance->depth = displayList[i].depth;
    else if (displayList[i].textInstance != nullptr)
      displayList[i].textInstance->depth = displayList[i].depth;
  }
  std::sort(displayList.list, displayList.list + displayList.Num(),
            [](const swfDisplayEntry_t& left, const swfDisplayEntry_t& right) {
              return left.depth < right.depth;
            });
}

swfDisplayEntry_t* idSWFSpriteInstance::AddDisplayEntry(int displayDepth, int characterID,
                                                         idStr& objectName) {
  int insertion = 0;
  while (insertion < displayList.Num() && displayList[insertion].depth < displayDepth)
    ++insertion;
  if (insertion < displayList.Num() && displayList[insertion].depth == displayDepth)
    return nullptr;

  swfDisplayEntry_t entry;
  entry.depth = static_cast<std::uint16_t>(displayDepth);
  entry.characterID = static_cast<std::uint16_t>(characterID);
  const int index = displayList.Insert(entry, insertion);
  swfDisplayEntry_t* result = &displayList[index];

  idSWFDictionaryEntry* dictionaryEntry = swf->FindDictionaryEntry(characterID);
  if (dictionaryEntry == nullptr)
    return result;
  if (dictionaryEntry->type == SWF_DICT_SPRITE) {
    result->spriteInstance = swf->ExtractSpriteFromOrphanage(
        objectName, dictionaryEntry->sprite, displayDepth, this);
    if (result->spriteInstance == nullptr) {
      result->spriteInstance = new idSWFSpriteInstance(swf, dictionaryEntry->sprite);
      result->spriteInstance->Init(this, objectName.data, displayDepth);
      result->spriteInstance->RunTo(1);
    }
  } else if (dictionaryEntry->type == SWF_DICT_EDITTEXT) {
    result->textInstance = swf->ExtractTextFromOrphanage(
        objectName, dictionaryEntry->edittext, displayDepth, this);
    if (result->textInstance == nullptr) {
      result->textInstance = new idSWFTextInstance(swf, dictionaryEntry->edittext);
      result->textInstance->Init(this, objectName.data, displayDepth);
    }
  }
  return result;
}

bool idSWFSpriteInstance::Run() {
  if (!isVisible)
    return false;
  bool childRunning = false;
  if (childrenRunning) {
    childrenRunning = false;
    for (int i = 0; i < displayList.Num(); ++i) {
      if (displayList[i].spriteInstance != nullptr)
        childrenRunning = displayList[i].spriteInstance->Run() || childrenRunning;
    }
    childRunning = childrenRunning;
  }
  if (isPlaying && frameCount != 0) {
    if (currentFrame == frameCount) {
      if (frameCount > 1) {
        FreeDisplayList();
        RunTo(1);
      }
    } else {
      RunTo(currentFrame + 1);
    }
  }
  return childrenRunning || isPlaying || childRunning;
}

void idSWFSpriteInstance::RunQueuedActions() {
  for (int i = 0; i < actions.Num(); ++i) {
    actionScript->data = actions[i].data;
    actionScript->length = actions[i].dataLength;
    idSWFParmList parameters;
    actionScript->Call(scriptObject, parameters);
  }
  actions.Clear();
}
bool idSWFSpriteInstance::RunActions() {
  if (!isVisible) {
    ClearQueuedActions();
    return false;
  }
  if (firstRun && scriptObject->HasProperty("onLoad")) {
    firstRun = false;
    idSWFScriptVar onLoad = scriptObject->Get("onLoad");
    if (onLoad.IsFunction()) {
      idSWFParmList parameters;
      onLoad.GetFunction()->Call(scriptObject, parameters);
    }
  }
  if (onEnterFrame.IsFunction()) {
    idSWFParmList parameters;
    onEnterFrame.GetFunction()->Call(scriptObject, parameters);
  }
  for (int i = 0; i < tweens.Num(); ++i) {
    tween_t& tween = tweens[i];
    int elapsed = 0;
    if (tween.pauseTime < 0) {
      elapsed = swf->lastRenderTime - tween.startTime;
      if (elapsed >= tween.duration) {
        scriptObject->Set(tween.variable.str, idSWFScriptVar(tween.stopValue));
        tweens.RemoveIndex(i--);
        continue;
      }
      if (elapsed < 0) {
        tween.startTime = swf->lastRenderTime + 1;
        elapsed = 0;
      }
    } else {
      const int period = tween.pauseTime + tween.duration;
      const int cycle = (swf->lastRenderTime - tween.startTime) / period;
      const int cycleStart = tween.startTime + cycle * period;
      elapsed = std::min(swf->lastRenderTime - cycleStart, tween.duration);
      if ((cycle & 1) != 0 && !tween.snapBack)
        elapsed = tween.duration - elapsed;
    }
    const float value = tween.function(elapsed * 0.001f, tween.startValue,
                                       tween.stopValue - tween.startValue,
                                       tween.duration * 0.001f);
    scriptObject->Set(tween.variable.str, idSWFScriptVar(value));
  }
  RunQueuedActions();
  for (int i = 0; i < displayList.Num(); ++i)
    if (displayList[i].spriteInstance != nullptr)
      displayList[i].spriteInstance->RunActions();
  return true;
}

void idSWFSpriteInstance::StartSound(idSWFBitStream&) {}

void idSWFSpriteInstance::RunTo(int target) {
  if (target == currentFrame) return;
  if (target < currentFrame) FreeDisplayList();
  if (target >= 1 && sprite != nullptr) { sprite->RunFrameCommands(this,currentFrame,target); currentFrame=static_cast<std::uint16_t>(target); }
}
void idSWFSpriteInstance::PlayFrame(int frame) { ClearQueuedActions(); RunTo(std::max(frame,1)); Play(); }
void idSWFSpriteInstance::StopFrame(int frame) { RunTo(std::max(frame,1)); Stop(); }
void idSWFSpriteInstance::PlayFrame(const char* frame) { PlayFrame(static_cast<int>(FindFrame(frame))); }
void idSWFSpriteInstance::StopFrame(const char* frame) { StopFrame(static_cast<int>(FindFrame(frame))); }
void idSWFSpriteInstance::NextFrame() { if (currentFrame < frameCount) RunTo(currentFrame+1); }
void idSWFSpriteInstance::PrevFrame() { if (currentFrame > 1) RunTo(currentFrame-1); }

bool idSWFSpriteInstance::HandleSWFCommand(const swfSpriteCommand_t& command) {
  idSWFBitStream stream(command.data, static_cast<std::uint32_t>(command.dataLength), false);
  switch (command.tag) {
    case Tag_DoAction: actions.Append({command.data, static_cast<std::uint32_t>(command.dataLength)}); return true;
    case Tag_StartSound: StartSound(stream); return true;
    case Tag_PlaceObject2: PlaceObject2(stream); return true;
    case Tag_RemoveObject2: RemoveObject2(stream); return true;
    case Tag_PlaceObject3: PlaceObject3(stream); return true;
    default: return false;
  }
}

int idSWF::mouseX = -1;
int idSWF::mouseY = -1;

const char* idSWFScriptSprite::Class() const { return CLASS; }
bool idSWFScriptSprite::DerivedFrom(const char* className) const {
  return className == CLASS || className == idSWFScriptObject::CLASS;
}

namespace {
idSWFSpriteInstance* SpriteFromScriptObject(idSWFScriptObject* object) {
  if (object == nullptr || !object->DerivedFrom(idSWFScriptSprite::CLASS))
    return nullptr;
  return static_cast<idSWFScriptSprite*>(object)->sprite;
}

swfDisplayEntry_t* SpriteDisplayEntry(idSWFSpriteInstance* sprite) {
  if (sprite == nullptr || sprite->parent == nullptr)
    return nullptr;
  swfDisplayEntry_t* entry = sprite->parent->FindDisplayEntry(sprite->depth);
  return entry != nullptr && entry->spriteInstance == sprite ? entry : nullptr;
}

float (*FindTweenFunction(const char* name))(float, float, float, float) {
  if (name == nullptr || idStr::Icmp(name, "linear") == 0)
    return idSWFEasing::Linear::ease;
#define MATCH_EASING(prefix, family, member) \
  if (idStr::Icmp(name, prefix #family) == 0) return idSWFEasing::family::member
  MATCH_EASING("easeInOut", Back, easeInOut);
  MATCH_EASING("easeInOut", Bounce, easeInOut);
  MATCH_EASING("easeInOut", Circ, easeInOut);
  MATCH_EASING("easeInOut", Cubic, easeInOut);
  MATCH_EASING("easeInOut", Elastic, easeInOut);
  MATCH_EASING("easeInOut", Expo, easeInOut);
  MATCH_EASING("easeInOut", Quad, easeInOut);
  MATCH_EASING("easeInOut", Quart, easeInOut);
  MATCH_EASING("easeInOut", Quint, easeInOut);
  MATCH_EASING("easeInOut", Sine, easeInOut);
  MATCH_EASING("easeIn", Back, easeIn);
  MATCH_EASING("easeIn", Bounce, easeIn);
  MATCH_EASING("easeIn", Circ, easeIn);
  MATCH_EASING("easeIn", Cubic, easeIn);
  MATCH_EASING("easeIn", Elastic, easeIn);
  MATCH_EASING("easeIn", Expo, easeIn);
  MATCH_EASING("easeIn", Quad, easeIn);
  MATCH_EASING("easeIn", Quart, easeIn);
  MATCH_EASING("easeIn", Quint, easeIn);
  MATCH_EASING("easeIn", Sine, easeIn);
  MATCH_EASING("easeOut", Back, easeOut);
  MATCH_EASING("easeOut", Bounce, easeOut);
  MATCH_EASING("easeOut", Circ, easeOut);
  MATCH_EASING("easeOut", Cubic, easeOut);
  MATCH_EASING("easeOut", Elastic, easeOut);
  MATCH_EASING("easeOut", Expo, easeOut);
  MATCH_EASING("easeOut", Quad, easeOut);
  MATCH_EASING("easeOut", Quart, easeOut);
  MATCH_EASING("easeOut", Quint, easeOut);
  MATCH_EASING("easeOut", Sine, easeOut);
#undef MATCH_EASING
  return nullptr;
}
} // namespace

idSWFScriptObject_SpriteInstancePrototype::idSWFScriptObject_SpriteInstancePrototype() {
#define SET_FUNCTION(name) Set(#name, idSWFScriptVar(&scriptFunction_##name))
  SET_FUNCTION(duplicateMovieClip);
  SET_FUNCTION(gotoAndPlay);
  SET_FUNCTION(gotoAndStop);
  SET_FUNCTION(swapDepths);
  SET_FUNCTION(nextFrame);
  SET_FUNCTION(prevFrame);
  SET_FUNCTION(play);
  SET_FUNCTION(stop);
  SET_FUNCTION(toString);
  SET_FUNCTION(tween);
  SET_FUNCTION(removeTweens);
#undef SET_FUNCTION
#define SET_NATIVE(name) SetNative(#name, &swfScriptVar_##name)
  SET_NATIVE(_x);
  SET_NATIVE(_y);
  SET_NATIVE(_xscale);
  SET_NATIVE(_yscale);
  SET_NATIVE(_alpha);
  SET_NATIVE(_brightness);
  SET_NATIVE(_visible);
  SET_NATIVE(_width);
  SET_NATIVE(_height);
  SET_NATIVE(_rotation);
  SET_NATIVE(_name);
  SET_NATIVE(_currentframe);
  SET_NATIVE(_totalframes);
  SET_NATIVE(_target);
  SET_NATIVE(_framesloaded);
  SET_NATIVE(_droptarget);
  SET_NATIVE(_url);
  SET_NATIVE(_highquality);
  SET_NATIVE(_focusrect);
  SET_NATIVE(_soundbuftime);
  SET_NATIVE(_quality);
  SET_NATIVE(_mousex);
  SET_NATIVE(_mousey);
  SET_NATIVE(material);
  SET_NATIVE(materialWidth);
  SET_NATIVE(materialHeight);
  SET_NATIVE(xOffset);
  SET_NATIVE(onEnterFrame);
  SET_NATIVE(_followBorder);
  SET_NATIVE(_ignoreMouse);
#undef SET_NATIVE
}

#define SPRITE_CALL(name)                                                             \
  idSWFScriptVar idSWFScriptObject_SpriteInstancePrototype::idSWFScriptFunction_##name::Call( \
      idSWFScriptObject* object, idSWFParmList& parms)

SPRITE_CALL(play) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->Play();
  else idLib::Warning("SWF: tried to call play on a NULL sprite");
  return idSWFScriptVar();
}
SPRITE_CALL(stop) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->Stop();
  else idLib::Warning("SWF: tried to call stop on a NULL sprite");
  return idSWFScriptVar();
}
SPRITE_CALL(nextFrame) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->NextFrame();
  else idLib::Warning("SWF: tried to call nextFrame on a NULL sprite");
  return idSWFScriptVar();
}
SPRITE_CALL(prevFrame) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->PrevFrame();
  else idLib::Warning("SWF: tried to call prevFrame on a NULL sprite");
  return idSWFScriptVar();
}
SPRITE_CALL(gotoAndPlay) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite == nullptr) {
    idLib::Warning("SWF: tried to call gotoAndPlay on a NULL sprite");
  } else if (parms.Num() < 1) {
    idLib::Warning("SWF %s: gotoAndPlay expected 1 parameter", sprite->swf->filename.str);
  } else {
    const idStr frame = parms[0].ToString();
    sprite->PlayFrame(static_cast<int>(sprite->FindFrame(frame.data)));
  }
  return idSWFScriptVar();
}
SPRITE_CALL(gotoAndStop) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite == nullptr) {
    idLib::Warning("SWF: tried to call gotoAndStop on a NULL sprite");
  } else if (parms.Num() < 1) {
    idLib::Warning("SWF %s: gotoAndStop expected 1 parameter", sprite->swf->filename.str);
  } else if (parms[0].IsNumeric() && parms[0].ToInteger() < 1) {
    sprite->RunTo(static_cast<int>(sprite->FindFrame("1")));
    sprite->Stop();
  } else {
    const idStr frame = parms[0].ToString();
    sprite->StopFrame(static_cast<int>(sprite->FindFrame(frame.data)));
  }
  return idSWFScriptVar();
}
SPRITE_CALL(swapDepths) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite == nullptr) {
    idLib::Warning("SWF: tried to call swapDepths on a NULL sprite");
  } else if (sprite->parent == nullptr) {
    idLib::Warning("SWF %s: swapDepths attempted on root movie clip", sprite->swf->filename.str);
  } else if (parms.Num() < 1) {
    idLib::Warning("SWF %s: swapDepths expected 1 parameters", sprite->swf->filename.str);
  } else {
    sprite->parent->SwapDepths(sprite->depth, static_cast<int>(parms[0].ToInteger()));
  }
  return idSWFScriptVar();
}
SPRITE_CALL(toString) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite == nullptr) {
    idLib::Warning("SWF: tried to call toString on a NULL sprite");
    return idSWFScriptVar();
  }
  idStr path = sprite->GetTargetPath();
  path.ReplaceChar('/', '.');
  return idSWFScriptVar(path);
}
SPRITE_CALL(duplicateMovieClip) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite == nullptr) {
    idLib::Warning("SWF: tried to call duplicateMovieClip on a NULL sprite");
    return idSWFScriptVar();
  }
  if (parms.Num() < 2) {
    idLib::Warning("SWF %s: duplicateMovieClip expected 2 parameters", sprite->swf->filename.str);
    return idSWFScriptVar();
  }
  idStr objectName = parms[0].ToString();
  idSWFSpriteInstance* duplicate =
      sprite->DuplicateMovieClip(static_cast<int>(parms[1].ToInteger()) + 0x4000, objectName);
  return duplicate != nullptr ? idSWFScriptVar(duplicate->scriptObject) : idSWFScriptVar();
}
SPRITE_CALL(removeTweens) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite == nullptr) {
    idLib::Warning("SWF: tried to call removeTweens on a NULL sprite");
  } else if (parms.Num() == 0) {
    sprite->RemoveTweens(nullptr);
  } else {
    const idStr variable = parms[0].ToString();
    sprite->RemoveTweens(variable.data);
  }
  return idSWFScriptVar();
}
SPRITE_CALL(tween) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite == nullptr) {
    idLib::Warning("SWF: tried to call tween on a NULL sprite");
    return idSWFScriptVar();
  }
  if (parms.Num() < 4) {
    idLib::Warning("SWF %s: tween expected at least 4 parameters", sprite->swf->filename.str);
    return idSWFScriptVar();
  }
  float (*function)(float, float, float, float) = idSWFEasing::Linear::ease;
  if (parms.Num() > 4) {
    const idStr tweenType = parms[4].ToString();
    float (*selected)(float, float, float, float) = FindTweenFunction(tweenType.data);
    if (selected == nullptr)
      idLib::Warning("SWF %s: unknown tween type '%s'", sprite->swf->filename.str,
                     tweenType.data);
    else
      function = selected;
  }
  const int pauseTime = parms.Num() > 5 ? static_cast<int>(parms[5].ToNumber() * 1000.0) : -1;
  const bool snapBack = parms.Num() > 6 && parms[6].ToBool();
  const idStr variable = parms[0].ToString();
  sprite->Tween(variable.data, static_cast<float>(parms[1].ToNumber()),
                static_cast<float>(parms[2].ToNumber()),
                static_cast<int>(parms[3].ToNumber() * 1000.0), function, pauseTime, snapBack);
  return idSWFScriptVar();
}
#undef SPRITE_CALL

#define SPRITE_GET(name)                                                              \
  idSWFScriptVar idSWFScriptObject_SpriteInstancePrototype::idSWFScriptNativeVar_##name::Get( \
      idSWFScriptObject* object)
#define SPRITE_SET(name)                                                              \
  void idSWFScriptObject_SpriteInstancePrototype::idSWFScriptNativeVar_##name::Set(    \
      idSWFScriptObject* object, const idSWFScriptVar& value)

SPRITE_GET(_target) { return idSWFScriptVar(""); }
SPRITE_GET(_droptarget) { return idSWFScriptVar(""); }
SPRITE_GET(_url) { return idSWFScriptVar(""); }
SPRITE_GET(_highquality) { return idSWFScriptVar(2); }
SPRITE_GET(_focusrect) { return idSWFScriptVar(true); }
SPRITE_GET(_soundbuftime) { return idSWFScriptVar(0); }
SPRITE_GET(_quality) { return idSWFScriptVar("BEST"); }
SPRITE_GET(_width) { return idSWFScriptVar(0.0); }
SPRITE_GET(_height) { return idSWFScriptVar(0.0); }

SPRITE_GET(_z) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(sprite->z) : idSWFScriptVar();
}
SPRITE_SET(_z) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->z = static_cast<float>(value.ToNumber());
  else idLib::Warning("SWF: tried to set _z on a NULL sprite");
}
SPRITE_GET(_x) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(sprite->GetPos().x) : idSWFScriptVar();
}
SPRITE_SET(_x) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->SetPosX(static_cast<float>(value.ToNumber()));
  else idLib::Warning("SWF: tried to set _x on a NULL sprite");
}
SPRITE_GET(_y) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(sprite->GetPos().y) : idSWFScriptVar();
}
SPRITE_SET(_y) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->SetPosY(static_cast<float>(value.ToNumber()));
  else idLib::Warning("SWF: tried to set _y on a NULL sprite");
}
SPRITE_GET(_xscale) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  swfDisplayEntry_t* entry = SpriteDisplayEntry(sprite);
  return idSWFScriptVar(entry != nullptr
                            ? std::hypot(entry->matrix.xx, entry->matrix.yx) * 100.0
                            : 1.0);
}
SPRITE_SET(_xscale) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  swfDisplayEntry_t* entry = SpriteDisplayEntry(sprite);
  if (entry == nullptr) return;
  const float scale = static_cast<float>(value.ToNumber() / 100.0);
  const float length = std::hypot(entry->matrix.xx, entry->matrix.yx);
  if (length == 0.0f) { entry->matrix.xx = scale; entry->matrix.yx = 0.0f; }
  else { entry->matrix.xx *= scale / length; entry->matrix.yx *= scale / length; }
}
SPRITE_GET(_yscale) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  swfDisplayEntry_t* entry = SpriteDisplayEntry(sprite);
  return idSWFScriptVar(entry != nullptr
                            ? std::hypot(entry->matrix.xy, entry->matrix.yy) * 100.0
                            : 1.0);
}
SPRITE_SET(_yscale) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  swfDisplayEntry_t* entry = SpriteDisplayEntry(sprite);
  if (entry == nullptr) return;
  const float scale = static_cast<float>(value.ToNumber() / 100.0);
  const float length = std::hypot(entry->matrix.xy, entry->matrix.yy);
  if (length == 0.0f) { entry->matrix.yy = scale; entry->matrix.xy = 0.0f; }
  else { entry->matrix.yy *= scale / length; entry->matrix.xy *= scale / length; }
}
SPRITE_GET(_alpha) {
  swfDisplayEntry_t* entry = SpriteDisplayEntry(SpriteFromScriptObject(object));
  return idSWFScriptVar(entry != nullptr ? entry->cxf.mul.w : 1.0f);
}
SPRITE_SET(_alpha) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->SetAlpha(static_cast<float>(value.ToNumber()));
}
SPRITE_GET(_brightness) {
  swfDisplayEntry_t* entry = SpriteDisplayEntry(SpriteFromScriptObject(object));
  if (entry == nullptr) return idSWFScriptVar(1.0f);
  const float averageMultiply = (entry->cxf.mul.x + entry->cxf.mul.y + entry->cxf.mul.z) / 3.0f;
  const float averageAdd = (entry->cxf.add.x + entry->cxf.add.y + entry->cxf.add.z) / 3.0f;
  return idSWFScriptVar(averageAdd > 1.0f ? averageAdd : averageMultiply - 1.0f);
}
SPRITE_SET(_brightness) {
  swfDisplayEntry_t* entry = SpriteDisplayEntry(SpriteFromScriptObject(object));
  if (entry == nullptr) return;
  float brightness = static_cast<float>(value.ToNumber());
  float multiply = 1.0f - brightness;
  if (brightness < 0.0f) { multiply = 1.0f + brightness; brightness = 0.0f; }
  entry->cxf.add.x = entry->cxf.add.y = entry->cxf.add.z = brightness;
  entry->cxf.mul.x = entry->cxf.mul.y = entry->cxf.mul.z = multiply;
}
SPRITE_GET(_visible) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(sprite->isVisible) : idSWFScriptVar();
}
SPRITE_SET(_visible) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->SetVisible(value.ToBool());
}
SPRITE_GET(_rotation) {
  swfDisplayEntry_t* entry = SpriteDisplayEntry(SpriteFromScriptObject(object));
  return idSWFScriptVar(entry != nullptr
                            ? std::atan2(entry->matrix.yx, entry->matrix.xx) *
                                  (180.0 / 3.14159265358979323846)
                            : 0.0);
}
SPRITE_SET(_rotation) {
  swfDisplayEntry_t* entry = SpriteDisplayEntry(SpriteFromScriptObject(object));
  if (entry == nullptr) return;
  const float xScale = std::hypot(entry->matrix.xx, entry->matrix.yx);
  const float yScale = std::hypot(entry->matrix.xy, entry->matrix.yy);
  const float radians = static_cast<float>(value.ToNumber() * 0.017453292519943295);
  const float sine = std::sin(radians), cosine = std::cos(radians);
  entry->matrix.xx = cosine * xScale; entry->matrix.yx = sine * xScale;
  entry->matrix.xy = -sine * yScale; entry->matrix.yy = cosine * yScale;
}
SPRITE_GET(_name) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(sprite->name.str) : idSWFScriptVar();
}
SPRITE_GET(_currentframe) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(static_cast<int>(sprite->currentFrame)) : idSWFScriptVar();
}
SPRITE_GET(_totalframes) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(static_cast<int>(sprite->frameCount)) : idSWFScriptVar();
}
SPRITE_GET(_framesloaded) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(static_cast<int>(sprite->frameCount)) : idSWFScriptVar();
}
SPRITE_GET(_mousex) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  swfDisplayEntry_t* entry = SpriteDisplayEntry(sprite);
  return entry != nullptr ? idSWFScriptVar(static_cast<float>(idSWF::mouseX) - entry->matrix.ty)
                          : idSWFScriptVar(idSWF::mouseX);
}
SPRITE_GET(_mousey) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  swfDisplayEntry_t* entry = SpriteDisplayEntry(sprite);
  return entry != nullptr ? idSWFScriptVar(static_cast<float>(idSWF::mouseY) - entry->matrix.ty)
                          : idSWFScriptVar(idSWF::mouseY);
}
SPRITE_GET(material) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr && sprite->materialOverride != nullptr
             ? idSWFScriptVar(sprite->materialOverride->GetName())
             : idSWFScriptVar();
}
SPRITE_SET(material) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite == nullptr) return;
  if (value.IsString()) { const idStr name = value.ToString(); sprite->SetMaterial(name.data, -1, -1); }
  else sprite->materialOverride = nullptr;
}
SPRITE_GET(materialWidth) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(static_cast<int>(sprite->materialWidth)) : idSWFScriptVar();
}
SPRITE_SET(materialWidth) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->materialWidth = static_cast<std::uint16_t>(value.ToInteger());
}
SPRITE_GET(materialHeight) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(static_cast<int>(sprite->materialHeight)) : idSWFScriptVar();
}
SPRITE_SET(materialHeight) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->materialHeight = static_cast<std::uint16_t>(value.ToInteger());
}
SPRITE_GET(xOffset) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(sprite->xOffset) : idSWFScriptVar();
}
SPRITE_SET(xOffset) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->xOffset = static_cast<float>(value.ToNumber());
}
SPRITE_GET(onEnterFrame) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? sprite->onEnterFrame : idSWFScriptVar();
}
SPRITE_SET(onEnterFrame) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->onEnterFrame = value;
}
SPRITE_GET(_followBorder) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(sprite->followBorder) : idSWFScriptVar();
}
SPRITE_SET(_followBorder) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->followBorder = static_cast<int>(value.ToInteger());
}
SPRITE_GET(_ignoreMouse) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  return sprite != nullptr ? idSWFScriptVar(sprite->ignoreMouse) : idSWFScriptVar();
}
SPRITE_SET(_ignoreMouse) {
  idSWFSpriteInstance* sprite = SpriteFromScriptObject(object);
  if (sprite != nullptr) sprite->ignoreMouse = value.ToBool();
}
#undef SPRITE_SET
#undef SPRITE_GET
