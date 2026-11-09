#include "engine/gamelib/effects/tracermanager.h"

#include "engine/decls/decl.h"
#pragma warning(push)
#pragma warning(disable : 4201 4324)
#include "engine/models/particles/rendermodeleffects.h"
#include "engine/models/serializer.h"
#include "engine/renderer/declmaterial.h"
#pragma warning(pop)
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#pragma comment(linker,                                                        \
                "/alternatename:?ReadBits@idBitMsg@@QEBAHH@Z="               \
                "?ReadBits@idBitMsg@@QEAAHH@Z")

idCVar net_tracerMgrDebug("net_tracerMgrDebug", "0", CVAR_BOOL,
                          "net_tracerMgrDebug",
                          static_cast<argCompletionRef_t>(nullptr));

namespace {

int ReadBitsRetail(idBitMsg* message, int bits) {
  return static_cast<const idBitMsg*>(message)->ReadBits(bits);
}

int QuantizeSigned(float value, float scale, int maximum) {
  int quantized = static_cast<int>(value * scale);
  if (quantized < -maximum) {
    quantized = -maximum;
  } else if (quantized > maximum) {
    quantized = maximum;
  }
  return quantized;
}

} // namespace

idTracerManager::idTracerManager() {
  effectsModel = nullptr;
  *reinterpret_cast<std::uint64_t*>(&tracerBufferPos) = 0;
  catchingUp = false;
  localPlayerIndex = 0;
  std::memset(tracerBuffer, 0, sizeof(tracerBuffer));
}

void idTracerManager::Init(idRenderModelEffects* effectsModel_,
                           int playerIndex) {
  effectsModel = effectsModel_;
  localPlayerIndex = playerIndex;
}

void idTracerManager::AddTracer(
    const idMaterial* material, const idVec3& origin,
    const idVec3& direction, const idVec3& maximumDistance, float speed,
    float length, float height, bool ensureVisual, float fractionInsured,
    int playerIndex) {
  idRenderModelEffects* const currentEffectsModel = effectsModel;
  if (currentEffectsModel != nullptr) {
    currentEffectsModel->AddTracer(
        material, origin, direction, maximumDistance, speed, length, height,
        ensureVisual, fractionInsured, -1.0f);
  }

  if (!catchingUp) {
    if (net_tracerMgrDebug.valueInteger != 0) {
      idLib::Printf(
          "::AddTracer. tracerBufferIndex=%d origin=<%.2f,%.2f,%.2f>\n",
          tracerBufferPos, origin.x, origin.y, origin.z);
    }
    tracerManagerRecord_t& record = tracerBuffer[tracerBufferPos];
    record.dir = direction;
    record.mat = material;
    record.origin = origin;
    record.speed = speed;
    record.height = height;
    record.length = length;
    record.maxDist = maximumDistance;
    record.ensureVisual = ensureVisual;
    record.fractionInsured = fractionInsured;
    record.playerIndex = playerIndex;
    record.lifeTime = 0;
    record.startTime = 0;
    tracerBufferPos = (tracerBufferPos + 1) % MAX_TRACERS;
  }
}

void idTracerManager::Update(int) {
  catchingUp = true;
  if (serializeTracerCount <= 0) {
    --serializeTracerCount;
    catchingUp = false;
    return;
  }

  do {
    const int bufferPosition = tracerBufferPos;
    --serializeTracerCount;
    tracerManagerRecord_t& record = tracerBuffer[bufferPosition];
    if (net_tracerMgrDebug.valueInteger != 0) {
      idLib::Printf(
          "::Update. serializeTracerCount=%d tracerBufferIndex=%d "
          "origin=<%.2f,%.2f,%.2f>\n",
          serializeTracerCount, bufferPosition, record.origin.x,
          record.origin.y, record.origin.z);
    }
    if (record.playerIndex != localPlayerIndex) {
      AddTracer(record.mat, record.origin, record.dir, record.maxDist,
                record.speed, record.length, record.height,
                record.ensureVisual, record.fractionInsured);
    }
    tracerBufferPos = (tracerBufferPos + 1) % MAX_TRACERS;
  } while (serializeTracerCount > 0);

  --serializeTracerCount;
  catchingUp = false;
}

template <>
__declspec(noinline) void
idBitMsg::WriteQuantizedVector<idVec3, 1, 12>(const idVec3& vector) {
  for (int index = 0; index < 3; ++index) {
    WriteBits(QuantizeSigned(vector[index], 2047.0f, 2047), -12);
  }
}

void idTracerManager::Serialize(idSerializer* serializer) {
  int serializedBufferPosition = tracerBufferPos;
  if (serializer->writing) {
    serializer->msg->WriteBits(serializedBufferPosition, 2);
  } else {
    serializedBufferPosition = ReadBitsRetail(serializer->msg, 2);
  }

  for (int recordIndex = 0; recordIndex < MAX_TRACERS; ++recordIndex) {
    tracerManagerRecord_t& record = tracerBuffer[recordIndex];
    if (serializer->writing) {
      serializer->msg->WriteBits(
          QuantizeSigned(record.speed, 0.062469482f, 2047), -12);
    } else {
      record.speed =
          static_cast<float>(ReadBitsRetail(serializer->msg, -12)) *
          16.007816f;
    }

    if (serializer->writing) {
      serializer->msg->WriteQuantizedVector<idVec3, 32768, 12>(
          record.origin);
    } else {
      for (int component = 0; component < 3; ++component) {
        record.origin[component] =
            static_cast<float>(ReadBitsRetail(serializer->msg, -12)) *
            16.007816f;
      }
    }

    if (serializer->writing) {
      serializer->msg->WriteQuantizedVector<idVec3, 1, 12>(record.dir);
    } else {
      for (int component = 0; component < 3; ++component) {
        record.dir[component] =
            static_cast<float>(ReadBitsRetail(serializer->msg, -12)) *
            0.00048851978f;
      }
    }

    if (serializer->writing) {
      serializer->msg->WriteBits(
          QuantizeSigned(record.length, 0.49902344f, 511), -10);
    } else {
      record.length =
          static_cast<float>(ReadBitsRetail(serializer->msg, -10)) *
          2.0039139f;
    }
    if (serializer->writing) {
      serializer->msg->WriteBits(
          QuantizeSigned(record.height, 0.49902344f, 511), -10);
    } else {
      record.height =
          static_cast<float>(ReadBitsRetail(serializer->msg, -10)) *
          2.0039139f;
    }

    if (serializer->writing) {
      idMaterial::resourceList.WriteResourceToMsg(
          reinterpret_cast<const idResource*>(record.mat),
          *serializer->msg);
    } else {
      record.mat = reinterpret_cast<const idMaterial*>(
          idMaterial::resourceList.ReadResourceFromMsg(*serializer->msg));
    }

    if (serializer->writing) {
      for (int component = 0; component < 3; ++component) {
        serializer->msg->WriteBits(
            QuantizeSigned(record.maxDist[component], 0.49609375f, 127),
            -8);
      }
    } else {
      for (int component = 0; component < 3; ++component) {
        record.maxDist[component] =
            static_cast<float>(ReadBitsRetail(serializer->msg, -8)) *
            2.015748f;
      }
    }

    if (serializer->writing) {
      serializer->msg->WriteBits(
          QuantizeSigned(record.fractionInsured, 0.49609375f, 127), -8);
    } else {
      record.fractionInsured =
          static_cast<float>(ReadBitsRetail(serializer->msg, -8)) *
          2.015748f;
    }

    if (serializer->writing) {
      serializer->msg->WriteBits(record.playerIndex, 32);
    } else {
      record.playerIndex = ReadBitsRetail(serializer->msg, 32);
    }

    std::uint8_t ensureVisual = record.ensureVisual;
    if (serializer->writing) {
      serializer->msg->WriteBits(ensureVisual, 1);
    } else {
      ensureVisual = ReadBitsRetail(serializer->msg, 1) == 1;
    }
    record.ensureVisual = ensureVisual;
  }

  int serializedCount = 4;
  const int difference = serializedBufferPosition - tracerBufferPos;
  if (difference < 4) {
    serializedCount = difference;
  }
  serializeTracerCount = serializedCount;
  if (serializedCount < 0) {
    serializeTracerCount = serializedCount + 4;
  }

  if (net_tracerMgrDebug.valueInteger != 0 &&
      serializeTracerCount != 0) {
    idLib::Printf("::Serialize. serializeTracerCount=%d\n",
                  serializeTracerCount);
  }
}
