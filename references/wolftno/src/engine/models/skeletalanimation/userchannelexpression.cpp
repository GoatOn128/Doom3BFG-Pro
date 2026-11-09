#include "engine/models/skeletalanimation/userchannelexpression.h"

#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/text/lexer.h"

#include <algorithm>
#include <cstring>
#include <new>

extern bool forceIdListsToReallocateEveryAppend;

namespace {

using Expression = idUserChannelExpression::Expression;
using Node = Expression::ExpNode;
using Builder = idRuntimeExpressionBuilder<Expression>;

class ExpressionParser {
public:
  ExpressionParser(const char* text, const char* source,
                   idUserChannelExpression::VarContext& context,
                   idList<Node, 5>& nodes)
      : expressionText(text), sourceName(source), variableContext(context),
        nodeList(nodes), lexer(LEXFL_NOSTRINGCONCAT |
                               LEXFL_NOSTRINGESCAPECHARS) {
    lexer.LoadMemory(text, idStr::Length(text), "idRuntimeExpression::Parse");
    Read();
  }

  int Parse() {
    const int result = ParseAddSub();
    if (!failed && hasToken)
      Fail("decl '%s' unsupported or unknown token '%s' in expr '%s'",
           sourceName, token.data, expressionText);
    return failed ? -1 : result;
  }

private:
  void Read() { hasToken = lexer.ReadToken(token); }

  bool Is(const char* text) const {
    return hasToken && idStr::Cmp(token.data, text) == 0;
  }

  void Fail(const char* format, const char* first = nullptr,
            const char* second = nullptr, const char* third = nullptr) {
    failed = true;
    lexer.Error(format, first, second, third);
  }

  int NewNode(Node::op_t op) {
    if (nodeList.num >= Node::MAX_NODES) {
      Fail("Expression '%s' has to many nodes, max is 253", expressionText);
      return -1;
    }
    Node& node = nodeList.Alloc();
    node = Node();
    node.op = op;
    return static_cast<int>(&node - nodeList.list);
  }

  int Binary(Node::op_t op, int left, int right) {
    const int index = NewNode(op);
    if (index < 0)
      return index;
    nodeList[index].var0 = static_cast<std::uint8_t>(left);
    nodeList[index].var1 = static_cast<std::uint8_t>(right);
    return index;
  }

  int ParseAddSub() {
    int left = ParseMulDiv();
    while (!failed && (Is("+") || Is("-"))) {
      const Node::op_t op = Is("+") ? Node::OP_ADD : Node::OP_SUB;
      Read();
      left = Binary(op, left, ParseMulDiv());
    }
    return left;
  }

  int ParseMulDiv() {
    int left = ParseUnary();
    while (!failed && (Is("*") || Is("/"))) {
      const Node::op_t op = Is("*") ? Node::OP_MUL : Node::OP_DIV;
      Read();
      left = Binary(op, left, ParseUnary());
    }
    return left;
  }

  int ParseUnary() {
    if (Is("+")) {
      Read();
      return ParseUnary();
    }
    if (Is("-")) {
      Read();
      const int coefficient = NewNode(Node::OP_VAL);
      if (coefficient >= 0)
        nodeList[coefficient].coef = -1.0f;
      return Binary(Node::OP_MUL, coefficient, ParseUnary());
    }
    return ParsePrimary();
  }

  int ParsePrimary() {
    if (!hasToken) {
      Fail("decl '%s' unexpected end of expression in expr '%s'", sourceName,
           expressionText);
      return -1;
    }
    if (token.type == TT_NUMBER) {
      if ((token.subtype & TT_VALUESVALID) == 0)
        token.NumberValue();
      const int index = NewNode(Node::OP_VAL);
      if (index >= 0)
        nodeList[index].coef = token.floatvalue;
      Read();
      return index;
    }
    if (token.type == TT_NAME) {
      idStr name(token);
      Read();
      if (Is("("))
        return ParseFunction(name.data);
      const int index = NewNode(Node::OP_VAR);
      if (index >= 0 &&
          !variableContext.LookUpVar(name.data, nodeList[index].varId)) {
        Fail("decl '%s' unknown variable '%s' in expr '%s'", sourceName,
             name.data, expressionText);
      }
      return index;
    }
    if (Is("(")) {
      Read();
      const int result = ParseAddSub();
      if (!Is(")")) {
        Fail("decl '%s' found '(' without a matching ')' in expr '%s'",
             sourceName, expressionText);
        return -1;
      }
      Read();
      return result;
    }
    Fail("decl '%s' unknown token type '%s' in expr '%s'", sourceName,
         token.data, expressionText);
    return -1;
  }

  int ParseFunction(const char* name) {
    Node::op_t op = Node::OP_NONE;
    int requiredArguments = 0;
    if (idStr::Cmp(name, "max") == 0) {
      op = Node::OP_MAX;
      requiredArguments = 2;
    } else if (idStr::Cmp(name, "min") == 0) {
      op = Node::OP_MIN;
      requiredArguments = 2;
    } else if (idStr::Cmp(name, "lerp") == 0) {
      op = Node::OP_LERP;
      requiredArguments = 3;
    } else if (idStr::Cmp(name, "clamp") == 0) {
      op = Node::OP_CLAMP;
      requiredArguments = 3;
    } else {
      Fail("decl '%s' unknown function %s in expr '%s'", sourceName, name,
           expressionText);
      return -1;
    }
    Read();
    int arguments[3] = {-1, -1, -1};
    int count = 0;
    while (!failed && !Is(")")) {
      if (count >= 3) {
        Fail("decl '%s' Wrong number of arguments in function in expr '%s'",
             sourceName, expressionText);
        return -1;
      }
      arguments[count++] = ParseAddSub();
      if (!Is(","))
        break;
      Read();
    }
    if (!Is(")") || count != requiredArguments) {
      Fail("decl '%s' Wrong number of arguments in function in expr '%s'",
           sourceName, expressionText);
      return -1;
    }
    Read();
    const int index = NewNode(op);
    if (index < 0)
      return index;
    nodeList[index].var0 = static_cast<std::uint8_t>(arguments[0]);
    nodeList[index].var1 = static_cast<std::uint8_t>(arguments[1]);
    if (requiredArguments == 3)
      nodeList[index].var2 = static_cast<std::uint8_t>(arguments[2]);
    return index;
  }

  const char* expressionText;
  const char* sourceName;
  idUserChannelExpression::VarContext& variableContext;
  idList<Node, 5>& nodeList;
  idLexer lexer;
  idToken token;
  bool hasToken = false;
  bool failed = false;
};

} // namespace

template <>
void* idListArrayNew<Node>(int num, memTag_t tag, bool zeroBuffer) {
  if (num <= 0)
    return nullptr;
  idMem* memory = idMem::Singleton();
  auto* result = static_cast<Node*>(memory->AllocWithLocation(
      zeroBuffer
          ? "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(27) : tag"
          : "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag",
      sizeof(Node) * static_cast<std::size_t>(num), tag, zeroBuffer, ALIGN_16,
      HEAP_DEFAULTHEAP));
  if (result != nullptr) {
    for (int index = 0; index < num; ++index)
      ::new (static_cast<void*>(&result[index])) Node;
  }
  return result;
}

template <>
void* idListArrayResize<Node>(void* oldPointer, int oldNum, int newNum,
                              memTag_t tag, bool zeroBuffer) {
  Node* replacement = static_cast<Node*>(
      idListArrayNew<Node>(newNum, tag, zeroBuffer));
  Node* oldNodes = static_cast<Node*>(oldPointer);
  if (newNum > 0 && replacement == nullptr)
    return nullptr;
  if (replacement != nullptr) {
    const int copyCount = std::min(oldNum, newNum);
    for (int index = 0; index < copyCount; ++index)
      replacement[index] = oldNodes[index];
  }
  idMem::Singleton()->Free(oldPointer, ALIGN_16);
  return replacement;
}

bool idUserChannelExpression::VarContext::LookUpVar(const char* name,
                                                    VarId& result) {
  idUserChannelIndex channel =
      decl->model->skeleton->GetUserChannelIndex(name);
  if (channel.value < 0) {
    idStrStatic<240> alternateName;
    alternateName.Format("facedata_%s", name);
    channel = decl->model->skeleton->GetUserChannelIndex(alternateName.data);
    if (channel.value < 0)
      return false;
  }
  result.index = channel.value;
  return true;
}

template <>
bool idList<Node, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearFree();
    return true;
  }
  if (newSize == size || listStatic != 0)
    return true;
  Node* replacement = static_cast<Node*>(idListArrayResize<Node>(
      list, size, newSize, static_cast<memTag_t>(memTag), false));
  if (replacement == nullptr)
    return false;
  list = replacement;
  size = newSize;
  if (num > newSize)
    num = newSize;
  return true;
}

template <>
Node& idList<Node, 5>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    int newSize = required;
    if (!forceIdListsToReallocateEveryAppend) {
      if (granularity != 0) {
        newSize = required + granularity - 1 -
                  (required + granularity - 1) % granularity;
      } else {
        newSize = 1;
        while (newSize < required)
          newSize *= 2;
      }
    }
    Resize(newSize);
  }
  if (num >= size)
    return list[size - 1];
  return list[num++];
}

template <>
int Builder::Optimize(int root, idList<Node, 5>& sourceNodes,
                      idArray<Node, Expression::MAX_SIZE>& destinationNodes) {
  struct localOptExp {
    static __declspec(noinline) int OptSubExpR(int nodeIndex,
                                               idList<Node, 5>& nodes) {
      Node& node = nodes[nodeIndex];
      int removed = 0;
      if (node.var0 != Node::INVALID_INDEX) {
        removed += OptSubExpR(node.var0, nodes);
        if (node.var1 != Node::INVALID_INDEX) {
          removed += OptSubExpR(node.var1, nodes);
          if (node.var2 != Node::INVALID_INDEX)
            removed += OptSubExpR(node.var2, nodes);
        }
      }
      if (node.op == Node::OP_MUL &&
          nodes[node.var0].op == Node::OP_VAL &&
          nodes[node.var1].op != Node::OP_VAL) {
        std::swap(node.var0, node.var1);
      }
      if (node.op == Node::OP_ADD || node.op == Node::OP_SUB) {
        if (nodes[node.var0].op == Node::OP_VAL &&
            nodes[node.var1].op == Node::OP_VAL) {
          const float folded =
              node.op == Node::OP_ADD
                  ? nodes[node.var0].coef + nodes[node.var1].coef
                  : nodes[node.var0].coef - nodes[node.var1].coef;
          node.op = Node::OP_VAL;
          node.var0 = node.var1 = Node::INVALID_INDEX;
          node.coef *= folded;
          return removed + 2;
        }
        return removed;
      }
      if ((node.op != Node::OP_MUL && node.op != Node::OP_DIV) ||
          nodes[node.var1].op != Node::OP_VAL)
        return removed;
      if (nodes[node.var0].op == Node::OP_VAL) {
        const float folded =
            node.op == Node::OP_MUL
                ? nodes[node.var0].coef * nodes[node.var1].coef
                : nodes[node.var0].coef / nodes[node.var1].coef;
        node.op = Node::OP_VAL;
        node.var0 = node.var1 = Node::INVALID_INDEX;
        node.coef *= folded;
        return removed + 2;
      }
      const Node left = nodes[node.var0];
      const float absorbed = node.op == Node::OP_MUL
                                 ? node.coef * nodes[node.var1].coef
                                 : node.coef / nodes[node.var1].coef;
      node = left;
      node.coef *= absorbed;
      return removed + 2;
    }
  };
  {
    struct localCopyExp {
    static __declspec(noinline) int CopyExpR(
        int sourceIndex, idList<Node, 5>& source,
        idArray<Node, Expression::MAX_SIZE>& destination, int& allocIndex) {
      const int destinationIndex = allocIndex++;
      Node& node = destination.ptr[destinationIndex];
      node = source[sourceIndex];
      if (node.var0 != Node::INVALID_INDEX)
        node.var0 = static_cast<std::uint8_t>(
            CopyExpR(node.var0, source, destination, allocIndex));
      if (node.var1 != Node::INVALID_INDEX)
        node.var1 = static_cast<std::uint8_t>(
            CopyExpR(node.var1, source, destination, allocIndex));
      if (node.var2 != Node::INVALID_INDEX)
        node.var2 = static_cast<std::uint8_t>(
            CopyExpR(node.var2, source, destination, allocIndex));
      return destinationIndex;
      }
    };
    const int removed = localOptExp::OptSubExpR(root, sourceNodes);
    const int optimizedCount = sourceNodes.num - removed;
    if (optimizedCount > Expression::MAX_SIZE)
      return -1;
    int allocationIndex = 0;
    localCopyExp::CopyExpR(root, sourceNodes, destinationNodes,
                           allocationIndex);
    return allocationIndex;
  }
}

template <>
bool Builder::Parse(Expression& expression, const char* expressionText,
                    idUserChannelExpression::VarContext& variableContext,
                    const char* source) {
  idStr cleanupError;
  idToken cleanupToken;
  idToken cleanupLastToken;
  idToken cleanupTemporaryToken;
  expression.root = Node::INVALID_INDEX;
  try {
    idStaticList<Node, Node::MAX_NODES> nodeList;
    ExpressionParser parser(expressionText, source, variableContext, nodeList);
    const int temporaryRoot = parser.Parse();
    if (temporaryRoot < 0)
      return false;
    const int optimizedCount =
        Optimize(temporaryRoot, nodeList, expression.nodeAlloc);
    if (optimizedCount < 0) {
      expression.root = Node::INVALID_INDEX;
      return false;
    }
    expression.root = 0;
    return true;
  } catch (...) {
    cleanupError = "runtime expression parse exception";
    (void)cleanupToken;
    (void)cleanupLastToken;
    (void)cleanupTemporaryToken;
    return false;
  }
}

bool idUserChannelExpression::Parse(const char* expressionText,
                                    const idDeclMD6* declaration) {
  VarContext context;
  context.decl = declaration;
  context.channels = nullptr;
  context.numChannels = 0;
  return Builder::Parse(expr, expressionText, context, declaration->name.str);
}
