#pragma once

#include "engine/animation/md6.h"
#include "shared/idlib/containers/array.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

template <typename VarIdType, typename VarContextType, int MaxSize>
class idRuntimeExpression {
public:
  static const int MAX_SIZE = MaxSize;
  using VarId_t = VarIdType;
  using VarContext_t = VarContextType;

  class ExpNode {
  public:
    enum { MAX_NODES = 253 };
    enum : std::uint8_t { INVALID_INDEX = 255, OPEN_PAR = 254 };
    enum op_t : int {
      OP_NONE = 0,
      OP_VAL = 1,
      OP_VAR = 2,
      OP_MUL = 3,
      OP_DIV = 4,
      OP_MAX = 5,
      OP_MIN = 6,
      OP_LERP = 7,
      OP_CLAMP = 8,
      OP_SUB = 9,
      OP_ADD = 10,
    };

    ExpNode()
        : coef(1.0f), op(OP_NONE), parent(INVALID_INDEX),
          var0(INVALID_INDEX), var1(INVALID_INDEX), var2(INVALID_INDEX),
          varId() {}

    float coef;
    op_t op;
    std::uint8_t parent;
    std::uint8_t var0;
    std::uint8_t var1;
    std::uint8_t var2;
    VarIdType varId;
  };

  idRuntimeExpression() : root(ExpNode::INVALID_INDEX), nodeAlloc{} {}

  float Eval(VarContextType& context) {
    if (!HasExpr())
      return 0.0f;
    return EvalNode(static_cast<std::uint8_t>(root), context);
  }
  bool HasExpr() const { return root != ExpNode::INVALID_INDEX; }

  int root;
  idArray<ExpNode, MaxSize> nodeAlloc;

private:
  float EvalNode(std::uint8_t index, VarContextType& context) const {
    const ExpNode& node = nodeAlloc.ptr[index];
    const auto child = [&](std::uint8_t childIndex) {
      return EvalNode(childIndex, context);
    };
    float value = 0.0f;
    switch (node.op) {
    case ExpNode::OP_VAL:
      return node.coef;
    case ExpNode::OP_VAR:
      value = context.EvalVar(const_cast<VarIdType&>(node.varId));
      break;
    case ExpNode::OP_MUL:
      value = child(node.var0) * child(node.var1);
      break;
    case ExpNode::OP_DIV:
      value = child(node.var0) / child(node.var1);
      break;
    case ExpNode::OP_MAX:
      value = std::max(child(node.var0), child(node.var1));
      break;
    case ExpNode::OP_MIN:
      value = std::min(child(node.var0), child(node.var1));
      break;
    case ExpNode::OP_LERP: {
      const float start = child(node.var0);
      value = start + (child(node.var1) - start) * child(node.var2);
      break;
    }
    case ExpNode::OP_CLAMP:
      value = std::max(child(node.var0),
                       std::min(child(node.var1), child(node.var2)));
      break;
    case ExpNode::OP_SUB:
      value = child(node.var0) - child(node.var1);
      break;
    case ExpNode::OP_ADD:
      value = child(node.var0) + child(node.var1);
      break;
    default:
      return 0.0f;
    }
    return value * node.coef;
  }
};

template <typename ExpressionType> class idRuntimeExpressionBuilder {
public:
  static bool Parse(ExpressionType& expression, const char* expressionText,
                    typename ExpressionType::VarContext_t& variableContext,
                    const char* source);

private:
  static int Optimize(
      int root,
      idList<typename ExpressionType::ExpNode, 5>& sourceNodes,
      idArray<typename ExpressionType::ExpNode, ExpressionType::MAX_SIZE>&
          destinationNodes);
};

class idUserChannelExpression {
public:
  enum { SIZE = 20 };

  class VarId {
  public:
    VarId() : index(-1) {}
    void GetStr(idStr& text) { text.Format("%d", index); }
    int index;
  };

  class VarContext {
  public:
    VarContext() : decl(nullptr), channels(nullptr), numChannels(0) {}
    void SetDecl(const idDeclMD6* value) { decl = value; }
    void SetChannels(const float* values, int count) {
      channels = values;
      numChannels = count;
    }
    bool LookUpVar(const char* name, VarId& result);
    float EvalVar(VarId& id) { return channels[id.index]; }

    const idDeclMD6* decl;
    const float* channels;
    int numChannels;
  };

  using Expression = idRuntimeExpression<VarId, VarContext, SIZE>;

  idUserChannelExpression() = default;
  float Eval(const float* channels, int numChannels) {
    VarContext context;
    context.SetChannels(channels, numChannels);
    return expr.Eval(context);
  }
  bool Parse(const char* expressionText, const idDeclMD6* decl);

  Expression expr;
};

using idUserChannelExpressionNode = idUserChannelExpression::Expression::ExpNode;

template <>
struct idListAllocReturnsReference<idUserChannelExpressionNode>
    : std::true_type {};

template <typename T>
void* idListArrayNew(int num, memTag_t tag, bool zeroBuffer);
template <typename T>
void* idListArrayResize(void* oldPointer, int oldNum, int newNum,
                        memTag_t tag, bool zeroBuffer);

template <>
bool idList<idUserChannelExpressionNode, 5>::Resize(int newSize);
template <>
idUserChannelExpressionNode&
idList<idUserChannelExpressionNode, 5>::Alloc();

static_assert(sizeof(idUserChannelExpression::VarId) == 4,
              "user-channel expression variable ABI drift");
static_assert(sizeof(idUserChannelExpression::VarContext) == 24,
              "user-channel expression context ABI drift");
static_assert(sizeof(idUserChannelExpressionNode) == 16,
              "runtime expression node ABI drift");
static_assert(offsetof(idUserChannelExpressionNode, parent) == 8,
              "runtime expression node index offset drift");
static_assert(offsetof(idUserChannelExpressionNode, varId) == 12,
              "runtime expression variable offset drift");
static_assert(sizeof(idUserChannelExpression::Expression) == 324,
              "runtime expression ABI drift");
static_assert(sizeof(idUserChannelExpression) == 324,
              "user-channel expression ABI drift");
