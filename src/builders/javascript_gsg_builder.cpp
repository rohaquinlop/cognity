#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../include/builders/javascript_gsg_builder.h"

using std::string;
using std::string_view;

static inline string t(TSNode n) { return string(ts_node_grammar_type(n)); }

SourceLoc JavaScriptGSGBuilder::loc(TSNode n) {
  TSPoint p = ts_node_start_point(n), q = ts_node_end_point(n);
  return SourceLoc{.row = p.row,
                   .start_col = p.column,
                   .end_col = q.column,
                   .start_byte = ts_node_start_byte(n),
                   .end_byte = ts_node_end_byte(n)};
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

static TSNode js_unwrap_parens(TSNode n) {
  while (!ts_node_is_null(n) && t(n) == "parenthesized_expression") {
    TSNode only{};
    uint32_t count = 0;
    uint32_t m = ts_node_named_child_count(n);
    for (uint32_t i = 0; i < m; ++i) {
      TSNode ch = ts_node_named_child(n, i);
      if (ts_node_is_extra(ch)) continue;
      only = ch;
      ++count;
    }
    if (count != 1) break;
    n = only;
  }
  return n;
}

static bool js_is_bool(TSNode n) {
  if (ts_node_is_null(n) || t(n) != "binary_expression") return false;
  string_view op{ts_node_type(ts_node_child_by_field_name(n, "operator", 8))};
  return op == "&&" || op == "||";
}

static bool js_is_statement(const string& ty) {
  return ty == "if_statement" || ty == "while_statement" ||
         ty == "for_statement" || ty == "for_in_statement" ||
         ty == "do_statement" || ty == "switch_statement" ||
         ty == "try_statement" || ty == "statement_block" ||
         ty == "expression_statement" || ty == "return_statement" ||
         ty == "throw_statement" || ty == "lexical_declaration" ||
         ty == "variable_declaration";
}

static bool js_is_function_decl(const string& ty) {
  return ty == "function_declaration" ||
         ty == "generator_function_declaration" || ty == "method_definition";
}

static bool js_is_function_expr(TSNode n) {
  if (ts_node_is_null(n)) return false;
  string ty = t(n);
  return ty == "arrow_function" || ty == "function_expression" ||
         ty == "generator_function";
}

GSGNode JavaScriptGSGBuilder::build_function_expr(TSNode n, const string& src) {
  GSGNode fn;
  fn.kind = GSGNodeKind::Function;
  fn.loc = loc(n);
  GSGNode defaults;
  defaults.kind = GSGNodeKind::Header;
  defaults.loc = loc(n);
  collect_defaults(ts_node_child_by_field_name(n, "parameters", 10), src,
                   defaults);
  if (defaults.addl_cost || !defaults.children.empty())
    fn.children.emplace_back(std::move(defaults));
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body) && t(body) == "statement_block")
    build_block_children(body, src, fn.children);
  else
    push_expr(fn.children, n, body, src);
  return fn;
}

void JavaScriptGSGBuilder::collect_defaults(TSNode params, const string& src,
                                            GSGNode& into) {
  uint32_t m = ts_node_is_null(params) ? 0 : ts_node_named_child_count(params);
  for (uint32_t i = 0; i < m; ++i) {
    TSNode p = ts_node_named_child(params, i);
    string pt = t(p);
    if (pt == "assignment_pattern")
      collect_expr(ts_node_child_by_field_name(p, "right", 5), src, into);
    else if (pt == "required_parameter" || pt == "optional_parameter")
      collect_expr(ts_node_child_by_field_name(p, "value", 5), src, into);
  }
}

void JavaScriptGSGBuilder::collect_expr(TSNode root, const string& src,
                                        GSGNode& into) {
  enum class Step : uint8_t { Expr, Operand, Op, ChainEnd };
  std::vector<std::pair<TSNode, Step>> stack{{root, Step::Expr}};
  std::vector<string_view> previous;
  auto expand = [&stack](TSNode b) {
    stack.emplace_back(ts_node_child_by_field_name(b, "right", 5),
                       Step::Operand);
    stack.emplace_back(b, Step::Op);
    stack.emplace_back(ts_node_child_by_field_name(b, "left", 4),
                       Step::Operand);
  };
  while (!stack.empty()) {
    auto [n, step] = stack.back();
    stack.pop_back();
    if (step == Step::ChainEnd) {
      previous.pop_back();
    } else if (step == Step::Op) {
      string_view op{
        ts_node_type(ts_node_child_by_field_name(n, "operator", 8))};
      if (op != previous.back()) into.addl_cost += 1;
      previous.back() = op;
    } else if (step == Step::Operand) {
      TSNode inner = js_unwrap_parens(n);
      if (js_is_bool(inner))
        expand(inner);
      else
        stack.emplace_back(n, Step::Expr);
    } else if (ts_node_is_null(n)) {
      continue;
    } else if (js_is_bool(n)) {
      previous.emplace_back();
      stack.emplace_back(n, Step::ChainEnd);
      expand(n);
    } else if (!collect_special(n, src, into, ts_node_eq(n, root))) {
      for (uint32_t i = ts_node_named_child_count(n); i > 0; --i)
        stack.emplace_back(ts_node_named_child(n, i - 1), Step::Expr);
    }
  }
}

bool JavaScriptGSGBuilder::collect_special(TSNode n, const string& src,
                                           GSGNode& into, bool is_root) {
  string ty = t(n);
  if (js_is_function_expr(n)) {
    into.children.emplace_back(build_function_expr(n, src));
    return true;
  }
  if (js_is_function_decl(ty)) {
    into.children.emplace_back(build_function(n, src));
    return true;
  }
  if (!is_root && js_is_statement(ty)) {
    build_statement(n, src, into.children);
    return true;
  }
  if (ty == "ternary_expression") {
    GSGNode tern;
    tern.kind = GSGNodeKind::Ternary;
    tern.loc = loc(n);
    push_header(tern, ts_node_child_by_field_name(n, "condition", 9), src);
    collect_expr(ts_node_child_by_field_name(n, "consequence", 11), src, tern);
    collect_expr(ts_node_child_by_field_name(n, "alternative", 11), src, tern);
    into.children.emplace_back(std::move(tern));
    return true;
  }
  return false;
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
  for (int i = 0; i < n; ++i)
    build_top_level(ts_node_named_child(root, i), src, funcs);
  return funcs;
}

void JavaScriptGSGBuilder::build_top_level(TSNode ch, const string& src,
                                           std::vector<GSGNode>& funcs) {
  if (ts_node_is_null(ch)) return;
  string ty = t(ch);
  if (ty == "function_declaration") {
    funcs.emplace_back(build_function(ch, src));
  } else if (ty == "class_declaration") {
    TSNode body = ts_node_child_by_field_name(ch, "body", 4);
    uint32_t m = ts_node_named_child_count(body);
    for (uint32_t j = 0; j < m; ++j) {
      TSNode mem = ts_node_named_child(body, j);
      if (t(mem) == "method_definition") {
        funcs.emplace_back(build_function(mem, src));
      }
    }
  } else if (ty == "export_statement") {
    build_top_level(ts_node_child_by_field_name(ch, "declaration", 11), src,
                    funcs);
  } else if (ty == "lexical_declaration" || ty == "variable_declaration") {
    uint32_t m = ts_node_named_child_count(ch);
    for (uint32_t j = 0; j < m; ++j) {
      TSNode decl = ts_node_named_child(ch, j);
      if (t(decl) != "variable_declarator") continue;
      TSNode name = ts_node_child_by_field_name(decl, "name", 4);
      TSNode value =
        js_unwrap_parens(ts_node_child_by_field_name(decl, "value", 5));
      if (ts_node_is_null(name) || t(name) != "identifier" ||
          !js_is_function_expr(value))
        continue;
      GSGNode fn = build_function_expr(value, src);
      fn.name = string(slice(src, name));
      funcs.emplace_back(std::move(fn));
    }
  }
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
  push_header(sw, ts_node_child_by_field_name(n, "value", 5), src);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  uint32_t kmax = ts_node_is_null(body) ? 0 : ts_node_named_child_count(body);
  for (uint32_t k = 0; k < kmax; ++k) {
    TSNode cc = ts_node_named_child(body, k);
    string cty = t(cc);
    if (cty != "switch_case" && cty != "switch_default") continue;
    GSGNode cs;
    cs.kind = GSGNodeKind::Case;
    cs.loc = loc(cc);
    TSNode value = ts_node_child_by_field_name(cc, "value", 5);
    uint32_t cm = ts_node_named_child_count(cc);
    for (uint32_t i = 0; i < cm; ++i) {
      TSNode ch = ts_node_named_child(cc, i);
      if (!ts_node_eq(ch, value)) build_statement(ch, src, cs.children);
    }
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
  for (int i = 0; i < m; ++i)
    build_statement(ts_node_named_child(n, i), src, out);
}

void JavaScriptGSGBuilder::build_statement(TSNode s, const string& src,
                                           std::vector<GSGNode>& out) {
  if (ts_node_is_null(s)) return;
  string ty = t(s);
  if (ty == "if_statement")
    build_if(s, src, out);
  else if (ty == "while_statement")
    out.emplace_back(build_while(s, src));
  else if (ty == "for_statement" || ty == "for_in_statement")
    out.emplace_back(build_for(s, src));
  else if (ty == "do_statement")
    out.emplace_back(build_do_while(s, src));
  else if (js_is_function_decl(ty))
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
  } else {
    push_expr(out, s, s, src);
  }
}

void JavaScriptGSGBuilder::build_if(TSNode n, const string& src,
                                    std::vector<GSGNode>& out,
                                    GSGNodeKind kind) {
  GSGNode g;
  g.kind = kind;
  g.loc = loc(n);
  push_header(g, ts_node_child_by_field_name(n, "condition", 9), src);
  build_statement(ts_node_child_by_field_name(n, "consequence", 11), src,
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
  build_statement(ts_node_child_by_field_name(n, "body", 4), src, g.children);
  return g;
}

GSGNode JavaScriptGSGBuilder::build_for(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::For;
  g.loc = loc(n);
  for (string_view f :
       {"initializer", "condition", "increment", "left", "right"})
    push_header(g, ts_node_child_by_field_name(n, f.data(), f.size()), src);
  build_statement(ts_node_child_by_field_name(n, "body", 4), src, g.children);
  return g;
}

GSGNode JavaScriptGSGBuilder::build_do_while(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::DoWhile;
  g.loc = loc(n);
  push_header(g, ts_node_child_by_field_name(n, "condition", 9), src);
  build_statement(ts_node_child_by_field_name(n, "body", 4), src, g.children);
  return g;
}
