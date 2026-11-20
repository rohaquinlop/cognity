#ifndef C_GSG_BUILDER_H
#define C_GSG_BUILDER_H

#include "../../include/builders/gsg_builder_base.h"

// Minimal C/C++ builder (reused for both languages)
class CLikeGSGBuilder : public StmtBuilderBase {
 public:
  explicit CLikeGSGBuilder(bool cpp = false);

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
  bool cpp_;

  static std::string function_name_from_declarator(TSNode decl,
                                                   const std::string& src);

  void collect_functions_in_scope(TSNode n, const std::string& src,
                                  const std::string& qual,
                                  std::vector<GSGNode>& out);
  GSGNode build_function(TSNode n, const std::string& src);
  GSGNode build_function(TSNode n, const std::string& src,
                         const std::string& qual);
  void build_preproc(TSNode n, const std::string& src,
                     std::vector<GSGNode>& out, GSGNodeKind kind);
  GSGNode build_lambda(TSNode n, const std::string& src);
};

#endif
