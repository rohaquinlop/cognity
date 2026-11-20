#include "../../include/builders/gsg_builder_base.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using std::string;
using std::string_view;

SourceLoc ExprBuilderBase::loc(TSNode n) {
  TSPoint p = ts_node_start_point(n), q = ts_node_end_point(n);
  return SourceLoc{.row = p.row,
                   .start_col = p.column,
                   .end_col = q.column,
                   .start_byte = ts_node_start_byte(n),
                   .end_byte = ts_node_end_byte(n)};
}

string_view ExprBuilderBase::slice(const string& src, TSNode n) {
  const uint32_t a = ts_node_start_byte(n);
  const uint32_t b = ts_node_end_byte(n);
  return string_view(src).substr(a, b - a);
}

string ExprBuilderBase::grammar_type(TSNode n) {
  return {ts_node_grammar_type(n)};
}

TSNode ExprBuilderBase::child_field(TSNode n, string_view name) {
  return ts_node_child_by_field_name(n, name.data(),
                                     static_cast<uint32_t>(name.size()));
}

bool ExprBuilderBase::type_in(const TypeList& types, string_view ty) {
  return std::ranges::find(types, ty) != types.end();
}

TSNode ExprBuilderBase::unwrap_parens(TSNode n) const {
  while (!ts_node_is_null(n) &&
         type_in(expr_spec_.paren_types, grammar_type(n))) {
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

string_view ExprBuilderBase::bool_op(TSNode n) const {
  if (ts_node_is_null(n) || !type_in(expr_spec_.bool_types, grammar_type(n)))
    return {};
  string_view op{ts_node_type(child_field(n, "operator"))};
  if (type_in(expr_spec_.and_ops, op)) return "&&";
  if (type_in(expr_spec_.or_ops, op)) return "||";
  return {};
}

void ExprBuilderBase::collect_expr(TSNode root, const string& src,
                                   GSGNode& into) {
  enum class Step : uint8_t { Expr, Operand, Op, ChainEnd };
  std::vector<std::pair<TSNode, Step>> stack{{root, Step::Expr}};
  std::vector<string_view> previous;
  auto expand = [&stack](TSNode b) {
    stack.emplace_back(child_field(b, "right"), Step::Operand);
    stack.emplace_back(b, Step::Op);
    stack.emplace_back(child_field(b, "left"), Step::Operand);
  };
  while (!stack.empty()) {
    auto [n, step] = stack.back();
    stack.pop_back();
    if (step == Step::ChainEnd) {
      previous.pop_back();
    } else if (step == Step::Op) {
      string_view op = bool_op(n);
      if (op != previous.back()) into.addl_cost += 1;
      previous.back() = op;
    } else if (step == Step::Operand) {
      TSNode inner = unwrap_parens(n);
      if (!bool_op(inner).empty())
        expand(inner);
      else
        stack.emplace_back(n, Step::Expr);
    } else if (ts_node_is_null(n)) {
      continue;
    } else if (!bool_op(n).empty()) {
      previous.emplace_back();
      stack.emplace_back(n, Step::ChainEnd);
      expand(n);
    } else if (!collect_special(n, src, into, ts_node_eq(n, root))) {
      for (uint32_t i = ts_node_named_child_count(n); i > 0; --i)
        stack.emplace_back(ts_node_named_child(n, i - 1), Step::Expr);
    }
  }
}

void ExprBuilderBase::push_expr(std::vector<GSGNode>& out, TSNode loc_node,
                                TSNode expr, const string& src) {
  GSGNode e;
  e.kind = GSGNodeKind::Expr;
  e.loc = loc(loc_node);
  collect_expr(expr, src, e);
  if (e.addl_cost || !e.children.empty()) out.emplace_back(std::move(e));
}

void ExprBuilderBase::push_header(GSGNode& stmt, TSNode expr,
                                  const string& src) {
  if (ts_node_is_null(expr)) return;
  GSGNode h;
  h.kind = GSGNodeKind::Header;
  h.loc = loc(expr);
  collect_expr(expr, src, h);
  if (h.addl_cost || !h.children.empty())
    stmt.children.emplace_back(std::move(h));
}

bool StmtBuilderBase::collect_special(TSNode n, const string& src,
                                      GSGNode& into, bool is_root) {
  if (collect_function_like(n, src, into)) return true;
  if (!is_root && build_known_statement(n, src, into.children)) return true;
  if (type_in(spec_.ternary_types, grammar_type(n))) {
    build_ternary(n, src, into);
    return true;
  }
  return false;
}

void StmtBuilderBase::build_ternary(TSNode n, const string& src,
                                    GSGNode& into) {
  GSGNode tern;
  tern.kind = GSGNodeKind::Ternary;
  tern.loc = loc(n);
  push_header(tern, child_field(n, "condition"), src);
  collect_expr(child_field(n, "consequence"), src, tern);
  collect_expr(child_field(n, "alternative"), src, tern);
  into.children.emplace_back(std::move(tern));
}

void StmtBuilderBase::build_block_children(TSNode n, const string& src,
                                           std::vector<GSGNode>& out) {
  if (ts_node_is_null(n)) return;
  uint32_t m = ts_node_named_child_count(n);
  for (uint32_t i = 0; i < m; ++i)
    build_statement(ts_node_named_child(n, i), src, out);
}

void StmtBuilderBase::build_statement(TSNode s, const string& src,
                                      std::vector<GSGNode>& out) {
  if (ts_node_is_null(s)) return;
  if (!build_known_statement(s, src, out)) push_expr(out, s, s, src);
}

bool StmtBuilderBase::build_known_statement(TSNode s, const string& src,
                                            std::vector<GSGNode>& out) {
  static const TypeList kCondition = {"condition"};
  string ty = grammar_type(s);
  if (type_in(spec_.if_types, ty))
    build_if(s, src, out, GSGNodeKind::If);
  else if (type_in(spec_.while_types, ty))
    out.emplace_back(build_loop(s, src, GSGNodeKind::While, kCondition));
  else if (type_in(spec_.for_types, ty))
    out.emplace_back(
      build_loop(s, src, GSGNodeKind::For, spec_.for_header_fields));
  else if (type_in(spec_.do_types, ty))
    out.emplace_back(build_loop(s, src, GSGNodeKind::DoWhile, kCondition));
  else if (type_in(spec_.switch_types, ty))
    build_switch(s, src, out);
  else if (type_in(spec_.try_types, ty))
    build_try(s, src, out);
  else if (type_in(spec_.block_types, ty))
    build_block_children(s, src, out);
  else if (ty == "expression_statement") {
    if (ts_node_named_child_count(s) > 0) {
      TSNode expr = ts_node_named_child(s, 0);
      push_expr(out, expr, expr, src);
    }
  } else {
    return build_jump(s, ty, out);
  }
  return true;
}

bool StmtBuilderBase::build_jump(TSNode s, string_view ty,
                                 std::vector<GSGNode>& out) {
  for (const auto& [type, required] : spec_.jumps) {
    if (type != ty) continue;
    if (required.empty() || !ts_node_is_null(child_field(s, required))) {
      GSGNode j;
      j.kind = GSGNodeKind::Jump;
      j.loc = loc(s);
      out.emplace_back(std::move(j));
    }
    return true;
  }
  return false;
}

void StmtBuilderBase::build_if(TSNode n, const string& src,
                               std::vector<GSGNode>& out, GSGNodeKind kind) {
  GSGNode g;
  g.kind = kind;
  g.loc = loc(n);
  push_header(g, child_field(n, "condition"), src);
  build_statement(child_field(n, "consequence"), src, g.children);
  out.emplace_back(std::move(g));
  TSNode alt = child_field(n, "alternative");
  if (ts_node_is_null(alt)) return;
  if (type_in(spec_.if_types, grammar_type(alt))) {
    build_if(alt, src, out, GSGNodeKind::ElseIf);
    return;
  }
  if (ts_node_named_child_count(alt) == 1) {
    TSNode only = ts_node_named_child(alt, 0);
    if (type_in(spec_.if_types, grammar_type(only))) {
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

GSGNode StmtBuilderBase::build_loop(TSNode n, const string& src,
                                    GSGNodeKind kind,
                                    const TypeList& header_fields) {
  GSGNode g;
  g.kind = kind;
  g.loc = loc(n);
  for (string_view f : header_fields) push_header(g, child_field(n, f), src);
  build_statement(child_field(n, "body"), src, g.children);
  return g;
}

void StmtBuilderBase::build_switch(TSNode n, const string& src,
                                   std::vector<GSGNode>& out) {
  GSGNode sw;
  sw.kind = GSGNodeKind::Switch;
  sw.loc = loc(n);
  push_header(sw, child_field(n, spec_.switch_header_field), src);
  TSNode body = child_field(n, "body");
  uint32_t cn = ts_node_is_null(body) ? 0 : ts_node_named_child_count(body);
  for (uint32_t j = 0; j < cn; ++j) {
    TSNode cc = ts_node_named_child(body, j);
    GSGNode cs;
    cs.kind = GSGNodeKind::Case;
    cs.loc = loc(cc);
    if (type_in(spec_.case_types, grammar_type(cc))) {
      TSNode value = child_field(cc, "value");
      uint32_t cm = ts_node_named_child_count(cc);
      for (uint32_t i = 0; i < cm; ++i) {
        TSNode ch = ts_node_named_child(cc, i);
        if (!ts_node_eq(ch, value)) build_statement(ch, src, cs.children);
      }
    } else {
      build_statement(cc, src, cs.children);
    }
    sw.children.emplace_back(std::move(cs));
  }
  out.emplace_back(std::move(sw));
}

void StmtBuilderBase::build_try(TSNode n, const string& src,
                                std::vector<GSGNode>& out) {
  GSGNode tr;
  tr.kind = GSGNodeKind::Try;
  tr.loc = loc(n);
  build_block_children(child_field(n, "body"), src, tr.children);
  out.emplace_back(std::move(tr));
  uint32_t cn = ts_node_named_child_count(n);
  for (uint32_t j = 0; j < cn; ++j) {
    TSNode cc = ts_node_named_child(n, j);
    string ty = grammar_type(cc);
    GSGNode c;
    if (type_in(spec_.catch_types, ty))
      c.kind = GSGNodeKind::Catch;
    else if (type_in(spec_.finally_types, ty))
      c.kind = GSGNodeKind::Finally;
    else
      continue;
    c.loc = loc(cc);
    build_block_children(child_field(cc, "body"), src, c.children);
    out.emplace_back(std::move(c));
  }
}
