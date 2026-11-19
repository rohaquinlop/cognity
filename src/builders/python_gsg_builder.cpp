#include <string>
#include <string_view>
#include <vector>

#include "../../include/builders/python_gsg_builder.h"

using std::string;
using std::string_view;

static inline string node_type(TSNode n) {
  return string(ts_node_grammar_type(n));
}

SourceLoc PythonGSGBuilder::loc_from_node(TSNode node) {
  TSPoint p = ts_node_start_point(node), q = ts_node_end_point(node);
  return SourceLoc{p.row, p.column, q.column};
}

string_view PythonGSGBuilder::slice_source(const string& source, TSNode node) {
  const uint32_t a = ts_node_start_byte(node);
  const uint32_t b = ts_node_end_byte(node);
  return string_view(source).substr(a, b - a);
}

string_view PythonGSGBuilder::get_identifier(TSNode node,
                                             const string& source) {
  TSNode name = ts_node_child_by_field_name(node, "name", 4);
  return slice_source(source, name);
}

static TSNode unwrap_parens(TSNode node) {
  while (!ts_node_is_null(node) &&
         node_type(node) == "parenthesized_expression" &&
         ts_node_named_child_count(node) == 1)
    node = ts_node_named_child(node, 0);
  return node;
}

void PythonGSGBuilder::collect_bool_chain(TSNode node, const string& source,
                                          GSGNode& into, string& previous) {
  for (const char* side : {"left", "right"}) {
    if (side[0] == 'r') {
      string op = node_type(ts_node_child_by_field_name(node, "operator", 8));
      if (op != previous) into.addl_cost += 1;
      previous = op;
    }
    TSNode operand =
      ts_node_child_by_field_name(node, side, side[0] == 'l' ? 4 : 5);
    TSNode inner = unwrap_parens(operand);
    if (!ts_node_is_null(inner) && node_type(inner) == "boolean_operator")
      collect_bool_chain(inner, source, into, previous);
    else
      collect_expr(operand, source, into);
  }
}

void PythonGSGBuilder::push_defaults(GSGNode& fn, TSNode parameters,
                                     const string& source) {
  if (ts_node_is_null(parameters)) return;
  uint32_t n = ts_node_named_child_count(parameters);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode p = ts_node_named_child(parameters, i);
    string pt = node_type(p);
    if (pt == "default_parameter" || pt == "typed_default_parameter")
      push_header(fn, ts_node_child_by_field_name(p, "value", 5), source);
  }
}

void PythonGSGBuilder::collect_expr(TSNode node, const string& source,
                                    GSGNode& into) {
  if (ts_node_is_null(node)) return;
  string t = node_type(node);
  if (t == "boolean_operator") {
    string previous;
    collect_bool_chain(node, source, into, previous);
    return;
  }
  if (t == "lambda") {
    GSGNode fn = expr_node(GSGNodeKind::Function, node);
    push_defaults(fn, ts_node_child_by_field_name(node, "parameters", 10),
                  source);
    collect_expr(ts_node_child_by_field_name(node, "body", 4), source, fn);
    into.children.emplace_back(std::move(fn));
    return;
  }
  if (t == "conditional_expression") {
    GSGNode tern;
    tern.kind = GSGNodeKind::Ternary;
    tern.loc = loc_from_node(node);
    int part = 0;
    uint32_t m = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < m; ++i) {
      TSNode ch = ts_node_named_child(node, i);
      if (ts_node_is_extra(ch)) continue;
      if (part++ == 1)
        push_header(tern, ch, source);
      else
        collect_expr(ch, source, tern);
    }
    into.children.emplace_back(std::move(tern));
    return;
  }
  int m = ts_node_named_child_count(node);
  for (int i = 0; i < m; ++i)
    collect_expr(ts_node_named_child(node, i), source, into);
}

GSGNode PythonGSGBuilder::expr_node(GSGNodeKind kind, TSNode loc_node) {
  GSGNode e;
  e.kind = kind;
  e.loc = loc_from_node(loc_node);
  return e;
}

void PythonGSGBuilder::push_expr(std::vector<GSGNode>& out, TSNode loc_node,
                                 TSNode expr, const string& source) {
  GSGNode e = expr_node(GSGNodeKind::Expr, loc_node);
  collect_expr(expr, source, e);
  if (e.addl_cost || !e.children.empty()) out.emplace_back(std::move(e));
}

void PythonGSGBuilder::push_header(GSGNode& stmt, TSNode expr,
                                   const string& source) {
  if (ts_node_is_null(expr)) return;
  GSGNode h = expr_node(GSGNodeKind::Header, expr);
  collect_expr(expr, source, h);
  if (h.addl_cost || !h.children.empty())
    stmt.children.emplace_back(std::move(h));
}

void PythonGSGBuilder::push_else(TSNode else_clause, const string& source,
                                 std::vector<GSGNode>& out) {
  if (ts_node_is_null(else_clause)) return;
  GSGNode el = expr_node(GSGNodeKind::Else, else_clause);
  build_block_children(clause_body(else_clause), source, el.children);
  out.emplace_back(std::move(el));
}

TSNode PythonGSGBuilder::clause_body(TSNode clause) {
  TSNode body = ts_node_child_by_field_name(clause, "body", 4);
  if (!ts_node_is_null(body)) return body;
  uint32_t n = ts_node_named_child_count(clause);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode cand = ts_node_named_child(clause, i);
    if (node_type(cand) == "block") return cand;
  }
  return body;
}

std::vector<GSGNode> PythonGSGBuilder::build_functions(TSNode root,
                                                       const string& source) {
  std::vector<GSGNode> funcs;
  int n = ts_node_named_child_count(root);
  for (int i = 0; i < n; ++i) {
    TSNode child = ts_node_named_child(root, i);
    auto t = node_type(child);
    if (t == "function_definition") {
      funcs.emplace_back(build_function(child, source));
    } else if (t == "decorated_definition") {
      TSNode def = ts_node_child_by_field_name(child, "definition", 10);
      if (!ts_node_is_null(def) && node_type(def) == "function_definition")
        funcs.emplace_back(build_function(def, source));
      else if (!ts_node_is_null(def) && node_type(def) == "class_definition") {
        TSNode body = ts_node_child_by_field_name(def, "body", 4);
        if (!ts_node_is_null(body)) {
          int m = ts_node_named_child_count(body);
          for (int j = 0; j < m; ++j) {
            TSNode member = ts_node_named_child(body, j);
            if (node_type(member) == "function_definition")
              funcs.emplace_back(build_function(member, source));
          }
        }
      }
    } else if (t == "class_definition") {
      TSNode body = ts_node_child_by_field_name(child, "body", 4);
      if (!ts_node_is_null(body)) {
        int m = ts_node_named_child_count(body);
        for (int j = 0; j < m; ++j) {
          TSNode member = ts_node_named_child(body, j);
          if (node_type(member) == "function_definition")
            funcs.emplace_back(build_function(member, source));
        }
      }
    }
  }
  return funcs;
}

GSGNode PythonGSGBuilder::build_function(TSNode node, const string& source) {
  GSGNode f;
  f.kind = GSGNodeKind::Function;
  f.name = std::string(get_identifier(node, source));
  f.loc = loc_from_node(node);

  TSNode body = ts_node_child_by_field_name(node, "body", 4);
  if (!ts_node_is_null(body)) {
    int bn = ts_node_named_child_count(body);
    if (bn == 2) {
      TSNode first = ts_node_named_child(body, 0);
      TSNode second = ts_node_named_child(body, 1);
      if (node_type(first) == "function_definition" &&
          node_type(second) == "return_statement") {
        TSNode inner_body = ts_node_child_by_field_name(first, "body", 4);
        if (!ts_node_is_null(inner_body)) {
          build_block_children(inner_body, source, f.children);
          return f;
        }
      }
    }
    build_block_children(body, source, f.children);
  }
  return f;
}

void PythonGSGBuilder::build_try(TSNode node, const string& source,
                                 std::vector<GSGNode>& out) {
  GSGNode tr = expr_node(GSGNodeKind::Try, node);
  TSNode body = ts_node_child_by_field_name(node, "body", 4);
  if (!ts_node_is_null(body)) build_block_children(body, source, tr.children);
  out.emplace_back(std::move(tr));

  uint32_t m = ts_node_named_child_count(node);
  for (uint32_t j = 0; j < m; ++j) {
    TSNode ch = ts_node_named_child(node, j);
    string cht = node_type(ch);
    if (cht == "except_clause") {
      GSGNode ex = expr_node(GSGNodeKind::Except, ch);
      TSNode exbody = clause_body(ch);
      uint32_t en = ts_node_named_child_count(ch);
      for (uint32_t k = 0; k < en; ++k) {
        TSNode part = ts_node_named_child(ch, k);
        if (!ts_node_eq(part, exbody) && !ts_node_is_extra(part))
          push_header(ex, part, source);
      }
      build_block_children(exbody, source, ex.children);
      out.emplace_back(std::move(ex));
    } else if (cht == "else_clause") {
      push_else(ch, source, out);
    } else if (cht == "finally_clause") {
      GSGNode fin = expr_node(GSGNodeKind::Finally, ch);
      build_block_children(clause_body(ch), source, fin.children);
      out.emplace_back(std::move(fin));
    }
  }
}

void PythonGSGBuilder::build_match(TSNode node, const string& source,
                                   std::vector<GSGNode>& out) {
  GSGNode sw = expr_node(GSGNodeKind::Switch, node);
  TSNode body = ts_node_child_by_field_name(node, "body", 4);
  uint32_t sn = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < sn; ++i) {
    TSNode ch = ts_node_named_child(node, i);
    if (!ts_node_eq(ch, body) && !ts_node_is_extra(ch))
      push_header(sw, ch, source);
  }
  uint32_t n = ts_node_is_null(body) ? 0 : ts_node_named_child_count(body);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode cc = ts_node_named_child(body, i);
    if (node_type(cc) != "case_clause") continue;
    GSGNode cs = expr_node(GSGNodeKind::Case, cc);
    push_header(cs, ts_node_child_by_field_name(cc, "guard", 5), source);
    build_block_children(ts_node_child_by_field_name(cc, "consequence", 11),
                         source, cs.children);
    sw.children.emplace_back(std::move(cs));
  }
  out.emplace_back(std::move(sw));
}

void PythonGSGBuilder::collect_assignment(TSNode node, const string& source,
                                          GSGNode& into) {
  collect_expr(ts_node_child_by_field_name(node, "left", 4), source, into);
  TSNode right = ts_node_child_by_field_name(node, "right", 5);
  if (!ts_node_is_null(right) && node_type(right) == "assignment")
    collect_assignment(right, source, into);
  else
    collect_expr(right, source, into);
}

void PythonGSGBuilder::build_expression_statement(TSNode stmt,
                                                  const string& source,
                                                  std::vector<GSGNode>& out) {
  GSGNode e = expr_node(GSGNodeKind::Expr, stmt);
  uint32_t n = ts_node_named_child_count(stmt);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode sub = ts_node_named_child(stmt, i);
    if (node_type(sub) == "assignment")
      collect_assignment(sub, source, e);
    else
      collect_expr(sub, source, e);
  }
  if (e.addl_cost || !e.children.empty()) out.emplace_back(std::move(e));
}

void PythonGSGBuilder::build_block_children(TSNode block, const string& source,
                                            std::vector<GSGNode>& out) {
  if (ts_node_is_null(block)) return;
  uint32_t n = ts_node_named_child_count(block);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode stmt = ts_node_named_child(block, i);
    string t = node_type(stmt);
    if (t == "for_statement" || t == "while_statement")
      build_loop(stmt, source, out);
    else if (t == "if_statement")
      build_if(stmt, source, out);
    else if (t == "match_statement")
      build_match(stmt, source, out);
    else if (t == "try_statement")
      build_try(stmt, source, out);
    else if (t == "return_statement" || t == "raise_statement" ||
             t == "assert_statement" || t == "delete_statement")
      push_expr(out, stmt, stmt, source);
    else if (t == "with_statement") {
      GSGNode w = expr_node(GSGNodeKind::With, stmt);
      uint32_t m = ts_node_named_child_count(stmt);
      for (uint32_t j = 0; j < m; ++j) {
        TSNode ch = ts_node_named_child(stmt, j);
        if (node_type(ch) == "with_clause") push_header(w, ch, source);
      }
      build_block_children(ts_node_child_by_field_name(stmt, "body", 4), source,
                           w.children);
      out.emplace_back(std::move(w));
    } else if (t == "expression_statement") {
      build_expression_statement(stmt, source, out);
    } else if (t == "function_definition") {
      out.emplace_back(build_function(stmt, source));
    }
  }
}

void PythonGSGBuilder::build_loop(TSNode node, const string& source,
                                  std::vector<GSGNode>& out) {
  GSGNode g = expr_node(
    node_type(node) == "for_statement" ? GSGNodeKind::For : GSGNodeKind::While,
    node);
  for (string_view f : {"condition", "left", "right"})
    push_header(g, ts_node_child_by_field_name(node, f.data(), f.size()),
                source);
  build_block_children(ts_node_child_by_field_name(node, "body", 4), source,
                       g.children);
  out.emplace_back(std::move(g));
  push_else(ts_node_child_by_field_name(node, "alternative", 11), source, out);
}

void PythonGSGBuilder::build_if(TSNode node, const string& source,
                                std::vector<GSGNode>& out) {
  GSGNode g = expr_node(GSGNodeKind::If, node);
  push_header(g, ts_node_child_by_field_name(node, "condition", 9), source);
  build_block_children(ts_node_child_by_field_name(node, "consequence", 11),
                       source, g.children);
  out.emplace_back(std::move(g));

  int n = ts_node_named_child_count(node);
  for (int i = 0; i < n; ++i) {
    TSNode ch = ts_node_named_child(node, i);
    string t = node_type(ch);
    if (t == "elif_clause") {
      GSGNode eif = expr_node(GSGNodeKind::ElseIf, ch);
      push_header(eif, ts_node_child_by_field_name(ch, "condition", 9), source);
      build_block_children(ts_node_child_by_field_name(ch, "consequence", 11),
                           source, eif.children);
      out.emplace_back(std::move(eif));
    } else if (t == "else_clause") {
      push_else(ch, source, out);
    }
  }
}
