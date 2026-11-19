#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../include/builders/javascript_gsg_builder.h"
#include "../../include/recursion.h"

namespace {

using std::string;
using std::string_view;

constexpr int kNone = -1;

struct Names {
  std::vector<int> defs;
  bool other{false};
};

struct Scope {
  int parent;
  int method{kNone};
  bool arrow{false};
  std::unordered_map<string, Names> names;
};

struct Def {
  uint32_t start;
  int cls{kNone};
  bool is_static{false};
};

struct Call {
  int owner;
  int scope;
  uint32_t pos;
  TSNode node;
};

string_view type_of(TSNode n) { return ts_node_type(n); }

TSNode field(TSNode n, string_view name) {
  return ts_node_child_by_field_name(n, name.data(), name.size());
}

bool is_function(string_view t) {
  return t == "function_declaration" || t == "generator_function_declaration" ||
         t == "function_expression" || t == "generator_function" ||
         t == "arrow_function" || t == "method_definition";
}

bool has_static(TSNode method) {
  uint32_t n = ts_node_child_count(method);
  for (uint32_t i = 0; i < n; ++i)
    if (type_of(ts_node_child(method, i)) == "static") return true;
  return false;
}

class Resolver {
 public:
  explicit Resolver(const string& src) : src_(src) {}

  std::vector<RecursiveDef> run(TSNode root) {
    scopes_.push_back(Scope{.parent = kNone});
    walk(root, 0, kNone, kNone);
    for (const auto& [scope, name] : assigned_) {
      int s = scope;
      while (s != 0 && !scopes_[s].names.contains(name)) s = scopes_[s].parent;
      scopes_[s].names[name].other = true;
    }
    std::vector<std::vector<CallEdge>> edges(defs_.size());
    for (const auto& c : calls_)
      for (int callee : callees(c))
        edges[c.owner].push_back({.callee = static_cast<std::size_t>(callee),
                                  .call = JavaScriptGSGBuilder::loc(c.node)});
    std::vector<uint32_t> starts;
    starts.reserve(defs_.size());
    for (const auto& d : defs_) starts.push_back(d.start);
    return recursive_defs_from_graph(starts, edges);
  }

 private:
  const string& src_;
  std::vector<Scope> scopes_;
  std::vector<Def> defs_;
  std::vector<std::unordered_map<string, std::vector<int>>> classes_;
  std::vector<Call> calls_;
  std::vector<std::pair<int, string>> assigned_;

  [[nodiscard]] string text(TSNode n) const {
    const uint32_t a = ts_node_start_byte(n);
    return src_.substr(a, ts_node_end_byte(n) - a);
  }

  void bind_pattern(TSNode root, int scope, int owner, int cls,
                    bool assign = false) {
    std::vector<std::pair<TSNode, bool>> stack{{root, false}};
    while (!stack.empty()) {
      auto [p, is_value] = stack.back();
      stack.pop_back();
      if (ts_node_is_null(p)) continue;
      if (is_value) {
        walk(p, scope, owner, cls);
        continue;
      }
      string_view t = type_of(p);
      if (t == "identifier" || t == "shorthand_property_identifier_pattern") {
        if (assign)
          assigned_.emplace_back(scope, text(p));
        else
          scopes_[scope].names[text(p)].other = true;
      } else if (t == "member_expression" || t == "subscript_expression") {
        walk(p, scope, owner, cls);
      } else if (t == "assignment_pattern" ||
                 t == "object_assignment_pattern") {
        stack.emplace_back(field(p, "right"), true);
        stack.emplace_back(field(p, "left"), false);
      } else if (t == "required_parameter" || t == "optional_parameter") {
        stack.emplace_back(field(p, "value"), true);
        stack.emplace_back(field(p, "pattern"), false);
      } else if (t == "pair_pattern") {
        stack.emplace_back(field(p, "value"), false);
      } else if (t != "type_annotation") {
        for (uint32_t i = ts_node_named_child_count(p); i > 0; --i)
          stack.emplace_back(ts_node_named_child(p, i - 1), false);
      }
    }
  }

  void walk_function(TSNode n, int scope, int owner, int cls) {
    string_view t = type_of(n);
    int def = kNone;
    if (t == "function_declaration" || t == "generator_function_declaration" ||
        t == "method_definition") {
      def = static_cast<int>(defs_.size());
      defs_.push_back(Def{.start = ts_node_start_byte(n)});
      if (t == "method_definition") {
        defs_[def].cls = cls;
        defs_[def].is_static = has_static(n);
        if (cls != kNone) classes_[cls][text(field(n, "name"))].push_back(def);
      } else {
        scopes_[scope].names[text(field(n, "name"))].defs.push_back(def);
      }
    }
    const int inner = static_cast<int>(scopes_.size());
    scopes_.push_back(Scope{.parent = scope,
                            .method = t == "method_definition" ? def : kNone,
                            .arrow = t == "arrow_function"});
    if (t == "function_expression" || t == "generator_function") {
      TSNode name = field(n, "name");
      if (!ts_node_is_null(name)) scopes_[inner].names[text(name)].other = true;
    }
    const int body_owner = def != kNone ? def : owner;
    bind_pattern(field(n, "parameters"), inner, body_owner, cls);
    bind_pattern(field(n, "parameter"), inner, body_owner, cls);
    walk(field(n, "body"), inner, body_owner, cls);
  }

  void walk(TSNode n, int scope, int owner, int cls) {
    std::vector<std::pair<TSNode, int>> stack{{n, cls}};
    while (!stack.empty()) {
      auto [cur, cur_cls] = stack.back();
      stack.pop_back();
      if (ts_node_is_null(cur) || walk_special(cur, scope, owner, cur_cls))
        continue;
      if (type_of(cur) == "object") cur_cls = kNone;
      if (type_of(cur) == "call_expression" && owner != kNone)
        calls_.push_back({.owner = owner,
                          .scope = scope,
                          .pos = ts_node_start_byte(cur),
                          .node = cur});
      for (uint32_t i = ts_node_named_child_count(cur); i > 0; --i)
        stack.emplace_back(ts_node_named_child(cur, i - 1), cur_cls);
    }
  }

  bool walk_special(TSNode n, int scope, int owner, int cls) {
    string_view t = type_of(n);
    if (is_function(t)) {
      walk_function(n, scope, owner, cls);
      return true;
    }
    if (t == "class_declaration" || t == "class") {
      TSNode name = field(n, "name");
      if (t == "class_declaration" && !ts_node_is_null(name))
        scopes_[scope].names[text(name)].other = true;
      const int id = static_cast<int>(classes_.size());
      classes_.emplace_back();
      walk(field(n, "body"), scope, owner, id);
      return true;
    }
    if (t == "variable_declarator") {
      bind_pattern(field(n, "name"), scope, owner, cls);
      walk(field(n, "value"), scope, owner, cls);
      return true;
    }
    if (t == "for_in_statement") {
      walk(field(n, "right"), scope, owner, cls);
      walk(field(n, "value"), scope, owner, cls);
      bind_pattern(field(n, "left"), scope, owner, cls,
                   ts_node_is_null(field(n, "kind")));
      walk(field(n, "body"), scope, owner, cls);
      return true;
    }
    if (t == "catch_clause") {
      bind_pattern(field(n, "parameter"), scope, owner, cls);
      walk(field(n, "body"), scope, owner, cls);
      return true;
    }
    if (t == "assignment_expression" ||
        t == "augmented_assignment_expression") {
      bind_pattern(field(n, "left"), scope, owner, cls, true);
      walk(field(n, "right"), scope, owner, cls);
      return true;
    }
    if (t == "import_specifier" || t == "namespace_import" ||
        t == "import_clause") {
      bind_pattern(field(n, "alias"), scope, owner, cls);
      if (ts_node_is_null(field(n, "alias")))
        bind_pattern(t == "import_specifier" ? field(n, "name") : n, scope,
                     owner, cls);
      return true;
    }
    return false;
  }

  [[nodiscard]] std::vector<int> callees(const Call& c) const {
    TSNode fn = field(c.node, "function");
    if (type_of(fn) == "identifier") {
      const string name = text(fn);
      for (int s = c.scope; s != kNone; s = scopes_[s].parent) {
        auto it = scopes_[s].names.find(name);
        if (it == scopes_[s].names.end()) continue;
        if (it->second.other) return {};
        return it->second.defs;
      }
      return {};
    }
    if (type_of(fn) != "member_expression" ||
        type_of(field(fn, "object")) != "this")
      return {};
    int s = c.scope;
    while (s != kNone && scopes_[s].arrow) s = scopes_[s].parent;
    if (s == kNone || scopes_[s].method == kNone) return {};
    const Def& method = defs_[scopes_[s].method];
    if (method.cls == kNone) return {};
    auto it = classes_[method.cls].find(text(field(fn, "property")));
    if (it == classes_[method.cls].end()) return {};
    std::vector<int> out;
    for (int id : it->second)
      if (defs_[id].is_static == method.is_static) out.push_back(id);
    return out;
  }
};

}  // namespace

std::vector<RecursiveDef> JavaScriptGSGBuilder::recursive_defs(
  TSNode root, const string& src) {
  return Resolver(src).run(root);
}
