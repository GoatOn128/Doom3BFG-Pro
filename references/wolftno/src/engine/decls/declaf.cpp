#include "declaf.h"

#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr int CONTENTS_SOLID = 0x0001;
constexpr int CONTENTS_PLAYERCLIP = 0x0008;
constexpr int CONTENTS_MONSTERCLIP = 0x0010;
constexpr int CONTENTS_IKCLIP = 0x0100;
constexpr int CONTENTS_AI = 0x0400;
constexpr int CONTENTS_CORPSE = 0x1000;
constexpr int CONTENTS_PLAYER = 0x8000;

constexpr int TRM_INVALID = 0;
constexpr int TRM_BOX = 1;
constexpr int TRM_OCTAHEDRON = 2;
constexpr int TRM_DODECAHEDRON = 3;
constexpr int TRM_CYLINDER = 4;
constexpr int TRM_CONE = 5;
constexpr int TRM_BONE = 6;
constexpr int TRM_CUSTOM = 9;

template <typename T, int Tag> void DeletePointerList(idList<T*, Tag>& values) {
  for (int i = 0; i < values.Num(); ++i)
    delete values[i];
  if (values.listStatic == 0 || values.listStatic == 2) {
    delete[] values.list;
    values.list = nullptr;
    values.size = 0;
  }
  values.num = 0;
}

bool ReadAtomicString(idParser& source, idAtomicString& destination, bool lower = false) {
  idToken token;
  if (!source.ExpectTokenType(TT_STRING, 0, token))
    return false;
  if (lower)
    token.ToLower();
  destination.Set(token.data);
  return true;
}

idDecl* FindExternalDecl(const char* typeName, const char* declName, bool makeDefault) {
  idDeclInfo* type = declManager != nullptr ? declManager->GetDeclType(typeName) : nullptr;
  return type != nullptr ? type->FindWithInheritance(declName, makeDefault, true) : nullptr;
}

const char* ExternalDeclName(const void* decl) {
  return decl != nullptr ? reinterpret_cast<const idResource*>(decl)->name.str : "";
}

const char* JointModToString(declAFJointMod_t value) {
  switch (value) {
    case DECLAF_JOINTMOD_ORIGIN: return "position";
    case DECLAF_JOINTMOD_BOTH: return "both";
    default: return "orientation";
  }
}

bool IsZero(const idVec3& value) {
  return value.x == 0.0f && value.y == 0.0f && value.z == 0.0f;
}

bool IsIdentity(const idMat3& value) {
  return value[0].x == 1.0f && value[0].y == 0.0f && value[0].z == 0.0f &&
         value[1].x == 0.0f && value[1].y == 1.0f && value[1].z == 0.0f &&
         value[2].x == 0.0f && value[2].y == 0.0f && value[2].z == 1.0f;
}

float ClampTwist(float value) {
  return std::max(0.1f, std::min(value, 90.0f));
}
} // namespace

bool idAFVector::Parse(idParser& source) {
  idToken token;
  if (!source.ReadToken(token))
    return false;

  negate = token == "-";
  if (negate && !source.ReadToken(token))
    return false;

  if (token == "(") {
    type = VEC_COORDS;
    vec.x = source.ParseFloat();
    if (!source.ExpectTokenString(",")) return false;
    vec.y = source.ParseFloat();
    if (!source.ExpectTokenString(",")) return false;
    vec.z = source.ParseFloat();
    return source.ExpectTokenString(")");
  }

  if (token.Icmp("joint") == 0) {
    type = VEC_JOINT;
    if (!source.ExpectTokenString("(") || !source.ReadToken(token)) return false;
    token.ToLower();
    joint1.Set(token.data);
    return source.ExpectTokenString(")");
  }

  const bool center = token.Icmp("bonecenter") == 0;
  const bool direction = token.Icmp("bonedir") == 0;
  if (center || direction) {
    type = center ? VEC_BONECENTER : VEC_BONEDIR;
    if (!source.ExpectTokenString("(") || !source.ReadToken(token)) return false;
    token.ToLower();
    joint1.Set(token.data);
    if (!source.ExpectTokenString(",") || !source.ReadToken(token)) return false;
    token.ToLower();
    joint2.Set(token.data);
    return source.ExpectTokenString(")");
  }

  source.Error("unknown token %s in vector", token.data);
  return false;
}

bool idAFVector::Finish(const char* fileName, getJointTransform_t getJointTransform,
                        const idJointMat* frame, void* model) {
  idMat3 jointAxis{};
  idVec3 start{};
  idVec3 end{};
  const auto getJoint = [&](const idAtomicString& joint, idVec3& origin,
                            const char* expression) {
    if (getJointTransform != nullptr &&
        getJointTransform(model, frame, joint.str, &origin, &jointAxis)) {
      return;
    }
    idLib::Warning("invalid joint %s in %s() in '%s'", joint.str, expression,
                   fileName != nullptr ? fileName : "");
    origin = {};
  };

  switch (type) {
    case VEC_COORDS: break;
    case VEC_JOINT: getJoint(joint1, vec, "joint"); break;
    case VEC_BONECENTER:
      getJoint(joint1, start, "bonecenter");
      getJoint(joint2, end, "bonecenter");
      vec = (start + end) * 0.5f;
      break;
    case VEC_BONEDIR:
      getJoint(joint1, start, "bonedir");
      getJoint(joint2, end, "bonedir");
      vec = end - start;
      break;
    default: vec = {}; break;
  }
  if (negate)
    vec = -vec;
  return true;
}

bool idAFVector::Finish(const char* fileName, getJointTransform_t getJointTransform,
                        const idJointMat* frame, void* model, const idAFVector& shaft0,
                        const idAFVector& shaft1) {
  if (type != VEC_AXIS)
    return Finish(fileName, getJointTransform, frame, model);
  vec = shaft0.vec.Cross(shaft1.vec);
  vec.Normalize();
  return true;
}

bool idAFVector::Write(idFile* file) const {
  if (file == nullptr)
    return false;
  if (negate)
    file->WriteFloatString("-");
  switch (type) {
    case VEC_COORDS: file->WriteFloatString("( %f, %f, %f )", vec.x, vec.y, vec.z); break;
    case VEC_JOINT: file->WriteFloatString("joint( \"%s\" )", joint1.str); break;
    case VEC_BONECENTER:
      file->WriteFloatString("bonecenter( \"%s\", \"%s\" )", joint1.str, joint2.str);
      break;
    case VEC_BONEDIR:
      file->WriteFloatString("bonedir( \"%s\", \"%s\" )", joint1.str, joint2.str);
      break;
    case VEC_AXIS: break;
  }
  return true;
}

idDeclAF_Body::idDeclAF_Body()
    : jointMod(DECLAF_JOINTMOD_AXIS), modelType(TRM_INVALID), numSides(0), width(0.0f),
      density(0.0f), angles{}, contents(0), clipMask(0), selfCollision(false),
      noSyncCollide(false), clearClipMaskInSolid(false), bouncyness(0.0f), inertiaScale{},
      linearFriction(0.0f), angularFriction(0.0f), contactFriction(0.0f),
      gravityScale(0.0f), buoyancyScale(0.0f) {}

void idDeclAF_Body::SetDefault(const idDeclAF* file) {
  name.Set("noname");
  jointName.Set(file->base.str);
  jointMod = DECLAF_JOINTMOD_AXIS;
  modelType = TRM_BOX;
  modelName.Set("");
  v1.type = v2.type = idAFVector::VEC_COORDS;
  v1.vec = {-10.0f, -10.0f, -10.0f};
  v2.vec = {10.0f, 10.0f, 10.0f};
  numSides = 3;
  width = 0.0f;
  density = 0.002f;
  origin.type = idAFVector::VEC_COORDS;
  origin.vec = {};
  angles = {};
  contents = file->contents;
  clipMask = file->clipMask;
  selfCollision = file->selfCollision;
  noSyncCollide = false;
  clearClipMaskInSolid = false;
  bouncyness = file->defaultBouncyness;
  inertiaScale[0] = {1.0f, 0.0f, 0.0f};
  inertiaScale[1] = {0.0f, 1.0f, 0.0f};
  inertiaScale[2] = {0.0f, 0.0f, 1.0f};
  linearFriction = file->defaultLinearFriction;
  angularFriction = file->defaultAngularFriction;
  contactFriction = file->defaultContactFriction;
  gravityScale = file->defaultGravityScale;
  buoyancyScale = file->defaultBuoyancyScale;
  containedJoints.Set("*origin");
  frictionDirection.vec = {};
  contactMotorDirection.vec = {};
  containedJointIndices.Clear();
}

idDeclAF_Constraint::idDeclAF_Constraint()
    : type(DECLAF_CONSTRAINT_INVALID), friction(0.0f), stretch(0.0f), compress(0.0f),
      damping(0.0f), restLength(0.0f), minLength(0.0f), maxLength(0.0f), minTwist(0.0f),
      maxTwist(0.0f), limit(LIMIT_NONE), limitAngles{}, limitFudge(false),
      syncConstraint(false) {}

void idDeclAF_Constraint::SetDefault(const idDeclAF* file) {
  name.Set("noname");
  body1.Set(file->bodies.Num() > 0 ? file->bodies[0]->name.str : "world");
  body2.Set("world");
  type = DECLAF_CONSTRAINT_UNIVERSALJOINT;
  friction = file->defaultConstraintFriction;
  stretch = compress = damping = restLength = minLength = maxLength = 0.0f;
  minTwist = maxTwist = 0.1f;
  anchor.vec = anchor2.vec = {};
  axis.vec = {1.0f, 0.0f, 0.0f};
  shaft[0].vec = {0.0f, 0.0f, -1.0f};
  shaft[1].vec = {0.0f, 0.0f, 1.0f};
  limit = LIMIT_NONE;
  limitAxis.vec = {0.0f, 0.0f, -1.0f};
  std::fill(std::begin(limitAngles), std::end(limitAngles), 0.0f);
  limitFudge = true;
  syncConstraint = false;
}

idDeclAF::idDeclAF()
    : modified(false), modelDef(nullptr), skin(), defaultBouncyness(0.0f),
      defaultLinearFriction(0.0f), defaultAngularFriction(0.0f),
      defaultContactFriction(0.0f), defaultConstraintFriction(0.0f),
      defaultGravityScale(0.0f), defaultBuoyancyScale(0.0f), suspendVelocity{},
      suspendAcceleration{}, noMoveTime(0.0f), noMoveTranslation(0.0f),
      noMoveRotation(0.0f), minMoveTime(0.0f), maxMoveTime(0.0f), contents(0), clipMask(0),
      solverConstants{0.5f, 256.0f, 0.00001f, 0.001f, 0.3f, 256.0f, 0.0001f,
                      0.2f, 256.0f, 0.000001f, 0.5f, 256.0f, 0.1f, 0.1f, 1.0f},
      syncSolverConstants{0.5f, 512.0f, 0.00001f, 0.001f, 0.1f, 128.0f, 0.000001f,
                          0.2f, 256.0f, 0.000001f, 0.5f, 512.0f, 0.1f, 0.1f, 0.5f},
      selfCollision(false), base(), clipMaterial(nullptr) {
  resourceListPtr = &resourceList;
  FreeData();
}

idDeclAF::~idDeclAF() {
  DeletePointerList(bodies);
  DeletePointerList(constraints);
}

idDeclInfo* idDeclAF::GetDeclInfo() const { return &resourceList; }

void idDeclAF::FreeData() {
  modified = false;
  modelDef = nullptr;
  skin.Set("");
  defaultBouncyness = 0.3f;
  defaultLinearFriction = 0.01f;
  defaultAngularFriction = 0.01f;
  defaultContactFriction = 0.8f;
  defaultConstraintFriction = 0.5f;
  defaultGravityScale = 1.0f;
  defaultBuoyancyScale = 1.0f;
  suspendVelocity = {20.0f, 30.0f};
  suspendAcceleration = {40.0f, 60.0f};
  noMoveTime = 1.0f;
  noMoveTranslation = 10.0f;
  noMoveRotation = 10.0f;
  minMoveTime = maxMoveTime = -1.0f;
  contents = CONTENTS_CORPSE;
  clipMask = CONTENTS_SOLID | CONTENTS_CORPSE;
  selfCollision = true;
  base.Set("origin");
  clipMaterial = nullptr;
  DeletePointerList(bodies);
  DeletePointerList(constraints);
}

const char* idDeclAF::DefaultDefinition() const {
  return "{\n"
         "\tsettings {\n"
         "\t\tmodel \"\"\n"
         "\t\tskin \"\"\n"
         "\t\tdefaultBouncyness 0.3\n"
         "\t\tdefaultLinearFriction 0.01\n"
         "\t\tdefaultAngularFriction 0.01\n"
         "\t\tdefaultContactFriction 0.8\n"
         "\t\tdefaultConstraintFriction 0.5\n"
         "\t\tdefaultGravityScale 1.0\n"
         "\t\tsuspendSpeed 20, 30, 40, 60\n"
         "\t\tnoMoveTime 1\n"
         "\t\tnoMoveTranslation 10\n"
         "\t\tnoMoveRotation 10\n"
         "\t\tminMoveTime -1\n"
         "\t\tmaxMoveTime -1\n"
         "\t\tcontents corpse\n"
         "\t\tclipMask solid, corpse\n"
         "\t\tselfCollision 1\n"
         "\t\tbase \"origin\"\n"
         "\t}\n"
         "\tbody \"body\" {\n"
         "\t\tjoint \"origin\"\n"
         "\t\tmod orientation\n"
         "\t\tmodel box( ( -10, -10, -10 ), ( 10, 10, 10 ) )\n"
         "\t\torigin ( 0, 0, 0 )\n"
         "\t\tdensity 0.2\n"
         "\t\tcontents corpse\n"
         "\t\tclipMask solid, corpse\n"
         "\t\tselfCollision 1\n"
         "\t\tcontainedJoints \"*origin\"\n"
         "\t\tnoSyncCollide 0\n"
         "\t\tclearClipMaskInSolid 0\n"
         "\t}\n"
         "}\n";
}

int idDeclAF::ContentsFromString(const char* text) {
  if (text == nullptr)
    return 0;
  idLexer source;
  source.LoadMemory(text, static_cast<std::size_t>(idStr::Length(text)),
                    "idDeclAF::ContentsFromString");
  idToken token;
  int result = 0;
  while (source.ReadToken(token)) {
    if (token.Icmp("none") == 0) result = 0;
    else if (token.Icmp("solid") == 0) result |= CONTENTS_SOLID;
    else if (token.Icmp("ikclip") == 0) result |= CONTENTS_IKCLIP;
    else if (token.Icmp("ai") == 0) result |= CONTENTS_AI;
    else if (token.Icmp("corpse") == 0) result |= CONTENTS_CORPSE;
    else if (token.Icmp("player") == 0) result |= CONTENTS_PLAYER;
    else if (token.Icmp("playerclip") == 0) result |= CONTENTS_PLAYERCLIP;
    else if (token.Icmp("monsterclip") == 0) result |= CONTENTS_MONSTERCLIP;
    else if (token != ",") break;
  }
  return result;
}

const char* idDeclAF::ContentsToString(int value, idStr& text) {
  text.Clear();
  const auto append = [&](int bit, const char* contentName) {
    if ((value & bit) == 0) return;
    if (!text.IsEmpty()) text += ", ";
    text += contentName;
  };
  append(CONTENTS_SOLID, "solid");
  append(CONTENTS_IKCLIP, "ikclip");
  append(CONTENTS_AI, "ai");
  append(CONTENTS_CORPSE, "corpse");
  append(CONTENTS_PLAYERCLIP, "playerclip");
  append(CONTENTS_MONSTERCLIP, "monsterclip");
  if (text.IsEmpty()) text = "none";
  return text.data;
}

declAFJointMod_t idDeclAF::JointModFromString(const char* text) {
  if (idStr::Icmp(text, "position") == 0) return DECLAF_JOINTMOD_ORIGIN;
  if (idStr::Icmp(text, "both") == 0) return DECLAF_JOINTMOD_BOTH;
  return DECLAF_JOINTMOD_AXIS;
}

bool idDeclAF::ParseContents(idParser& source, int& value) const {
  idStr text;
  idToken token;
  while (source.ReadToken(token)) {
    text += token;
    if (!source.CheckTokenString(",")) break;
    text += ',';
  }
  value = ContentsFromString(text.data);
  return true;
}

bool idDeclAF::ParseSolverConstants(idParser& source, solverConstants_t& constants) {
  if (!source.ExpectTokenString("{")) return false;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    float* target = nullptr;
    if (token.Icmp("errorReduction") == 0) target = &constants.errorReduction;
    else if (token.Icmp("errorReductionMax") == 0) target = &constants.errorReductionMax;
    else if (token.Icmp("lcpEpsilon") == 0) target = &constants.lcpEpsilon;
    else if (token.Icmp("maxLcpEpsilon") == 0) target = &constants.maxLcpEpsilon;
    else if (token.Icmp("limitErrorReduction") == 0) target = &constants.limitErrorReduction;
    else if (token.Icmp("limitErrorReductionMax") == 0) target = &constants.limitErrorReductionMax;
    else if (token.Icmp("limitLcpEpsilon") == 0) target = &constants.limitLcpEpsilon;
    else if (token.Icmp("contactErrorReduction") == 0) target = &constants.contactErrorReduction;
    else if (token.Icmp("contactErrorReductionMax") == 0) target = &constants.contactErrorReductionMax;
    else if (token.Icmp("contactLcpEpsilon") == 0) target = &constants.contactLcpEpsilon;
    else if (token.Icmp("universalErrorReduction") == 0) target = &constants.universalErrorReduction;
    else if (token.Icmp("universalErrorReductionMax") == 0) target = &constants.universalErrorReductionMax;
    else if (token.Icmp("universalTorsionLcpEpsilon") == 0) target = &constants.universalTorsionLcpEpsilon;
    else if (token.Icmp("minTwistErrorReduction") == 0) target = &constants.minTwistErrorReduction;
    else if (token.Icmp("maxTwistErrorReduction") == 0) target = &constants.maxTwistErrorReduction;
    if (target == nullptr) {
      source.Error("unknown token in solver constants: %s", token.data);
      return false;
    }
    *target = source.ParseFloat();
  }
  return true;
}

bool idDeclAF::ParseSettings(idParser& source) {
  if (!source.ExpectTokenString("{")) return false;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("mesh") == 0 || token.Icmp("anim") == 0) {
      if (!source.ExpectTokenType(TT_STRING, 0, token)) return false;
    } else if (token.Icmp("model") == 0) {
      if (!source.ExpectTokenType(TT_STRING, 0, token)) return false;
      // NEEDS_WORK(engine/model): replace the generic decl lookup with
      // idDeclMD6::resourceList once that module is reconstructed.
      idDecl* model = FindExternalDecl("md6", token.data, true);
      if (model == nullptr) model = FindExternalDecl("modelDef", token.data, true);
      if (model == nullptr) model = FindExternalDecl("model", token.data, true);
      modelDef = reinterpret_cast<const idDeclMD6*>(model);
      if (modelDef == nullptr && token.Length() != 0)
        idLib::Warning("AF '%s' references an invalid model '%s'.", name.str, token.data);
    } else if (token.Icmp("clipMaterial") == 0) {
      if (!source.ExpectTokenType(TT_STRING, 0, token)) return false;
      clipMaterial = reinterpret_cast<const idMaterial*>(
          FindExternalDecl("material", token.data, false));
    } else if (token.Icmp("skin") == 0) {
      if (!ReadAtomicString(source, skin)) return false;
    } else if (token.Icmp("defaultBouncyness") == 0) {
      defaultBouncyness = source.ParseFloat();
    } else if (token.Icmp("defaultLinearFriction") == 0) {
      defaultLinearFriction = source.ParseFloat();
    } else if (token.Icmp("defaultAngularFriction") == 0) {
      defaultAngularFriction = source.ParseFloat();
    } else if (token.Icmp("defaultContactFriction") == 0) {
      defaultContactFriction = source.ParseFloat();
    } else if (token.Icmp("defaultConstraintFriction") == 0) {
      defaultConstraintFriction = source.ParseFloat();
    } else if (token.Icmp("defaultGravityScale") == 0) {
      defaultGravityScale = source.ParseFloat();
    } else if (token.Icmp("defaultBuoyancyScale") == 0) {
      defaultBuoyancyScale = source.ParseFloat();
    } else if (token.Icmp("suspendSpeed") == 0) {
      suspendVelocity.x = source.ParseFloat();
      if (!source.ExpectTokenString(",")) return false;
      suspendVelocity.y = source.ParseFloat();
      if (!source.ExpectTokenString(",")) return false;
      suspendAcceleration.x = source.ParseFloat();
      if (!source.ExpectTokenString(",")) return false;
      suspendAcceleration.y = source.ParseFloat();
    } else if (token.Icmp("noMoveTime") == 0) {
      noMoveTime = source.ParseFloat();
    } else if (token.Icmp("noMoveTranslation") == 0) {
      noMoveTranslation = source.ParseFloat();
    } else if (token.Icmp("noMoveRotation") == 0) {
      noMoveRotation = source.ParseFloat();
    } else if (token.Icmp("minMoveTime") == 0) {
      minMoveTime = source.ParseFloat();
    } else if (token.Icmp("maxMoveTime") == 0) {
      maxMoveTime = source.ParseFloat();
    } else if (token.Icmp("contents") == 0) {
      ParseContents(source, contents);
    } else if (token.Icmp("clipMask") == 0) {
      ParseContents(source, clipMask);
    } else if (token.Icmp("selfCollision") == 0) {
      selfCollision = source.ParseBool();
    } else if (token.Icmp("base") == 0) {
      if (!ReadAtomicString(source, base, true)) return false;
    } else if (token.Icmp("solverConstants") == 0) {
      if (!ParseSolverConstants(source, solverConstants)) return false;
    } else if (token.Icmp("syncSolverConstants") == 0) {
      if (!ParseSolverConstants(source, syncSolverConstants)) return false;
    } else {
      source.Error("unknown token %s in settings", token.data);
      return false;
    }
  }
  return true;
}

bool idDeclAF::ParseBody(idParser& source) {
  auto* body = new idDeclAF_Body;
  body->SetDefault(this);
  bodies.Append(body);

  idToken token;
  if (!source.ExpectTokenType(TT_STRING, 0, token) || !source.ExpectTokenString("{"))
    return false;
  body->name.Set(token.data);
  if (idStr::Icmp(body->name.str, "origin") == 0 || idStr::Icmp(body->name.str, "world") == 0) {
    source.Error("a body may not be named \"origin\" or \"world\"");
    return false;
  }

  bool hasJoint = false;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("model") == 0) {
      if (!source.ExpectTokenType(TT_NAME, 0, token)) return false;
      const bool custom = token.Icmp("custom") == 0;
      if (token.Icmp("box") == 0) body->modelType = TRM_BOX;
      else if (token.Icmp("octahedron") == 0) body->modelType = TRM_OCTAHEDRON;
      else if (token.Icmp("dodecahedron") == 0) body->modelType = TRM_DODECAHEDRON;
      else if (token.Icmp("cylinder") == 0) body->modelType = TRM_CYLINDER;
      else if (token.Icmp("cone") == 0) body->modelType = TRM_CONE;
      else if (token.Icmp("bone") == 0) body->modelType = TRM_BONE;
      else if (custom) body->modelType = TRM_CUSTOM;
      else {
        source.Error("unknown model type %s", token.data);
        return false;
      }

      if (!source.ExpectTokenString("(")) return false;
      if (custom) {
        if (!source.ExpectTokenType(TT_STRING, 0, token)) return false;
        body->modelName.Set(token.data);
        if (!source.ExpectTokenString(",")) return false;
      }
      if (!body->v1.Parse(source) || !source.ExpectTokenString(",") ||
          !body->v2.Parse(source)) return false;
      if (body->modelType == TRM_CYLINDER || body->modelType == TRM_CONE) {
        if (!source.ExpectTokenString(",")) return false;
        body->numSides = source.ParseInt();
      } else if (body->modelType == TRM_BONE) {
        if (!source.ExpectTokenString(",")) return false;
        body->width = source.ParseFloat();
      }
      if (!source.ExpectTokenString(")")) return false;
    } else if (token.Icmp("origin") == 0) {
      if (!body->origin.Parse(source)) return false;
    } else if (token.Icmp("angles") == 0) {
      idAFVector angles;
      if (!angles.Parse(source)) return false;
      body->angles = {angles.vec.x, angles.vec.y, angles.vec.z};
    } else if (token.Icmp("joint") == 0) {
      if (!ReadAtomicString(source, body->jointName, true)) return false;
      hasJoint = true;
    } else if (token.Icmp("mod") == 0) {
      if (!source.ExpectAnyToken(token)) return false;
      body->jointMod = JointModFromString(token.data);
    } else if (token.Icmp("density") == 0) {
      body->density = source.ParseFloat();
    } else if (token.Icmp("inertiaScale") == 0) {
      if (!source.Parse1DMatrix(9, &body->inertiaScale.mat[0].x)) return false;
    } else if (token.Icmp("linearFriction") == 0) {
      body->linearFriction = source.ParseFloat();
    } else if (token.Icmp("angularFriction") == 0) {
      body->angularFriction = source.ParseFloat();
    } else if (token.Icmp("contactFriction") == 0) {
      body->contactFriction = source.ParseFloat();
    } else if (token.Icmp("contents") == 0) {
      ParseContents(source, body->contents);
    } else if (token.Icmp("clipMask") == 0) {
      ParseContents(source, body->clipMask);
    } else if (token.Icmp("selfCollision") == 0) {
      body->selfCollision = source.ParseBool();
    } else if (token.Icmp("bouncyness") == 0) {
      body->bouncyness = source.ParseFloat();
    } else if (token.Icmp("noSyncCollide") == 0) {
      body->noSyncCollide = source.ParseBool();
    } else if (token.Icmp("clearClipMaskInSolid") == 0) {
      body->clearClipMaskInSolid = source.ParseBool();
    } else if (token.Icmp("containedjoints") == 0) {
      if (!ReadAtomicString(source, body->containedJoints)) return false;
      // NEEDS_WORK(engine/model): idDeclMD6::GetJointList populates these
      // indices when the model declaration implementation is reconstructed.
      body->containedJointIndices.Clear();
    } else if (token.Icmp("frictionDirection") == 0) {
      if (!body->frictionDirection.Parse(source)) return false;
    } else if (token.Icmp("contactMotorDirection") == 0) {
      if (!body->contactMotorDirection.Parse(source)) return false;
    } else {
      source.Error("unknown token %s in body", token.data);
      return false;
    }
  }

  if (body->modelType == TRM_INVALID) {
    source.Error("no model set for body");
    return false;
  }
  if (!hasJoint) {
    source.Error("no joint set for body");
    return false;
  }
  return true;
}

bool idDeclAF::ParseFixed(idParser& source) {
  auto* constraint = new idDeclAF_Constraint;
  constraint->SetDefault(this);
  constraints.Append(constraint);
  idToken token;
  if (!source.ExpectTokenType(TT_STRING, 0, token) || !source.ExpectTokenString("{"))
    return false;
  constraint->type = DECLAF_CONSTRAINT_FIXED;
  constraint->name.Set(token.data);
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("body1") == 0) {
      if (!ReadAtomicString(source, constraint->body1)) return false;
    } else if (token.Icmp("body2") == 0) {
      if (!ReadAtomicString(source, constraint->body2)) return false;
    } else if (token.Icmp("syncConstraint") == 0) {
      constraint->syncConstraint = source.ParseBool();
    } else {
      source.Error("unknown token %s in fixed constraint", token.data);
      return false;
    }
  }
  return true;
}

bool idDeclAF::ParseBallAndSocketJoint(idParser& source) {
  auto* constraint = new idDeclAF_Constraint;
  constraint->SetDefault(this);
  constraints.Append(constraint);
  idToken token;
  if (!source.ExpectTokenType(TT_STRING, 0, token) || !source.ExpectTokenString("{"))
    return false;
  constraint->type = DECLAF_CONSTRAINT_BALLANDSOCKETJOINT;
  constraint->limit = idDeclAF_Constraint::LIMIT_NONE;
  constraint->name.Set(token.data);
  constraint->friction = 0.5f;
  constraint->anchor.vec = {};
  constraint->shaft[0].vec = {};

  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("body1") == 0) {
      if (!ReadAtomicString(source, constraint->body1)) return false;
    } else if (token.Icmp("body2") == 0) {
      if (!ReadAtomicString(source, constraint->body2)) return false;
    } else if (token.Icmp("anchor") == 0) {
      if (!constraint->anchor.Parse(source)) return false;
    } else if (token.Icmp("shafts") == 0) {
      if (!constraint->shaft[0].Parse(source) || !source.ExpectTokenString(",") ||
          !constraint->shaft[1].Parse(source)) return false;
    } else if (token.Icmp("coneLimit") == 0) {
      if (!constraint->limitAxis.Parse(source) || !source.ExpectTokenString(",")) return false;
      constraint->limitAngles[0] = source.ParseFloat();
      if (!source.ExpectTokenString(",") || !constraint->shaft[0].Parse(source)) return false;
      constraint->limit = idDeclAF_Constraint::LIMIT_CONE;
    } else if (token.Icmp("pyramidLimit") == 0) {
      if (!constraint->limitAxis.Parse(source) || !source.ExpectTokenString(",")) return false;
      for (int i = 0; i < 3; ++i) {
        constraint->limitAngles[i] = source.ParseFloat();
        if (!source.ExpectTokenString(",")) return false;
      }
      if (!constraint->shaft[0].Parse(source)) return false;
      constraint->limit = idDeclAF_Constraint::LIMIT_PYRAMID;
    } else if (token.Icmp("limitFudge") == 0) {
      constraint->limitFudge = source.ParseBool();
    } else if (token.Icmp("friction") == 0) {
      constraint->friction = source.ParseFloat();
    } else if (token.Icmp("syncConstraint") == 0) {
      constraint->syncConstraint = source.ParseBool();
    } else if (token.Icmp("minTwist") == 0) {
      constraint->minTwist = ClampTwist(source.ParseFloat());
    } else if (token.Icmp("maxTwist") == 0) {
      constraint->maxTwist = ClampTwist(source.ParseFloat());
    } else {
      source.Error("unknown token %s in ball and socket joint", token.data);
      return false;
    }
  }
  return true;
}

bool idDeclAF::ParseUniversalJoint(idParser& source) {
  auto* constraint = new idDeclAF_Constraint;
  constraint->SetDefault(this);
  constraints.Append(constraint);
  idToken token;
  if (!source.ExpectTokenType(TT_STRING, 0, token) || !source.ExpectTokenString("{"))
    return false;
  constraint->type = DECLAF_CONSTRAINT_UNIVERSALJOINT;
  constraint->limit = idDeclAF_Constraint::LIMIT_NONE;
  constraint->name.Set(token.data);
  constraint->friction = 0.5f;
  constraint->anchor.vec = {};
  constraint->shaft[0].vec = constraint->shaft[1].vec = {};

  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("body1") == 0) {
      if (!ReadAtomicString(source, constraint->body1)) return false;
    } else if (token.Icmp("body2") == 0) {
      if (!ReadAtomicString(source, constraint->body2)) return false;
    } else if (token.Icmp("anchor") == 0) {
      if (!constraint->anchor.Parse(source)) return false;
    } else if (token.Icmp("shafts") == 0) {
      if (!constraint->shaft[0].Parse(source) || !source.ExpectTokenString(",") ||
          !constraint->shaft[1].Parse(source)) return false;
    } else if (token.Icmp("coneLimit") == 0) {
      if (!constraint->limitAxis.Parse(source) || !source.ExpectTokenString(",")) return false;
      constraint->limitAngles[0] = source.ParseFloat();
      constraint->limit = idDeclAF_Constraint::LIMIT_CONE;
    } else if (token.Icmp("pyramidLimit") == 0) {
      if (!constraint->limitAxis.Parse(source) || !source.ExpectTokenString(",")) return false;
      for (int i = 0; i < 3; ++i) {
        constraint->limitAngles[i] = source.ParseFloat();
        if (i != 2 && !source.ExpectTokenString(",")) return false;
      }
      constraint->limit = idDeclAF_Constraint::LIMIT_PYRAMID;
    } else if (token.Icmp("limitFudge") == 0) {
      constraint->limitFudge = source.ParseBool();
    } else if (token.Icmp("friction") == 0) {
      constraint->friction = source.ParseFloat();
    } else if (token.Icmp("syncConstraint") == 0) {
      constraint->syncConstraint = source.ParseBool();
    } else if (token.Icmp("minTwist") == 0) {
      constraint->minTwist = ClampTwist(source.ParseFloat());
    } else if (token.Icmp("maxTwist") == 0) {
      constraint->maxTwist = ClampTwist(source.ParseFloat());
    } else {
      source.Error("unknown token %s in universal joint", token.data);
      return false;
    }
  }
  return true;
}

bool idDeclAF::ParseHinge(idParser& source) {
  auto* constraint = new idDeclAF_Constraint;
  constraint->SetDefault(this);
  constraints.Append(constraint);
  idToken token;
  if (!source.ExpectTokenType(TT_STRING, 0, token) || !source.ExpectTokenString("{"))
    return false;
  constraint->type = DECLAF_CONSTRAINT_HINGE;
  constraint->limit = idDeclAF_Constraint::LIMIT_NONE;
  constraint->name.Set(token.data);
  constraint->friction = 0.5f;
  constraint->anchor.vec = {};
  constraint->axis.vec = {};
  constraint->minTwist = constraint->maxTwist = 0.1f;

  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("body1") == 0) {
      if (!ReadAtomicString(source, constraint->body1)) return false;
    } else if (token.Icmp("body2") == 0) {
      if (!ReadAtomicString(source, constraint->body2)) return false;
    } else if (token.Icmp("anchor") == 0) {
      if (!constraint->anchor.Parse(source)) return false;
    } else if (token.Icmp("axis") == 0) {
      if (!constraint->axis.Parse(source)) return false;
    } else if (token.Icmp("shafts") == 0) {
      if (!constraint->shaft[0].Parse(source) || !source.ExpectTokenString(",") ||
          !constraint->shaft[1].Parse(source)) return false;
      constraint->axis.type = idAFVector::VEC_AXIS;
      constraint->axis.vec = {};
    } else if (token.Icmp("limit") == 0) {
      constraint->limitAngles[0] = source.ParseFloat();
      if (!source.ExpectTokenString(",")) return false;
      constraint->limitAngles[1] = source.ParseFloat();
      if (!source.ExpectTokenString(",")) return false;
      constraint->limitAngles[2] = source.ParseFloat();
      if (source.CheckTokenString(","))
        constraint->limitAngles[3] = source.ParseFloat();
      else
        constraint->limitAngles[2] = constraint->limitAngles[3] = 0.0f;
      constraint->limit = idDeclAF_Constraint::LIMIT_CONE;
    } else if (token.Icmp("limitFudge") == 0) {
      constraint->limitFudge = source.ParseBool();
    } else if (token.Icmp("friction") == 0) {
      constraint->friction = source.ParseFloat();
    } else if (token.Icmp("syncConstraint") == 0) {
      constraint->syncConstraint = source.ParseBool();
    } else if (token.Icmp("minTwist") == 0) {
      constraint->minTwist = ClampTwist(source.ParseFloat());
    } else if (token.Icmp("maxTwist") == 0) {
      constraint->maxTwist = ClampTwist(source.ParseFloat());
    } else {
      source.Error("unknown token %s in hinge", token.data);
      return false;
    }
  }
  return true;
}

bool idDeclAF::ParseSlider(idParser& source) {
  auto* constraint = new idDeclAF_Constraint;
  constraint->SetDefault(this);
  constraints.Append(constraint);
  idToken token;
  if (!source.ExpectTokenType(TT_STRING, 0, token) || !source.ExpectTokenString("{"))
    return false;
  constraint->type = DECLAF_CONSTRAINT_SLIDER;
  constraint->limit = idDeclAF_Constraint::LIMIT_NONE;
  constraint->name.Set(token.data);
  constraint->friction = 0.5f;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("body1") == 0) {
      if (!ReadAtomicString(source, constraint->body1)) return false;
    } else if (token.Icmp("body2") == 0) {
      if (!ReadAtomicString(source, constraint->body2)) return false;
    } else if (token.Icmp("axis") == 0) {
      if (!constraint->axis.Parse(source)) return false;
    } else if (token.Icmp("friction") == 0) {
      constraint->friction = source.ParseFloat();
    } else if (token.Icmp("syncConstraint") == 0) {
      constraint->syncConstraint = source.ParseBool();
    } else {
      source.Error("unknown token %s in slider", token.data);
      return false;
    }
  }
  return true;
}

bool idDeclAF::ParseSpring(idParser& source) {
  auto* constraint = new idDeclAF_Constraint;
  constraint->SetDefault(this);
  constraints.Append(constraint);
  idToken token;
  if (!source.ExpectTokenType(TT_STRING, 0, token) || !source.ExpectTokenString("{"))
    return false;
  constraint->type = DECLAF_CONSTRAINT_SPRING;
  constraint->limit = idDeclAF_Constraint::LIMIT_NONE;
  constraint->name.Set(token.data);
  constraint->friction = 0.5f;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("body1") == 0) {
      if (!ReadAtomicString(source, constraint->body1)) return false;
    } else if (token.Icmp("body2") == 0) {
      if (!ReadAtomicString(source, constraint->body2)) return false;
    } else if (token.Icmp("anchor1") == 0) {
      if (!constraint->anchor.Parse(source)) return false;
    } else if (token.Icmp("anchor2") == 0) {
      if (!constraint->anchor2.Parse(source)) return false;
    } else if (token.Icmp("friction") == 0) constraint->friction = source.ParseFloat();
    else if (token.Icmp("stretch") == 0) constraint->stretch = source.ParseFloat();
    else if (token.Icmp("compress") == 0) constraint->compress = source.ParseFloat();
    else if (token.Icmp("damping") == 0) constraint->damping = source.ParseFloat();
    else if (token.Icmp("restLength") == 0) constraint->restLength = source.ParseFloat();
    else if (token.Icmp("minLength") == 0) constraint->minLength = source.ParseFloat();
    else if (token.Icmp("maxLength") == 0) constraint->maxLength = source.ParseFloat();
    else if (token.Icmp("syncConstraint") == 0) constraint->syncConstraint = source.ParseBool();
    else {
      source.Error("unknown token %s in spring", token.data);
      return false;
    }
  }
  return true;
}

void idDeclAF::Parse(idParser& source) {
  idToken token;
  while (source.ReadToken(token)) {
    bool parsed = true;
    if (token.Icmp("settings") == 0) parsed = ParseSettings(source);
    else if (token.Icmp("body") == 0) parsed = ParseBody(source);
    else if (token.Icmp("fixed") == 0) parsed = ParseFixed(source);
    else if (token.Icmp("ballAndSocketJoint") == 0) parsed = ParseBallAndSocketJoint(source);
    else if (token.Icmp("universalJoint") == 0) parsed = ParseUniversalJoint(source);
    else if (token.Icmp("hinge") == 0) parsed = ParseHinge(source);
    else if (token.Icmp("slider") == 0) parsed = ParseSlider(source);
    else if (token.Icmp("spring") == 0) parsed = ParseSpring(source);
    else if (token == "}") break;
    else {
      source.Error("unknown keyword %s", token.data);
      return;
    }
    if (!parsed) return;
  }

  for (int i = 0; i < bodies.Num(); ++i) {
    for (int j = i + 1; j < bodies.Num(); ++j) {
      if (idStr::Icmp(bodies[i]->name.str, bodies[j]->name.str) == 0)
        source.Error("two bodies with the same name \"%s\"", bodies[i]->name.str);
    }
  }
  for (int i = 0; i < constraints.Num(); ++i) {
    for (int j = i + 1; j < constraints.Num(); ++j) {
      if (idStr::Icmp(constraints[i]->name.str, constraints[j]->name.str) == 0)
        source.Error("two constraints with the same name \"%s\"", constraints[i]->name.str);
    }
    if (constraints[i]->body1.str[0] == '\0')
      source.Error("no valid body1 specified for constraint '%s'", constraints[i]->name.str);
    if (constraints[i]->body2.str[0] == '\0')
      source.Error("no valid body2 specified for constraint '%s'", constraints[i]->name.str);
  }

  for (int i = 0; i < bodies.Num(); ++i) {
    if (idStr::Icmp(bodies[i]->jointName.str, base.str) == 0) {
      if (i != 0) std::swap(bodies[0], bodies[i]);
      break;
    }
  }
}

void idDeclAF::Finish(getJointTransform_t getJointTransform, const idJointMat* frame,
                      void* model) const {
  const char* fileName = name.str;
  for (int i = 0; i < bodies.Num(); ++i) {
    idDeclAF_Body* body = bodies[i];
    body->v1.Finish(fileName, getJointTransform, frame, model);
    body->v2.Finish(fileName, getJointTransform, frame, model);
    body->origin.Finish(fileName, getJointTransform, frame, model);
    body->frictionDirection.Finish(fileName, getJointTransform, frame, model);
    body->contactMotorDirection.Finish(fileName, getJointTransform, frame, model);
  }
  for (int i = 0; i < constraints.Num(); ++i) {
    idDeclAF_Constraint* constraint = constraints[i];
    constraint->anchor.Finish(fileName, getJointTransform, frame, model);
    constraint->anchor2.Finish(fileName, getJointTransform, frame, model);
    constraint->shaft[0].Finish(fileName, getJointTransform, frame, model);
    constraint->shaft[1].Finish(fileName, getJointTransform, frame, model);
    constraint->axis.Finish(fileName, getJointTransform, frame, model,
                            constraint->shaft[0], constraint->shaft[1]);
    constraint->limitAxis.Finish(fileName, getJointTransform, frame, model);
  }
}

void idDeclAF::WriteSolverConstants(idFile* file, const solverConstants_t& c,
                                    const char* blockName, const char* indent) const {
  file->WriteFloatString("%s%s {\n", indent, blockName);
  file->WriteFloatString("\t%serrorReduction %f\n", indent, c.errorReduction);
  file->WriteFloatString("\t%serrorReductionMax %f\n", indent, c.errorReductionMax);
  file->WriteFloatString("\t%slcpEpsilon %f\n", indent, c.lcpEpsilon);
  file->WriteFloatString("\t%smaxLcpEpsilon %f\n", indent, c.maxLcpEpsilon);
  file->WriteFloatString("\t%slimitErrorReduction %f\n", indent, c.limitErrorReduction);
  file->WriteFloatString("\t%slimitErrorReductionMax %f\n", indent, c.limitErrorReductionMax);
  file->WriteFloatString("\t%slimitLcpEpsilon %f\n", indent, c.limitLcpEpsilon);
  file->WriteFloatString("\t%scontactErrorReduction %f\n", indent, c.contactErrorReduction);
  file->WriteFloatString("\t%scontactErrorReductionMax %f\n", indent, c.contactErrorReductionMax);
  file->WriteFloatString("\t%scontactLcpEpsilon %f\n", indent, c.contactLcpEpsilon);
  file->WriteFloatString("\t%suniversalErrorReduction %f\n", indent, c.universalErrorReduction);
  file->WriteFloatString("\t%suniversalErrorReductionMax %f\n", indent, c.universalErrorReductionMax);
  file->WriteFloatString("\t%suniversalTorsionLcpEpsilon %f\n", indent,
                         c.universalTorsionLcpEpsilon);
  file->WriteFloatString("\t%sminTwistErrorReduction %f\n", indent, c.minTwistErrorReduction);
  file->WriteFloatString("\t%smaxTwistErrorReduction %f\n", indent, c.maxTwistErrorReduction);
  file->WriteFloatString("%s}\n", indent);
}

bool idDeclAF::WriteBody(idFile* file, const idDeclAF_Body& body) const {
  idStr text;
  file->WriteFloatString("\nbody \"%s\" {\n", body.name.str);
  file->WriteFloatString("\tjoint \"%s\"\n", body.jointName.str);
  file->WriteFloatString("\tmod %s\n", JointModToString(body.jointMod));
  const char* shape = nullptr;
  switch (body.modelType) {
    case TRM_BOX: shape = "box"; break;
    case TRM_OCTAHEDRON: shape = "octahedron"; break;
    case TRM_DODECAHEDRON: shape = "dodecahedron"; break;
    case TRM_CYLINDER: shape = "cylinder"; break;
    case TRM_CONE: shape = "cone"; break;
    case TRM_BONE: shape = "bone"; break;
    case TRM_CUSTOM: shape = "custom"; break;
    default: return false;
  }
  if (body.modelType == TRM_CUSTOM)
    file->WriteFloatString("\tmodel %s( \"%s\", ", shape, body.modelName.str);
  else
    file->WriteFloatString("\tmodel %s( ", shape);
  body.v1.Write(file);
  file->WriteFloatString(", ");
  body.v2.Write(file);
  if (body.modelType == TRM_CYLINDER || body.modelType == TRM_CONE)
    file->WriteFloatString(", %d", body.numSides);
  else if (body.modelType == TRM_BONE)
    file->WriteFloatString(", %f", body.width);
  file->WriteFloatString(" )\n");

  file->WriteFloatString("\torigin ");
  body.origin.Write(file);
  file->WriteFloatString("\n");
  if (body.angles.pitch != 0.0f || body.angles.yaw != 0.0f || body.angles.roll != 0.0f)
    file->WriteFloatString("\tangles ( %f, %f, %f )\n", body.angles.pitch,
                           body.angles.yaw, body.angles.roll);
  file->WriteFloatString("\tdensity %f\n", body.density);
  if (!IsIdentity(body.inertiaScale)) {
    const idMat3& m = body.inertiaScale;
    file->WriteFloatString("\tinertiaScale (%f %f %f %f %f %f %f %f %f)\n",
                           m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2],
                           m[2][0], m[2][1], m[2][2]);
  }
  if (body.linearFriction != -1.0f)
    file->WriteFloatString("\tlinearFriction %f\n", body.linearFriction);
  if (body.angularFriction != -1.0f)
    file->WriteFloatString("\tangularFriction %f\n", body.angularFriction);
  if (body.contactFriction != -1.0f)
    file->WriteFloatString("\tcontactFriction %f\n", body.contactFriction);
  file->WriteFloatString("\tcontents %s\n", ContentsToString(body.contents, text));
  file->WriteFloatString("\tclipMask %s\n", ContentsToString(body.clipMask, text));
  file->WriteFloatString("\tselfCollision %d\n", body.selfCollision);
  file->WriteFloatString("\tnoSyncCollide %d\n", body.noSyncCollide);
  file->WriteFloatString("\tclearClipMaskInSolid %d\n", body.clearClipMaskInSolid);
  file->WriteFloatString("\tbouncyness %f\n", body.bouncyness);
  if (!IsZero(body.frictionDirection.vec)) {
    file->WriteFloatString("\tfrictionDirection ");
    body.frictionDirection.Write(file);
    file->WriteFloatString("\n");
  }
  if (!IsZero(body.contactMotorDirection.vec)) {
    file->WriteFloatString("\tcontactMotorDirection ");
    body.contactMotorDirection.Write(file);
    file->WriteFloatString("\n");
  }
  file->WriteFloatString("\tcontainedJoints \"%s\"\n", body.containedJoints.str);
  file->WriteFloatString("}\n");
  return true;
}

bool idDeclAF::WriteBallAndSocketJoint(idFile* file,
                                       const idDeclAF_Constraint& c) const {
  file->WriteFloatString("\nballAndSocketJoint \"%s\" {\n", c.name.str);
  file->WriteFloatString("\tbody1 \"%s\"\n\tbody2 \"%s\"\n\tanchor ", c.body1.str,
                         c.body2.str);
  c.anchor.Write(file);
  file->WriteFloatString("\n\tshafts ");
  c.shaft[0].Write(file);
  file->WriteFloatString(", ");
  c.shaft[1].Write(file);
  file->WriteFloatString("\n\tminTwist %f\n\tmaxTwist %f\n\tfriction %f\n",
                         c.minTwist, c.maxTwist, c.friction);
  if (c.limit == idDeclAF_Constraint::LIMIT_CONE) {
    file->WriteFloatString("\tconeLimit ");
    c.limitAxis.Write(file);
    file->WriteFloatString(", %f, ", c.limitAngles[0]);
    c.shaft[0].Write(file);
    file->WriteFloatString("\n\tlimitFudge %d\n", c.limitFudge);
  } else if (c.limit == idDeclAF_Constraint::LIMIT_PYRAMID) {
    file->WriteFloatString("\tpyramidLimit ");
    c.limitAxis.Write(file);
    file->WriteFloatString(", %f, %f, %f, ", c.limitAngles[0], c.limitAngles[1],
                           c.limitAngles[2]);
    c.shaft[0].Write(file);
    file->WriteFloatString("\n\tlimitFudge %d\n", c.limitFudge);
  }
  file->WriteFloatString("\tsyncConstraint %d\n}\n", c.syncConstraint);
  return true;
}

bool idDeclAF::WriteUniversalJoint(idFile* file, const idDeclAF_Constraint& c) const {
  file->WriteFloatString("\nuniversalJoint \"%s\" {\n", c.name.str);
  file->WriteFloatString("\tbody1 \"%s\"\n\tbody2 \"%s\"\n\tanchor ", c.body1.str,
                         c.body2.str);
  c.anchor.Write(file);
  file->WriteFloatString("\n\tshafts ");
  c.shaft[0].Write(file);
  file->WriteFloatString(", ");
  c.shaft[1].Write(file);
  file->WriteFloatString("\n\tminTwist %f\n\tmaxTwist %f\n\tfriction %f\n",
                         c.minTwist, c.maxTwist, c.friction);
  if (c.limit == idDeclAF_Constraint::LIMIT_CONE) {
    file->WriteFloatString("\tconeLimit ");
    c.limitAxis.Write(file);
    file->WriteFloatString(", %f\n\tlimitFudge %d\n", c.limitAngles[0], c.limitFudge);
  } else if (c.limit == idDeclAF_Constraint::LIMIT_PYRAMID) {
    file->WriteFloatString("\tpyramidLimit ");
    c.limitAxis.Write(file);
    file->WriteFloatString(", %f, %f, %f\n\tlimitFudge %d\n", c.limitAngles[0],
                           c.limitAngles[1], c.limitAngles[2], c.limitFudge);
  }
  file->WriteFloatString("\tsyncConstraint %d\n}\n", c.syncConstraint);
  return true;
}

bool idDeclAF::WriteHinge(idFile* file, const idDeclAF_Constraint& c) const {
  file->WriteFloatString("\nhinge \"%s\" {\n", c.name.str);
  file->WriteFloatString("\tbody1 \"%s\"\n\tbody2 \"%s\"\n\tanchor ", c.body1.str,
                         c.body2.str);
  c.anchor.Write(file);
  if (c.shaft[0].type == idAFVector::VEC_BONEDIR &&
      c.shaft[1].type == idAFVector::VEC_BONEDIR) {
    file->WriteFloatString("\n\tshafts ");
    c.shaft[0].Write(file);
    file->WriteFloatString(", ");
    c.shaft[1].Write(file);
  } else {
    file->WriteFloatString("\n\taxis ");
    c.axis.Write(file);
  }
  file->WriteFloatString("\n\tminTwist %f\n\tmaxTwist %f\n\tfriction %f\n",
                         c.minTwist, c.maxTwist, c.friction);
  if (c.limit == idDeclAF_Constraint::LIMIT_CONE) {
    file->WriteFloatString("\tlimit %f, %f, %f, %f\n", c.limitAngles[0],
                           c.limitAngles[1], c.limitAngles[2], c.limitAngles[3]);
    file->WriteFloatString("\tlimitFudge %d\n", c.limitFudge);
  }
  file->WriteFloatString("\tsyncConstraint %d\n}\n", c.syncConstraint);
  return true;
}

bool idDeclAF::WriteSpring(idFile* file, const idDeclAF_Constraint& c) const {
  file->WriteFloatString("\nspring \"%s\" {\n", c.name.str);
  file->WriteFloatString("\tbody1 \"%s\"\n\tbody2 \"%s\"\n\tanchor1 ", c.body1.str,
                         c.body2.str);
  c.anchor.Write(file);
  file->WriteFloatString("\n\tanchor2 ");
  c.anchor2.Write(file);
  file->WriteFloatString(
      "\n\tfriction %f\n\tstretch %f\n\tcompress %f\n\tdamping %f\n"
      "\trestLength %f\n\tminLength %f\n\tmaxLength %f\n\tsyncConstraint %d\n}\n",
      c.friction, c.stretch, c.compress, c.damping, c.restLength, c.minLength,
      c.maxLength, c.syncConstraint);
  return true;
}

bool idDeclAF::WriteSettings(idFile* file) const {
  idStr text;
  file->WriteFloatString("\nsettings {\n");
  file->WriteFloatString("\tmodel \"%s\"\n", ExternalDeclName(modelDef));
  file->WriteFloatString("\tskin \"%s\"\n", skin.str);
  if (clipMaterial != nullptr)
    file->WriteFloatString("\tclipMaterial \"%s\"\n", ExternalDeclName(clipMaterial));
  file->WriteFloatString("\tdefaultBouncyness %f\n", defaultBouncyness);
  file->WriteFloatString("\tdefaultLinearFriction %f\n", defaultLinearFriction);
  file->WriteFloatString("\tdefaultAngularFriction %f\n", defaultAngularFriction);
  file->WriteFloatString("\tdefaultContactFriction %f\n", defaultContactFriction);
  file->WriteFloatString("\tdefaultConstraintFriction %f\n", defaultConstraintFriction);
  file->WriteFloatString("\tdefaultGravityScale %f\n", defaultGravityScale);
  file->WriteFloatString("\tdefaultBuoyancyScale %f\n", defaultBuoyancyScale);
  file->WriteFloatString("\tsuspendSpeed %f, %f, %f, %f\n", suspendVelocity.x,
                         suspendVelocity.y, suspendAcceleration.x, suspendAcceleration.y);
  file->WriteFloatString("\tnoMoveTime %f\n", noMoveTime);
  file->WriteFloatString("\tnoMoveTranslation %f\n", noMoveTranslation);
  file->WriteFloatString("\tnoMoveRotation %f\n", noMoveRotation);
  file->WriteFloatString("\tminMoveTime %f\n", minMoveTime);
  file->WriteFloatString("\tmaxMoveTime %f\n", maxMoveTime);
  file->WriteFloatString("\tcontents %s\n", ContentsToString(contents, text));
  file->WriteFloatString("\tclipMask %s\n", ContentsToString(clipMask, text));
  file->WriteFloatString("\tselfCollision %d\n", selfCollision);
  WriteSolverConstants(file, solverConstants, "solverConstants", "\t");
  WriteSolverConstants(file, syncSolverConstants, "syncSolverConstants", "\t");
  file->WriteFloatString("}\n");
  return true;
}

bool idDeclAF::RebuildTextSource() {
  idFile_String file("declAF");
  file.WriteFloatString("{\n");
  if (!WriteSettings(&file)) return false;
  for (int i = 0; i < bodies.Num(); ++i) {
    if (!WriteBody(&file, *bodies[i])) return false;
  }
  for (int i = 0; i < constraints.Num(); ++i) {
    const idDeclAF_Constraint& c = *constraints[i];
    bool ok = true;
    switch (c.type) {
      case DECLAF_CONSTRAINT_FIXED:
        file.WriteFloatString("\nfixed \"%s\" {\n\tbody1 \"%s\"\n\tbody2 \"%s\"\n}\n",
                              c.name.str, c.body1.str, c.body2.str);
        break;
      case DECLAF_CONSTRAINT_BALLANDSOCKETJOINT: ok = WriteBallAndSocketJoint(&file, c); break;
      case DECLAF_CONSTRAINT_UNIVERSALJOINT: ok = WriteUniversalJoint(&file, c); break;
      case DECLAF_CONSTRAINT_HINGE: ok = WriteHinge(&file, c); break;
      case DECLAF_CONSTRAINT_SLIDER:
        file.WriteFloatString("\nslider \"%s\" {\n\tbody1 \"%s\"\n\tbody2 \"%s\"\n\taxis ",
                              c.name.str, c.body1.str, c.body2.str);
        c.axis.Write(&file);
        file.WriteFloatString("\n\tfriction %f\n\tsyncConstraint %d\n}\n", c.friction,
                              c.syncConstraint);
        break;
      case DECLAF_CONSTRAINT_SPRING: ok = WriteSpring(&file, c); break;
      default: ok = false; break;
    }
    if (!ok) return false;
  }
  file.WriteFloatString("\n}");
  SetText(file.GetDataPtr(), static_cast<int>(file.Length()));
  return true;
}
