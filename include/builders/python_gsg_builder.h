#ifndef PYTHON_GSG_BUILDER_H
#define PYTHON_GSG_BUILDER_H

#include "../../include/builders/gsg_builder_base.h"

class PythonGSGBuilder : public ExprBuilderBase {
 public:
  PythonGSGBuilder();
  std::vector<GSGNode> build_functions(TSNode root,
                                       const std::string& source) override;

  std::vector<RecursiveDef> recursive_defs(TSNode root,
                                           const std::string& src) override;

 protected:
  bool collect_special(TSNode node, const std::string& source, GSGNode& into,
                       bool is_root) override;

 private:
  void collect_functions(TSNode node, const std::string& source,
                         const std::string& qual, std::vector<GSGNode>& out);
  GSGNode build_function(TSNode node, const std::string& source);
  void build_function_body(TSNode def, const std::string& source,
                           std::vector<GSGNode>& out);
  void build_definition(TSNode stmt, const std::string& source,
                        std::vector<GSGNode>& out);
  void build_block_children(TSNode block, const std::string& source,
                            std::vector<GSGNode>& out);
  void build_expression_statement(TSNode stmt, const std::string& source,
                                  std::vector<GSGNode>& out);
  void build_loop(TSNode node, const std::string& source,
                  std::vector<GSGNode>& out);
  void build_if(TSNode node, const std::string& source,
                std::vector<GSGNode>& out);
  void build_try(TSNode node, const std::string& source,
                 std::vector<GSGNode>& out);
  void build_match(TSNode node, const std::string& source,
                   std::vector<GSGNode>& out);
  void push_else(TSNode else_clause, const std::string& source,
                 std::vector<GSGNode>& out);

  static std::string_view get_identifier(TSNode node,
                                         const std::string& source);
  static TSNode clause_body(TSNode clause);
  static GSGNode expr_node(GSGNodeKind kind, TSNode loc_node);
  void collect_assignment(TSNode node, const std::string& source,
                          GSGNode& into);
  void collect_defaults(TSNode parameters, const std::string& source,
                        GSGNode& into);
  void collect_definition_header(TSNode decorated, TSNode def,
                                 const std::string& source, GSGNode& into);
  static bool returns_inner(TSNode ret, std::string_view inner,
                            const std::string& source);
};

#endif
