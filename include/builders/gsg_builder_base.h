#ifndef GSG_BUILDER_BASE_H
#define GSG_BUILDER_BASE_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../include/gsg.h"

using TypeList = std::vector<std::string_view>;

struct ExprSpec {
  TypeList paren_types;
  TypeList bool_types;
  TypeList and_ops;
  TypeList or_ops;
};

struct StmtSpec {
  TypeList block_types;
  TypeList if_types;
  TypeList while_types;
  TypeList do_types;
  TypeList for_types;
  TypeList for_header_fields;
  TypeList switch_types;
  std::string_view switch_header_field;
  TypeList case_types;
  TypeList try_types;
  TypeList catch_types;
  TypeList finally_types;
  TypeList ternary_types;
  std::vector<std::pair<std::string_view, std::string_view>> jumps;
};

class ExprBuilderBase : public IBuilder {
 public:
  static SourceLoc loc(TSNode n);
  static std::string_view slice(const std::string& src, TSNode n);

 protected:
  explicit ExprBuilderBase(ExprSpec spec) : expr_spec_(std::move(spec)) {}

  static std::string grammar_type(TSNode n);
  static TSNode child_field(TSNode n, std::string_view name);
  static bool type_in(const TypeList& types, std::string_view ty);

  [[nodiscard]] TSNode unwrap_parens(TSNode n) const;
  [[nodiscard]] std::string_view bool_op(TSNode n) const;
  void collect_expr(TSNode root, const std::string& src, GSGNode& into);
  virtual bool collect_special(TSNode n, const std::string& src, GSGNode& into,
                               bool is_root) = 0;
  void push_expr(std::vector<GSGNode>& out, TSNode loc_node, TSNode expr,
                 const std::string& src);
  void push_header(GSGNode& stmt, TSNode expr, const std::string& src);

 private:
  ExprSpec expr_spec_;
};

class StmtBuilderBase : public ExprBuilderBase {
 protected:
  StmtBuilderBase(ExprSpec expr_spec, StmtSpec stmt_spec)
      : ExprBuilderBase(std::move(expr_spec)), spec_(std::move(stmt_spec)) {}

  bool collect_special(TSNode n, const std::string& src, GSGNode& into,
                       bool is_root) override;
  virtual bool collect_function_like(TSNode n, const std::string& src,
                                     GSGNode& into) = 0;
  virtual bool build_known_statement(TSNode s, const std::string& src,
                                     std::vector<GSGNode>& out);
  void build_block_children(TSNode n, const std::string& src,
                            std::vector<GSGNode>& out);
  void build_statement(TSNode s, const std::string& src,
                       std::vector<GSGNode>& out);

 private:
  StmtSpec spec_;

  void build_if(TSNode n, const std::string& src, std::vector<GSGNode>& out,
                GSGNodeKind kind);
  GSGNode build_loop(TSNode n, const std::string& src, GSGNodeKind kind,
                     const TypeList& header_fields);
  void build_switch(TSNode n, const std::string& src,
                    std::vector<GSGNode>& out);
  void build_try(TSNode n, const std::string& src, std::vector<GSGNode>& out);
  void build_ternary(TSNode n, const std::string& src, GSGNode& into);
  bool build_jump(TSNode s, std::string_view ty, std::vector<GSGNode>& out);
};

#endif
