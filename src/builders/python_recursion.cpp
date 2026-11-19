#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../include/builders/python_gsg_builder.h"
#include "../../include/recursion.h"

namespace {

using std::string;
using std::string_view;

constexpr uint32_t kUnordered = std::numeric_limits<uint32_t>::max();
constexpr int kNone = -1;
constexpr int kModule = 0;

enum class ScopeKind : uint8_t {
  Module,
  Function,
  Lambda,
  Comprehension,
  Class,
  TypeParams
};
enum class BindKind : uint8_t { Def, Class, Other, Declaration, Delete };

struct Binding {
  BindKind kind;
  uint32_t pos;
  int id;
  uint32_t block;
};

struct Scope {
  ScopeKind kind;
  int parent;
  int def{kNone};
  std::unordered_map<string, std::vector<Binding>> names;
  std::unordered_set<string> globals;
  std::unordered_set<string> nonlocals;
  bool star{false};
};

struct Def {
  uint32_t start;
  int cls{kNone};
  string receiver;
};

struct ClassInfo {
  uint32_t start;
  uint32_t end;
  int body_scope;
  std::unordered_map<string, std::vector<int>> methods;
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

bool is_comprehension(string_view t) {
  return t == "list_comprehension" || t == "set_comprehension" ||
         t == "dictionary_comprehension" || t == "generator_expression";
}

class Resolver {
 public:
  explicit Resolver(const string& src) : src_(src) {}

  std::vector<RecursiveDef> run(TSNode root) {
    scopes_.push_back(Scope{.kind = ScopeKind::Module, .parent = kNone});
    walk(root, kModule, kNone, ts_node_start_byte(root));
    for (const auto& [scope, name] : pending_nonlocals_) {
      for (int s = scopes_[scope].parent; s != kNone; s = scopes_[s].parent) {
        auto it = scopes_[s].names.find(name);
        if (scopes_[s].kind == ScopeKind::Function &&
            it != scopes_[s].names.end()) {
          it->second.push_back({.kind = BindKind::Other,
                                .pos = kUnordered,
                                .id = kNone,
                                .block = 0});
          break;
        }
      }
    }
    std::vector<std::vector<CallEdge>> edges(defs_.size());
    std::ranges::stable_sort(calls_, {}, &Call::pos);
    for (const auto& c : calls_)
      for (int callee : callees(c))
        edges[c.owner].push_back(
          {.callee = static_cast<std::size_t>(callee),
           .call = PythonGSGBuilder::loc_from_node(c.node)});
    std::vector<uint32_t> starts;
    starts.reserve(defs_.size());
    for (const auto& d : defs_) starts.push_back(d.start);
    return recursive_defs_from_graph(starts, edges);
  }

 private:
  const string& src_;
  std::vector<Scope> scopes_;
  std::vector<Def> defs_;
  std::vector<ClassInfo> classes_;
  std::vector<Call> calls_;
  std::vector<std::pair<int, string>> pending_nonlocals_;
  mutable std::map<std::pair<int, string>, std::vector<int>> defs_cache_;

  string text(TSNode n) const {
    const uint32_t a = ts_node_start_byte(n);
    return src_.substr(a, ts_node_end_byte(n) - a);
  }

  int new_scope(ScopeKind kind, int parent, int def = kNone) {
    scopes_.push_back(Scope{.kind = kind, .parent = parent, .def = def});
    return static_cast<int>(scopes_.size()) - 1;
  }

  void bind(int scope, const string& name, BindKind kind, uint32_t pos,
            uint32_t block, int id = kNone) {
    Scope& sc = scopes_[scope];
    if (sc.globals.contains(name)) {
      scopes_[kModule].names[name].push_back(
        {.kind = BindKind::Other, .pos = kUnordered, .id = kNone, .block = 0});
      return;
    }
    if (sc.nonlocals.contains(name)) {
      pending_nonlocals_.emplace_back(scope, name);
      return;
    }
    sc.names[name].push_back(
      {.kind = kind, .pos = pos, .id = id, .block = block});
  }

  void bind_targets(TSNode target, int scope, uint32_t pos, uint32_t block,
                    BindKind kind, int owner) {
    std::vector<TSNode> stack{target};
    while (!stack.empty()) {
      TSNode cur = stack.back();
      stack.pop_back();
      if (ts_node_is_null(cur)) continue;
      string_view t = type_of(cur);
      if (t == "identifier") {
        bind(scope, text(cur), kind, pos, block);
      } else if (t == "attribute" || t == "subscript") {
        walk(cur, scope, owner, block);
      } else {
        for (uint32_t i = ts_node_named_child_count(cur); i > 0; --i)
          stack.push_back(ts_node_named_child(cur, i - 1));
      }
    }
  }

  void bind_captures(TSNode pattern, int scope, uint32_t pos, uint32_t block,
                     int owner) {
    std::vector<TSNode> stack{pattern};
    while (!stack.empty()) {
      TSNode cur = stack.back();
      stack.pop_back();
      if (ts_node_is_null(cur)) continue;
      string_view t = type_of(cur);
      if (t == "dotted_name") {
        if (ts_node_named_child_count(cur) == 1)
          bind(scope, text(cur), BindKind::Other, pos, block);
      } else if (t == "splat_pattern" || t == "as_pattern_target") {
        bind_targets(cur, scope, pos, block, BindKind::Other, owner);
      } else {
        const uint32_t first =
          t == "class_pattern" || t == "keyword_pattern" ? 1 : 0;
        for (uint32_t i = ts_node_named_child_count(cur); i > first; --i)
          stack.push_back(ts_node_named_child(cur, i - 1));
      }
    }
  }

  void bind_type_params(TSNode params, int scope, uint32_t pos) {
    if (ts_node_is_null(params)) return;
    uint32_t n = ts_node_named_child_count(params);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode p = ts_node_named_child(params, i);
      while (!ts_node_is_null(p) && type_of(p) != "identifier" &&
             ts_node_named_child_count(p) > 0)
        p = ts_node_named_child(p, 0);
      if (!ts_node_is_null(p) && type_of(p) == "identifier")
        bind(scope, text(p), BindKind::Other, pos, 0);
    }
  }

  void bind_params(TSNode params, int scope, int outer, int owner,
                   uint32_t block, string* first) {
    if (ts_node_is_null(params)) return;
    uint32_t n = ts_node_named_child_count(params);
    const uint32_t pos = ts_node_start_byte(params);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode p = ts_node_named_child(params, i);
      string_view t = type_of(p);
      TSNode name = p;
      if (t == "default_parameter" || t == "typed_default_parameter") {
        walk(field(p, "value"), outer, owner, block);
        name = field(p, "name");
      } else if (t == "typed_parameter") {
        name = ts_node_named_child(p, 0);
      }
      if (first && i == 0 && type_of(name) == "identifier") *first = text(name);
      bind_targets(name, scope, pos, block, BindKind::Other, owner);
    }
  }

  static bool is_staticmethod(TSNode decorated, const string& src) {
    if (ts_node_is_null(decorated)) return false;
    uint32_t n = ts_node_named_child_count(decorated);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode d = ts_node_named_child(decorated, i);
      if (type_of(d) != "decorator" || ts_node_named_child_count(d) == 0)
        continue;
      TSNode e = ts_node_named_child(d, 0);
      if (type_of(e) == "attribute") e = field(e, "attribute");
      if (type_of(e) != "identifier") continue;
      const uint32_t a = ts_node_start_byte(e);
      if (src.compare(a, ts_node_end_byte(e) - a, "staticmethod") == 0)
        return true;
    }
    return false;
  }

  void walk_def(TSNode def, TSNode decorated, int scope, int owner,
                uint32_t block) {
    const int id = static_cast<int>(defs_.size());
    defs_.push_back(Def{.start = ts_node_start_byte(def)});
    int header = scope;
    TSNode tparams = field(def, "type_parameters");
    if (!ts_node_is_null(tparams)) {
      header = new_scope(ScopeKind::TypeParams, scope);
      bind_type_params(tparams, header, ts_node_start_byte(def));
    }
    const int body = new_scope(ScopeKind::Function, header, id);
    string first;
    bind_params(field(def, "parameters"), body, scope, owner, block, &first);
    if (scopes_[scope].kind == ScopeKind::Class) {
      int c = static_cast<int>(classes_.size()) - 1;
      while (c >= 0 && classes_[c].body_scope != scope) --c;
      defs_[id].cls = c;
      if (!is_staticmethod(decorated, src_)) defs_[id].receiver = first;
      if (c >= 0) classes_[c].methods[text(field(def, "name"))].push_back(id);
    }
    walk(field(def, "body"), body, id, block);
    bind(scope, text(field(def, "name")), BindKind::Def, ts_node_end_byte(def),
         block, id);
  }

  void walk_class(TSNode cls, TSNode outer_node, int scope, int owner,
                  uint32_t block) {
    int header = scope;
    TSNode tparams = field(cls, "type_parameters");
    if (!ts_node_is_null(tparams)) {
      header = new_scope(ScopeKind::TypeParams, scope);
      bind_type_params(tparams, header, ts_node_start_byte(cls));
    }
    walk(field(cls, "superclasses"), header, owner, block);
    const int body = new_scope(ScopeKind::Class, header);
    const int id = static_cast<int>(classes_.size());
    classes_.push_back(ClassInfo{.start = ts_node_start_byte(outer_node),
                                 .end = ts_node_end_byte(outer_node),
                                 .body_scope = body});
    walk(field(cls, "body"), body, owner, block);
    bind(scope, text(field(cls, "name")), BindKind::Class,
         ts_node_end_byte(cls), block, id);
  }

  void walk_comprehension(TSNode n, int scope, int owner, uint32_t block) {
    const int inner = new_scope(ScopeKind::Comprehension, scope);
    bool first = true;
    uint32_t m = ts_node_named_child_count(n);
    for (uint32_t i = 0; i < m; ++i) {
      TSNode ch = ts_node_named_child(n, i);
      if (type_of(ch) != "for_in_clause") continue;
      walk(field(ch, "right"), first ? scope : inner, owner, block);
      first = false;
      bind_targets(field(ch, "left"), inner, 0, block, BindKind::Other, owner);
    }
    for (uint32_t i = 0; i < m; ++i) {
      TSNode ch = ts_node_named_child(n, i);
      if (type_of(ch) != "for_in_clause") walk(ch, inner, owner, block);
    }
  }

  int walrus_scope(int scope) const {
    while (scopes_[scope].kind == ScopeKind::Comprehension)
      scope = scopes_[scope].parent;
    return scope;
  }

  void walk_children(TSNode n, int scope, int owner, uint32_t block) {
    uint32_t m = ts_node_named_child_count(n);
    for (uint32_t i = 0; i < m; ++i)
      walk(ts_node_named_child(n, i), scope, owner, block);
  }

  void walk_import(TSNode n, int scope, uint32_t block) {
    const uint32_t pos = ts_node_end_byte(n);
    TSNode module = field(n, "module_name");
    uint32_t m = ts_node_named_child_count(n);
    for (uint32_t i = 0; i < m; ++i) {
      TSNode ch = ts_node_named_child(n, i);
      if (!ts_node_is_null(module) && ts_node_eq(ch, module)) continue;
      string_view t = type_of(ch);
      if (t == "wildcard_import") {
        scopes_[scope].star = true;
      } else if (t == "aliased_import") {
        bind(scope, text(field(ch, "alias")), BindKind::Other, pos, block);
      } else if (t == "dotted_name" && ts_node_named_child_count(ch) > 0) {
        bind(scope, text(ts_node_named_child(ch, 0)), BindKind::Other, pos,
             block);
      }
    }
  }

  void walk_except(TSNode n, int scope, int owner, uint32_t block) {
    bool after_as = false;
    uint32_t m = ts_node_child_count(n);
    for (uint32_t i = 0; i < m; ++i) {
      TSNode ch = ts_node_child(n, i);
      if (!ts_node_is_named(ch)) {
        after_as = after_as || type_of(ch) == "as";
        continue;
      }
      if (after_as && type_of(ch) == "identifier") {
        bind(scope, text(ch), BindKind::Other, ts_node_end_byte(ch), block);
        after_as = false;
        continue;
      }
      walk(ch, scope, owner, block);
    }
  }

  void walk(TSNode n, int scope, int owner, uint32_t block) {
    std::vector<std::pair<TSNode, int>> stack{{n, scope}};
    while (!stack.empty()) {
      auto [cur, cur_scope] = stack.back();
      stack.pop_back();
      if (ts_node_is_null(cur)) continue;
      if (type_of(cur) == "lambda") {
        const int inner = new_scope(ScopeKind::Lambda, cur_scope);
        bind_params(field(cur, "parameters"), inner, cur_scope, owner, block,
                    nullptr);
        stack.emplace_back(field(cur, "body"), inner);
        continue;
      }
      if (walk_special(cur, cur_scope, owner, block)) continue;
      if (type_of(cur) == "call" && owner != kNone)
        calls_.push_back({.owner = owner,
                          .scope = cur_scope,
                          .pos = ts_node_start_byte(cur),
                          .node = cur});
      for (uint32_t i = ts_node_named_child_count(cur); i > 0; --i)
        stack.emplace_back(ts_node_named_child(cur, i - 1), cur_scope);
    }
  }

  bool walk_special(TSNode n, int scope, int owner, uint32_t block) {
    string_view t = type_of(n);
    if (t == "block" || t == "module") {
      walk_children(n, scope, owner, ts_node_start_byte(n));
    } else if (t == "decorated_definition") {
      TSNode def = field(n, "definition");
      uint32_t m = ts_node_named_child_count(n);
      for (uint32_t i = 0; i < m; ++i) {
        TSNode ch = ts_node_named_child(n, i);
        if (type_of(ch) == "decorator") walk(ch, scope, owner, block);
      }
      if (type_of(def) == "class_definition")
        walk_class(def, n, scope, owner, block);
      else
        walk_def(def, n, scope, owner, block);
    } else if (t == "function_definition") {
      walk_def(n, TSNode{}, scope, owner, block);
    } else if (t == "class_definition") {
      walk_class(n, n, scope, owner, block);
    } else if (is_comprehension(t)) {
      walk_comprehension(n, scope, owner, block);
    } else if (t == "assignment") {
      walk(field(n, "right"), scope, owner, block);
      const BindKind kind = ts_node_is_null(field(n, "right"))
                              ? BindKind::Declaration
                              : BindKind::Other;
      bind_targets(field(n, "left"), scope, ts_node_end_byte(n), block, kind,
                   owner);
    } else if (t == "augmented_assignment") {
      walk(field(n, "right"), scope, owner, block);
      bind_targets(field(n, "left"), scope, ts_node_end_byte(n), block,
                   BindKind::Other, owner);
    } else if (t == "named_expression") {
      walk(field(n, "value"), scope, owner, block);
      bind_targets(field(n, "name"), walrus_scope(scope), ts_node_end_byte(n),
                   block, BindKind::Other, owner);
    } else if (t == "for_statement") {
      walk(field(n, "right"), scope, owner, block);
      TSNode left = field(n, "left");
      bind_targets(left, scope, ts_node_end_byte(left), block, BindKind::Other,
                   owner);
      walk(field(n, "body"), scope, owner, block);
      walk(field(n, "alternative"), scope, owner, block);
    } else if (t == "as_pattern") {
      walk(ts_node_named_child(n, 0), scope, owner, block);
      bind_targets(field(n, "alias"), scope, ts_node_end_byte(n), block,
                   BindKind::Other, owner);
    } else if (t == "except_clause") {
      walk_except(n, scope, owner, block);
    } else if (t == "import_statement" || t == "import_from_statement") {
      walk_import(n, scope, block);
    } else if (t == "delete_statement") {
      bind_targets(ts_node_named_child(n, 0), scope, ts_node_end_byte(n), block,
                   BindKind::Delete, owner);
    } else if (t == "global_statement" || t == "nonlocal_statement") {
      auto& set = t == "global_statement" ? scopes_[scope].globals
                                          : scopes_[scope].nonlocals;
      uint32_t m = ts_node_named_child_count(n);
      for (uint32_t i = 0; i < m; ++i)
        set.insert(text(ts_node_named_child(n, i)));
    } else if (t == "type_alias_statement") {
      TSNode name = ts_node_named_child(n, 0);
      while (!ts_node_is_null(name) && type_of(name) != "identifier" &&
             ts_node_named_child_count(name) > 0)
        name = ts_node_named_child(name, 0);
      if (!ts_node_is_null(name) && type_of(name) == "identifier")
        bind(scope, text(name), BindKind::Other, ts_node_end_byte(n), block);
    } else if (t == "case_clause") {
      uint32_t m = ts_node_named_child_count(n);
      for (uint32_t i = 0; i < m; ++i) {
        TSNode ch = ts_node_named_child(n, i);
        if (type_of(ch) == "case_pattern")
          bind_captures(ch, scope, ts_node_end_byte(ch), block, owner);
        else
          walk(ch, scope, owner, block);
      }
    } else {
      return false;
    }
    return true;
  }

  int lookup(int scope, const string& name, uint32_t pos) const {
    bool start = true;
    int s = scope;
    while (s != kNone) {
      const Scope& sc = scopes_[s];
      auto it = sc.names.find(name);
      const bool bound = it != sc.names.end();
      switch (sc.kind) {
        case ScopeKind::Module:
          return bound && !sc.star ? s : kNone;
        case ScopeKind::Class:
          if (start && bound) {
            const Binding* last = nullptr;
            for (const auto& b : it->second)
              if (b.kind != BindKind::Declaration && b.pos < pos) last = &b;
            if (last && last->kind != BindKind::Delete) return s;
            if (!last) return lookup_module(name);
            if (last->kind == BindKind::Delete) return lookup_module(name);
          }
          break;
        case ScopeKind::TypeParams:
          if (bound) return s;
          break;
        default:
          if (sc.globals.contains(name)) return lookup_module(name);
          if (!sc.nonlocals.contains(name) && bound) return s;
          break;
      }
      start = false;
      s = sc.parent;
    }
    return kNone;
  }

  int lookup_module(const string& name) const {
    const Scope& m = scopes_[kModule];
    return m.names.contains(name) && !m.star ? kModule : kNone;
  }

  const std::vector<int>& defs_of(int scope, const string& name) const {
    auto key = std::make_pair(scope, name);
    auto cached = defs_cache_.find(key);
    if (cached != defs_cache_.end()) return cached->second;
    std::vector<int> out;
    const Scope& sc = scopes_[scope];
    auto it = sc.names.find(name);
    if (!sc.star && sc.kind != ScopeKind::TypeParams && it != sc.names.end()) {
      uint32_t last_other = 0;
      bool any_other = false;
      std::unordered_map<uint32_t, uint32_t> last_def_in_block;
      for (const auto& b : it->second) {
        if (b.kind == BindKind::Def) {
          auto& d = last_def_in_block[b.block];
          d = std::max(d, b.pos);
        } else if (b.kind != BindKind::Declaration) {
          last_other = std::max(last_other, b.pos);
          any_other = true;
        }
      }
      for (const auto& b : it->second)
        if (b.kind == BindKind::Def && (!any_other || b.pos > last_other) &&
            b.pos == last_def_in_block[b.block])
          out.push_back(b.id);
    }
    return defs_cache_.emplace(key, std::move(out)).first->second;
  }

  int class_of(int scope, const string& name) const {
    const Scope& sc = scopes_[scope];
    if (sc.star || sc.kind == ScopeKind::TypeParams) return kNone;
    auto it = sc.names.find(name);
    if (it == sc.names.end()) return kNone;
    const Binding* cls = nullptr;
    for (const auto& b : it->second) {
      if (b.kind != BindKind::Class) continue;
      if (cls) return kNone;
      cls = &b;
    }
    if (!cls) return kNone;
    for (const auto& b : it->second)
      if (b.kind != BindKind::Class && b.kind != BindKind::Declaration &&
          b.pos > cls->pos)
        return kNone;
    return cls->id;
  }

  bool deferred(int scope, int class_body) const {
    bool under_function = false;
    for (int s = scope; s != kNone; s = scopes_[s].parent) {
      if (s == class_body) return under_function;
      under_function = under_function ||
                       scopes_[s].kind == ScopeKind::Function ||
                       scopes_[s].kind == ScopeKind::Lambda;
    }
    return false;
  }

  std::vector<int> methods(int cls, const string& name) const {
    auto it = classes_[cls].methods.find(name);
    return it == classes_[cls].methods.end() ? std::vector<int>{} : it->second;
  }

  std::vector<int> callees(const Call& c) const {
    TSNode fn = field(c.node, "function");
    if (type_of(fn) == "identifier") {
      const string name = text(fn);
      const int s = lookup(c.scope, name, c.pos);
      return s == kNone ? std::vector<int>{} : defs_of(s, name);
    }
    if (type_of(fn) != "attribute") return {};
    TSNode obj = field(fn, "object");
    if (type_of(obj) != "identifier") return {};
    const string recv = text(obj);
    const string attr = text(field(fn, "attribute"));
    const int s = lookup(c.scope, recv, c.pos);
    if (s == kNone) return {};
    const Scope& sc = scopes_[s];
    if (sc.kind == ScopeKind::Function && sc.def != kNone) {
      const Def& d = defs_[sc.def];
      if (d.cls != kNone && d.receiver == recv && sc.names.at(recv).size() == 1)
        return methods(d.cls, attr);
    }
    const int cls = class_of(s, recv);
    if (cls == kNone) return {};
    const ClassInfo& k = classes_[cls];
    if (c.pos >= k.start && c.pos < k.end && !deferred(c.scope, k.body_scope))
      return {};
    return methods(cls, attr);
  }
};

}  // namespace

std::vector<RecursiveDef> PythonGSGBuilder::recursive_defs(TSNode root,
                                                           const string& src) {
  return Resolver(src).run(root);
}
