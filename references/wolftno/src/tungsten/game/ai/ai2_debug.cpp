#include "tungsten/game/ai/ai2_actions.h"

namespace {

int aiDebugLevelValue = 0;

}  // namespace

idCVar ai_drawBounds(
    "ai_drawBounds", "0", 1, "draw AI bounds for debugging.");

// DIA: idAI2::SetDebugText
void idAI2::SetDebugText(const char* text, aiDebugLevel_t debugLevel) {
  if (debugState.textLevel == AIDEBUGLEVEL_NONE ||
      debugLevel <= debugState.textLevel) {
    debugState.textLevel = debugLevel;
    debugState.debugText = text != nullptr ? text : "";
  }
}

// DIA: idAI2::ClearErrorFlags
void idAI2::ClearErrorFlags(int flags) {
  debugState.errorFlags &= ~flags;
}

// DIA: idAI2::ShowDebugDamageText
void idAI2::ShowDebugDamageText(
    float baseDamage, float difficultyScale, float bodyDamage,
    float limbDamage, float armorDamage, float shieldDamage,
    float armorDamageScale, float armoredHealthDamageScale,
    float bleedThroughDamage, float excessDamage, float attitudeDamageScale,
    const idDamageGroup* damageGroup, const idVec3* impactPoint) {
  const float locationScale =
      damageGroup != nullptr ? damageGroup->damageScale : 1.0F;
  const char* location =
      damageGroup != nullptr ? damageGroup->groupName.data : "no loc";
  const idVec3 point = impactPoint != nullptr ? *impactPoint : idVec3{};
  debugState.damageText.Format(
      "net: %.1f base: %.1f limb: %.1f difficulty: %.2f "
      "location: %s x%.2f shield: %.1f armor: %.1f x%.2f "
      "bleed: %.1f x%.2f excess: %.1f friendly: %.2f at %.1f %.1f %.1f",
      bodyDamage, baseDamage, limbDamage, difficultyScale, location,
      locationScale, shieldDamage, armorDamage, armorDamageScale,
      bleedThroughDamage, armoredHealthDamageScale, excessDamage,
      attitudeDamageScale, point.x, point.y, point.z);
}

// DIA: `idAI2::ShowDebugDamageText'::`1'::dtor$0
// DIA: `idAI2::ShowDebugDamageText'::`1'::dtor$1

// DIA: idAI2::GetDebugLevel
std::int64_t idAI2::GetDebugLevel() {
  return aiDebugLevelValue;
}

void idAI2::SetDebugLevel(int value) {
  aiDebugLevelValue = value;
}

// DIA: `dynamic initializer for 'ai_drawBounds''
// DIA: `dynamic atexit destructor for 'ai_drawBounds''
