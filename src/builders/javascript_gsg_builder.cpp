#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../include/builders/javascript_gsg_builder.h"

using std::string;
using std::string_view;

static inline string t(TSNode n) { return string(ts_node_grammar_type(n)); }

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

std::vector<GSGNode> JavaScriptGSGBuilder::build_functions(TSNode root,
                                                           const string& src) {
  std::vector<GSGNode> funcs;
  collect_children(root, src, "", funcs);
  return funcs;
}

static TSNode js_field(TSNode n, string_view f) {
  return ts_node_child_by_field_name(n, f.data(),
                                     static_cast<uint32_t>(f.size()));
}

static string js_qualify(const string& qual, string_view name) {
  if (qual.empty()) return string(name);
  return qual + "::" + string(name);
}

static string js_dotted(string_view s) {
  string out;
  for (char c : s) {
    if (c == '.')
      out += "::";
    else if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
      out += c;
  }
  return out;
}

static string_view js_text(const string& src, TSNode n) {
  if (ts_node_is_null(n)) return {};
  const uint32_t a = ts_node_start_byte(n);
  return string_view(src).substr(a, ts_node_end_byte(n) - a);
}

static constexpr std::array<string_view, 3> kJsUnitChildren = {
  "statement_block", "else_clause", "switch_body"};

static constexpr std::array<string_view, 9> kJsUnitBody = {
  "catch_clause",      "finally_clause",  "for_statement",
  "for_in_statement",  "while_statement", "do_statement",
  "labeled_statement", "with_statement",  "switch_statement"};

static bool js_is_member_chain(TSNode n) {
  while (!ts_node_is_null(n) && t(n) == "member_expression")
    n = js_field(n, "object");
  return !ts_node_is_null(n) && (t(n) == "identifier" || t(n) == "this");
}

void JavaScriptGSGBuilder::collect_children(TSNode n, const string& src,
                                            const string& qual,
                                            std::vector<GSGNode>& funcs) {
  if (ts_node_is_null(n)) return;
  uint32_t m = ts_node_named_child_count(n);
  for (uint32_t i = 0; i < m; ++i)
    collect_units(ts_node_named_child(n, i), src, qual, funcs);
}

void JavaScriptGSGBuilder::collect_units(TSNode n, const string& src,
                                         const string& qual,
                                         std::vector<GSGNode>& funcs) {
  if (ts_node_is_null(n)) return;
  string ty = t(n);
  if (ty == "function_declaration" || ty == "generator_function_declaration") {
    GSGNode fn = build_function(n, src);
    fn.name = js_qualify(qual, fn.name.empty() ? "default" : fn.name);
    funcs.emplace_back(std::move(fn));
  } else if (ty == "class_declaration" || ty == "abstract_class_declaration") {
    string_view name = js_text(src, js_field(n, "name"));
    collect_container(n, src, js_qualify(qual, name.empty() ? "default" : name),
                      funcs);
  } else if (ty == "export_statement") {
    collect_units(js_field(n, "declaration"), src, qual, funcs);
    collect_value(js_field(n, "value"), src, js_qualify(qual, "default"),
                  funcs);
  } else if (ty == "lexical_declaration" || ty == "variable_declaration") {
    uint32_t m = ts_node_named_child_count(n);
    for (uint32_t j = 0; j < m; ++j) {
      TSNode decl = ts_node_named_child(n, j);
      if (t(decl) != "variable_declarator") continue;
      TSNode name = js_field(decl, "name");
      if (ts_node_is_null(name) || t(name) != "identifier") continue;
      collect_value(js_field(decl, "value"), src,
                    js_qualify(qual, slice(src, name)), funcs);
    }
  } else if (ty == "internal_module" || ty == "module") {
    TSNode name = js_field(n, "name");
    if (ts_node_is_null(name) || t(name) == "string") return;
    collect_children(js_field(n, "body"), src,
                     js_qualify(qual, js_dotted(slice(src, name))), funcs);
  } else if (ty == "expression_statement") {
    TSNode e = unwrap_parens(ts_node_named_child(n, 0));
    if (ts_node_is_null(e)) return;
    if (t(e) == "internal_module") {
      collect_units(e, src, qual, funcs);
    } else if (t(e) == "assignment_expression") {
      TSNode left = js_field(e, "left");
      if (js_is_member_chain(left))
        collect_value(js_field(e, "right"), src,
                      js_qualify(qual, js_dotted(slice(src, left))), funcs);
    }
  } else if (std::ranges::find(kJsUnitChildren, ty) != kJsUnitChildren.end()) {
    collect_children(n, src, qual, funcs);
  } else if (std::ranges::find(kJsUnitBody, ty) != kJsUnitBody.end()) {
    collect_units(js_field(n, "body"), src, qual, funcs);
  } else if (ty == "if_statement") {
    collect_units(js_field(n, "consequence"), src, qual, funcs);
    collect_units(js_field(n, "alternative"), src, qual, funcs);
  } else if (ty == "try_statement") {
    collect_units(js_field(n, "body"), src, qual, funcs);
    collect_units(js_field(n, "handler"), src, qual, funcs);
    collect_units(js_field(n, "finalizer"), src, qual, funcs);
  } else if (ty == "switch_case" || ty == "switch_default") {
    TSNode value = js_field(n, "value");
    uint32_t m = ts_node_named_child_count(n);
    for (uint32_t j = 0; j < m; ++j) {
      TSNode ch = ts_node_named_child(n, j);
      if (!ts_node_eq(ch, value)) collect_units(ch, src, qual, funcs);
    }
  }
}

void JavaScriptGSGBuilder::collect_value(TSNode v, const string& src,
                                         const string& name,
                                         std::vector<GSGNode>& funcs) {
  v = unwrap_parens(v);
  if (ts_node_is_null(v)) return;
  if (js_is_function_expr(v)) {
    GSGNode fn = build_function_expr(v, src);
    fn.name = name;
    funcs.emplace_back(std::move(fn));
  } else if (t(v) == "class" || t(v) == "object") {
    collect_container(v, src, name, funcs);
  }
}

void JavaScriptGSGBuilder::collect_container(TSNode n, const string& src,
                                             const string& qual,
                                             std::vector<GSGNode>& funcs) {
  TSNode body = t(n) == "object" ? n : js_field(n, "body");
  if (ts_node_is_null(body)) return;
  uint32_t m = ts_node_named_child_count(body);
  for (uint32_t i = 0; i < m; ++i) {
    TSNode mem = ts_node_named_child(body, i);
    string ty = t(mem);
    if (ty == "method_definition") {
      if (ts_node_is_null(js_field(mem, "body"))) continue;
      GSGNode fn = build_function(mem, src);
      fn.name = js_qualify(qual, js_text(src, js_field(mem, "name")));
      funcs.emplace_back(std::move(fn));
    } else if (ty == "field_definition") {
      collect_value(js_field(mem, "value"), src,
                    js_qualify(qual, js_text(src, js_field(mem, "property"))),
                    funcs);
    } else if (ty == "public_field_definition") {
      collect_value(js_field(mem, "value"), src,
                    js_qualify(qual, js_text(src, js_field(mem, "name"))),
                    funcs);
    } else if (ty == "pair") {
      collect_value(js_field(mem, "value"), src,
                    js_qualify(qual, js_text(src, js_field(mem, "key"))),
                    funcs);
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

JavaScriptGSGBuilder::JavaScriptGSGBuilder()
    : StmtBuilderBase(
        ExprSpec{.paren_types = {"parenthesized_expression"},
                 .bool_types = {"binary_expression"},
                 .and_ops = {"&&"},
                 .or_ops = {"||"}},
        StmtSpec{.block_types = {"statement_block"},
                 .if_types = {"if_statement"},
                 .while_types = {"while_statement"},
                 .do_types = {"do_statement"},
                 .for_types = {"for_statement", "for_in_statement"},
                 .for_header_fields = {"initializer", "condition", "increment",
                                       "left", "right"},
                 .switch_types = {"switch_statement"},
                 .switch_header_field = "value",
                 .case_types = {"switch_case", "switch_default"},
                 .try_types = {"try_statement"},
                 .catch_types = {"catch_clause"},
                 .finally_types = {"finally_clause"},
                 .ternary_types = {"ternary_expression"},
                 .jumps = {{"break_statement", "label"},
                           {"continue_statement", "label"}}}) {}

bool JavaScriptGSGBuilder::collect_function_like(TSNode n, const string& src,
                                                 GSGNode& into) {
  if (js_is_function_expr(n)) {
    into.children.emplace_back(build_function_expr(n, src));
    return true;
  }
  if (js_is_function_decl(t(n))) {
    into.children.emplace_back(build_function(n, src));
    return true;
  }
  return false;
}

bool JavaScriptGSGBuilder::build_known_statement(TSNode s, const string& src,
                                                 std::vector<GSGNode>& out) {
  string ty = t(s);
  if (js_is_function_decl(ty)) {
    out.emplace_back(build_function(s, src));
  } else if (ty == "return_statement") {
    TSNode arg = ts_node_child_by_field_name(s, "argument", 8);
    if (ts_node_is_null(arg) && ts_node_named_child_count(s) > 0)
      arg = ts_node_named_child(s, 0);
    if (!ts_node_is_null(arg)) push_expr(out, s, arg, src);
  } else if (ty == "throw_statement") {
    TSNode arg = ts_node_child_by_field_name(s, "argument", 8);
    if (!ts_node_is_null(arg)) push_expr(out, s, arg, src);
  } else {
    return StmtBuilderBase::build_known_statement(s, src, out);
  }
  return true;
}
