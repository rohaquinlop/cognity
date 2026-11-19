#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../include/builders/c_gsg_builder.h"
#include "../../include/recursion.h"

namespace {

using std::string;
using std::string_view;

constexpr uint32_t kVariadic = std::numeric_limits<uint32_t>::max();

struct Def {
  string name;
  string scope;
  uint32_t min_args{0};
  uint32_t max_args{0};
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

void arity(TSNode def, const string& src, uint32_t& min_args,
           uint32_t& max_args) {
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

void collect_locals(TSNode n, const string& src,
                    std::unordered_set<string>& out) {
  if (ts_node_is_null(n)) return;
  std::vector<TSNode> stack{n};
  while (!stack.empty()) {
    TSNode cur = stack.back();
    stack.pop_back();
    string_view t = type_of(cur);
    if (t == "parameter_declaration" || t == "optional_parameter_declaration" ||
        t == "declaration") {
      uint32_t m = ts_node_child_count(cur);
      for (uint32_t i = 0; i < m; ++i) {
        const char* f = ts_node_field_name_for_child(cur, i);
        if (!f || string_view(f) != "declarator") continue;
        TSNode decl = ts_node_child(cur, i);
        if (t == "declaration" && declares_function(decl)) continue;
        TSNode name = declared_name(decl);
        if (!ts_node_is_null(name)) {
          const uint32_t a = ts_node_start_byte(name);
          out.insert(src.substr(a, ts_node_end_byte(name) - a));
        }
      }
    }
    if (t == "lambda_expression") continue;
    uint32_t m = ts_node_named_child_count(cur);
    for (uint32_t i = 0; i < m; ++i)
      stack.push_back(ts_node_named_child(cur, i));
  }
}

uint32_t argument_count(TSNode call) {
  TSNode args = field(call, "arguments");
  if (ts_node_is_null(args)) return 0;
  uint32_t count = 0;
  uint32_t n = ts_node_named_child_count(args);
  for (uint32_t i = 0; i < n; ++i)
    if (!ts_node_is_extra(ts_node_named_child(args, i))) ++count;
  return count;
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

  auto text = [&](TSNode n) {
    const uint32_t a = ts_node_start_byte(n);
    return src.substr(a, ts_node_end_byte(n) - a);
  };
  auto resolve = [&](const string& scope, const string& name,
                     uint32_t args) -> std::vector<std::size_t> {
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
        return out;
      }
      if (prefix.empty()) return {};
      prefix = scope_of(prefix);
    }
  };

  std::vector<std::vector<CallEdge>> edges(funcs.size());
  std::vector<std::unordered_set<string>> locals(funcs.size());
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
            arity(n, src, defs[owner].min_args, defs[owner].max_args);
            collect_locals(
              field(function_declarator(field(n, "declarator")), "parameters"),
              src, locals[owner]);
            collect_locals(field(n, "body"), src, locals[owner]);
          }
        }
      } else if (calls && t == "call_expression" && owner < funcs.size()) {
        TSNode fn = field(n, "function");
        string_view ft = type_of(fn);
        string name;
        if (ft == "qualified_identifier" ||
            (ft == "identifier" && !locals[owner].contains(text(fn))))
          name = text(fn);
        else if (ft == "field_expression" &&
                 type_of(field(fn, "argument")) == "this")
          name = text(field(fn, "field"));
        if (!name.empty())
          for (std::size_t callee :
               resolve(defs[owner].scope, name, argument_count(n)))
            edges[owner].push_back({.callee = callee, .call = loc(n)});
      }
      uint32_t m = ts_node_named_child_count(n);
      for (uint32_t i = m; i > 0; --i)
        stack.emplace_back(ts_node_named_child(n, i - 1), owner);
    }
  }
  return recursive_defs_from_graph(starts, edges);
}
