#pragma once

#include "decl.h"
#include "shared/idlib/color.h"

enum typeInfoGraphTypes_t : int { TYPEINFO_GRAPH_UNKNOWN = 0 };
enum typeInfoSubGraphTypes_t : int { TYPEINFO_SUBGRAPH_UNKNOWN = 0 };
enum typeInfoGraphNodeTypes_t : int { TYPEINFO_GRAPH_NODE_UNKNOWN = 0 };
enum typeInfoGraphLinkTypes_t : int { TYPEINFO_GRAPH_LINK_UNKNOWN = 0 };

class idTypeInfoGraphItem {
public:
  virtual ~idTypeInfoGraphItem() = default;
  virtual idColor GetColor() const { return {1.0f, 1.0f, 1.0f, 1.0f}; }
  virtual void GetDisplayName(idStr& result) const { result = name.str; }
  idAtomicString name;
  idAtomicString className;
};

class idTypeInfoSubGraph : public idTypeInfoGraphItem {
public:
  virtual bool IsType(typeInfoSubGraphTypes_t) const { return false; }
  virtual bool IsValid() const { return true; }
  virtual bool IsRoot() const { return false; }
  idList<int> subnodes;
  idVec2 position;
  unsigned int layersMask = 0;
};

class idTypeInfoGraphNode : public idTypeInfoGraphItem {
public:
  virtual bool IsType(typeInfoGraphNodeTypes_t) const { return false; }
  virtual void GetInfo(idStr& result, bool) const { GetDisplayName(result); }
  virtual bool IsValid() const { return true; }
  idVec2 position;
  unsigned int layersMask = 0;
};

class idTypeInfoGraphLink : public idTypeInfoGraphItem {
public:
  virtual bool IsGraphLinkType(typeInfoGraphLinkTypes_t) const { return false; }
  int startNodeIndex = -1;
  int endNodeIndex = -1;
};

struct idGraphItemType {
  idAtomicString className;
  idAtomicString displayedName;
};

template <typename T> class idGraphItemFactory {
public:
  using creationFunction_t = T* (*)();
  struct idTypeCreationPair {
    idGraphItemType type;
    creationFunction_t creationFunction = nullptr;
  };
  virtual ~idGraphItemFactory() = default;
  T* Create(const char* itemClassName) const {
    const char* requested = itemClassName != nullptr ? itemClassName : "";
    for (int i = 0; i < registry.Num(); ++i) {
      if (idStr::Cmp(registry[i].type.className.str, requested) != 0)
        continue;
      T* item = registry[i].creationFunction != nullptr ? registry[i].creationFunction() : nullptr;
      if (item != nullptr)
        item->className.Set(requested);
      return item;
    }
    return nullptr;
  }
  idList<idTypeCreationPair, 87> registry;
};

class idDeclTypeInfoGraph : public idDeclTypeInfo {
public:
  struct idLayersConfig {
    idAtomicString name;
    unsigned int layersMask = 0;
  };

  idDeclTypeInfoGraph();
  ~idDeclTypeInfoGraph() override;
  void Parse(idParser& source) override;
  virtual idGraphItemFactory<idTypeInfoSubGraph>* GetMySubGraphFactory();
  virtual idGraphItemFactory<idTypeInfoGraphNode>* GetMyGraphNodeFactory();
  virtual idGraphItemFactory<idTypeInfoGraphLink>* GetMyGraphLinkFactory();
  virtual bool IsType(typeInfoGraphTypes_t) const { return false; }
  virtual bool CanCreateSubGraphs() const { return true; }
  virtual bool CanCreateNodesOutsideOfSubGraphs() const { return true; }
  virtual bool AllowMultipleLinksBetweenNodes() const { return false; }
  virtual bool AllowLinkBetweenNodes(int, int) const { return true; }
  virtual void OnDeclParsed() {}
  virtual void OnCreateLink(idTypeInfoGraphLink*) {}
  virtual void OnDeleteLink(idTypeInfoGraphLink*) {}
  virtual idColor GetSubGraphColor(const idTypeInfoSubGraph* subGraph) const;
  virtual idColor GetNodeColor(const idTypeInfoGraphNode* node) const {
    return node != nullptr ? node->GetColor() : idColor{1.0f, 1.0f, 1.0f, 1.0f};
  }
  virtual idColor GetLinkColor(const idTypeInfoGraphLink* link) const {
    return link != nullptr ? link->GetColor() : idColor{1.0f, 1.0f, 1.0f, 1.0f};
  }
  virtual void GetNodeInfo(const idTypeInfoGraphNode* node, idStr& info,
                           bool isSelected) const;
  virtual bool NotifyGraphChanged() { return true; }
  virtual bool NotifySubGraphChanged(idTypeInfoSubGraph*) { return true; }
  virtual bool NotifyNodeChanged(idTypeInfoGraphNode*) { return true; }
  virtual bool NotifyLinkChanged(idTypeInfoGraphLink*) { return true; }

  void DeleteContents();
  void GetLinksFrom(const idTypeInfoGraphNode* node,
                    idList<idTypeInfoGraphLink*>& linksOut) const;
  static idGraphItemFactory<idTypeInfoSubGraph>* GetSubGraphFactory();
  static idGraphItemFactory<idTypeInfoGraphNode>* GetGraphNodeFactory();
  static idGraphItemFactory<idTypeInfoGraphLink>* GetGraphLinkFactory();

  idList<idTypeInfoSubGraph*> subGraphs;
  idList<idTypeInfoGraphNode*> nodes;
  idList<idTypeInfoGraphLink*> links;
  idList<idAtomicString> layers;
  idList<idLayersConfig> layersConfigs;
};

static_assert(sizeof(idTypeInfoGraphItem) == 24, "idTypeInfoGraphItem ABI drift");
static_assert(sizeof(idTypeInfoSubGraph) == 64, "idTypeInfoSubGraph ABI drift");
static_assert(sizeof(idTypeInfoGraphNode) == 40, "idTypeInfoGraphNode ABI drift");
static_assert(sizeof(idTypeInfoGraphLink) == 32, "idTypeInfoGraphLink ABI drift");
static_assert(sizeof(idGraphItemFactory<idTypeInfoSubGraph>) == 32,
              "idGraphItemFactory ABI drift");
static_assert(sizeof(idDeclTypeInfoGraph::idLayersConfig) == 16,
              "idLayersConfig ABI drift");
static_assert(sizeof(idDeclTypeInfoGraph) == 216, "idDeclTypeInfoGraph ABI drift");
