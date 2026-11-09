#include "declanimweb.h"
#include "engine/animation/animationpreload.h"
#include "shared/idlib/lib.h"

#include <algorithm>

namespace {

constexpr const char* kBlendTypeNames[] = {
    "BLEND_LINEAR", "BLEND_EASEIN", "BLEND_EASEOUT", "BLEND_EASEIN_EASEOUT"};
constexpr const char* kSubWebBlendNames[] = {"sourceNode", "blendOutAnywhere"};
constexpr const char* kNodePropNames[] = {
    "showAnimName", "showWrapMode", "showRate", "showDelta", "showBlendEq", "showTransition"};

int BlendTypeForName(const char* name) {
  for (int i = 0; i < 4; ++i)
    if (idStr::Cmp(kBlendTypeNames[i], name) == 0) return i;
  return -1;
}

int SubWebBlendForName(const char* name) {
  for (int i = 0; i < 2; ++i)
    if (idStr::Icmp(kSubWebBlendNames[i], name) == 0) return i;
  return -1;
}

int NextPowerOfTwo(int value) {
  int result = 1;
  while (result < value && result <= 0x40000000) result <<= 1;
  return result;
}

int FindModel(const idList<const idDeclMD6*,32>& models,const idDeclMD6* model) {
  for (int i = 0; i < models.Num(); ++i) if (models[i] == model) return i;
  return -1;
}

int FindModelByName(const idDeclAnimWeb& web,const char* name) {
  for (int i = 0; i < web.models.Num(); ++i)
    if (web.models[i] && idStr::Icmp(web.models[i]->name.str,name) == 0) return i;
  return -1;
}

animWebStateIndex_t AddState(idDeclAnimWeb& web,const char* name) {
  animWebStateIndex_t index = web.GetStateIndexForName(name);
  if (index.value < 0) {
    idDeclAnimWeb::idStateInfo info;
    info.name.Set(name);
    index = animWebStateIndex_t(web.states.Append(info));
    ++web.changeId;
  }
  return index;
}

idDeclAnimWeb::stateList_t MapStates(const idDeclAnimWeb& source,
                                     const idDeclAnimWebNode& node,
                                     idDeclAnimWeb& output) {
  idDeclAnimWeb::stateList_t result;
  for (int i = 0; i < node.states.numStates && result.numStates < 4; ++i)
    result.stateIndex[result.numStates++] = AddState(output,source.GetStateName(node.states.stateIndex[i]));
  return result;
}

idDeclAnimWeb::stateList_t MapStates(const idDeclAnimSubWeb& source,
                                     const idDeclAnimWebNode& node,
                                     idDeclAnimWeb& output) {
  idDeclAnimWeb::stateList_t result;
  for (int i = 0; i < node.states.numStates && result.numStates < 4; ++i) {
    const int state = node.states.stateIndex[i].value;
    result.stateIndex[result.numStates++] = AddState(output,
        state >= 0 && state < source.states.Num() ? source.states[state].str : "");
  }
  return result;
}

animWebNodeIndex_t FindExactNode(const idDeclAnimWeb& web,animWebSubWebIndex_t subWeb,
                                 const idDeclAnimWeb::stateList_t& states) {
  if (subWeb.value < 0 || subWeb.value >= web.subWebs.Num()) return animWebNodeIndex_t();
  for (int i = 0; i < web.subWebs[subWeb.value].nodes.Num(); ++i) {
    const animWebNodeIndex_t index = web.subWebs[subWeb.value].nodes[i];
    const idDeclAnimWebNode* node = web.nodes[index.value];
    if (node->states.numStates == states.numStates && node->MatchStates(states) == states.numStates)
      return index;
  }
  return animWebNodeIndex_t();
}

animWebNodeIndex_t AppendNode(idDeclAnimWeb& web,animWebSubWebIndex_t subWeb,
                              const idDeclAnimWeb::stateList_t& states) {
  const animWebNodeIndex_t index(web.nodes.Num());
  web.nodes.Append(new idDeclAnimWebNode(index,subWeb,states));
  web.subWebs[subWeb.value].nodes.Append(index);
  ++web.changeId;
  return index;
}

void CopyNodeProperties(idDeclAnimWebNode& destination,const idDeclAnimWebNode& source) {
  destination.animDelta = source.animDelta;
  destination.flags = source.flags;
  destination.customFlags = source.customFlags;
  destination.settings = source.settings;
}

bool HasEdge(const idDeclAnimWeb& web,const idDeclAnimWebNode& node,
             animWebSubWebIndex_t subWeb,animWebNodeIndex_t destination) {
  for (int i = 0; i < node.edges.Num(); ++i) {
    const int edge = node.edges[i];
    if (edge >= 0 && edge < web.edges.Num() &&
        web.edges[edge].destSubWebIndex.value == subWeb.value &&
        web.edgeCache[edge].destNodeIndex.value == destination.value) return true;
  }
  return false;
}

void AppendMappedEdge(idDeclAnimWeb& output,idDeclAnimWebNode& outputNode,
                      const idDeclAnimWebEdge& sourceEdge,const idAnimWebEdgeCache& sourceCache,
                      animWebSubWebIndex_t destinationSubWeb,animWebNodeIndex_t destinationNode) {
  if (HasEdge(output,outputNode,destinationSubWeb,destinationNode)) return;
  idDeclAnimWebEdge edge(sourceEdge);
  idAnimWebEdgeCache cache(sourceCache);
  edge.destSubWebIndex = destinationSubWeb;
  cache.destNodeIndex = destinationNode;
  const animWebEdgeIndex_t index = output.AllocEdge();
  output.edges[index.value] = edge;
  output.edgeCache[index.value] = cache;
  outputNode.edges.Append(index.value);
}

void FinalizeAnimWeb(idDeclAnimWeb& web) {
  if (web.expandSubWebLinks)
    for (int i = 0; i < web.nodes.Num(); ++i) web.nodes[i]->CreateSubWebEdges(web);

  delete[] web.edgeIndexCache;
  web.edgeIndexCacheSize = 2u * static_cast<unsigned>(web.edgeCache.Num());
  web.edgeIndexCache = web.edgeCache.Num() ? new std::int16_t[web.edgeCache.Num()] : nullptr;
  web.nodeCache.SetNum(web.nodes.Num());
  unsigned offset = 0;
  for (int i = 0; i < web.nodes.Num(); ++i) {
    web.nodeCache[i].numEdges = static_cast<std::uint16_t>(web.nodes[i]->edges.Num());
    web.nodeCache[i].edgeIndexOffset = offset;
    web.nodeCache[i].flags = web.nodes[i]->flags;
    for (int e = 0; e < web.nodes[i]->edges.Num(); ++e)
      web.edgeIndexCache[offset++] = web.nodes[i]->edges[e];
  }

  web.nodes.ResizeForPort(web.nodes.Num());
  web.layers.ResizeForPort(web.layers.Num());
  web.subWebs.ResizeForPort(web.subWebs.Num());
  web.states.ResizeForPort(web.states.Num());
  web.edges.ResizeForPort(web.edges.Num());
  web.edgeCache.ResizeForPort(web.edgeCache.Num());
  web.nodeCache.ResizeForPort(web.nodeCache.Num());
  web.models.ResizeForPort(web.models.Num());
  web.editorModels.ResizeForPort(web.editorModels.Num());
  web.subWebDecls.ResizeForPort(web.subWebDecls.Num());
  web.SetupHashes();
}

} // namespace

idDeclInfoTemplate<idDeclAnimWeb> idDeclAnimWeb::resourceList("animWeb","idDeclAnimWeb","aweb",false);

idDeclAnimWeb::idDeclAnimWeb()
    : edgeIndexCache(nullptr),edgeIndexCacheSize(0),changeId(0),expandSubWebLinks(true),hashChangeId(-1) {
  resourceListPtr = &resourceList;
}

idDeclAnimWeb::~idDeclAnimWeb() { FreeData(); }
const char* idDeclAnimWeb::DefaultDefinition() const { return "{\n\tprops {\n\t}\nstates {\n\t}\n\tsubWebs {\n\t}\n}\n"; }
idDeclInfo* idDeclAnimWeb::GetDeclInfo() const { return &resourceList; }

void idDeclAnimWeb::FreeData() {
  for (int i = 0; i < nodes.Num(); ++i) delete nodes[i];
  nodes.ClearFree(); layers.ClearFree(); subWebs.ClearFree(); states.ClearFree(); models.ClearFree();
  editorModels.ClearFree(); edges.ClearFree(); edgeCache.ClearFree(); nodeCache.ClearFree();
  scalars.ClearFree(); subWebDecls.ClearFree();
  delete[] edgeIndexCache; edgeIndexCache = nullptr; edgeIndexCacheSize = 0;
  ++changeId;
}

void idDeclAnimWeb::SetupHashes() {
  hashChangeId = changeId;
  const int stateHashSize = NextPowerOfTwo(std::max(1,states.Num()/4));
  stateHash.InternalFree(); stateHash.InternalInit(stateHashSize,std::max(1,states.Num()));
  for (int i = 0; i < states.Num(); ++i) stateHash.Add(idStr::IHash(states[i].name.str),i);
  const int subWebHashSize = NextPowerOfTwo(std::max(1,subWebs.Num()/4));
  subWebHash.InternalFree(); subWebHash.InternalInit(subWebHashSize,std::max(1,subWebs.Num()));
  for (int i = 0; i < subWebs.Num(); ++i) subWebHash.Add(idStr::IHash(subWebs[i].name.str),i);
}

const char* idDeclAnimWeb::GetStateName(animWebStateIndex_t index) const {
  return index.value >= 0 && index.value < states.Num() ? states[index.value].name.str : "";
}
idAtomicString idDeclAnimWeb::GetStateNameAtomic(animWebStateIndex_t index) const { return idAtomicString(GetStateName(index)); }

animWebStateIndex_t idDeclAnimWeb::GetStateIndexForName(const char* stateName) const {
  if (!stateName) { idLib::Warning("idDeclAnimWeb::GetStateIndexForName() - NULL state name passed to animWeb '%s'",name.str); return animWebStateIndex_t(); }
  if (hashChangeId == changeId)
    for (int i = stateHash.First(idStr::IHash(stateName)); i >= 0; i = stateHash.Next(i))
      if (idStr::Icmp(states[i].name.str,stateName) == 0) return animWebStateIndex_t(i);
  for (int i = 0; i < states.Num(); ++i)
    if (idStr::Icmp(states[i].name.str,stateName) == 0) return animWebStateIndex_t(i);
  return animWebStateIndex_t();
}

animWebSubWebIndex_t idDeclAnimWeb::GetSubWebIndexForName(const char* subWebName) const {
  if (!subWebName || !*subWebName) return animWebSubWebIndex_t();
  if (hashChangeId == changeId)
    for (int i = subWebHash.First(idStr::IHash(subWebName)); i >= 0; i = subWebHash.Next(i))
      if (idStr::Icmp(subWebs[i].name.str,subWebName) == 0) return animWebSubWebIndex_t(i);
  for (int i = 0; i < subWebs.Num(); ++i)
    if (idStr::Icmp(subWebs[i].name.str,subWebName) == 0) return animWebSubWebIndex_t(i);
  return animWebSubWebIndex_t();
}

const char* idDeclAnimWeb::GetSubWebName(animWebSubWebIndex_t index) const {
  return index.value >= 0 && index.value < subWebs.Num() ? subWebs[index.value].name.str : "";
}
idAtomicString idDeclAnimWeb::GetSubWebNameAtomic(animWebSubWebIndex_t index) const { return idAtomicString(GetSubWebName(index)); }

animWebNodeIndex_t idDeclAnimWeb::FindNodeIndex(animWebSubWebIndex_t subWeb,const stateList_t& list,bool exact) const {
  if (subWeb.value < 0 || subWeb.value >= subWebs.Num()) return animWebNodeIndex_t();
  int best = -1, score = 0;
  for (int i = 0; i < subWebs[subWeb.value].nodes.Num(); ++i) {
    const int nodeIndex = subWebs[subWeb.value].nodes[i].value;
    const int matched = nodes[nodeIndex]->MatchStates(list);
    if (matched == list.numStates) return animWebNodeIndex_t(nodeIndex);
    if (matched > score) { score = matched; best = nodeIndex; }
  }
  return exact ? animWebNodeIndex_t() : animWebNodeIndex_t(best);
}

animWebNodeIndex_t idDeclAnimWeb::FindNodeIndex(animWebSubWebIndex_t subWeb,animWebStateIndex_t state) const {
  if (subWeb.value < 0 || subWeb.value >= subWebs.Num() || state.value < 0) return animWebNodeIndex_t();
  for (int i = 0; i < subWebs[subWeb.value].nodes.Num(); ++i) {
    idDeclAnimWebNode* node = nodes[subWebs[subWeb.value].nodes[i].value];
    if (node->states.numStates == 1 && node->states.stateIndex[0].value == state.value) return node->nodeIndex;
  }
  return animWebNodeIndex_t();
}

animWebModelIndex_t idDeclAnimWeb::FindModelIndex(const idDeclMD6* model) const { return animWebModelIndex_t(FindModel(models,model)); }
idList<idAnimWebScalar,32>& idDeclAnimWeb::GetScalars() { return scalars; }
animWebNodeIndex_t idDeclAnimWeb::GetNodeIndexForName(const char* subWeb,const char* state) const { return FindNodeIndex(GetSubWebIndexForName(subWeb),GetStateIndexForName(state)); }

animWebNodeIndex_t idDeclAnimWeb::FindNodeIndices(animWebSubWebIndex_t subWeb,const stateList_t& list,idList<animWebNodeIndex_t>& result) const {
  if (subWeb.value < 0 || subWeb.value >= subWebs.Num()) return animWebNodeIndex_t();
  for (int i = 0; i < subWebs[subWeb.value].nodes.Num(); ++i) {
    const animWebNodeIndex_t node = subWebs[subWeb.value].nodes[i];
    if (nodes[node.value]->MatchStates(list) == list.numStates) result.Append(node);
  }
  return result.Num() ? result[0] : animWebNodeIndex_t();
}

animWebNodeIndex_t idDeclAnimWeb::AddNode(animWebSubWebIndex_t subWeb,const stateList_t& list) {
  if (subWeb.value >= 0 && subWeb.value < subWebs.Num())
    for (int i = 0; i < subWebs[subWeb.value].nodes.Num(); ++i) {
      idDeclAnimWebNode* node = nodes[subWebs[subWeb.value].nodes[i].value];
      if (node->MatchStates(list) == list.numStates) return node->nodeIndex;
    }
  return AppendNode(*this,subWeb,list);
}

void idDeclAnimWeb::AddNode(const char* subWebName,const idStaticList<idStr,4>& stateNames,
                            animWebNodeIndex_t& nodeIndex,animWebSubWebIndex_t& subWebIndex,
                            stateList_t& stateIndices) {
  subWebIndex = AddSubWeb(subWebName);
  for (int i = 0; i < stateNames.Num() && stateIndices.numStates < 4; ++i)
    stateIndices.stateIndex[stateIndices.numStates++] = AddState(*this,stateNames[i].data);
  nodeIndex = AddNode(subWebIndex,stateIndices);
  ++changeId;
}

animWebSubWebIndex_t idDeclAnimWeb::AddSubWeb(const char* subWebName,bool hub) {
  animWebSubWebIndex_t existing = GetSubWebIndexForName(subWebName);
  if (existing.value >= 0) { subWebs[existing.value].isHub |= hub; return existing; }
  idSubWebInfo info; info.name.Set(subWebName); info.isHub = hub;
  const animWebSubWebIndex_t result(subWebs.Append(info)); ++changeId; return result;
}

animWebEdgeIndex_t idDeclAnimWeb::AllocEdge() {
  const animWebEdgeIndex_t index(edges.Num()); edges.Alloc(); edgeCache.Alloc(); return index;
}

void idDeclAnimWeb::Composite(const idList<const idDeclAnimWeb*,5>& mergeDecls,idDeclAnimWeb& output) {
  for (int d = 0; d < mergeDecls.Num(); ++d) {
    const idDeclAnimWeb& source = *mergeDecls[d];
    idList<animWebModelIndex_t,5> modelMap; modelMap.SetNum(source.models.Num(),animWebModelIndex_t());
    for (int m = 0; m < source.models.Num(); ++m) {
      int mapped = output.settings.mergeModels ? FindModel(output.models,source.models[m]) : m;
      if (output.settings.mergeModels && mapped < 0) mapped = output.models.Append(source.models[m]);
      modelMap[m] = animWebModelIndex_t(mapped);
    }
    for (int s = 0; s < source.states.Num(); ++s) AddState(output,source.states[s].name.str);
    for (int s = 0; s < source.subWebs.Num(); ++s) {
      animWebSubWebIndex_t mapped = output.GetSubWebIndexForName(source.subWebs[s].name.str);
      if (mapped.value < 0) {
        idSubWebInfo info = source.subWebs[s]; info.nodes.ClearFree();
        output.subWebs.Append(info); ++output.changeId;
      } else {
        output.subWebs[mapped.value].isHub |= source.subWebs[s].isHub;
        output.subWebs[mapped.value].isVisible |= source.subWebs[s].isVisible;
        output.subWebs[mapped.value].used |= source.subWebs[s].used;
      }
    }

    idList<animWebNodeIndex_t,5> nodeMap; nodeMap.SetNum(source.nodes.Num(),animWebNodeIndex_t());
    for (int n = 0; n < source.nodes.Num(); ++n) {
      const idDeclAnimWebNode& sourceNode = *source.nodes[n];
      const animWebSubWebIndex_t subWeb = output.GetSubWebIndexForName(source.GetSubWebName(sourceNode.subWebIndex));
      const stateList_t states = MapStates(source,sourceNode,output);
      animWebNodeIndex_t destinationIndex = FindExactNode(output,subWeb,states);
      const bool added = destinationIndex.value < 0;
      if (added) destinationIndex = AppendNode(output,subWeb,states);
      nodeMap[n] = destinationIndex;
      idDeclAnimWebNode& destination = *output.nodes[destinationIndex.value];
      for (int m = 0; m < sourceNode.modelInfo.Num(); ++m) {
        const idDeclAnimWebNode::idModelInfo& info = sourceNode.modelInfo[m];
        const int sourceModel = info.modelIndex.value;
        const animWebModelIndex_t model = sourceModel >= 0 && sourceModel < modelMap.Num()
            ? modelMap[sourceModel] : animWebModelIndex_t();
        if (!destination.FindModelInfo(model)) destination.AddModelInfo(info,model);
      }
      if (added) CopyNodeProperties(destination,sourceNode);
    }

    for (int n = 0; n < source.nodes.Num(); ++n) {
      idDeclAnimWebNode& destination = *output.nodes[nodeMap[n].value];
      const idDeclAnimWebNode& sourceNode = *source.nodes[n];
      for (int e = 0; e < sourceNode.edges.Num(); ++e) {
        const int edgeIndex = sourceNode.edges[e];
        const idDeclAnimWebEdge& edge = source.edges[edgeIndex];
        const idAnimWebEdgeCache& cache = source.edgeCache[edgeIndex];
        const animWebSubWebIndex_t subWeb = output.GetSubWebIndexForName(source.GetSubWebName(edge.destSubWebIndex));
        animWebNodeIndex_t node;
        if (cache.destNodeIndex.value >= 0 && cache.destNodeIndex.value < nodeMap.Num()) node = nodeMap[cache.destNodeIndex.value];
        AppendMappedEdge(output,destination,edge,cache,subWeb,node);
      }
    }
  }
}

void idDeclAnimWeb::MergeSubWebs() {
  for (int d = 0; d < subWebDecls.Num(); ++d)
    for (int s = 0; s < subWebDecls[d]->subWebs.Num(); ++s) AddSubWeb(subWebDecls[d]->subWebs[s].name.str);

  for (int d = 0; d < subWebDecls.Num(); ++d) {
    const idDeclAnimSubWeb& source = *subWebDecls[d];
    if (source.subWebs.Num()) {
      const animWebSubWebIndex_t primary = GetSubWebIndexForName(source.subWebs[0].name.str);
      subWebs[primary.value].color = source.color;
      subWebs[primary.value].isHub = source.isHub;
      subWebs[primary.value].isVisible = source.isVisible;
    }
    idList<animWebNodeIndex_t,5> nodeMap; nodeMap.SetNum(source.nodes.Num(),animWebNodeIndex_t());
    for (int n = 0; n < source.nodes.Num(); ++n) {
      const idDeclAnimWebNode& sourceNode = *source.nodes[n];
      const animWebSubWebIndex_t subWeb = GetSubWebIndexForName(source.subWebs[sourceNode.subWebIndex.value].name.str);
      const stateList_t mappedStates = MapStates(source,sourceNode,*this);
      animWebNodeIndex_t destinationIndex = FindExactNode(*this,subWeb,mappedStates);
      const bool added = destinationIndex.value < 0;
      if (added) destinationIndex = AppendNode(*this,subWeb,mappedStates);
      nodeMap[n] = destinationIndex;
      idDeclAnimWebNode& destination = *nodes[destinationIndex.value];
      for (int m = 0; m < sourceNode.modelInfo.Num(); ++m) {
        const idDeclAnimWebNode::idModelInfo& info = sourceNode.modelInfo[m];
        int model = -1;
        if (info.modelIndex.value >= 0 && info.modelIndex.value < source.models.Num())
          model = FindModelByName(*this,source.models[info.modelIndex.value].str);
        const animWebModelIndex_t mappedModel(model);
        if (!destination.FindModelInfo(mappedModel)) destination.AddModelInfo(info,mappedModel);
      }
      if (added) CopyNodeProperties(destination,sourceNode);
      else {
        if (sourceNode.animDelta) destination.animDelta = sourceNode.animDelta;
        if (sourceNode.settings.pos.x != 0.0f || sourceNode.settings.pos.y != 0.0f || sourceNode.settings.pos.z != 0.0f)
          destination.settings = sourceNode.settings;
        if (sourceNode.flags) destination.flags = sourceNode.flags;
        if (sourceNode.customFlags) destination.customFlags = sourceNode.customFlags;
      }
    }

    for (int n = 0; n < source.nodes.Num(); ++n) {
      idDeclAnimWebNode& destination = *nodes[nodeMap[n].value];
      const idDeclAnimWebNode& sourceNode = *source.nodes[n];
      for (int e = 0; e < sourceNode.edges.Num(); ++e) {
        const int edgeIndex = sourceNode.edges[e];
        const idDeclAnimWebEdge& edge = source.edges[edgeIndex];
        const idAnimWebEdgeCache& cache = source.edgeCache[edgeIndex];
        animWebSubWebIndex_t destinationSubWeb;
        if (edge.destSubWebIndex.value >= 0 && edge.destSubWebIndex.value < source.subWebs.Num())
          destinationSubWeb = GetSubWebIndexForName(source.subWebs[edge.destSubWebIndex.value].name.str);
        animWebNodeIndex_t destinationNode;
        if (cache.destNodeIndex.value >= 0 && cache.destNodeIndex.value < nodeMap.Num()) destinationNode = nodeMap[cache.destNodeIndex.value];
        AppendMappedEdge(*this,destination,edge,cache,destinationSubWeb,destinationNode);
      }
    }
  }
}

std::uint64_t idDeclAnimWeb::Size() const {
  std::uint64_t bytes = 504;
  for (int i = 0; i < nodes.Num(); ++i) bytes += nodes[i]->Size();
  for (int i = 0; i < subWebs.Num(); ++i) bytes += 2ULL*subWebs[i].nodes.size + 56ULL;
  bytes += 8ULL*states.Num() + edgeIndexCacheSize;
  bytes += 4ULL*(edgeCache.size + 2ULL*(models.size + nodeCache.size + 7ULL*edges.size));
  return bytes;
}

void idDeclAnimWeb::ParseSubWeb(idParser& source) {
  idToken token;
  while (source.CheckTokenString("subWeb")) {
    source.ExpectTokenType(TT_STRING,0,token);
    const animWebSubWebIndex_t subWeb = AddSubWeb(token.data);
    source.ExpectTokenString("{");
    while (source.ReadToken(token) && token != "}") {
      if (token == "props") {
        source.ExpectTokenString("{");
        while (source.ReadToken(token) && token != "}") {
          if (token == "color") source.Parse1DMatrix(4,&subWebs[subWeb.value].color.x);
          else if (token == "hub") subWebs[subWeb.value].isHub = source.ParseBool();
          else if (token == "visible") subWebs[subWeb.value].isVisible = source.ParseBool();
          else source.Error("Unexpected token '%s' in subWeb props",token.data);
        }
      } else if (token == "node") {
        idStaticList<idStr,4> stateNames;
        while (source.ReadToken(token) && token != "{") {
          if (stateNames.Num() >= 4) source.Error("Node contains too many states.");
          else stateNames.Append(idStr(token.data));
        }
        animWebNodeIndex_t nodeIndex; animWebSubWebIndex_t nodeSubWeb; stateList_t stateIndices;
        AddNode(subWebs[subWeb.value].name.str,stateNames,nodeIndex,nodeSubWeb,stateIndices);
        nodes[nodeIndex.value]->Parse(*this,source);
      } else source.Error("Unknown token '%s' in subWeb block",token.data);
    }
  }
}

void idDeclAnimWeb::Parse(idParser& source) {
  FreeData();
  ++changeId;
  const int oldFlags = source.GetFlags();
  source.SetFlags(oldFlags | 0x40000);
  idToken token;
  if (source.PeekTokenString("{")) source.ExpectTokenString("{");
  if (source.PeekTokenString("}")) { source.ExpectTokenString("}"); FinalizeAnimWeb(*this); source.SetFlags(oldFlags); return; }

  idList<const idDeclAnimWeb*,5> mergeDecls;
  source.ExpectTokenString("props"); source.ExpectTokenString("{");
  while (source.ReadToken(token) && token != "}") {
    if (token == "editPos") source.Parse1DMatrix(3,&settings.editPos.x);
    else if (token == "gridSize") settings.gridSize = source.ParseInt();
    else if (token == "showGrid") settings.showGrid = source.ParseBool();
    else if (token == "alwaysShowIntraSubwebLinks") settings.alwaysShowIntraSubwebLinks = source.ParseBool();
    else if (token == "snapToGrid") settings.snapToGrid = source.ParseBool();
    else if (token == "alwaysShowNodeProps") settings.alwaysShowNodeProps = source.ParseBool();
    else if (token == "showAbbreviatedNodeProps") settings.showAbbreviatedNodeProps = source.ParseBool();
    else if (token == "showIncomingLinks") settings.showIncomingLinks = source.ParseBool();
    else if (token == "defaultBlendOutWindow") settings.defaultBlendOutWindow = source.ParseInt();
    else if (token == "defaultBlendDuration") settings.defaultBlendDuration = source.ParseInt();
    else if (token == "defaultBlendType") {
      source.ExpectTokenType(TT_NAME,0,token); settings.defaultBlendType = BlendTypeForName(token.data);
      if (settings.defaultBlendType < 0) source.Error("Unknown default blend type '%s'",token.data);
    } else if (token == "subWebBlend") {
      source.ExpectTokenType(TT_STRING,0,token); settings.subWebBlend = SubWebBlendForName(token.data);
      if (settings.subWebBlend < 0) source.Error("Unknown subWebBlend type: '%s'",token.data);
    } else if (token == "mergeModels") settings.mergeModels = source.ParseBool();
    else if (token == "models") {
      source.ExpectTokenString("{");
      while (source.ReadToken(token) && token != "}") {
        if (token.type != TT_STRING) { source.Error("Unknown token '%s' in 'models' block.",token.data); continue; }
        const idDeclMD6* model = AnimationPreloadService() ? AnimationPreloadService()->ResolveModel(token.data) : nullptr;
        if (model) models.Append(model); else source.Error("Could not find model '%s' in 'models' block.",token.data);
      }
    } else if (token == "editorModels") {
      source.ExpectTokenString("{");
      while (source.ReadToken(token) && token != "}") {
        if (token.type == TT_STRING) editorModels.Append(idAtomicString(token.data));
        else source.Error("Unknown token '%s' in 'editorModels' block.",token.data);
      }
    } else if (token == "composite") {
      source.ExpectTokenString("{");
      while (source.ReadToken(token) && token != "}") {
        const idDeclAnimWeb* decl = static_cast<const idDeclAnimWeb*>(resourceList.FindWithInheritance(token.data,false,true));
        if (decl) mergeDecls.Append(decl); else source.Error("Could not find animweb decl: '%s'",token.data);
      }
      settings.isComposite = true;
    } else {
      int nodeProp = -1;
      for (int i = 0; i < 6; ++i) if (token == kNodePropNames[i]) { nodeProp = i; break; }
      if (nodeProp >= 0) settings.nodeProps[nodeProp] = source.ParseBool();
      else if (token == "showWeakRefs") source.ParseBool();
      else source.Error("Unknown token '%s' in 'props' block.",token.data);
    }
  }

  if (settings.isComposite) {
    Composite(mergeDecls,*this);
  } else {
    if (source.CheckTokenString("layers")) {
      source.ExpectTokenString("{");
      while (source.ReadToken(token) && token != "}") {
        if (token != "layer") { source.Error("Unknown token '%s' in 'layers' block.",token.data); continue; }
        idLayerInfo& layer = *layers.Alloc();
        source.ExpectTokenType(TT_STRING,0,token); layer.name.Set(token.data); source.ExpectTokenString("{");
        while (source.ReadToken(token) && token != "}") {
          if (token == "visible") layer.isVisible = source.ParseBool();
          else if (token == "subwebs") {
            source.ExpectTokenString("{");
            while (source.ReadToken(token) && token != "}") {
              if (token.type == TT_STRING) layer.subWebsOnLayer.Append(idAtomicString(token.data));
              else source.Error("Unknown token '%s' in layer 'subwebs' block.",token.data);
            }
          } else source.Error("Unknown token '%s' in 'layer' block.",token.data);
        }
      }
    }

    source.ExpectTokenString("states"); source.ExpectTokenString("{");
    while (source.ReadToken(token) && token != "}") {
      if (token != "state") { source.Error("Unknown token '%s' in 'states' block.",token.data); continue; }
      source.ExpectTokenType(TT_STRING,0,token); AddState(*this,token.data); source.ExpectTokenString("{");
      while (source.ReadToken(token) && token != "}")
        if (token != "destination") source.Error("Unknown token '%s' in 'state' block.",token.data);
    }

    if (source.CheckTokenString("scalars")) {
      source.ExpectTokenString("{");
      while (source.ReadToken(token) && token != "}") {
        if (token.type != TT_NAME) { source.Error("'%s' is not a valid scalar name.",token.data); continue; }
        idAnimWebScalar scalar; scalar.Init(token.data,source.ParseInt() != 0 ? idAnimWebScalar::DECLARED : 0); scalars.Append(scalar);
      }
    }

    subWebDecls.ClearFree();
    if (source.CheckTokenString("subWebFiles")) {
      source.ExpectTokenString("{");
      while (source.ReadToken(token) && token != "}") {
        if (token == "(") continue;
        if (token.type != TT_STRING) { source.Error("'%s' is not a valid subWeb filename.",token.data); continue; }
        const idDeclAnimSubWeb* decl = static_cast<const idDeclAnimSubWeb*>(idDeclAnimSubWeb::resourceList.FindWithInheritance(token.data,true,true));
        if (decl) subWebDecls.Append(decl); else source.Error("Could not find subweb decl: '%s'",token.data);
      }
      MergeSubWebs();
    } else {
      source.ExpectTokenString("subWebs"); source.ExpectTokenString("{"); ParseSubWeb(source); source.ExpectTokenString("}");
    }
  }
  if (source.PeekTokenString("}")) source.ExpectTokenString("}");
  FinalizeAnimWeb(*this);
  source.SetFlags(oldFlags);
}
