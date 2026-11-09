#include "declbreakable.h"

#include "declfx.h"
#include "declsimple.h"
#include "shared/idlib/lib.h"

#include <algorithm>

namespace {
template <typename T> const T* FindDecl(const char* typeName, const char* name) {
  if (declManager == nullptr || name == nullptr)
    return nullptr;
  idDeclInfo* type = declManager->GetDeclType(typeName);
  return type != nullptr ? reinterpret_cast<const T*>(type->FindWithInheritance(name, false, true))
                         : nullptr;
}

int TokenInt(const idToken& token) {
  return token.type == TT_NUMBER ? static_cast<int>(token.intvalue) : 0;
}

void ParseIndexedValues(idDeclBreakable& owner, idParser& source, idList<int, 59>& values,
                        const char* description) {
  values.SetNum(owner.numPieces);
  for (int i = 0; i < values.Num(); ++i)
    values[i] = 0;
  if (!source.ExpectTokenString("{"))
    return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token == ",")
      continue;
    if (token == "all") {
      const int value = source.ParseInt();
      for (int i = 0; i < values.Num(); ++i)
        values[i] = value;
      continue;
    }
    const int piece = TokenInt(token);
    const int value = source.ParseInt();
    if (piece < 1 || piece > values.Num()) {
      source.Error("%s piece index %d/%d is out of range in breakable '%s'", description,
                   piece - 1, values.Num(), owner.name.str);
      continue;
    }
    values[piece - 1] = value;
  }
}

void ParsePieceSet(idParser& source, int numPieces, idList<int, 59>& pieces, bool allByDefault) {
  pieces.Clear();
  if (!source.ExpectTokenString("{"))
    return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token == ",")
      continue;
    if (token == "all") {
      pieces.SetNum(numPieces);
      for (int i = 0; i < numPieces; ++i)
        pieces[i] = i;
      continue;
    }
    const int piece = TokenInt(token);
    if (piece >= 1 && piece <= numPieces)
      pieces.Append(piece - 1);
  }
  if (allByDefault && pieces.Num() == 0) {
    pieces.SetNum(numPieces);
    for (int i = 0; i < numPieces; ++i)
      pieces[i] = i;
  }
}

void RemoveExcluded(idList<int, 59>& pieces, const idList<int, 59>& excluded) {
  for (int e = 0; e < excluded.Num(); ++e) {
    for (int i = pieces.Num() - 1; i >= 0; --i) {
      if (pieces[i] == excluded[e]) {
        for (int j = i + 1; j < pieces.Num(); ++j)
          pieces[j - 1] = pieces[j];
        pieces.SetNum(pieces.Num() - 1);
      }
    }
  }
}
} // namespace

idDeclInfoTemplate<idDeclBreakable> idDeclBreakable::resourceList(
    "breakable", "idDeclBreakable", "break", false);

idBreakableSpark::idBreakableSpark()
    : sizeMin(0.0f), sizeMax(0.0f), velocityScaleMin(1.0f), velocityScaleMax(1.0f) {}

idDeclBreakable::idDeclBreakable() : numPieces(0), settings{} {
  resourceListPtr = &resourceList;
  settings.gravityVector = {0.0f, 0.0f, -250.0f};
  settings.bouncyness = 1.0f;
  settings.maxSimulationTime = 5.0f;
  settings.stopSpeed = 10.0f;
  settings.maxLinearVelocity = 5000.0f;
  settings.maxAngularVelocity = 12.5663706f;
  settings.clipMask = 1;
  settings.dampeningDecay = 0.0f;
}

idDeclBreakable::~idDeclBreakable() { Clear(); }

const char* idDeclBreakable::DefaultDefinition() const { return "{ }\n"; }
idDeclInfo* idDeclBreakable::GetDeclInfo() const { return &resourceList; }

void idDeclBreakable::Clear() {
  settings.linearFriction = 0.6f;
  settings.angularFriction = 0.6f;
  settings.contactFriction = 0.5f;
  settings.linearFrictionWater = 1.0f;
  settings.angularFrictionWater = 1.0f;
  settings.bouncyness = 0.6f;
  settings.gravityVector = {0.0f, 0.0f, -250.0f};
  settings.worldCollisionOnly = false;
  settings.simplePointCollision = false;
  settings.crazyBounceChance = 0.0f;
  settings.maxSimulationTime = 10.0f;
  settings.noShadows = false;
  settings.dampeningDecay = 0.0f;
  numPieces = 0;
  explosions.Clear();
  trails.Clear();
  decals.Clear();
  pieceDecals.Clear();
}

int idDeclBreakable::ContentsFromString(const char* text) {
  idLexer lexer;
  lexer.LoadMemory(text ? text : "", idStr::Length(text ? text : ""),
                   "idDeclBreakable::ContentsFromString");
  int result = 0;
  idToken token;
  while (lexer.ReadToken(token)) {
    if (token == "none" || token == ",")
      continue;
    if (idStr::Icmp(token.data, "solid") == 0) result |= 0x1;
    else if (idStr::Icmp(token.data, "playerclip") == 0) result |= 0x8;
    else if (idStr::Icmp(token.data, "monsterclip") == 0) result |= 0x10;
    else if (idStr::Icmp(token.data, "vehicleclip") == 0) result |= 0x20;
    else if (idStr::Icmp(token.data, "shotclip") == 0) result |= 0x80;
    else if (idStr::Icmp(token.data, "ikclip") == 0) result |= 0x100;
    else if (idStr::Icmp(token.data, "ai") == 0) result |= 0x400;
    else if (idStr::Icmp(token.data, "corpse") == 0) result |= 0x1000;
    else if (idStr::Icmp(token.data, "vehicle") == 0) result |= 0x10000;
    else break;
  }
  return result;
}

void idDeclBreakable::ParseArmoredPieces(idParser& source) {
  ParseIndexedValues(*this, source, armoredPieces, "Armored");
}
void idDeclBreakable::ParseHealthPieces(idParser& source) {
  ParseIndexedValues(*this, source, healthPieces, "Health");
}
void idDeclBreakable::ParseDestroyPieces(idParser& source) {
  ParseIndexedValues(*this, source, destroyPieces, "Destroy");
}

void idDeclBreakable::ParseContents(idParser& source, int& contents) {
  idStr text;
  idToken token;
  while (source.ReadToken(token)) {
    text.Append(token.data);
    if (!source.CheckTokenString(","))
      break;
    text.Append(",");
  }
  contents = ContentsFromString(text.data);
}

void idDeclBreakable::ParseSpark(idParser& source) {
  sparks.sizeMin = sparks.sizeMax = 1.0f;
  sparks.velocityScaleMin = sparks.velocityScaleMax = 1.0f;
  if (!source.ExpectTokenString("{"))
    return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token == "sizeMin") sparks.sizeMin = source.ParseFloat();
    else if (token == "sizeMax") sparks.sizeMax = source.ParseFloat();
    else if (token == "velocityScaleMin") sparks.velocityScaleMin = source.ParseFloat();
    else if (token == "velocityScaleMax") sparks.velocityScaleMax = source.ParseFloat();
    else if (token == "material") {
      if (!source.ExpectTokenString("{")) return;
      while (source.ReadToken(token) && token != "}") {
        if (const idMaterial* material = FindDecl<idMaterial>("material", token.data))
          sparks.materials.Append(material);
      }
    } else {
      source.Warning("Bad token '%s'\n", token.data);
      return;
    }
  }
}

void idDeclBreakable::ParseExplosion(idParser& source) {
  idBreakableExplosion value{};
  value.impulse = 3000.0f;
  value.falloff = 0.2f;
  value.activateAllDelay = -1;
  value.duration = 200;
  idList<int, 59> excluded;
  if (!source.ExpectTokenString("{")) return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token == "particle") { source.ReadToken(token); value.particle = FindDecl<idDeclParticle>("particle", token.data); }
    else if (token == "fx") { source.ReadToken(token); value.fx = FindDecl<idDeclFX>("fx", token.data); }
    else if (token == "position") source.Parse1DMatrix(3, &value.position.x);
    else if (token == "radius") value.radius = source.ParseFloat();
    else if (token == "impulse") value.impulse = source.ParseFloat();
    else if (token == "falloff") value.falloff = source.ParseFloat();
    else if (token == "angular") value.angular = source.ParseFloat();
    else if (token == "delay") value.delay = static_cast<int>(source.ParseFloat() * 1000.0f);
    else if (token == "activateAllDelay") value.activateAllDelay = static_cast<int>(source.ParseFloat() * 1000.0f);
    else if (token == "duration") value.duration = static_cast<int>(source.ParseFloat() * 1000.0f);
    else if (token == "distTimeScaleDelay") value.distTimeScaleDelay = source.ParseFloat();
    else if (token == "useLinear") value.useLinear = source.ParseBool();
    else if (token == "endLinear") source.Parse1DMatrix(3, &value.endLinear.x);
    else if (token == "randomLinear") { for (int i=0;i<3;++i) value.randomLinear.p[i]=source.ParseFloat(); for(int i=0;i<3;++i)value.randomLinear.p[i+3]=value.randomLinear.p[i]; }
    else if (token == "randomLinearMinMax") source.Parse1DMatrix(6, value.randomLinear.p);
    else if (token == "randomAngular") { for (int i=0;i<3;++i) value.randomAngular.p[i]=source.ParseFloat(); for(int i=0;i<3;++i)value.randomAngular.p[i+3]=value.randomAngular.p[i]; }
    else if (token == "randomAngularMinMax") source.Parse1DMatrix(6, value.randomAngular.p);
    else if (token == "pieces") ParsePieceSet(source, numPieces, value.pieces, false);
    else if (token == "excludePieces") ParsePieceSet(source, numPieces, excluded, false);
    else { source.Warning("Bad token '%s'\n", token.data); return; }
  }
  if (value.pieces.Num() == 0) { value.pieces.SetNum(numPieces); for(int i=0;i<numPieces;++i)value.pieces[i]=i; }
  RemoveExcluded(value.pieces, excluded);
  explosions.Append(value);
}

void idDeclBreakable::ParseTrail(idParser& source) {
  idBreakableTrail value{};
  value.glowQuadSize = 20.0f;
  value.minTrailVelocity = value.minBounceVelocity = 10.0f;
  value.trailSpacing = 20.0f;
  idList<int, 59> excluded;
  if (!source.ExpectTokenString("{")) return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token == "particleTrail") { source.ReadToken(token); value.particleTrail = FindDecl<idDeclParticle>("particle", token.data); }
    else if (token == "particleBounce") { source.ReadToken(token); value.particleBounce = FindDecl<idDeclParticle>("particle", token.data); }
    else if (token == "particleDie") { source.ReadToken(token); value.particleDie = FindDecl<idDeclParticle>("particle", token.data); }
    else if (token == "glowQuadMaterial") { source.ReadToken(token); value.glowQuadMaterial = FindDecl<idMaterial>("material", token.data); }
    else if (token == "glowQuadSize") value.glowQuadSize = source.ParseFloat();
    else if (token == "fadeInDuration") value.fadeInDuration = static_cast<int>(source.ParseFloat()*1000.0f);
    else if (token == "fadeOutDuration") value.fadeOutDuration = static_cast<int>(source.ParseFloat()*1000.0f);
    else if (token == "minTrailVelocity") value.minTrailVelocity = source.ParseFloat();
    else if (token == "minBounceVelocity") value.minBounceVelocity = source.ParseFloat();
    else if (token == "trailSpacing") value.trailSpacing = std::clamp(source.ParseFloat(), 1.0f, 1.0e30f);
    else if (token == "trailVelocityScale") value.trailVelocityScale = source.ParseFloat();
    else if (token == "delay") value.delay = static_cast<int>(source.ParseFloat()*1000.0f);
    else if (token == "duration") value.duration = static_cast<int>(source.ParseFloat()*1000.0f);
    else if (token == "pieces") ParsePieceSet(source, numPieces, value.pieces, false);
    else if (token == "excludePieces") ParsePieceSet(source, numPieces, excluded, false);
    else { source.Warning("Bad token '%s'\n", token.data); return; }
  }
  if (value.pieces.Num() == 0) { value.pieces.SetNum(numPieces); for(int i=0;i<numPieces;++i)value.pieces[i]=i; }
  RemoveExcluded(value.pieces, excluded);
  trails.Append(value);
}

void idDeclBreakable::ParseDecals(idParser& source) {
  idBreakableDecal value{};
  value.minSize = {0.0f, 0.0f};
  value.maxSize = {32.0f, 32.0f};
  value.lifetime = value.fadeOutStartTime = 30 * 1000;
  value.fadeInEndTime = 10000;
  value.minBounceVelocity = 2000.0f;
  value.decalSpacing = 2000.0f;
  value.depth = 10.0f;
  if (!source.ExpectTokenString("{")) return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token == "maxSizeX") value.maxSize.x = source.ParseFloat();
    else if (token == "maxSizeY") value.maxSize.y = source.ParseFloat();
    else if (token == "minSizeX") value.minSize.x = source.ParseFloat();
    else if (token == "minSizeY") value.minSize.y = source.ParseFloat();
    else if (token == "lifetime") value.lifetime = static_cast<int>(source.ParseFloat()*1000.0f);
    else if (token == "fadetime") value.fadeOutStartTime = static_cast<int>(source.ParseFloat()*1000.0f);
    else if (token == "fadeInEndTime") value.fadeInEndTime = static_cast<int>(source.ParseFloat()*1000.0f);
    else if (token == "decalSpacing") value.decalSpacing = source.ParseFloat();
    else if (token == "minBounceVelocity") value.minBounceVelocity = source.ParseFloat();
    else if (token == "splatterOnBounce") value.splatterOnBounce = source.ParseBool();
    else if (token == "splatterOnRest") value.splatterOnRest = source.ParseBool();
    else if (token == "depth") value.depth = source.ParseFloat();
    else if (token == "pieces") ParsePieceSet(source, numPieces, value.pieces, false);
    else if (token == "materials") {
      if (!source.ExpectTokenString("{")) return;
      while (source.ReadToken(token) && token != "}")
        if (const idMaterial* material = FindDecl<idMaterial>("material", token.data)) value.materials.Append(material);
    } else { source.Warning("Bad token '%s'\n", token.data); return; }
  }
  if (value.pieces.Num() == 0) { value.pieces.SetNum(numPieces); for(int i=0;i<numPieces;++i)value.pieces[i]=i; }
  if (value.materials.Num() != 0) decals.Append(value);
}

void idDeclBreakable::ParsePieceDecal(idParser& source) {
  idBreakablePieceDecal value{};
  if (!source.ExpectTokenString("{")) return;
  idToken token;
  source.ReadToken(token);
  value.material = FindDecl<idMaterial>("material", token.data);
  source.Parse1DMatrix(2, &value.size.x);
  value.depth = source.ParseFloat();
  value.angle = source.ParseFloat();
  source.Parse1DMatrix(3, &value.position.x);
  source.Parse1DMatrix(3, &value.direction.x);
  if (source.ExpectTokenString("{")) {
    while (source.ReadToken(token) && token != "}") {
      const int piece = TokenInt(token);
      if (piece >= 1 && piece <= numPieces) value.affectedPieces.Append(piece - 1);
    }
  }
  source.ExpectTokenString("}");
  pieceDecals.Append(value);
}

void idDeclBreakable::Parse(idParser& source) {
  idToken token;
  if (source.PeekTokenString("}")) return;
  Clear();
  while (source.ReadToken(token) && token != "}") {
    if (token == "model") { source.ReadToken(token); modelName.Set(token.data); numPieces = 0; if (declManager) declManager->AddDependency(*this, modelName.str); }
    else if (token == "linearFriction") settings.linearFriction = source.ParseFloat();
    else if (token == "angularFriction") settings.angularFriction = source.ParseFloat();
    else if (token == "contactFriction") settings.contactFriction = source.ParseFloat();
    else if (token == "linearFrictionWater") settings.linearFrictionWater = source.ParseFloat();
    else if (token == "angularFrictionWater") settings.angularFrictionWater = source.ParseFloat();
    else if (token == "bouncyness") settings.bouncyness = source.ParseFloat();
    else if (token == "dampeningDecay") settings.dampeningDecay = source.ParseFloat();
    else if (token == "gravity") source.Parse1DMatrix(3, &settings.gravityVector.x);
    else if (token == "worldCollisionOnly") settings.worldCollisionOnly = source.ParseBool();
    else if (token == "simplePointCollision") settings.simplePointCollision = source.ParseBool();
    else if (token == "crazyBounceChance") settings.crazyBounceChance = source.ParseFloat();
    else if (token == "maxSimulationTime") settings.maxSimulationTime = source.ParseFloat();
    else if (token == "stopSpeed") settings.stopSpeed = source.ParseFloat();
    else if (token == "maxLinearVelocity") settings.maxLinearVelocity = source.ParseFloat();
    else if (token == "maxAngularVelocity") settings.maxAngularVelocity = source.ParseFloat();
    else if (token == "noShadows") settings.noShadows = source.ParseBool();
    else if (token == "impactParticle") { source.ReadToken(token); settings.impactParticle=FindDecl<idDeclParticle>("particle",token.data); }
    else if (token == "deathParticle") { source.ReadToken(token); settings.deathParticle=FindDecl<idDeclParticle>("particle",token.data); }
    else if (token == "harmlessParticle") { source.ReadToken(token); settings.harmlessParticle=FindDecl<idDeclParticle>("particle",token.data); }
    else if (token == "impactSoundTable") { source.ReadToken(token); settings.impactSoundTable=FindDecl<idDeclImpactSound>("impactSound",token.data); }
    else if (token == "clipMask") ParseContents(source, settings.clipMask);
    else if (token == "explosion") ParseExplosion(source);
    else if (token == "trail") ParseTrail(source);
    else if (token == "armoredPieces") ParseArmoredPieces(source);
    else if (token == "healthPieces") ParseHealthPieces(source);
    else if (token == "destroyPieces") ParseDestroyPieces(source);
    else if (token == "decal") ParseDecals(source);
    else if (token == "spark") ParseSpark(source);
    else if (token == "pieceDecal") ParsePieceDecal(source);
    else { source.Warning("Bad token '%s'\n", token.data); return; }
  }
  if (modelName.str[0] == '\0') source.Warning("No model specified for breakable\n");
}
