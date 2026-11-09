#include "decltypeinfograph.h"

template class idGraphItemFactory<idTypeInfoSubGraph>;
template class idGraphItemFactory<idTypeInfoGraphNode>;
template class idGraphItemFactory<idTypeInfoGraphLink>;

idDeclTypeInfoGraph::idDeclTypeInfoGraph() = default;

idDeclTypeInfoGraph::~idDeclTypeInfoGraph() { DeleteContents(); }

void idDeclTypeInfoGraph::Parse(idParser& source) {
  idDeclTypeInfo::Parse(source);
  OnDeclParsed();
}

idColor idDeclTypeInfoGraph::GetSubGraphColor(const idTypeInfoSubGraph* subGraph) const {
  return subGraph != nullptr ? subGraph->GetColor() : idColor{1.0f, 1.0f, 1.0f, 1.0f};
}

void idDeclTypeInfoGraph::GetNodeInfo(const idTypeInfoGraphNode* node, idStr& info,
                                      bool isSelected) const {
  if (node != nullptr)
    node->GetInfo(info, isSelected);
  else
    info.Clear();
}

void idDeclTypeInfoGraph::DeleteContents() {
  for (int i = 0; i < subGraphs.Num(); ++i)
    delete subGraphs[i];
  for (int i = 0; i < nodes.Num(); ++i)
    delete nodes[i];
  for (int i = 0; i < links.Num(); ++i)
    delete links[i];
  subGraphs.Clear();
  nodes.Clear();
  links.Clear();
  layers.Clear();
  layersConfigs.Clear();
}

void idDeclTypeInfoGraph::GetLinksFrom(const idTypeInfoGraphNode* node,
                                       idList<idTypeInfoGraphLink*>& linksOut) const {
  if (node == nullptr)
    return;
  for (int i = 0; i < links.Num(); ++i) {
    idTypeInfoGraphLink* link = links[i];
    if (link == nullptr || link->startNodeIndex < 0 || link->startNodeIndex >= nodes.Num())
      continue;
    if (nodes[link->startNodeIndex] == node)
      linksOut.Append(link);
  }
}

idGraphItemFactory<idTypeInfoSubGraph>* idDeclTypeInfoGraph::GetSubGraphFactory() {
  static idGraphItemFactory<idTypeInfoSubGraph> factory;
  return &factory;
}

idGraphItemFactory<idTypeInfoGraphNode>* idDeclTypeInfoGraph::GetGraphNodeFactory() {
  static idGraphItemFactory<idTypeInfoGraphNode> factory;
  return &factory;
}

idGraphItemFactory<idTypeInfoGraphLink>* idDeclTypeInfoGraph::GetGraphLinkFactory() {
  static idGraphItemFactory<idTypeInfoGraphLink> factory;
  return &factory;
}

idGraphItemFactory<idTypeInfoSubGraph>* idDeclTypeInfoGraph::GetMySubGraphFactory() {
  return GetSubGraphFactory();
}

idGraphItemFactory<idTypeInfoGraphNode>* idDeclTypeInfoGraph::GetMyGraphNodeFactory() {
  return GetGraphNodeFactory();
}

idGraphItemFactory<idTypeInfoGraphLink>* idDeclTypeInfoGraph::GetMyGraphLinkFactory() {
  return GetGraphLinkFactory();
}
