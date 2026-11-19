#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../../include/builders/c_gsg_builder.h"
#include "../../include/recursion.h"

namespace {

using std::string;
using std::string_view;

constexpr uint32_t kVariadic = std::numeric_limits<uint32_t>::max();

enum class Kind : uint8_t { Unknown, Integer, Floating, Char, Bool };

struct Def {
  string name;
  string scope;
  uint32_t min_args{0};
  uint32_t max_args{0};
  std::vector<Kind> params;
};

struct Local {
  string name;
  uint32_t from;
  uint32_t start;
  uint32_t end;
};

string_view type_of(TSNode n) { return ts_node_type(n); }

TSNode field(TSNode n, string_view name) {
  if (ts_node_is_null(n)) return n;
  return ts_node_child_by_field_name(n, name.data(), name.size());
}

string scope_of(const string& qualified) {
  const size_t p = qualified.rfind("::");
  return p == string::npos ? string{} : qualified.substr(0, p);
}

TSNode function_declarator(TSNode decl) {
  TSNode found{};
  while (!ts_node_is_null(decl) && type_of(decl).ends_with("_declarator")) {
    if (type_of(decl) == "function_declarator") found = decl;
    TSNode inner = field(decl, "declarator");
    if (ts_node_is_null(inner) && ts_node_named_child_count(decl) > 0)
      inner = ts_node_named_child(decl, ts_node_named_child_count(decl) - 1);
    decl = inner;
  }
  return found;
}

string_view slice(const string& src, TSNode n) {
  return string_view(src).substr(ts_node_start_byte(n),
                                 ts_node_end_byte(n) - ts_node_start_byte(n));
}

Kind param_kind(TSNode p, const string& src) {
  TSNode decl = field(p, "declarator");
  if (!ts_node_is_null(decl) && type_of(decl) == "reference_declarator" &&
      ts_node_named_child_count(decl) == 1)
    decl = ts_node_named_child(decl, 0);
  if (!ts_node_is_null(decl) && type_of(decl) != "identifier")
    return Kind::Unknown;
  TSNode type = field(p, "type");
  if (ts_node_is_null(type)) return Kind::Unknown;
  const string_view text = slice(src, type);
  if (type_of(type) == "sized_type_specifier")
    return text.find("double") != string_view::npos ? Kind::Floating
                                                    : Kind::Integer;
  if (type_of(type) != "primitive_type") return Kind::Unknown;
  if (text == "int" || text == "short" || text == "long") return Kind::Integer;
  if (text == "float" || text == "double") return Kind::Floating;
  if (text == "char") return Kind::Char;
  if (text == "bool") return Kind::Bool;
  return Kind::Unknown;
}

Kind argument_kind(TSNode a, const string& src) {
  string_view t = type_of(a);
  if (t == "unary_expression") {
    const string_view op = slice(src, field(a, "operator"));
    TSNode arg = field(a, "argument");
    return (op == "-" || op == "+") && type_of(arg) == "number_literal"
             ? argument_kind(arg, src)
             : Kind::Unknown;
  }
  if (t == "char_literal") return Kind::Char;
  if (t == "true" || t == "false") return Kind::Bool;
  if (t != "number_literal") return Kind::Unknown;
  string_view text = slice(src, a);
  if (text.starts_with('-') || text.starts_with('+')) text.remove_prefix(1);
  if (text.starts_with("0x") || text.starts_with("0X"))
    return text.find_first_of("pP") != string_view::npos ? Kind::Floating
                                                         : Kind::Integer;
  return text.find_first_of(".eE") != string_view::npos ? Kind::Floating
                                                        : Kind::Integer;
}

void signature(TSNode def, const string& src, Def& d) {
  uint32_t& min_args = d.min_args;
  uint32_t& max_args = d.max_args;
  TSNode params =
    field(function_declarator(field(def, "declarator")), "parameters");
  uint32_t n = ts_node_is_null(params) ? 0 : ts_node_named_child_count(params);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode p = ts_node_named_child(params, i);
    string_view t = type_of(p);
    if (t == "variadic_parameter") {
      max_args = kVariadic;
    } else if (t == "optional_parameter_declaration") {
      ++max_args;
      d.params.push_back(param_kind(p, src));
    } else if (t == "parameter_declaration") {
      TSNode type = field(p, "type");
      if (n == 1 && ts_node_is_null(field(p, "declarator")) &&
          !ts_node_is_null(type) &&
          src.compare(ts_node_start_byte(type),
                      ts_node_end_byte(type) - ts_node_start_byte(type),
                      "void") == 0)
        continue;
      ++min_args;
      ++max_args;
      d.params.push_back(param_kind(p, src));
    }
  }
  if (max_args != kVariadic && max_args < min_args) max_args = min_args;
}

TSNode declared_name(TSNode decl) {
  while (!ts_node_is_null(decl) && type_of(decl) != "identifier") {
    TSNode inner = field(decl, "declarator");
    if (ts_node_is_null(inner) && ts_node_named_child_count(decl) > 0)
      inner = ts_node_named_child(decl, 0);
    decl = inner;
  }
  return decl;
}

bool declares_function(TSNode decl) {
  string_view last;
  while (!ts_node_is_null(decl) && type_of(decl).ends_with("_declarator")) {
    if (type_of(decl) != "parenthesized_declarator") last = type_of(decl);
    TSNode inner = field(decl, "declarator");
    if (ts_node_is_null(inner) && ts_node_named_child_count(decl) > 0)
      inner = ts_node_named_child(decl, ts_node_named_child_count(decl) - 1);
    decl = inner;
  }
  return last == "function_declarator";
}

bool is_region(string_view t) {
  return t == "compound_statement" || t == "for_statement" ||
         t == "for_range_loop" || t == "if_statement" ||
         t == "while_statement" || t == "switch_statement" ||
         t == "catch_clause" || t == "lambda_expression";
}

void collect_locals(TSNode n, TSNode region, const string& src,
                    std::vector<Local>& out) {
  if (ts_node_is_null(n)) return;
  std::vector<std::pair<TSNode, TSNode>> stack{{n, region}};
  while (!stack.empty()) {
    auto [cur, reg] = stack.back();
    stack.pop_back();
    string_view t = type_of(cur);
    if (is_region(t)) reg = cur;
    if (t == "function_declarator" ||
        (t == "abstract_function_declarator" &&
         type_of(ts_node_parent(cur)) != "lambda_expression"))
      continue;
    if (t == "for_range_loop") {
      TSNode decl = field(cur, "declarator");
      while (!ts_node_is_null(decl) && type_of(decl) != "identifier" &&
             type_of(decl) != "structured_binding_declarator") {
        TSNode inner = field(decl, "declarator");
        if (ts_node_is_null(inner) && ts_node_named_child_count(decl) > 0)
          inner = ts_node_named_child(decl, 0);
        decl = inner;
      }
      const bool binding = !ts_node_is_null(decl) &&
                           type_of(decl) == "structured_binding_declarator";
      const uint32_t k = binding ? ts_node_named_child_count(decl) : 1;
      for (uint32_t i = 0; i < k && !ts_node_is_null(decl); ++i) {
        TSNode name = binding ? ts_node_named_child(decl, i) : decl;
        if (type_of(name) != "identifier") continue;
        out.push_back({.name = string(slice(src, name)),
                       .from = ts_node_start_byte(field(cur, "body")),
                       .start = ts_node_start_byte(cur),
                       .end = ts_node_end_byte(cur)});
      }
    }
    if (t == "parameter_declaration" || t == "optional_parameter_declaration" ||
        t == "declaration") {
      uint32_t m = ts_node_child_count(cur);
      for (uint32_t i = 0; i < m; ++i) {
        const char* f = ts_node_field_name_for_child(cur, i);
        if (!f || string_view(f) != "declarator") continue;
        TSNode decl = ts_node_child(cur, i);
        if (t == "declaration" && declares_function(decl)) continue;
        TSNode name = declared_name(decl);
        if (!ts_node_is_null(name))
          out.push_back({.name = string(slice(src, name)),
                         .from = ts_node_end_byte(name),
                         .start = ts_node_start_byte(reg),
                         .end = ts_node_end_byte(reg)});
      }
    }
    uint32_t m = ts_node_named_child_count(cur);
    for (uint32_t i = 0; i < m; ++i)
      stack.emplace_back(ts_node_named_child(cur, i), reg);
  }
}

bool hidden(const std::vector<Local>& locals, const string& name, uint32_t at) {
  return std::ranges::any_of(locals, [&](const Local& l) {
    return l.name == name && l.start <= at && at < l.end && at >= l.from;
  });
}

std::vector<Kind> argument_kinds(TSNode call, const string& src) {
  std::vector<Kind> kinds;
  TSNode args = field(call, "arguments");
  if (ts_node_is_null(args)) return kinds;
  uint32_t n = ts_node_named_child_count(args);
  for (uint32_t i = 0; i < n; ++i) {
    TSNode a = ts_node_named_child(args, i);
    if (!ts_node_is_extra(a)) kinds.push_back(argument_kind(a, src));
  }
  return kinds;
}

/**
 * Narrows C++ overloads by literal argument kinds. A candidate is dropped
 * only for a known parameter kind that differs from the argument, and only
 * when another candidate matches exactly: an Unknown parameter (template,
 * typedef, class type) may still be the real callee.
 */
void narrow_by_kinds(const std::vector<Def>& defs,
                     const std::vector<Kind>& kinds,
                     std::vector<std::size_t>& out) {
  if (out.size() < 2 ||
      std::ranges::any_of(kinds, [](Kind k) { return k == Kind::Unknown; }))
    return;
  std::vector<bool> mismatch(out.size(), false);
  bool exact = false;
  for (std::size_t c = 0; c < out.size(); ++c) {
    const std::vector<Kind>& params = defs[out[c]].params;
    bool match = false;
    for (std::size_t i = 0; i < kinds.size() && i < params.size(); ++i) {
      if (params[i] == Kind::Unknown) continue;
      if (params[i] == kinds[i])
        match = true;
      else
        mismatch[c] = true;
    }
    exact = exact || (match && !mismatch[c]);
  }
  if (!exact) return;
  std::vector<std::size_t> kept;
  for (std::size_t c = 0; c < out.size(); ++c)
    if (!mismatch[c]) kept.push_back(out[c]);
  out = std::move(kept);
}

}  // namespace

std::vector<RecursiveDef> CLikeGSGBuilder::recursive_defs(TSNode root,
                                                          const string& src) {
  const auto funcs = build_functions(root, src);
  std::unordered_map<uint32_t, std::size_t> by_start;
  std::unordered_map<string, std::vector<std::size_t>> by_name;
  std::vector<Def> defs(funcs.size());
  std::vector<uint32_t> starts(funcs.size());
  for (std::size_t i = 0; i < funcs.size(); ++i) {
    starts[i] = funcs[i].loc.start_byte;
    by_start[starts[i]] = i;
    defs[i].name = funcs[i].name;
    defs[i].scope = scope_of(funcs[i].name);
    by_name[funcs[i].name].push_back(i);
  }

  auto text = [&](TSNode n) { return string(slice(src, n)); };
  auto resolve = [&](
                   const string& scope, const string& name,
                   const std::vector<Kind>& kinds) -> std::vector<std::size_t> {
    const auto args = static_cast<uint32_t>(kinds.size());
    string prefix = scope;
    while (true) {
      string key = prefix;
      if (!key.empty()) key += "::";
      key += name;
      auto it = by_name.find(key);
      if (it != by_name.end()) {
        std::vector<std::size_t> out;
        for (std::size_t id : it->second)
          if (args >= defs[id].min_args &&
              (defs[id].max_args == kVariadic || args <= defs[id].max_args))
            out.push_back(id);
        if (cpp_) narrow_by_kinds(defs, kinds, out);
        return out;
      }
      if (prefix.empty()) return {};
      prefix = scope_of(prefix);
    }
  };

  std::vector<std::vector<CallEdge>> edges(funcs.size());
  std::vector<std::vector<Local>> locals(funcs.size());
  for (const bool calls : {false, true}) {
    std::vector<std::pair<TSNode, std::size_t>> stack{{root, funcs.size()}};
    while (!stack.empty()) {
      auto [n, owner] = stack.back();
      stack.pop_back();
      string_view t = type_of(n);
      if (t == "function_definition") {
        auto it = by_start.find(ts_node_start_byte(n));
        if (it != by_start.end()) {
          owner = it->second;
          if (!calls) {
            signature(n, src, defs[owner]);
            collect_locals(
              field(function_declarator(field(n, "declarator")), "parameters"),
              n, src, locals[owner]);
            collect_locals(field(n, "body"), n, src, locals[owner]);
          }
        }
      } else if (calls && t == "call_expression" && owner < funcs.size()) {
        TSNode fn = field(n, "function");
        string_view ft = type_of(fn);
        string name;
        if (ft == "qualified_identifier" ||
            (ft == "identifier" &&
             !hidden(locals[owner], text(fn), ts_node_start_byte(n))))
          name = text(fn);
        else if (ft == "field_expression" &&
                 type_of(field(fn, "argument")) == "this")
          name = text(field(fn, "field"));
        if (!name.empty())
          for (std::size_t callee :
               resolve(defs[owner].scope, name, argument_kinds(n, src)))
            edges[owner].push_back({.callee = callee, .call = loc(n)});
      }
      uint32_t m = ts_node_named_child_count(n);
      for (uint32_t i = m; i > 0; --i)
        stack.emplace_back(ts_node_named_child(n, i - 1), owner);
    }
  }
  return recursive_defs_from_graph(starts, edges);
}
