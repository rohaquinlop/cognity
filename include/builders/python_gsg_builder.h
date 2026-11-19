#ifndef PYTHON_GSG_BUILDER_H
#define PYTHON_GSG_BUILDER_H

#include "../../include/gsg.h"

class PythonGSGBuilder : public IBuilder {
 public:
  std::vector<GSGNode> build_functions(TSNode root,
                                       const std::string& source) override;

 private:
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

  static SourceLoc loc_from_node(TSNode node);
  static std::string_view slice_source(const std::string& source, TSNode node);
  static std::string_view get_identifier(TSNode node,
                                         const std::string& source);
  static TSNode clause_body(TSNode clause);
  static GSGNode expr_node(GSGNodeKind kind, TSNode loc_node);
  static void collect_bool_chain(TSNode node, const std::string& source,
                                 GSGNode& into, std::string& previous);
  static void collect_assignment(TSNode node, const std::string& source,
                                 GSGNode& into);
  static void collect_defaults(TSNode parameters, const std::string& source,
                               GSGNode& into);
  static void collect_definition_header(TSNode decorated, TSNode def,
                                        const std::string& source,
                                        GSGNode& into);
  static bool returns_inner(TSNode ret, std::string_view inner,
                            const std::string& source);
  static void collect_expr(TSNode node, const std::string& source,
                           GSGNode& into);
  static void push_expr(std::vector<GSGNode>& out, TSNode loc_node, TSNode expr,
                        const std::string& source);
  static void push_header(GSGNode& stmt, TSNode expr,
                          const std::string& source);
};

#endif
