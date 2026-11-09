#include "engine/guis/swf/swf_scriptfunctionas2.h"

#include "engine/guis/swf/swf_bitstream.h"
#include "engine/guis/swf/swf_enums.h"
#include "engine/guis/swf/swf_spriteinstance.h"
#include "shared/idlib/sys/sys_time.h"

#include <cmath>

namespace {
const char* PropertyName(int index) {
  static const char* names[] = {"_x", "_y", "_xscale", "_yscale", "_currentframe",
                                "_totalframes", "_alpha", "_visible", "_width", "_height",
                                "_rotation", "_target", "_framesloaded", "_name", "_droptarget",
                                "_url", "_highquality", "_focusrect", "_soundbuftime", "_quality",
                                "_mousex", "_mousey"};
  return index >= 0 && index < static_cast<int>(std::size(names)) ? names[index] : "";
}

idSWFScriptVar ObjectIndexedGet(idSWFScriptObject* object, const idSWFScriptVar& key) {
  if (object == nullptr)
    return idSWFScriptVar();
  if (key.IsNumeric() && object->DerivedFrom(idSWFScriptArray::CLASS))
    return static_cast<idSWFScriptArray*>(object)->Get(static_cast<int>(key.ToInteger()));
  return object->Get(key.ToString().data);
}

void ObjectIndexedSet(idSWFScriptObject* object, const idSWFScriptVar& key,
                      const idSWFScriptVar& value) {
  if (object == nullptr)
    return;
  if (key.IsNumeric() && object->DerivedFrom(idSWFScriptArray::CLASS))
    static_cast<idSWFScriptArray*>(object)->Set(static_cast<int>(key.ToInteger()), value);
  else
    object->Set(key.ToString().data, value);
}
} // namespace

idSWFConstantPoolAS2::~idSWFConstantPoolAS2() { Clear(); }

idSWFScriptArray::idSWFScriptArray() : length(0), sparseIndex(), sparseValues() {
  sparseIndex.memTag = 72;
  sparseValues.memTag = 72;
}

idSWFScriptArray::~idSWFScriptArray() = default;
const char* idSWFScriptArray::Class() const { return CLASS; }
bool idSWFScriptArray::DerivedFrom(const char* className) const {
  return className == CLASS || className == idSWFScriptObject::CLASS;
}

void idSWFStack::Pop(int count) { SetNum(Num() - count); }

idSWFScriptFunctionAS2::idSWFScriptFunctionAS2()
    : flags(0), data(nullptr), length(0), defaultSprite(nullptr), scope(), constants(),
      registers(), parameters() {
  registers.memTag = 72;
  parameters.memTag = 72;
  registers.SetNum(4);
}

idSWFScriptFunctionAS2::~idSWFScriptFunctionAS2() {
  for (int i = 0; i < scope.Num(); ++i)
    if (scope[i] != nullptr)
      scope[i]->Release();
}

void idSWFScriptFunctionAS2::AllocRegisters(int count) { registers.SetNum(count); }
void idSWFScriptFunctionAS2::AllocParameters(int count) { parameters.SetNum(count); }

void idSWFScriptFunctionAS2::SetScope(const idList<idSWFScriptObject*>& newScope) {
  for (int i = 0; i < scope.Num(); ++i)
    if (scope[i] != nullptr)
      scope[i]->Release();
  scope.Clear();
  for (int i = 0; i < newScope.Num(); ++i) {
    scope.Append(newScope[i]);
    if (scope[i] != nullptr)
      scope[i]->AddRef();
  }
}

idSWFScriptVar idSWFScriptFunctionAS2::Run(idSWFScriptObject* thisObject, idSWFStack& stack,
                                            idSWFBitStream& bitstream) {
  idSWFSpriteInstance* thisSprite = thisObject != nullptr ? thisObject->Get("_target").GetSprite() : nullptr;
  if (thisObject != nullptr && thisObject->DerivedFrom(idSWFScriptSprite::CLASS))
    thisSprite = static_cast<idSWFScriptSprite*>(thisObject)->sprite;
  idSWFSpriteInstance* currentTarget = thisSprite != nullptr ? thisSprite : defaultSprite;

  while (bitstream.Tell() < bitstream.Length()) {
    const swfAction_t code = static_cast<swfAction_t>(bitstream.ReadU8());
    const std::uint16_t recordLength = code >= 0x80 ? bitstream.ReadU16() : 0;
    switch (code) {
      case Action_End: return idSWFScriptVar();
      case Action_Return: return stack.Num() > 0 ? stack.A() : idSWFScriptVar();
      case Action_NextFrame: if (currentTarget) currentTarget->NextFrame(); break;
      case Action_PrevFrame: if (currentTarget) currentTarget->PrevFrame(); break;
      case Action_Play: if (currentTarget) currentTarget->Play(); break;
      case Action_Stop: if (currentTarget) currentTarget->Stop(); break;
      case Action_ToggleQuality:
      case Action_StopSounds: break;
      case Action_GotoFrame: {
        const int frame = bitstream.ReadU16() + 1;
        if (currentTarget) currentTarget->RunTo(frame);
        break;
      }
      case Action_SetTarget: {
        const char* target = reinterpret_cast<const char*>(bitstream.ReadData(recordLength));
        currentTarget = thisSprite != nullptr ? thisSprite->ResolveTarget(target) : nullptr;
        break;
      }
      case Action_GoToLabel: {
        const char* label = reinterpret_cast<const char*>(bitstream.ReadData(recordLength));
        if (currentTarget) currentTarget->RunTo(static_cast<int>(currentTarget->FindFrame(label)));
        break;
      }
      case Action_Push: {
        idSWFBitStream push(bitstream.ReadData(recordLength), recordLength, false);
        while (push.Tell() < push.Length()) {
          switch (push.ReadU8()) {
            case 0: stack.Alloc()->SetString(push.ReadString()); break;
            case 1: stack.Alloc()->SetFloat(push.ReadFloat()); break;
            case 2: stack.Alloc()->SetNULL(); break;
            case 3: stack.Alloc()->SetUndefined(); break;
            case 4: {
              const unsigned reg = push.ReadU8();
              *stack.Alloc() = reg < static_cast<unsigned>(registers.Num()) ? registers[reg]
                                                                            : idSWFScriptVar();
              break;
            }
            case 5: stack.Alloc()->SetBool(push.ReadU8() != 0); break;
            case 6: stack.Alloc()->SetDouble(push.ReadDouble()); break;
            case 7: stack.Alloc()->SetInteger(push.ReadS32()); break;
            case 8: {
              const unsigned i = push.ReadU8();
              stack.Alloc()->SetString(i < static_cast<unsigned>(constants.pool.Num())
                                          ? constants.pool[i]->data : "");
              break;
            }
            case 9: {
              const unsigned i = push.ReadU16();
              stack.Alloc()->SetString(i < static_cast<unsigned>(constants.pool.Num())
                                          ? constants.pool[i]->data : "");
              break;
            }
            default: return idSWFScriptVar();
          }
        }
        break;
      }
      case Action_Pop: if (stack.Num()) stack.Pop(1); break;
      case Action_Add: stack.B().SetDouble(stack.B().ToNumber() + stack.A().ToNumber()); stack.Pop(1); break;
      case Action_Subtract: stack.B().SetDouble(stack.B().ToNumber() - stack.A().ToNumber()); stack.Pop(1); break;
      case Action_Multiply: stack.B().SetDouble(stack.B().ToNumber() * stack.A().ToNumber()); stack.Pop(1); break;
      case Action_Divide: stack.B().SetDouble(stack.B().ToNumber() / stack.A().ToNumber()); stack.Pop(1); break;
      case Action_Equals: stack.B().SetBool(stack.B().ToNumber() == stack.A().ToNumber()); stack.Pop(1); break;
      case Action_Less: stack.B().SetBool(stack.B().ToNumber() < stack.A().ToNumber()); stack.Pop(1); break;
      case Action_And: stack.B().SetBool(stack.B().ToBool() && stack.A().ToBool()); stack.Pop(1); break;
      case Action_Or: stack.B().SetBool(stack.B().ToBool() || stack.A().ToBool()); stack.Pop(1); break;
      case Action_Not: stack.A().SetBool(!stack.A().ToBool()); break;
      case Action_StringEquals: stack.B().SetBool(idStr::Cmp(stack.B().ToString().data, stack.A().ToString().data) == 0); stack.Pop(1); break;
      case Action_StringLength:
      case Action_MBStringLength: stack.A().SetInteger(stack.A().ToString().Length()); break;
      case Action_StringAdd: { idStr s = stack.B().ToString(); s.Append(stack.A().ToString()); stack.B().SetString(s); stack.Pop(1); break; }
      case Action_StringExtract:
      case Action_MBStringExtract: stack.C().SetString(stack.C().ToString().Mid(static_cast<int>(stack.B().ToInteger()), static_cast<int>(stack.A().ToInteger()))); stack.Pop(2); break;
      case Action_StringLess: stack.B().SetBool(idStr::Cmp(stack.B().ToString().data, stack.A().ToString().data) < 0); stack.Pop(1); break;
      case Action_StringGreater: stack.B().SetBool(idStr::Cmp(stack.B().ToString().data, stack.A().ToString().data) > 0); stack.Pop(1); break;
      case Action_ToInteger: stack.A().SetInteger64(stack.A().ToInteger()); break;
      case Action_CharToAscii:
      case Action_MBCharToAscii: { const idStr s = stack.A().ToString(); stack.A().SetInteger(s.IsEmpty() ? 0 : static_cast<unsigned char>(s[0])); break; }
      case Action_AsciiToChar:
      case Action_MBAsciiToChar: { char s[2] = {static_cast<char>(stack.A().ToInteger()), 0}; stack.A().SetString(s); break; }
      case Action_Jump: bitstream.Seek(bitstream.ReadS16()); break;
      case Action_If: { const int offset = bitstream.ReadS16(); const bool condition = stack.A().ToBool(); stack.Pop(1); if (condition) bitstream.Seek(offset); break; }
      case Action_GetVariable: {
        const idStr name = stack.A().ToString();
        stack.A().SetUndefined();
        for (int i = scope.Num() - 1; i >= 0; --i) {
          stack.A() = scope[i]->Get(name.data);
          if (!stack.A().IsUndefined()) break;
        }
        break;
      }
      case Action_SetVariable: {
        const idStr name = stack.B().ToString();
        bool found = false;
        for (int i = scope.Num() - 1; i >= 0; --i) {
          if (scope[i]->HasProperty(name.data)) { scope[i]->Set(name.data, stack.A()); found = true; break; }
        }
        if (!found && thisObject) thisObject->Set(name.data, stack.A());
        stack.Pop(2); break;
      }
      case Action_GotoFrame2: {
        unsigned frame = 0; const unsigned actionFlags = bitstream.ReadU8();
        if (actionFlags & 2) frame += bitstream.ReadU16();
        if (thisSprite) {
          frame += stack.A().IsString() ? thisSprite->FindFrame(stack.A().ToString().data)
                                        : static_cast<unsigned>(stack.A().ToInteger());
          if (actionFlags & 1) thisSprite->Play(); else thisSprite->Stop();
          thisSprite->RunTo(static_cast<int>(frame));
        }
        stack.Pop(1); break;
      }
      case Action_GetProperty: {
        if (thisSprite) {
          idSWFSpriteInstance* target = thisSprite->ResolveTarget(stack.B().ToString().data);
          stack.B() = target && target->scriptObject ? target->scriptObject->Get(PropertyName(static_cast<int>(stack.A().ToInteger()))) : idSWFScriptVar();
        }
        stack.Pop(1); break;
      }
      case Action_SetProperty: {
        if (thisSprite) {
          idSWFSpriteInstance* target = thisSprite->ResolveTarget(stack.C().ToString().data);
          if (target && target->scriptObject) target->scriptObject->Set(PropertyName(static_cast<int>(stack.B().ToInteger())), stack.A());
        }
        stack.Pop(3); break;
      }
      case Action_Trace: stack.Pop(1); break;
      case Action_GetTime: stack.Alloc()->SetInteger(static_cast<int>(Sys_Milliseconds())); break;
      case Action_RandomNumber: {
        static std::uint32_t seed = 0x92d68ca2u;
        const int max = static_cast<int>(stack.A().ToInteger());
        seed = seed * 1664525u + 1013904223u;
        stack.A().SetInteger(max > 0 ? static_cast<int>((seed >> 10) & 0x7fff) % max : 0);
        break;
      }
      case Action_CallFunction: {
        const idStr name = stack.A().ToString(); idSWFScriptVar function; idSWFScriptObject* owner = nullptr; stack.Pop(1);
        for (int i = scope.Num() - 1; i >= 0; --i) { function = scope[i]->Get(name.data); if (!function.IsUndefined()) { owner = scope[i]; break; } }
        const int count = static_cast<int>(stack.A().ToInteger()); stack.Pop(1); idSWFParmList args; args.SetNum(std::clamp(count, 0, 16));
        for (int i = 0; i < args.Num(); ++i) { args[i] = stack.A(); stack.Pop(1); }
        *stack.Alloc() = function.IsFunction() ? function.GetFunction()->Call(owner, args) : idSWFScriptVar(); break;
      }
      case Action_CallMethod: {
        idStr name = stack.A().ToString(); if (name.IsEmpty() || stack.A().IsUndefined() || stack.A().IsNULL()) name = "__constructor__";
        idSWFScriptObject* owner = stack.B().IsObject() ? stack.B().GetObject() : nullptr;
        idSWFScriptVar function = owner ? owner->Get(name.data) : idSWFScriptVar(); stack.Pop(2);
        const int count = static_cast<int>(stack.A().ToInteger()); stack.Pop(1); idSWFParmList args; args.SetNum(std::clamp(count, 0, 16));
        for (int i = 0; i < args.Num(); ++i) { args[i] = stack.A(); stack.Pop(1); }
        *stack.Alloc() = function.IsFunction() ? function.GetFunction()->Call(owner, args) : idSWFScriptVar(); break;
      }
      case Action_ConstantPool: {
        constants.Clear(); const int count = bitstream.ReadU16();
        for (int i = 0; i < count; ++i) constants.pool.Append(new idStrRefCounted(bitstream.ReadString()));
        break;
      }
      case Action_DefineFunction:
      case Action_DefineFunction2: {
        const idStr name = bitstream.ReadString(); auto* function = new idSWFScriptFunctionAS2;
        function->SetScope(scope); function->constants.Copy(constants); function->defaultSprite = defaultSprite;
        const int count = bitstream.ReadU16(); int registerCount = 4;
        if (code == Action_DefineFunction2) { registerCount = bitstream.ReadU8() + 1; function->flags = bitstream.ReadU16(); function->AllocRegisters(registerCount); }
        function->AllocParameters(count);
        for (int i = 0; i < count; ++i) { unsigned reg = code == Action_DefineFunction2 ? bitstream.ReadU8() : 0; const char* parmName = bitstream.ReadString(); function->SetParameter(i, reg < static_cast<unsigned>(registerCount) ? static_cast<std::uint8_t>(reg) : 0, parmName); }
        const int size = bitstream.ReadU16(); function->data = bitstream.ReadData(size); function->length = size;
        if (name.IsEmpty()) stack.Alloc()->SetObject(function); else if (thisObject) thisObject->Set(name.data, idSWFScriptVar(function));
        function->Release(); break;
      }
      case Action_Enumerate:
      case Action_Enumerate2: {
        idSWFScriptObject* target = nullptr;
        if (code == Action_Enumerate) {
          const idStr name = stack.A().ToString();
          for (int i = scope.Num() - 1; i >= 0; --i) { idSWFScriptVar v = scope[i]->Get(name.data); if (v.IsObject()) { target = v.GetObject(); break; } }
        } else if (stack.A().IsObject()) target = stack.A().GetObject();
        stack.A().SetNULL();
        if (target) { idList<idSWFScriptVar> names; target->EnumVariables(names); for (int i = 0; i < names.Num(); ++i) *stack.Alloc() = names[i]; }
        break;
      }
      case Action_Equals2: stack.B().SetBool(stack.B().Equals(stack.A(), false)); stack.Pop(1); break;
      case Action_StrictEquals: stack.B().SetBool(stack.B().Equals(stack.A(), true)); stack.Pop(1); break;
      case Action_GetMember: {
        if (stack.B().IsObject()) stack.B() = ObjectIndexedGet(stack.B().GetObject(), stack.A());
        else if (stack.B().IsString() && idStr::Cmp(stack.A().ToString().data, "length") == 0) stack.B().SetInteger(stack.B().ToString().Length());
        else stack.B().SetUndefined();
        stack.Pop(1); break;
      }
      case Action_SetMember: if (stack.C().IsObject()) ObjectIndexedSet(stack.C().GetObject(), stack.B(), stack.A()); stack.Pop(3); break;
      case Action_InitArray: {
        auto* array = new idSWFScriptArray; const int count = static_cast<int>(stack.A().ToInteger()); stack.Pop(1);
        for (int i = 0; i < count; ++i) { array->Set(i, stack.A()); stack.Pop(1); }
        stack.Alloc()->SetObject(array); array->Release(); break;
      }
      case Action_InitObject: {
        auto* object = new idSWFScriptObject; const int count = static_cast<int>(stack.A().ToInteger()); stack.Pop(1);
        for (int i = 0; i < count; ++i) { object->Set(stack.B().ToString().data, stack.A()); stack.Pop(2); }
        stack.Alloc()->SetObject(object); object->Release(); break;
      }
      case Action_NewObject: {
        const idStr className = stack.A().ToString();
        stack.Pop(1);
        const int count = static_cast<int>(stack.A().ToInteger());
        stack.Pop(1);
        idSWFParmList args;
        args.SetNum(std::clamp(count, 0, 16));
        for (int i = 0; i < args.Num(); ++i) {
          args[i] = stack.A();
          stack.Pop(1);
        }
        idSWFScriptObject* object = idStr::Cmp(className.data, "Array") == 0
                                        ? static_cast<idSWFScriptObject*>(new idSWFScriptArray)
                                        : static_cast<idSWFScriptObject*>(new idSWFScriptObject);
        const idSWFScriptVar definition = scope.Num() > 0 ? scope[0]->Get(className.data)
                                                           : idSWFScriptVar();
        if (definition.IsFunction()) {
          idSWFScriptFunction* constructor = definition.GetFunction();
          object->SetPrototype(constructor->GetPrototype());
          object->Set("__proto__", idSWFScriptVar(constructor->GetPrototype()));
          constructor->Call(object, args);
        }
        stack.Alloc()->SetObject(object);
        object->Release();
        break;
      }
      case Action_Extends: {
        idSWFScriptFunction* superConstructor = stack.A().GetFunction();
        idSWFScriptFunction* subConstructor = stack.B().GetFunction();
        stack.Pop(2);
        if (superConstructor != nullptr && subConstructor != nullptr) {
          auto* prototypeObject = new idSWFScriptObject;
          prototypeObject->SetPrototype(superConstructor->GetPrototype());
          prototypeObject->Set("__proto__", idSWFScriptVar(superConstructor->GetPrototype()));
          prototypeObject->Set("__constructor__", idSWFScriptVar(superConstructor));
          subConstructor->SetPrototype(prototypeObject);
          prototypeObject->Release();
        }
        break;
      }
      case Action_TargetPath: {
        idSWFSpriteInstance* sprite = stack.A().GetSprite();
        if (!sprite) stack.A().SetUndefined(); else { idStr path = sprite->GetTargetPath(); path.ReplaceChar('/', '.'); stack.A().SetString(path); }
        break;
      }
      case Action_With: {
        const int size = bitstream.ReadU16(); idSWFBitStream nested(bitstream.ReadData(size), size, false);
        if (stack.A().IsObject()) { idSWFScriptObject* with = stack.A().GetObject(); with->AddRef(); stack.Pop(1); scope.Append(with); Run(thisObject, stack, nested); scope.SetNum(scope.Num() - 1); with->Release(); } else stack.Pop(1);
        break;
      }
      case Action_ToNumber: stack.A().SetDouble(stack.A().ToNumber()); break;
      case Action_ToString: stack.A().SetString(stack.A().ToString()); break;
      case Action_TypeOf: stack.A().SetString(stack.A().TypeOf()); break;
      case Action_Add2: if (stack.A().IsString() || stack.B().IsString()) { idStr s=stack.B().ToString(); s.Append(stack.A().ToString()); stack.B().SetString(s); } else stack.B().SetDouble(stack.B().ToNumber()+stack.A().ToNumber()); stack.Pop(1); break;
      case Action_Less2: stack.B().SetBool(stack.B().IsString() && stack.A().IsString() ? idStr::Cmp(stack.B().ToString().data, stack.A().ToString().data)<0 : stack.B().ToNumber()<stack.A().ToNumber()); stack.Pop(1); break;
      case Action_Greater: stack.B().SetBool(stack.B().IsString() && stack.A().IsString() ? idStr::Cmp(stack.B().ToString().data, stack.A().ToString().data)>0 : stack.B().ToNumber()>stack.A().ToNumber()); stack.Pop(1); break;
      case Action_Modulo: { const int a=static_cast<int>(stack.A().ToInteger()), b=static_cast<int>(stack.B().ToInteger()); if(a) stack.B().SetInteger(b%a); else stack.B().SetUndefined(); stack.Pop(1); break; }
      case Action_BitAnd: stack.B().SetInteger(static_cast<int>(stack.B().ToInteger()) & static_cast<int>(stack.A().ToInteger())); stack.Pop(1); break;
      case Action_BitOr: stack.B().SetInteger(static_cast<int>(stack.B().ToInteger()) | static_cast<int>(stack.A().ToInteger())); stack.Pop(1); break;
      case Action_BitXor: stack.B().SetInteger(static_cast<int>(stack.B().ToInteger()) ^ static_cast<int>(stack.A().ToInteger())); stack.Pop(1); break;
      case Action_BitLShift: stack.B().SetInteger(static_cast<int>(stack.B().ToInteger()) << (static_cast<int>(stack.A().ToInteger()) & 31)); stack.Pop(1); break;
      case Action_BitRShift: stack.B().SetInteger(static_cast<int>(stack.B().ToInteger()) >> (static_cast<int>(stack.A().ToInteger()) & 31)); stack.Pop(1); break;
      case Action_BitURShift: stack.B().SetInteger(static_cast<std::uint32_t>(stack.B().ToInteger()) >> (static_cast<int>(stack.A().ToInteger()) & 31)); stack.Pop(1); break;
      case Action_Decrement: stack.A().SetDouble(stack.A().ToNumber()-1.0); break;
      case Action_Increment: stack.A().SetDouble(stack.A().ToNumber()+1.0); break;
      case Action_PushDuplicate: *stack.Alloc()=stack.A(); break;
      case Action_StackSwap: { idSWFScriptVar temp=stack.A(); stack.A()=stack.B(); stack.B()=temp; break; }
      case Action_StoreRegister: { const unsigned reg=bitstream.ReadU8(); if(reg<static_cast<unsigned>(registers.Num())) registers[reg]=stack.A(); break; }
      case Action_DefineLocal: if(scope.Num()) scope[scope.Num()-1]->Set(stack.B().ToString().data,stack.A()); stack.Pop(2); break;
      case Action_DefineLocal2: if(scope.Num()) scope[scope.Num()-1]->Set(stack.A().ToString().data,idSWFScriptVar()); stack.Pop(1); break;
      case Action_Delete: if(stack.C().IsObject()) stack.C().GetObject()->Delete(stack.B().ToString().data); stack.Pop(2); break;
      case Action_Delete2: if(scope.Num()) scope[scope.Num()-1]->Delete(stack.A().ToString().data); stack.Pop(1); break;
      default:
        return idSWFScriptVar();
    }
  }
  return idSWFScriptVar();
}

idSWFScriptVar idSWFScriptFunctionAS2::Call(idSWFScriptObject* thisObject,
                                             idSWFParmList& parms) {
  idSWFStack stack;
  idSWFBitStream bitstream(data, length, false);
  if (scope.Num() == 0)
    return idSWFScriptVar();
  if (thisObject == nullptr)
    thisObject = scope[0];
  auto* locals = new idSWFScriptObject;
  stack.SetNum(parms.Num() + 1);
  for (int i = 0; i < parms.Num(); ++i) {
    stack[parms.Num() - i - 1] = parms[i];
    if (i < parameters.Num()) {
      if (parameters[i].reg > 0 && parameters[i].reg < registers.Num()) registers[parameters[i].reg] = parms[i];
      locals->Set(parameters[i].name, parms[i]);
    }
  }
  for (int i = parms.Num(); i < parameters.Num(); ++i) {
    if (parameters[i].reg > 0 && parameters[i].reg < registers.Num()) registers[parameters[i].reg].SetUndefined();
    locals->Set(parameters[i].name, idSWFScriptVar());
  }
  stack.A().SetInteger(parms.Num());
  int preload = 1;
  if ((flags & 1) && preload < registers.Num()) registers[preload++].SetObject(thisObject);
  if ((flags & 2) == 0) locals->Set("this", idSWFScriptVar(thisObject));
  auto makeArguments = [&]() {
    auto* args = new idSWFScriptArray;
    for (int i = 0; i < parms.Num(); ++i) args->Set(i, parms[i]);
    return args;
  };
  if ((flags & 4) && preload < registers.Num()) { auto* args=makeArguments(); registers[preload++].SetObject(args); args->Release(); }
  if ((flags & 8) == 0) { auto* args=makeArguments(); locals->Set("arguments", idSWFScriptVar(args)); args->Release(); }
  if ((flags & 16) && preload < registers.Num()) registers[preload++].SetObject(thisObject->GetPrototype());
  if ((flags & 32) == 0) locals->Set("super", idSWFScriptVar(thisObject->GetPrototype()));
  if ((flags & 64) && preload < registers.Num()) registers[preload++] = scope[0]->Get("_root");
  if ((flags & 128) && preload < registers.Num()) {
    idSWFSpriteInstance* sprite = thisObject->DerivedFrom(idSWFScriptSprite::CLASS)
                                      ? static_cast<idSWFScriptSprite*>(thisObject)->sprite
                                      : nullptr;
    if (sprite && sprite->parent) registers[preload].SetObject(sprite->parent->scriptObject); else registers[preload].SetNULL();
    ++preload;
  }
  if ((flags & 256) && preload < registers.Num()) registers[preload].SetObject(scope[0]);
  const int scopeSize = scope.Num(); scope.Append(locals); locals->AddRef();
  idSWFScriptVar result = Run(thisObject, stack, bitstream);
  for (int i = scopeSize; i < scope.Num(); ++i) scope[i]->Release();
  scope.SetNum(scopeSize); locals->Release();
  return result;
}
