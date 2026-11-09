#include "engine/models/skeletalanimation/md6util.h"

namespace {
idMat3 JointAxis(const idJointMat& joint) {
  idMat3 result;
  result.mat[0] = {joint.mat[0], joint.mat[4], joint.mat[8]};
  result.mat[1] = {joint.mat[1], joint.mat[5], joint.mat[9]};
  result.mat[2] = {joint.mat[2], joint.mat[6], joint.mat[10]};
  return result;
}
idVec3 JointTranslation(const idJointMat& joint) {
  return {joint.mat[3], joint.mat[7], joint.mat[11]};
}
} // namespace

extern idMD6UtilAnimWebLookup g_md6UtilAnimWebLookup;
extern idMD6UtilDeclLookup g_md6UtilDeclLookup;
extern idMD6UtilDecodeFrame g_md6UtilDecodeFrame;
extern int g_md6UtilGameTimePerSecond;

const idMD6Anim* idMD6Util::FindMD6Anim(
    const idDeclAnimWeb* webRef, const idAnimWebPath& webPath,
    const idDeclMD6* declMD6, idMD6UtilModelIndex modelIndex, bool loadAnim) {
  idStr animWebName;
  idStr subWebName;
  idStr stateName;
  webPath.GetWebPathNames(animWebName, subWebName, stateName);
  if (!webRef && g_md6UtilAnimWebLookup)
    webRef = g_md6UtilAnimWebLookup(animWebName.data);
  if (!webRef) return nullptr;
  if (!declMD6 && webRef->models.num > 0) declMD6 = webRef->models.list[0];
  const animWebNodeIndex_t nodeIndex = webRef->GetNodeIndexForName(
      subWebName.data, stateName.data);
  if (nodeIndex.value < 0 || modelIndex.value < 0) return nullptr;
  const idDeclAnimWebNode* node = webRef->nodes.list[nodeIndex.value];
  if (modelIndex.value >= node->modelInfo.num) return nullptr;
  const idDeclAnimWebNode::idModelInfo& info = node->modelInfo.list[modelIndex.value];
  if (info.animProps.num <= 0 || !info.animProps.list) return nullptr;
  return loadAnim ? info.animProps.list[0].SelectAndLoadAnim(declMD6, 0)
                  : info.animProps.list[0].SelectAnim(declMD6, 0);
}

bool idMD6Util::idJointCache::GetDelta(animDelta_t* delta,
                                       const char* jointName,
                                       const char* refJointName) {
  if (!validJointCache) return false;
  const idDeclMD6* declaration = md6Decl;
  if (webRef) {
    if (webRef->models.num <= 0) return false;
    declaration = webRef->models.list[0];
  }
  if (!declaration || !declaration->model || !declaration->model->skeleton)
    return false;
  const idMD6Skel& skeleton = *declaration->model->skeleton;
  const int joint = skeleton.GetJointIndex(jointName).value;
  const int reference = skeleton.GetJointIndex(refJointName).value;
  if (joint < 0 || reference < 0) return false;
  const idMat3 jointAxis = JointAxis(jointMatrices[joint]);
  if (joint == reference) {
    delta->deltaTranslation = JointTranslation(jointMatrices[joint]);
    delta->deltaAxis = jointAxis;
  } else {
    delta->deltaTranslation = JointTranslation(jointMatrices[joint]) -
                              JointTranslation(jointMatrices[reference]);
    const idAngles refAngles = JointAxis(jointMatrices[reference]).ToAngles();
    const idAngles angles = jointAxis.ToAngles();
    delta->deltaAxis = idAngles{angles.pitch - refAngles.pitch,
                                angles.yaw - refAngles.yaw,
                                angles.roll - refAngles.roll}.ToMat3();
  }
  return true;
}

void idMD6Util::idJointCache::UpdateCache() {
  validJointCache = false;
  const idDeclMD6* declaration = md6Decl;
  if (webRef) {
    if (webRef->models.num <= 0) return;
    declaration = webRef->models.list[0];
  }
  if (!declaration || !declaration->model || !md6anim || !g_md6UtilDecodeFrame)
    return;
  const int time = g_md6UtilGameTimePerSecond * frameNum /
                   md6anim->animData->frameRate;
  g_md6UtilDecodeFrame(declaration->model, md6anim, time,
                       g_md6UtilGameTimePerSecond, jointMatrices, 256);
  validJointCache = true;
}

void idMD6Util::idJointCache::Set(const idDeclAnimWeb* newWebRef,
                                  const idAnimWebPath& webPath,
                                  int requestedFrame) {
  validJointCache = false;
  webRef = newWebRef;
  const idDeclMD6* declaration = nullptr;
  if (newWebRef) {
    if (newWebRef->editorModels.num > 0 && g_md6UtilDeclLookup)
      declaration = g_md6UtilDeclLookup(newWebRef->editorModels.list[0].str);
    if (!declaration) declaration = newWebRef->models.list[0];
  }
  md6Decl = declaration;
  md6anim = idMD6Util::FindMD6Anim(newWebRef, webPath, declaration,
                                   idMD6UtilModelIndex(0), false);
  if (!md6anim) return;
  frameNum = requestedFrame == -1 ? md6anim->animData->numFrames - 1
                                  : requestedFrame;
  validJointCache = false;
  UpdateCache();
}

idMD6Util::idJointCache::idJointCache(const idAnimWebPath& webPath, int frame)
    : webRef(nullptr), md6Decl(nullptr), md6anim(nullptr), frameNum(0),
      validJointCache(false) {
  idStr webName;
  webPath.GetWebName(webName);
  const idDeclAnimWeb* declaration = g_md6UtilAnimWebLookup
                                         ? g_md6UtilAnimWebLookup(webName.data)
                                         : nullptr;
  webRef = declaration;
  Set(declaration, webPath, frame);
}
