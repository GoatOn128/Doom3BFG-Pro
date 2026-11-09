#include "declanimwebedge.h"

#include <algorithm>
#include <type_traits>

idDeclAnimWebEdge::idDeclAnimWebEdge()
    : destSubWebIndex(), blendParms(), customFlags(0) {}

idDeclAnimWebEdge::~idDeclAnimWebEdge() = default;

namespace {

std::uint16_t EdgeCustomFlag(const idToken& token) {
  if (idStr::Icmp(token.data, "AW_EDGE_UNUSED") == 0) return 1;
  if (token.type == TT_NUMBER) {
    idToken value(token);
    value.NumberValue();
    return static_cast<std::uint16_t>(value.intvalue);
  }
  return 0;
}

template<class Decl>
void ParseEdgeRetail(idDeclAnimWebEdge& edge, Decl& decl, idDeclAnimWebNode& node,
                     idAnimWebEdgeCache& cache, idParser& source) {
  source.ExpectTokenString("{");
  idStr toSubWebName;
  if constexpr (std::is_same_v<Decl,idDeclAnimWeb>)
    toSubWebName = decl.GetSubWebName(node.subWebIndex);
  else if (node.subWebIndex.value >= 0 && node.subWebIndex.value < decl.subWebs.Num())
    toSubWebName = decl.subWebs[node.subWebIndex.value].name.str;

  idToken toStateName;
  source.ExpectTokenString("toState");
  source.ExpectTokenType(TT_STRING,0,toStateName);
  std::uint8_t edgeFlags = cache.flags & 0xfeu;
  idToken token;
  if (source.CheckTokenString("toSubWeb")) {
    source.ExpectTokenType(TT_STRING,0,token);
    toSubWebName = token.data;
  }
  if (source.CheckTokenString("weightScale")) {
    const int scaled = static_cast<int>(source.ParseFloat() * 16.0f);
    cache.weightScale = static_cast<std::uint8_t>(std::clamp(scaled, 0, 255));
  }
  if (source.CheckTokenString("randomizeWeight"))
    edgeFlags |= source.ParseBool() ? 1u : 0u;

  if (source.CheckTokenString("blendParms")) {
    const char* fromSubWeb = "";
    const char* fromState = "";
    if constexpr (std::is_same_v<Decl,idDeclAnimWeb>) {
      fromSubWeb = decl.GetSubWebName(node.subWebIndex);
      fromState = node.states.numStates ? decl.GetStateName(node.states.stateIndex[0]) : "";
    } else {
      if (node.subWebIndex.value >= 0 && node.subWebIndex.value < decl.subWebs.Num())
        fromSubWeb = decl.subWebs[node.subWebIndex.value].name.str;
      if (node.states.numStates && node.states.stateIndex[0].value >= 0 && node.states.stateIndex[0].value < decl.states.Num())
        fromState = decl.states[node.states.stateIndex[0].value].str;
    }
    edge.blendParms.Parse(fromSubWeb, fromState, toSubWebName.data, toStateName.data, source);
  }
  if (source.CheckTokenString("customFlags")) {
    while (source.ReadTokenOnLine(token)) {
      const std::uint16_t flag = EdgeCustomFlag(token);
      if (flag) edge.customFlags |= flag;
      else source.Warning("unrecognized custom flag '%s'", token.data);
    }
  }

  if (toStateName.IsEmpty()) {
    if (toSubWebName.IsEmpty()) {
      source.Error("idDeclAnimWebEdge: sub-web name cannot be empty.");
    } else {
      cache.destNodeIndex = animWebNodeIndex_t();
      edge.destSubWebIndex = decl.AddSubWeb(toSubWebName.data);
    }
  } else {
    idStaticList<idStr,4> stateNames;
    stateNames.Append(idStr(toStateName.data));
    idDeclAnimWeb::stateList_t stateIndices;
    decl.AddNode(toSubWebName.data, stateNames, cache.destNodeIndex,
                 edge.destSubWebIndex, stateIndices);
  }
  cache.flags = edgeFlags;
  source.ExpectTokenString("}");
}

}

void idDeclAnimWebEdge::Parse(idDeclAnimWeb& decl,idDeclAnimWebNode& node,
                              idAnimWebEdgeCache& cache,idParser& source) {
  ParseEdgeRetail(*this,decl,node,cache,source);
}

void idDeclAnimWebEdge::Parse(idDeclAnimSubWeb& decl,idDeclAnimWebNode& node,
                              idAnimWebEdgeCache& cache,idParser& source) {
  ParseEdgeRetail(*this,decl,node,cache,source);
}
