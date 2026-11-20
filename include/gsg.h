#ifndef GSG_H
#define GSG_H

#include <tree_sitter/api.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Supported languages. Add new values between Unknown and Count.
enum class Language {
  Unknown,
  Python,
  C,
  Cpp,
  JavaScript,
  TypeScript,
  Count,
};

// General Syntax Graph node kinds
enum class GSGNodeKind {
  Root,
  Class,
  Function,
  Block,
  Header,
  If,
  ElseIf,
  Else,
  For,
  While,
  DoWhile,
  Switch,
  Case,
  With,
  Except,
  Expr,
  Try,
  Catch,
  Finally,
  Ternary,
  Return,
  Break,
  Continue,
  Jump,
  Unknown,
};

struct SourceLoc {
  unsigned int row{0};
  unsigned int start_col{0};
  unsigned int end_col{0};
  uint32_t start_byte{0};
  uint32_t end_byte{0};
};

struct RecursiveDef {
  uint32_t def_start_byte{0};
  SourceLoc call{};
};

struct GSGNode {
  GSGNodeKind kind{GSGNodeKind::Unknown};
  std::string name{};         // for functions/classes/cases
  SourceLoc loc{};            // for line-complexity mapping
  unsigned int addl_cost{0};  // extra cost (e.g., boolean operator changes)
  std::vector<GSGNode> children{};
};

struct IBuilder {
  virtual ~IBuilder() = default;
  // Build function-level GSG nodes found in the file/module root
  virtual std::vector<GSGNode> build_functions(TSNode root,
                                               const std::string& source) = 0;
  virtual std::vector<RecursiveDef> recursive_defs(TSNode /*root*/,
                                                   const std::string& /*src*/) {
    return {};
  }
};

#endif
