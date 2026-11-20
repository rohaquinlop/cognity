#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../include/builders/c_gsg_builder.h"

using std::string;
using std::string_view;

static inline string t(TSNode n) { return string(ts_node_grammar_type(n)); }

static string compute_ancestor_qual(TSNode n, const string& src) {
  std::vector<string> parts;
  TSNode cur = ts_node_parent(n);
  while (!ts_node_is_null(cur)) {
    string ty = t(cur);
    if (ty == "class_specifier" || ty == "struct_specifier" ||
        ty == "union_specifier") {
      TSNode nm = ts_node_child_by_field_name(cur, "name", 4);
      if (!ts_node_is_null(nm))
        parts.push_back(string(string_view(src).substr(
          ts_node_start_byte(nm),
          ts_node_end_byte(nm) - ts_node_start_byte(nm))));
    } else if (ty == "namespace_definition") {
      TSNode nm = ts_node_child_by_field_name(cur, "name", 4);
      if (!ts_node_is_null(nm))
        parts.push_back(string(string_view(src).substr(
          ts_node_start_byte(nm),
          ts_node_end_byte(nm) - ts_node_start_byte(nm))));
    }
    cur = ts_node_parent(cur);
  }
  if (parts.empty()) return string{};
  std::reverse(parts.begin(), parts.end());
  string out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) out += "::";
    out += parts[i];
  }
  return out;
}

SourceLoc CLikeGSGBuilder::loc(TSNode n) {
  TSPoint p = ts_node_start_point(n), q = ts_node_end_point(n);
  return SourceLoc{.row = p.row,
                   .start_col = p.column,
                   .end_col = q.column,
                   .start_byte = ts_node_start_byte(n),
                   .end_byte = ts_node_end_byte(n)};
}

string_view CLikeGSGBuilder::slice(const string& src, TSNode n) {
  const uint32_t a = ts_node_start_byte(n);
  const uint32_t b = ts_node_end_byte(n);
  return string_view(src).substr(a, b - a);
}

std::vector<GSGNode> CLikeGSGBuilder::build_functions(TSNode root,
                                                      const string& src) {
  std::vector<GSGNode> funcs;
  collect_functions_in_scope(root, src, /*qual*/ "", funcs);
  return funcs;
}

void CLikeGSGBuilder::collect_functions_in_scope(TSNode n, const string& src,
                                                 const string& qual,
                                                 std::vector<GSGNode>& out) {
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i) {
    TSNode ch = ts_node_named_child(n, i);
    string ty = t(ch);
    // debug: std::cerr << "[CLike] node type: " << ty << "\n";
    if (ty == "function_definition") {
      string aq = compute_ancestor_qual(ch, src);
      string merged;
      if (qual.empty())
        merged = aq;
      else if (aq.empty())
        merged = qual;
      else if (aq == qual)
        merged = qual;
      else if (aq.rfind(qual + "::", 0) == 0)
        merged = aq;
      else if (qual.rfind(aq + "::", 0) == 0)
        merged = qual;
      else
        merged = qual + "::" + aq;
      out.emplace_back(build_function(ch, src, merged));
    } else if (ty == "template_declaration") {
      int tn = ts_node_named_child_count(ch);
      for (int ti = 0; ti < tn; ++ti) {
        TSNode inner = ts_node_named_child(ch, ti);
        string ity = t(inner);
        // std::cerr << "[CLike] template child: " << ity << "\n";
        if (ity == "function_definition") {
          out.emplace_back(build_function(inner, src, qual));
        } else if (ity == "field_declaration_list") {
          string q = qual;
          if (ti - 1 >= 0) {
            TSNode prev = ts_node_named_child(ch, ti - 1);
            if (!ts_node_is_null(prev) && t(prev) == "identifier") {
              string cn = string(slice(src, prev));
              q = q.empty() ? cn : (qual + "::" + cn);
            }
          }
          collect_functions_in_scope(inner, src, q, out);
        } else if (ity == "declaration" || ity == "class_specifier" ||
                   ity == "struct_specifier" || ity == "namespace_definition" ||
                   ity == "template_declaration") {
          collect_functions_in_scope(inner, src, qual, out);
        }
      }
    } else if (ty == "class_specifier" || ty == "struct_specifier" ||
               ty == "union_specifier") {
      TSNode nm = ts_node_child_by_field_name(ch, "name", 4);
      string q = qual;
      if (!ts_node_is_null(nm)) {
        string cn = string(slice(src, nm));
        q = q.empty() ? cn : (qual + "::" + cn);
      }
      TSNode body = ts_node_child_by_field_name(ch, "body", 4);
      if (!ts_node_is_null(body)) collect_functions_in_scope(body, src, q, out);
    } else if (ty == "namespace_definition") {
      TSNode nm = ts_node_child_by_field_name(ch, "name", 4);
      string q = qual;
      if (!ts_node_is_null(nm)) {
        string nn = string(slice(src, nm));
        q = q.empty() ? nn : (qual + "::" + nn);
      }
      TSNode body = ts_node_child_by_field_name(ch, "body", 4);
      if (!ts_node_is_null(body)) collect_functions_in_scope(body, src, q, out);
    } else {
      collect_functions_in_scope(ch, src, qual, out);
    }
  }
}

string CLikeGSGBuilder::function_name_from_declarator(TSNode decl,
                                                      const string& src) {
  string_view full = slice(src, decl);
  size_t p = full.find('(');
  if (p != string::npos) {
    string pre(full.substr(0, p));
    auto trim = [](string& s) {
      size_t a = s.find_first_not_of(" \t\n");
      size_t b = s.find_last_not_of(" \t\n");
      if (a == string::npos)
        s.clear();
      else
        s = s.substr(a, b - a + 1);
    };
    trim(pre);
    while (!pre.empty() && (pre[0] == '*' || pre[0] == '&' || pre[0] == '('))
      pre.erase(pre.begin());
    trim(pre);
    if (!pre.empty()) return pre;
  }
  int n = ts_node_named_child_count(decl);
  for (int i = 0; i < n; ++i) {
    TSNode ch = ts_node_named_child(decl, i);
    string ty = t(ch);
    if (ty == "identifier" || ty == "field_identifier")
      return string(slice(src, ch));
    string nested = function_name_from_declarator(ch, src);
    if (!nested.empty()) return nested;
  }
  return string{};
}

GSGNode CLikeGSGBuilder::build_function(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::Function;
  g.loc = loc(n);
  TSNode decl = ts_node_child_by_field_name(n, "declarator", 10);
  if (!ts_node_is_null(decl)) g.name = function_name_from_declarator(decl, src);
  build_block_children(ts_node_child_by_field_name(n, "body", 4), src,
                       g.children);
  return g;
}

GSGNode CLikeGSGBuilder::build_function(TSNode n, const string& src,
                                        const string& qual) {
  GSGNode g = build_function(n, src);
  if (!qual.empty() && !g.name.empty()) {
    string prefix = qual + "::";
    if (g.name.rfind(prefix, 0) != 0) g.name = prefix + g.name;
  }
  return g;
}

void CLikeGSGBuilder::push_expr(std::vector<GSGNode>& out, TSNode loc_node,
                                TSNode expr, const string& src) {
  GSGNode e;
  e.kind = GSGNodeKind::Expr;
  e.loc = loc(loc_node);
  collect_expr(expr, src, e);
  if (e.addl_cost || !e.children.empty()) out.emplace_back(std::move(e));
}

void CLikeGSGBuilder::push_header(GSGNode& stmt, TSNode expr,
                                  const string& src) {
  if (ts_node_is_null(expr)) return;
  GSGNode h;
  h.kind = GSGNodeKind::Header;
  h.loc = loc(expr);
  collect_expr(expr, src, h);
  if (h.addl_cost || !h.children.empty())
    stmt.children.emplace_back(std::move(h));
}

void CLikeGSGBuilder::build_switch(TSNode s, const string& src,
                                   std::vector<GSGNode>& out) {
  GSGNode sw;
  sw.kind = GSGNodeKind::Switch;
  sw.loc = loc(s);
  push_header(sw, ts_node_child_by_field_name(s, "condition", 9), src);
  TSNode body = ts_node_child_by_field_name(s, "body", 4);
  uint32_t cn = ts_node_is_null(body) ? 0 : ts_node_named_child_count(body);
  for (uint32_t j = 0; j < cn; ++j) {
    TSNode cc = ts_node_named_child(body, j);
    string cty = t(cc);
    GSGNode cs;
    cs.kind = GSGNodeKind::Case;
    cs.loc = loc(cc);
    if (cty == "case_statement" || cty == "default_statement") {
      TSNode value = ts_node_child_by_field_name(cc, "value", 5);
      uint32_t cm = ts_node_named_child_count(cc);
      for (uint32_t i = 0; i < cm; ++i) {
        TSNode ch = ts_node_named_child(cc, i);
        if (!ts_node_eq(ch, value)) build_statement(ch, src, cs.children);
      }
    } else
      build_statement(cc, src, cs.children);
    sw.children.emplace_back(std::move(cs));
  }
  out.emplace_back(std::move(sw));
}

void CLikeGSGBuilder::build_try(TSNode s, const string& src,
                                std::vector<GSGNode>& out) {
  GSGNode tr;
  tr.kind = GSGNodeKind::Try;
  tr.loc = loc(s);
  build_block_children(ts_node_child_by_field_name(s, "body", 4), src,
                       tr.children);
  out.emplace_back(std::move(tr));
  uint32_t cn = ts_node_named_child_count(s);
  for (uint32_t j = 0; j < cn; ++j) {
    TSNode cc = ts_node_named_child(s, j);
    if (t(cc) != "catch_clause") continue;
    GSGNode c;
    c.kind = GSGNodeKind::Catch;
    c.loc = loc(cc);
    build_block_children(ts_node_child_by_field_name(cc, "body", 4), src,
                         c.children);
    out.emplace_back(std::move(c));
  }
}

void CLikeGSGBuilder::build_block_children(TSNode n, const string& src,
                                           std::vector<GSGNode>& out) {
  if (ts_node_is_null(n)) return;
  int m = ts_node_named_child_count(n);
  for (int i = 0; i < m; ++i)
    build_statement(ts_node_named_child(n, i), src, out);
}

void CLikeGSGBuilder::build_statement(TSNode s, const string& src,
                                      std::vector<GSGNode>& out) {
  if (ts_node_is_null(s)) return;
  string ty = t(s);
  if (ty == "if_statement")
    build_if(s, src, out);
  else if (ty == "while_statement")
    out.emplace_back(build_while(s, src));
  else if (ty == "for_statement" || ty == "for_range_loop")
    out.emplace_back(build_for(s, src));
  else if (ty == "do_statement")
    out.emplace_back(build_do_while(s, src));
  else if (ty == "switch_statement")
    build_switch(s, src, out);
  else if (ty == "try_statement")
    build_try(s, src, out);
  else if (ty == "compound_statement")
    build_block_children(s, src, out);
  else if (ty == "goto_statement") {
    GSGNode j;
    j.kind = GSGNodeKind::Jump;
    j.loc = loc(s);
    out.emplace_back(std::move(j));
  } else if (ty == "expression_statement") {
    if (ts_node_named_child_count(s) > 0) {
      TSNode expr = ts_node_named_child(s, 0);
      push_expr(out, expr, expr, src);
    }
  } else {
    push_expr(out, s, s, src);
  }
}

void CLikeGSGBuilder::build_if(TSNode n, const string& src,
                               std::vector<GSGNode>& out, GSGNodeKind kind) {
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

GSGNode CLikeGSGBuilder::build_while(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::While;
  g.loc = loc(n);
  push_header(g, ts_node_child_by_field_name(n, "condition", 9), src);
  build_statement(ts_node_child_by_field_name(n, "body", 4), src, g.children);
  return g;
}

GSGNode CLikeGSGBuilder::build_for(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::For;
  g.loc = loc(n);
  for (string_view f : {"initializer", "condition", "update", "right"})
    push_header(g, ts_node_child_by_field_name(n, f.data(), f.size()), src);
  build_statement(ts_node_child_by_field_name(n, "body", 4), src, g.children);
  return g;
}

GSGNode CLikeGSGBuilder::build_do_while(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::DoWhile;
  g.loc = loc(n);
  push_header(g, ts_node_child_by_field_name(n, "condition", 9), src);
  build_statement(ts_node_child_by_field_name(n, "body", 4), src, g.children);
  return g;
}

static TSNode c_unwrap_parens(TSNode n) {
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

static bool c_is_statement(const string& ty) {
  return ty == "if_statement" || ty == "while_statement" ||
         ty == "for_statement" || ty == "for_range_loop" ||
         ty == "do_statement" || ty == "switch_statement" ||
         ty == "try_statement" || ty == "compound_statement" ||
         ty == "return_statement" || ty == "throw_statement" ||
         ty == "declaration" || ty == "expression_statement" ||
         ty == "goto_statement";
}

static string_view c_bool_op(TSNode n) {
  if (ts_node_is_null(n) || t(n) != "binary_expression") return {};
  string_view op{ts_node_type(ts_node_child_by_field_name(n, "operator", 8))};
  if (op == "&&" || op == "and") return "&&";
  if (op == "||" || op == "or") return "||";
  return {};
}

void CLikeGSGBuilder::collect_expr(TSNode root, const string& src,
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
      string_view op = c_bool_op(n);
      if (op != previous.back()) into.addl_cost += 1;
      previous.back() = op;
    } else if (step == Step::Operand) {
      TSNode inner = c_unwrap_parens(n);
      if (!c_bool_op(inner).empty())
        expand(inner);
      else
        stack.emplace_back(n, Step::Expr);
    } else if (ts_node_is_null(n)) {
      continue;
    } else if (!c_bool_op(n).empty()) {
      previous.emplace_back();
      stack.emplace_back(n, Step::ChainEnd);
      expand(n);
    } else if (!collect_special(n, src, into, ts_node_eq(n, root))) {
      for (uint32_t i = ts_node_named_child_count(n); i > 0; --i)
        stack.emplace_back(ts_node_named_child(n, i - 1), Step::Expr);
    }
  }
}

bool CLikeGSGBuilder::collect_special(TSNode n, const string& src,
                                      GSGNode& into, bool is_root) {
  string ty = t(n);
  if (ty == "lambda_expression") {
    into.children.emplace_back(build_lambda(n, src));
    return true;
  }
  if (ty == "function_definition") {
    into.children.emplace_back(build_function(n, src));
    return true;
  }
  if (!is_root && c_is_statement(ty)) {
    build_statement(n, src, into.children);
    return true;
  }
  if (ty == "conditional_expression") {
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

GSGNode CLikeGSGBuilder::build_lambda(TSNode n, const string& src) {
  GSGNode g;
  g.kind = GSGNodeKind::Function;
  g.loc = loc(n);
  g.name = "lambda @ " + std::to_string(g.loc.row) + ":" +
           std::to_string(g.loc.start_col);
  TSNode body = ts_node_child_by_field_name(n, "body", 4);
  if (!ts_node_is_null(body)) {
    build_block_children(body, src, g.children);
  }
  return g;
}
