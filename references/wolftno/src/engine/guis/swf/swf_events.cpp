#include "engine/guis/swf/swf.h"

#include "engine/framework/keyinput.h"
#include "engine/guis/swf/swf_spriteinstance.h"
#include "engine/guis/swf/swf_textinstance.h"
#include "engine/renderer/renderersystem.h"
#include "engine/sys/sys_public.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>

namespace {
bool PointInTriangle(float x, float y, const idVec2& a, const idVec2& b, const idVec2& c) {
  const float denominator = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
  if (denominator == 0.0f) return false;
  const float alpha = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / denominator;
  const float beta = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / denominator;
  const float gamma = 1.0f - alpha - beta;
  const bool positive = alpha > 0.0f;
  return positive == (beta > 0.0f) && positive == (gamma > 0.0f);
}
void Append(idSWFParmList& p, int v) { p.Append(idSWFScriptVar(v)); }
void Append(idSWFParmList& p, float v) { p.Append(idSWFScriptVar(v)); }
void Append(idSWFParmList& p, bool v) { p.Append(idSWFScriptVar(v)); }
void Append(idSWFParmList& p, const char* v) { p.Append(idSWFScriptVar(v)); }
void Append(idSWFParmList& p, idSWFScriptObject* v) { p.Append(idSWFScriptVar(v)); }
bool CallBool(const idSWFScriptVar& f, idSWFScriptObject* o, idSWFParmList& p) {
  return f.IsFunction() && f.GetFunction()->Call(o, p).ToBool();
}
bool HasMouseHandler(idSWFScriptObject* o) {
  return o->HasValidProperty("onRelease") || o->HasValidProperty("onPress") ||
         o->HasValidProperty("onRollOver") || o->HasValidProperty("onRollOut") ||
         o->HasValidProperty("onDrag");
}
}

idSWFScriptObject* idSWF::HitTest(idSWFSpriteInstance* spriteInstance,
                                  const swfRenderState_t& renderState, bool* overObject,
                                  int x, int y, idSWFScriptObject* parentObject) {
  if (spriteInstance == nullptr) return nullptr;
  if (spriteInstance->parent != nullptr) {
    swfDisplayEntry_t* parentEntry = spriteInstance->parent->FindDisplayEntry(spriteInstance->depth);
    if (parentEntry == nullptr) {
      idLib::Warning("SWF %s: HitTest called on sprite with parent, but not in parent's DisplayList!",
                     filename.str);
      return nullptr;
    }
    if (parentEntry->cxf.mul.w + parentEntry->cxf.add.w < 0.001f) return nullptr;
  }
  if (!spriteInstance->isVisible || spriteInstance->ignoreMouse) return nullptr;
  if (HasMouseHandler(spriteInstance->scriptObject)) parentObject = spriteInstance->scriptObject;

  idSWFScriptObject* result = nullptr;
  const float xOffset = spriteInstance->xOffset;
  const float yOffset = spriteInstance->yOffset;
  for (int displayIndex = 0; displayIndex < spriteInstance->displayList.Num(); ++displayIndex) {
    swfDisplayEntry_t& display = spriteInstance->displayList[displayIndex];
    idSWFDictionaryEntry* entry = FindDictionaryEntry(display.characterID);
    if (entry == nullptr) continue;
    swfRenderState_t state;
    state.matrix = display.matrix.Multiply(renderState.matrix);
    state.ratio = display.ratio;
    if (entry->type == SWF_DICT_SPRITE) {
      idSWFScriptObject* object = HitTest(display.spriteInstance, state, overObject, x, y, parentObject);
      if (object != nullptr && object->Get("_visible").ToBool()) result = object;
    } else if (entry->type == SWF_DICT_SHAPE && (parentObject != nullptr || arGui)) {
      idSWFShape* shape = entry->shape;
      for (int fillIndex = 0; fillIndex < shape->fillDraws.Num(); ++fillIndex) {
        idSWFShapeDrawFill& fill = shape->fillDraws[fillIndex];
        for (int index = 0; index + 2 < fill.indices.Num(); index += 3) {
          idVec2 a = state.matrix.Transform(fill.startVerts[fill.indices[index]]);
          idVec2 b = state.matrix.Transform(fill.startVerts[fill.indices[index + 1]]);
          idVec2 c = state.matrix.Transform(fill.startVerts[fill.indices[index + 2]]);
          a.x += xOffset; a.y += yOffset;
          b.x += xOffset; b.y += yOffset;
          c.x += xOffset; c.y += yOffset;
          if (PointInTriangle(static_cast<float>(x), static_cast<float>(y), a, b, c)) {
            if (parentObject != nullptr) result = parentObject;
            if (overObject != nullptr) *overObject = true;
          }
        }
      }
    } else if (entry->type == SWF_DICT_EDITTEXT && display.textInstance != nullptr) {
      idSWFTextInstance* textInstance = display.textInstance;
      idSWFScriptObject* editObject = nullptr;
      if (textInstance->scriptObject->HasProperty("onRelease") ||
          textInstance->scriptObject->HasProperty("onPress")) editObject = textInstance->scriptObject;
      else if (parentObject != nullptr) editObject = parentObject;
      if (editObject == nullptr || textInstance->text.IsEmpty()) continue;
      const idSWFEditText* shape = entry->edittext;
      const float textLength = textInstance->GetTextLength();
      const float difference = std::fabs(shape->bounds.br.x - shape->bounds.tl.x) - textLength;
      float left = shape->bounds.tl.x + xOffset;
      float right = shape->bounds.br.x + xOffset;
      if (textInstance->editText->align == SWF_ET_ALIGN_LEFT) right -= difference;
      else if (textInstance->editText->align == SWF_ET_ALIGN_RIGHT) left += difference;
      else if (textInstance->editText->align == SWF_ET_ALIGN_CENTER) {
        const float middle = (left + right) * 0.5f;
        left = middle - textLength * 0.5f;
        right = middle + textLength * 0.5f;
      }
      const float top = shape->bounds.tl.y + yOffset;
      const float bottom = shape->bounds.br.y + yOffset;
      const idVec2 tl = state.matrix.Transform({left, top});
      const idVec2 tr = state.matrix.Transform({right, top});
      const idVec2 br = state.matrix.Transform({right, bottom});
      const idVec2 bl = state.matrix.Transform({left, bottom});
      if (PointInTriangle(static_cast<float>(x), static_cast<float>(y), tl, tr, br)) result = editObject;
      if (PointInTriangle(static_cast<float>(x), static_cast<float>(y), tl, br, bl)) {
        result = editObject;
        if (overObject != nullptr) *overObject = true;
      }
    }
  }
  return result;
}

bool idSWF::HandleEvent(const sysEvent_t* event) {
  if (event == nullptr || frameRate == 0 || !isActive || (!inhibitControl && useInhibitControl))
    return false;
  if (event->evType == SE_KEY && static_cast<std::uint8_t>(event->evValue2 >> 1) == 0) {
    const bool down = (event->evValue2 & 1) != 0;
    if (event->evValue == K_MOUSE1) {
      mouseEnabled = true;
      idSWFScriptVar calledFunction;
      idSWFScriptObject* hitObject = nullptr;
      if (down) {
        idSWFScriptVar waitInput = GetGlobal("waitInput");
        if (waitInput.IsFunction()) {
          useMouse = false;
          idSWFParmList waitParms;
          Append(waitParms, event->evValue);
          Append(waitParms, true);
          waitInput.GetFunction()->Call(nullptr, waitParms);
        } else useMouse = true;
        cursorOverObject = false;
        hitObject = HitTest(mainspriteInstance, swfRenderState_t(), &cursorOverObject,
                            mouseX, mouseY, nullptr);
        if (hitObject != nullptr) {
          mouseObject = hitObject;
          mouseObject->AddRef();
          calledFunction = hitObject->Get("onPress");
          if (calledFunction.IsFunction()) {
            idSWFParmList parms;
            Append(parms, event->inputDevice);
            Append(parms, hitObject);
            calledFunction.GetFunction()->Call(hitObject, parms);
          }
          idSWFScriptVar drag = hitObject->Get("onDrag");
          if (drag.IsFunction()) {
            idSWFParmList parms;
            Append(parms, event->inputDevice); Append(parms, mouseX); Append(parms, mouseY);
            Append(parms, true);
            drag.GetFunction()->Call(hitObject, parms);
            return true;
          }
          if (calledFunction.IsFunction()) return true;
        }
        idSWFParmList hitParms;
        Append(hitParms, hitObject);
        Invoke("setHitObject", hitParms);
      } else {
        if (mouseObject != nullptr) {
          calledFunction = mouseObject->Get("onRelease");
          if (calledFunction.IsFunction()) {
            idSWFParmList parms;
            Append(parms, mouseObject);
            calledFunction.GetFunction()->Call(mouseObject, parms);
          }
          mouseObject->Release(); mouseObject = nullptr;
        }
        if (hoverObject != nullptr) { hoverObject->Release(); hoverObject = nullptr; }
        if (calledFunction.IsFunction()) return true;
      }
      return false;
    }
    if (focusWindow != nullptr) {
      idSWFScriptVar onKey = focusWindow->scriptObject->Get("onKey");
      if (onKey.IsFunction()) {
        idSWFScriptObject* focusObject = focusWindow->scriptObject;
        const bool wasPressed = focusObject->Get("_kpressed").ToBool();
        focusObject->Set("_kpressed", idSWFScriptVar(down));
        if (down || wasPressed) {
          idSWFParmList parms;
          Append(parms, event->evValue); Append(parms, down);
          if (CallBool(onKey, focusObject, parms)) return true;
        } else if (event->evValue == K_LSHIFT || event->evValue == K_RSHIFT) {
          idSWFParmList parms;
          Append(parms, event->evValue); Append(parms, down);
          onKey.GetFunction()->Call(focusObject, parms);
        }
      }
    }
    const char* keyName = idKeyInput::KeyNumToString(static_cast<keyNum_t>(event->evValue));
    idSWFScriptVar shortcut = shortcutKeys->Get(keyName);
    for (int runaway = 0; runaway < 32; ++runaway) {
      idSWFParmList eventParms;
      idSWFScriptVar waitInput = GetGlobal("waitInput");
      if (waitInput.IsFunction()) {
        useMouse = false;
        idSWFParmList waitParms;
        Append(waitParms, event->evValue); Append(waitParms, down); Append(waitParms, event->inputDevice);
        waitInput.GetFunction()->Call(nullptr, waitParms);
        return true;
      }
      useMouse = true;
      if (shortcut.IsString()) {
        shortcut = shortcutKeys->Get(shortcut.ToString().data);
        continue;
      }
      if (shortcut.IsObject()) {
        idSWFScriptObject* object = shortcut.GetObject();
        const bool wasPressed = object->Get("_pressed").ToBool();
        object->Set("_pressed", idSWFScriptVar(down));
        if (down) {
          shortcut = object->Get("onPress");
          Append(eventParms, event->inputDevice); Append(eventParms, object);
        } else if (wasPressed) shortcut = object->Get("onRelease");
        if (shortcut.IsFunction()) {
          shortcut.GetFunction()->Call(object, eventParms);
          return true;
        }
      } else if (shortcut.IsFunction()) {
        if (down) { shortcut.GetFunction()->Call(nullptr, eventParms); return true; }
        return false;
      }
      idSWFScriptVar useFunction = GetGlobal("useFunction");
      if (useFunction.IsFunction() && down) {
        const char* binding = idKeyInput::GetBinding(event->inputDevice, BIND_DEFAULT,
                                                     static_cast<keyNum_t>(event->evValue), 0);
        if (idStr::Cmp("_use", binding) == 0) {
          idSWFParmList noParms;
          useFunction.GetFunction()->Call(nullptr, noParms);
        }
      }
      return false;
    }
    idLib::Warning("Circular reference in %s shortcutKeys.%s", filename.str, keyName);
    return false;
  }
  if (event->evType == SE_CHAR) {
    if (focusWindow == nullptr) return false;
    idSWFScriptVar onChar = focusWindow->scriptObject->Get("onChar");
    if (event->evValue != 13 && onChar.IsFunction()) {
      idSWFParmList parms;
      Append(parms, event->evValue);
      Append(parms, idKeyInput::KeyNumToString(static_cast<keyNum_t>(event->evValue)));
      onChar.GetFunction()->Call(focusWindow->scriptObject, parms).ToBool();
      return true;
    }
    return false;
  }
  if (event->evType == SE_MOUSE || event->evType == SE_MOUSE_ABSOLUTE) {
    mouseEnabled = true;
    idSWFParmList mouseParms;
    Append(mouseParms, event->evValue); Append(mouseParms, event->evValue2);
    Invoke("onMouse", mouseParms);
    if (event->evType == SE_MOUSE_ABSOLUTE && renderSystem != nullptr) {
      const float aspect = renderSystem->GetPixelAspect();
      const float systemWidth = renderSystem->GetWidth() * std::max(aspect, 1.0f);
      const float systemHeight = renderSystem->GetHeight() / std::min(aspect, 1.0f);
      const float scale = swfScale * systemHeight / frameHeight;
      mouseX = static_cast<int>((event->evValue - 0.5f * (systemWidth - frameWidth * scale)) / scale);
      mouseY = static_cast<int>((event->evValue2 - 0.5f * (systemHeight - frameHeight * scale)) / scale);
    } else {
      mouseX = std::clamp(mouseX + event->evValue, static_cast<int>(-renderBorder),
                          static_cast<int>(frameWidth + renderBorder));
      mouseY = std::clamp(mouseY + event->evValue2, 0, static_cast<int>(frameHeight));
    }
    bool handled = false;
    cursorOverObject = false;
    idSWFScriptObject* hitObject = HitTest(mainspriteInstance, swfRenderState_t(),
                                           &cursorOverObject, mouseX, mouseY, nullptr);
    hasHitObject = hitObject != nullptr;
    if (hitObject != hoverObject) {
      if (hoverObject != nullptr) {
        idSWFScriptVar rollOut = hoverObject->Get("onRollOut");
        if (rollOut.IsFunction()) {
          idSWFParmList parms; rollOut.GetFunction()->Call(hoverObject, parms); handled = true;
        }
        hoverObject->Release(); hoverObject = nullptr;
      }
      if (hitObject != nullptr) {
        hoverObject = hitObject; hoverObject->AddRef();
        idSWFScriptVar rollOver = hitObject->Get("onRollOver");
        if (rollOver.IsFunction()) {
          idSWFParmList parms; rollOver.GetFunction()->Call(hitObject, parms); handled = true;
        }
      }
    }
    if (mouseObject != nullptr) {
      idSWFScriptVar drag = mouseObject->Get("onDrag");
      if (drag.IsFunction()) {
        idSWFParmList parms;
        Append(parms, event->inputDevice); Append(parms, mouseX); Append(parms, mouseY);
        Append(parms, false);
        drag.GetFunction()->Call(mouseObject, parms);
        return true;
      }
    }
    return handled;
  }
  if (event->evType == SE_MOUSE_LEAVE) { mouseEnabled = false; return true; }
  if (event->evType == SE_JOYSTICK) {
    idSWFParmList parms;
    Append(parms, event->evValue); Append(parms, event->evValue2 * (1.0f / 32.0f));
    return Invoke("onJoystick", parms);
  }
  return false;
}
