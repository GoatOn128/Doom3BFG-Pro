#include "engine/models/cloth/declcloth.h"

#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/token.h"

#include <cstdint>

namespace {

const idDeclClothRuntimeCallbacks* runtimeCallbacks = nullptr;

class idVersionSelector {
public:
  struct idOption {
    idStrStatic<256> str;
    bool exists{};
    bool used{};
    bool build{};
  };

  ~idVersionSelector();
  idOption options[4];
  int buildOptions[4]{};
  int numOptions{};
  int numBuildOptions{};
  int combinedMask{};
};

static_assert(sizeof(idVersionSelector::idOption) == 304,
              "idVersionSelector::idOption ABI drift");
static_assert(sizeof(idVersionSelector) == 1248,
              "idVersionSelector ABI drift");

idVersionSelector::~idVersionSelector() = default;

int JointIndex(const idDeclMD6* decl, const char* name) {
  return runtimeCallbacks != nullptr && runtimeCallbacks->getJointIndex != nullptr
             ? runtimeCallbacks->getJointIndex(decl, name)
             : -1;
}

bool InversePose(const idDeclMD6* decl, int jointIndex, idJointMat& pose) {
  return runtimeCallbacks != nullptr &&
                 runtimeCallbacks->getInverseBasePose != nullptr
             ? runtimeCallbacks->getInverseBasePose(decl, jointIndex, pose)
             : false;
}

const idDeclMD6* FindMD6(const char* name) {
  return runtimeCallbacks != nullptr && runtimeCallbacks->findMD6 != nullptr
             ? runtimeCallbacks->findMD6(name)
             : nullptr;
}

} // namespace

idDeclInfoTemplate<idDeclCloth> idDeclCloth::resourceList(
    "cloth", "idDeclCloth", "cloth", false);

void SetDeclClothRuntimeCallbacks(
    const idDeclClothRuntimeCallbacks* callbacks) {
  runtimeCallbacks = callbacks;
}

const char* idDeclCloth::DefaultDefinition() const {
  return "{\n\tmaterial\t_default\n\twidth\t\t12\n\theight\t\t12\n\thspacing\t12\n\tvspacing\t12\n}\n";
}

void idJointMat::Invert() {
  const float old3 = mat[3];
  const float old6 = mat[6];
  const float old2 = mat[2];
  const float old11 = mat[11];
  const float old1 = mat[1];
  const float old8 = mat[8];
  const float old9 = mat[9];
  const float translationX =
      old3 * mat[0] + mat[4] * mat[7] + old8 * old11;
  const float translationY =
      mat[7] * mat[5] + old1 * old3 + old9 * old11;
  const float translationZ = old2 * old3 + old6 * mat[7] + old11 * mat[10];
  mat[1] = mat[4];
  mat[4] = old1;
  mat[2] = old8;
  mat[3] = -translationX;
  mat[6] = old9;
  mat[7] = -translationY;
  mat[11] = -translationZ;
  mat[8] = old2;
  mat[9] = old6;
}

idDeclCloth::idDeclCloth() = default;
idDeclCloth::~idDeclCloth() = default;

void idDeclCloth::FreeData() { ropeData.ClearFree(); }

idDeclInfo* idDeclCloth::GetDeclInfo() const { return &resourceList; }

void idDeclCloth::Parse(idParser& src) {
  idToken token;
  if (!src.ReadToken(token))
    return;

  while (token != "}") {
    if (token.Icmp("width") == 0) {
      width = src.ParseInt();
    } else if (token.Icmp("height") == 0) {
      height = src.ParseInt();
    } else if (token.Icmp("vspacing") == 0) {
      vSpacing = src.ParseFloat();
    } else if (token.Icmp("hspacing") == 0) {
      hSpacing = src.ParseFloat();
    } else if (token.Icmp("springs") == 0) {
      src.ReadToken(token);
      if (token.Icmp("quad") == 0)
        springType = CLOTH_SPRING_QUAD;
      else if (token.Icmp("cross") == 0)
        springType = CLOTH_SPRING_CROSS;
      else
        springType = CLOTH_SPRING_BANDED;
    } else if (token.Icmp("mass") == 0) {
      mass = src.ParseFloat();
    } else if (token.Icmp("friction") == 0) {
      friction = src.ParseFloat();
    } else if (token.Icmp("collisionfriction") == 0) {
      collisionFriction = src.ParseFloat();
    } else if (token.Icmp("gravity") == 0) {
      gravity = src.ParseFloat();
    } else if (token.Icmp("springLength") == 0) {
      springLength = src.ParseFloat();
    } else if (token.Icmp("springFriction") == 0) {
      springFriction = src.ParseFloat();
    } else if (token.Icmp("material") == 0) {
      src.ReadToken(token);
      if (token == "multi") {
        material = runtimeCallbacks != nullptr &&
                           runtimeCallbacks->parseMultiMaterial != nullptr
                       ? runtimeCallbacks->parseMultiMaterial(src, name.str)
                       : nullptr;
      } else {
        material = runtimeCallbacks != nullptr &&
                           runtimeCallbacks->findMaterial != nullptr
                       ? runtimeCallbacks->findMaterial(token.data, true)
                       : nullptr;
      }
    } else if (token.Icmp("springMaxLengthMultiplier") == 0) {
      springMaxLengthMultiplier = src.ParseFloat();
    } else if (token.Icmp("bandSpringForce") == 0) {
      bandSpringForce = src.ParseFloat();
    } else if (token.Icmp("clothDeclFX") == 0) {
      src.ReadToken(token);
      clothDeclFX = runtimeCallbacks != nullptr &&
                            runtimeCallbacks->findFX != nullptr
                        ? runtimeCallbacks->findFX(token.data, true)
                        : nullptr;
    } else if (token.Icmp("generic") == 0) {
      type = CLOTH_GENERIC;
      src.ExpectTokenString("{");
      src.ReadToken(token);
      clothMD6 = FindMD6(token.data);
      if (clothMD6 == nullptr)
        idLib::Error("Unable to find md6 decl %s for %s", token.data,
                     name.str);

      const int particleCount = src.ParseInt();
      if (static_cast<unsigned>(particleCount) > 64u)
        idLib::Error("Invalid num particles for %s, -> %d", name.str,
                     particleCount);
      particleData.SetNum(particleCount);
      for (int particleIndex = 0; particleIndex < particleCount;
           ++particleIndex) {
        src.ReadToken(token);
        const int jointIndex = JointIndex(clothMD6, token.data);
        if (jointIndex < 0)
          idLib::Error("Invalid joint %s found while parsing %s", token.data,
                       name.str);

        bool anchored = false;
        particleData[particleIndex].forwardIndex = -1;
        particleData[particleIndex].rightIndex = -1;
        while (src.ReadToken(token)) {
          if (token == "}")
            break;
          if (token.Icmp("anchored") == 0)
            anchored = true;
          else if (token.Icmp("forward") == 0)
            particleData[particleIndex].forwardIndex = src.ParseInt();
          else if (token.Icmp("right") == 0)
            particleData[particleIndex].rightIndex = src.ParseInt();
        }

        idJointMat pose{};
        if (!InversePose(clothMD6, jointIndex, pose))
          idLib::Error("Invalid joint %s found while parsing %s", token.data,
                       name.str);
        pose.Invert();
        particleData[particleIndex].position =
            {pose.mat[3], pose.mat[7], pose.mat[11]};
        particleData[particleIndex].jointIndex =
            static_cast<std::int16_t>(jointIndex);
        particleData[particleIndex].anchored = anchored;
      }

      src.ExpectTokenString("springs");
      src.ExpectTokenString("{");
      const int springCount = src.ParseInt();
      springData.SetNum(springCount);
      for (int springIndex = 0; springIndex < springCount; ++springIndex) {
        const int index1 = src.ParseInt();
        const int index2 = src.ParseInt();
        if (index1 < 0 || index2 < 0 || index1 > 64 || index2 > 64)
          idLib::Error("Invalid index for spring %d in %s", springIndex,
                       name.str);
        springData[springIndex].index1 = static_cast<std::uint8_t>(index1);
        springData[springIndex].index2 = static_cast<std::uint8_t>(index2);
        if (src.ReadToken(token)) {
          while (token != "}" && src.ReadToken(token)) {
          }
        }
      }
      src.ExpectTokenString("}");
    } else if (token.Icmp("square") == 0) {
      type = CLOTH_SQUARE;
      src.ExpectTokenString("{");
      src.ReadToken(token);
      clothMD6 = FindMD6(token.data);
      if (clothMD6 == nullptr)
        idLib::Error("Unable to find md6 decl %s for %s", token.data,
                     name.str);
      width = src.ParseInt();
      height = src.ParseInt();
      if (width < 2 || height < 2 || width > 16 || height > 16)
        idLib::Error("Invalid width or height - %d x %d", width, height);
      particleData.SetNum(width * height);

      int particleIndex = 0;
      while (src.ReadToken(token) && token != "}") {
        const char* jointName = token.data;
        const int jointIndex = JointIndex(clothMD6, jointName);
        if (jointIndex < 0)
          idLib::Error("Invalid joint %s found while parsing %s", jointName,
                       name.str);
        bool anchored = false;
        while (src.ReadToken(token)) {
          if (token == "}")
            break;
          if (token.Icmp("anchored") == 0)
            anchored = true;
        }
        idJointMat pose{};
        if (!InversePose(clothMD6, jointIndex, pose))
          idLib::Error("Invalid joint %s found while parsing %s", jointName,
                       name.str);
        particleInfo_t& particle = particleData[particleIndex++];
        particle.position = {pose.mat[3], pose.mat[7], pose.mat[11]};
        particle.jointIndex = static_cast<std::int16_t>(jointIndex);
        particle.anchored = anchored;
        particle.rightIndex = -1;
        particle.forwardIndex = -1;
      }
    } else if (token.Icmp("ropeModel") == 0) {
      type = CLOTH_ROPE;
      src.ExpectTokenString("{");
      src.ReadToken(token);
      clothMD6 = FindMD6(token.data);
      if (clothMD6 == nullptr)
        idLib::Error("Unable to find rope decl %s", token.data);

      while (src.ReadToken(token) && token != "}") {
        const char* jointName = token.data;
        int jointIndex = JointIndex(clothMD6, jointName);
        if (jointIndex < 0)
          idLib::Error("Invalid joint index %s", jointName);
        if (!src.ReadToken(token) || token.type != TT_NUMBER)
          idLib::Error("Invalid number of particles %d", 0);
        if ((token.subtype & TT_VALUESVALID) == 0)
          token.NumberValue();
        const int particleCount = static_cast<int>(token.intvalue);
        if (particleCount < 2 || particleCount > 100)
          idLib::Error("Invalid number of particles %d", particleCount);

        ropeInfo* rope = ropeData.Alloc();
        for (int particleIndex = 0; particleIndex < particleCount;
             ++particleIndex) {
          if (jointIndex < 0)
            idLib::Error("Invalid joint found while parsing rope for %s",
                         name.str);
          idJointMat pose{};
          if (!InversePose(clothMD6, jointIndex, pose))
            idLib::Error("Invalid joint found while parsing rope for %s",
                         name.str);
          pose.Invert();
          idVec3* position = rope->positions.Alloc();
          *position = {pose.mat[3], pose.mat[7], pose.mat[11]};
          idMat3* inverseAxis = rope->inverseAxis.Alloc();
          inverseAxis->mat[0] = {pose.mat[0], pose.mat[4], pose.mat[8]};
          inverseAxis->mat[1] = {pose.mat[1], pose.mat[5], pose.mat[9]};
          inverseAxis->mat[2] = {pose.mat[2], pose.mat[6], pose.mat[10]};
          rope->indexOfJointToAttachTo.Append(
              static_cast<std::int16_t>(jointIndex));
          jointIndex = runtimeCallbacks != nullptr &&
                               runtimeCallbacks->getParentJointIndex != nullptr
                           ? runtimeCallbacks->getParentJointIndex(clothMD6,
                                                                   jointIndex)
                           : -1;
        }
      }
    } else {
      src.Warning("bad token %s", token.data);
      return;
    }

    if (!src.ReadToken(token))
      return;
  }
}

// Recovered explicit header/template records owned by DeclCloth.obj.
template bool idList<idDeclCloth::springInfo_t, 5>::SetNum(int);
template bool idList<idDeclCloth::particleInfo_t, 5>::Resize(int);
template idMat3* idList<idMat3, 5>::Alloc();
template bool idList<idDeclCloth::ropeInfo, 5>::Resize(int);
template idList<idDeclCloth::ropeInfo, 5>::~idList();
template idDeclCloth::ropeInfo* idList<idDeclCloth::ropeInfo, 5>::Alloc();

// The constructor's three cleanup funclets, destructor's four cleanup
// funclets, and Parse's two cleanup funclets are represented by normal C++
// base/member/local RAII. The scalar deleting destructor is compiler-owned.
