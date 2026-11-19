#include <string>
#include <string_view>
#include <vector>

#include "../../include/builders/javascript_gsg_builder.h"

using std::string;
using std::string_view;

static inline string t(TSNode n) { return string(ts_node_grammar_type(n)); }

SourceLoc JavaScriptGSGBuilder::loc(TSNode n) {
  TSPoint p = ts_node_start_point(n), q = ts_node_end_point(n);
  return SourceLoc{p.row, p.column, q.column};
}

string_view JavaScriptGSGBuilder::slice(const string& src, TSNode n) {
  const uint32_t a = ts_node_start_byte(n);
  const uint32_t b = ts_node_end_byte(n);
  return string_view(src).substr(a, b - a);
}

string_view JavaScriptGSGBuilder::name_of(TSNode n, const string& src) {
  TSNode name = ts_node_child_by_field_name(n, "name", 4);
  if (!ts_node_is_null(name)) return slice(src, name);
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i) {
    TSNode ch = ts_node_named_child(n, i);
    if (t(ch) == "identifier" || t(ch) == "property_identifier")
      return slice(src, ch);
  }
  return string_view{};
}

enum class JSBoolOp { And, Or, Not, Unknown };

static string_view js_slice(const string& src, TSNode n) {
  const uint32_t a = ts_node_start_byte(n);
  const uint32_t b = ts_node_end_byte(n);
  return string_view(src).substr(a, b - a);
}

static JSBoolOp js_op_from_text(string_view s) {
  if (s == "&&") return JSBoolOp::And;
  if (s == "||") return JSBoolOp::Or;
  if (s == "!") return JSBoolOp::Not;
  return JSBoolOp::Unknown;
}

static TSNode js_unwrap_parens(TSNode n) {
  while (!ts_node_is_null(n) &&
         string_view{ts_node_type(n)} == "parenthesized_expression") {
    TSNode inner = ts_node_child_by_field_name(n, "expression", 10);
    if (ts_node_is_null(inner)) break;
    n = inner;
  }
  return n;
}

static JSBoolOp js_get_bool_op(TSNode n, const string& src) {
  n = js_unwrap_parens(n);
  string ty = t(n);
  if (ty == "binary_expression") {
    auto s = js_slice(src, n);
    if (s.find("&&") != string::npos) return JSBoolOp::And;
    if (s.find("||") != string::npos) return JSBoolOp::Or;
  } else if (ty == "unary_expression") {
    auto s = js_slice(src, n);
    if (!s.empty() && s[0] == '!') return JSBoolOp::Not;
  }
  return JSBoolOp::Unknown;
}

static unsigned int js_count_bool_alternations(TSNode n, const string& src) {
  n = js_unwrap_parens(n);
  string ty = t(n);
  unsigned int c = 0;
  if (ty == "binary_expression") {
    TSNode left = ts_node_child_by_field_name(n, "left", 4);
    TSNode right = ts_node_child_by_field_name(n, "right", 5);
    JSBoolOp curr = js_get_bool_op(n, src);
    JSBoolOp lb = js_get_bool_op(left, src);
    JSBoolOp rb = js_get_bool_op(right, src);
    if (lb != JSBoolOp::Unknown && curr != lb) c++;
    if (rb != JSBoolOp::Unknown && curr != rb) c++;
    c += js_count_bool_alternations(left, src);
    c += js_count_bool_alternations(right, src);
  }
  return c;
}

static bool js_has_logical_op(TSNode n, const string& src) {
  auto s = js_slice(src, n);
  return s.find("&&") != string::npos || s.find("||") != string::npos ||
         s.find("!") != string::npos;
}

void JavaScriptGSGBuilder::collect_expr(TSNode n, const string& src,
                                        GSGNode& into) {
  if (ts_node_is_null(n)) return;
  string ty = t(n);
  if (ty == "binary_expression") {
    into.addl_cost +=
      (js_has_logical_op(n, src) ? 1 : 0) + js_count_bool_alternations(n, src);
    collect_expr(ts_node_child_by_field_name(n, "left", 4), src, into);
    collect_expr(ts_node_child_by_field_name(n, "right", 5), src, into);
    return;
  }
  if (ty == "unary_expression") {
    auto s = js_slice(src, n);
    if (!s.empty() && s[0] == '!') {
      into.addl_cost += 1;
      return;
    }
  }
  GSGNode* target = &into;
  GSGNode tern;
  if (ty == "ternary_expression") {
    tern.kind = GSGNodeKind::Ternary;
    tern.loc = loc(n);
    target = &tern;
  }
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i)
    collect_expr(ts_node_named_child(n, i), src, *target);
  if (target == &tern) into.children.emplace_back(std::move(tern));
}

void JavaScriptGSGBuilder::push_expr(std::vector<GSGNode>& out, TSNode loc_node,
                                     TSNode expr, const string& src) {
  GSGNode e;
  e.kind = GSGNodeKind::Expr;
  e.loc = loc(loc_node);
  collect_expr(expr, src, e);
  if (e.addl_cost || !e.children.empty()) out.emplace_back(std::move(e));
}

void JavaScriptGSGBuilder::push_header(GSGNode& stmt, TSNode expr,
                                       const string& src) {
  if (ts_node_is_null(expr)) return;
  GSGNode h;
  h.kind = GSGNodeKind::Header;
  h.loc = loc(expr);
  collect_expr(expr, src, h);
  if (h.addl_cost || !h.children.empty())
    stmt.children.emplace_back(std::move(h));
}

std::vector<GSGNode> JavaScriptGSGBuilder::build_functions(TSNode root,
                                                           const string& src) {
  std::vector<GSGNode> funcs;
  int n = ts_node_named_child_count(root);
  for (int i = 0; i < n; ++i) {
    TSNode ch = ts_node_named_child(root, i);
    string ty = t(ch);
    if (ty == "function_declaration") {
      funcs.emplace_back(build_function(ch, src));
    } else if (ty == "class_declaration") {
      TSNode body = ts_node_child_by_field_name(ch, "body", 4);
      int m = ts_node_named_child_count(body);
      for (int j = 0; j < m; ++j) {
        TSNode mem = ts_node_named_child(body, j);
        if (t(mem) == "method_definition") {
          funcs.emplace_back(build_function(mem, src));
        }
      }
    }
  }
  return funcs;
}

GSGNode JavaScriptGSGBuilder::build_function(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::Function;
  g.name = string(name_of(n, src));
  g.loc = loc(n);
  build_block_children(ts_node_child_by_field_name(n, "body", 4), src,
                       g.children);
  return g;
}

void JavaScriptGSGBuilder::build_switch(TSNode n, const string& src,
                                        std::vector<GSGNode>& out) {
  GSGNode sw;
  sw.kind = GSGNodeKind::Switch;
  sw.loc = loc(n);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  uint32_t kmax = ts_node_is_null(body) ? 0 : ts_node_named_child_count(body);
  for (uint32_t k = 0; k < kmax; ++k) {
    TSNode cc = ts_node_named_child(body, k);
    string cty = t(cc);
    if (cty != "switch_case" && cty != "switch_default") continue;
    GSGNode cs;
    cs.kind = GSGNodeKind::Case;
    cs.loc = loc(cc);
    build_block_children(cc, src, cs.children);
    sw.children.emplace_back(std::move(cs));
  }
  out.emplace_back(std::move(sw));
}

void JavaScriptGSGBuilder::build_try(TSNode n, const string& src,
                                     std::vector<GSGNode>& out) {
  GSGNode tr;
  tr.kind = GSGNodeKind::Try;
  tr.loc = loc(n);
  build_block_children(ts_node_child_by_field_name(n, "body", 4), src,
                       tr.children);
  out.emplace_back(std::move(tr));
  TSNode handler = ts_node_child_by_field_name(n, "handler", 7);
  if (!ts_node_is_null(handler)) {
    GSGNode c;
    c.kind = GSGNodeKind::Catch;
    c.loc = loc(handler);
    build_block_children(ts_node_child_by_field_name(handler, "body", 4), src,
                         c.children);
    out.emplace_back(std::move(c));
  }
  TSNode fin = ts_node_child_by_field_name(n, "finalizer", 9);
  if (!ts_node_is_null(fin)) {
    GSGNode f;
    f.kind = GSGNodeKind::Finally;
    f.loc = loc(fin);
    build_block_children(ts_node_child_by_field_name(fin, "body", 4), src,
                         f.children);
    out.emplace_back(std::move(f));
  }
}

void JavaScriptGSGBuilder::build_block_children(TSNode n, const string& src,
                                                std::vector<GSGNode>& out) {
  if (ts_node_is_null(n)) return;
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i) {
    TSNode s = ts_node_named_child(n, i);
    string ty = t(s);
    if (ty == "if_statement")
      build_if(s, src, out);
    else if (ty == "while_statement")
      out.emplace_back(build_while(s, src));
    else if (ty == "for_statement")
      out.emplace_back(build_for(s, src));
    else if (ty == "do_statement")
      out.emplace_back(build_do_while(s, src));
    else if (ty == "function_declaration" || ty == "method_definition")
      out.emplace_back(build_function(s, src));
    else if (ty == "switch_statement")
      build_switch(s, src, out);
    else if (ty == "try_statement")
      build_try(s, src, out);
    else if (ty == "statement_block")
      build_block_children(s, src, out);
    else if (ty == "expression_statement") {
      if (ts_node_named_child_count(s) > 0) {
        TSNode expr = ts_node_named_child(s, 0);
        push_expr(out, expr, expr, src);
      }
    } else if (ty == "return_statement") {
      TSNode arg = ts_node_child_by_field_name(s, "argument", 8);
      if (ts_node_is_null(arg) && ts_node_named_child_count(s) > 0)
        arg = ts_node_named_child(s, 0);
      if (!ts_node_is_null(arg)) push_expr(out, s, arg, src);
    } else if (ty == "throw_statement") {
      TSNode arg = ts_node_child_by_field_name(s, "argument", 8);
      if (!ts_node_is_null(arg)) push_expr(out, s, arg, src);
    } else if (ty == "lexical_declaration" || ty == "variable_declaration") {
      push_expr(out, s, s, src);
    }
  }
}

void JavaScriptGSGBuilder::build_if(TSNode n, const string& src,
                                    std::vector<GSGNode>& out,
                                    GSGNodeKind kind) {
  GSGNode g;
  g.kind = kind;
  g.loc = loc(n);
  push_header(g, ts_node_child_by_field_name(n, "condition", 9), src);
  build_block_children(ts_node_child_by_field_name(n, "consequence", 11), src,
                       g.children);
  out.emplace_back(std::move(g));
  TSNode alt = ts_node_child_by_field_name(n, "alternative", 11);
  if (ts_node_is_null(alt)) return;
  if (t(alt) == "if_statement") {
    build_if(alt, src, out, GSGNodeKind::ElseIf);
    return;
  }
  if (ts_node_named_child_count(alt) == 1) {
    TSNode only = ts_node_named_child(alt, 0);
    if (t(only) == "if_statement") {
      build_if(only, src, out, GSGNodeKind::ElseIf);
      return;
    }
  }
  GSGNode el;
  el.kind = GSGNodeKind::Else;
  el.loc = loc(alt);
  build_block_children(alt, src, el.children);
  out.emplace_back(std::move(el));
}

GSGNode JavaScriptGSGBuilder::build_while(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::While;
  g.loc = loc(n);
  push_header(g, ts_node_child_by_field_name(n, "condition", 9), src);
  build_block_children(ts_node_child_by_field_name(n, "body", 4), src,
                       g.children);
  return g;
}

GSGNode JavaScriptGSGBuilder::build_for(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::For;
  g.loc = loc(n);
  build_block_children(ts_node_child_by_field_name(n, "body", 4), src,
                       g.children);
  return g;
}

GSGNode JavaScriptGSGBuilder::build_do_while(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::DoWhile;
  g.loc = loc(n);
  TSNode cond = ts_node_child_by_field_name(n, "condition", 9);
  if (!ts_node_is_null(cond))
    g.addl_cost += js_count_bool_alternations(cond, src);
  build_block_children(ts_node_child_by_field_name(n, "body", 4), src,
                       g.children);
  return g;
}
