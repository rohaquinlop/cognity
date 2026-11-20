#ifndef JAVASCRIPT_GSG_BUILDER_H
#define JAVASCRIPT_GSG_BUILDER_H

#include "../../include/gsg.h"

class JavaScriptGSGBuilder : public IBuilder {
 public:
  std::vector<GSGNode> build_functions(TSNode root,
                                       const std::string& source) override;

  std::vector<RecursiveDef> recursive_defs(TSNode root,
                                           const std::string& src) override;
  static SourceLoc loc(TSNode n);

 private:
  static std::string_view slice(const std::string& src, TSNode n);
  static std::string_view name_of(TSNode n, const std::string& src);

  void collect_units(TSNode n, const std::string& src, const std::string& qual,
                     std::vector<GSGNode>& funcs);
  void collect_children(TSNode n, const std::string& src,
                        const std::string& qual, std::vector<GSGNode>& funcs);
  void collect_value(TSNode v, const std::string& src, const std::string& name,
                     std::vector<GSGNode>& funcs);
  void collect_container(TSNode n, const std::string& src,
                         const std::string& qual, std::vector<GSGNode>& funcs);
  GSGNode build_function(TSNode n, const std::string& src);
  GSGNode build_function_expr(TSNode n, const std::string& src);
  void build_block_children(TSNode n, const std::string& src,
                            std::vector<GSGNode>& out);
  void build_statement(TSNode s, const std::string& src,
                       std::vector<GSGNode>& out);
  bool build_known_statement(TSNode s, const std::string& src,
                             std::vector<GSGNode>& out);
  void build_if(TSNode n, const std::string& src, std::vector<GSGNode>& out,
                GSGNodeKind kind = GSGNodeKind::If);
  void build_switch(TSNode n, const std::string& src,
                    std::vector<GSGNode>& out);
  void build_try(TSNode n, const std::string& src, std::vector<GSGNode>& out);
  GSGNode build_while(TSNode n, const std::string& src);
  GSGNode build_for(TSNode n, const std::string& src);
  GSGNode build_do_while(TSNode n, const std::string& src);

  void collect_expr(TSNode n, const std::string& src, GSGNode& into);
  bool collect_special(TSNode n, const std::string& src, GSGNode& into,
                       bool is_root);
  void collect_defaults(TSNode params, const std::string& src, GSGNode& into);
  void push_expr(std::vector<GSGNode>& out, TSNode loc_node, TSNode expr,
                 const std::string& src);
  void push_header(GSGNode& stmt, TSNode expr, const std::string& src);
};

#endif
