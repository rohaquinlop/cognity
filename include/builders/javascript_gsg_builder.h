#ifndef JAVASCRIPT_GSG_BUILDER_H
#define JAVASCRIPT_GSG_BUILDER_H

#include "../../include/builders/gsg_builder_base.h"

class JavaScriptGSGBuilder : public StmtBuilderBase {
 public:
  JavaScriptGSGBuilder();
  std::vector<GSGNode> build_functions(TSNode root,
                                       const std::string& source) override;

  std::vector<RecursiveDef> recursive_defs(TSNode root,
                                           const std::string& src) override;

 protected:
  bool collect_function_like(TSNode n, const std::string& src,
                             GSGNode& into) override;
  bool build_known_statement(TSNode s, const std::string& src,
                             std::vector<GSGNode>& out) override;

 private:
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
  void collect_defaults(TSNode params, const std::string& src, GSGNode& into);
};

#endif
