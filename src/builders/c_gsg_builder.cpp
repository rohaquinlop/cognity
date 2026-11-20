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

void CLikeGSGBuilder::build_preproc(TSNode n, const string& src,
                                    std::vector<GSGNode>& out,
                                    GSGNodeKind kind) {
  GSGNode g;
  g.kind = kind;
  g.loc = loc(n);
  TSNode cond = ts_node_child_by_field_name(n, "condition", 9);
  TSNode name = ts_node_child_by_field_name(n, "name", 4);
  TSNode alt = ts_node_child_by_field_name(n, "alternative", 11);
  push_header(g, cond, src);
  uint32_t m = ts_node_named_child_count(n);
  for (uint32_t i = 0; i < m; ++i) {
    TSNode ch = ts_node_named_child(n, i);
    if (!ts_node_eq(ch, cond) && !ts_node_eq(ch, name) && !ts_node_eq(ch, alt))
      build_statement(ch, src, g.children);
  }
  out.emplace_back(std::move(g));
  if (ts_node_is_null(alt)) return;
  build_preproc(
    alt, src, out,
    t(alt) == "preproc_else" ? GSGNodeKind::Else : GSGNodeKind::ElseIf);
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

CLikeGSGBuilder::CLikeGSGBuilder(bool cpp)
    : StmtBuilderBase(
        ExprSpec{
          .paren_types = {"parenthesized_expression",
                          "preproc_parenthesized_expression"},
          .bool_types = {"binary_expression", "preproc_binary_expression"},
          .and_ops = {"&&", "and"},
          .or_ops = {"||", "or"}},
        StmtSpec{
          .block_types = {"compound_statement"},
          .if_types = {"if_statement"},
          .while_types = {"while_statement"},
          .do_types = {"do_statement"},
          .for_types = {"for_statement", "for_range_loop"},
          .for_header_fields = {"initializer", "condition", "update", "right"},
          .switch_types = {"switch_statement"},
          .switch_header_field = "condition",
          .case_types = {"case_statement", "default_statement"},
          .try_types = {"try_statement"},
          .catch_types = {"catch_clause"},
          .finally_types = {},
          .ternary_types = {"conditional_expression"},
          .jumps = {{"goto_statement", ""}}}),
      cpp_(cpp) {}

bool CLikeGSGBuilder::collect_function_like(TSNode n, const string& src,
                                            GSGNode& into) {
  string ty = t(n);
  if (ty == "lambda_expression") {
    into.children.emplace_back(build_lambda(n, src));
    return true;
  }
  if (ty == "function_definition") {
    into.children.emplace_back(build_function(n, src));
    return true;
  }
  return false;
}

bool CLikeGSGBuilder::build_known_statement(TSNode s, const string& src,
                                            std::vector<GSGNode>& out) {
  string ty = t(s);
  if (ty != "preproc_if" && ty != "preproc_ifdef")
    return StmtBuilderBase::build_known_statement(s, src, out);
  build_preproc(s, src, out, GSGNodeKind::If);
  return true;
}
