#ifndef C_GSG_BUILDER_H
#define C_GSG_BUILDER_H

#include "../../include/gsg.h"

// Minimal C/C++ builder (reused for both languages)
class CLikeGSGBuilder : public IBuilder {
 public:
  std::vector<GSGNode> build_functions(TSNode root,
                                       const std::string& source) override;

  std::vector<RecursiveDef> recursive_defs(TSNode root,
                                           const std::string& src) override;
  static SourceLoc loc(TSNode n);

 private:
  static std::string_view slice(const std::string& src, TSNode n);
  static std::string function_name_from_declarator(TSNode decl,
                                                   const std::string& src);

  void collect_functions_in_scope(TSNode n, const std::string& src,
                                  const std::string& qual,
                                  std::vector<GSGNode>& out);
  GSGNode build_function(TSNode n, const std::string& src);
  GSGNode build_function(TSNode n, const std::string& src,
                         const std::string& qual);
  void build_block_children(TSNode n, const std::string& src,
                            std::vector<GSGNode>& out);
  void build_if(TSNode n, const std::string& src, std::vector<GSGNode>& out,
                GSGNodeKind kind = GSGNodeKind::If);
  void build_switch(TSNode s, const std::string& src,
                    std::vector<GSGNode>& out);
  void build_try(TSNode s, const std::string& src, std::vector<GSGNode>& out);
  GSGNode build_while(TSNode n, const std::string& src);
  GSGNode build_for(TSNode n, const std::string& src);
  GSGNode build_do_while(TSNode n, const std::string& src);

  static void collect_expr(TSNode n, const std::string& src, GSGNode& into);
  static bool collect_special(TSNode n, const std::string& src, GSGNode& into);
  static void push_expr(std::vector<GSGNode>& out, TSNode loc_node, TSNode expr,
                        const std::string& src);
  static void push_header(GSGNode& stmt, TSNode expr, const std::string& src);

  // lambdas
  static void collect_lambdas_in_node(TSNode n, const std::string& src,
                                      std::vector<GSGNode>& out);
  static GSGNode build_lambda(TSNode n, const std::string& src);
};

#endif
