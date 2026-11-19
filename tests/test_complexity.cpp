#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "../include/builders/javascript_gsg_builder.h"
#include "../include/builders/python_gsg_builder.h"
#include "../include/cognitive_complexity.h"
#include "../include/recursion.h"

extern "C" {
const TSLanguage* tree_sitter_python();
const TSLanguage* tree_sitter_javascript();
const TSLanguage* tree_sitter_typescript();
const TSLanguage* tree_sitter_c();
const TSLanguage* tree_sitter_cpp();
}

static std::string read_file(const std::filesystem::path& p) {
  std::ifstream ifs(p, std::ios::binary);
  if (!ifs) return {};
  std::string data((std::istreambuf_iterator<char>(ifs)),
                   std::istreambuf_iterator<char>());
  return data;
}

static std::vector<FunctionComplexity> analyze_source(const std::string& src,
                                                      Language lang) {
  TSParser* parser = ts_parser_new();
  switch (lang) {
    case Language::Python:
      ts_parser_set_language(parser, tree_sitter_python());
      break;
    case Language::JavaScript:
    case Language::TypeScript:
      // TS code uses the TypeScript grammar; JS uses JS grammar
      if (lang == Language::JavaScript)
        ts_parser_set_language(parser, tree_sitter_javascript());
      else
        ts_parser_set_language(parser, tree_sitter_typescript());
      break;
    case Language::C:
      ts_parser_set_language(parser, tree_sitter_c());
      break;
    case Language::Cpp:
      ts_parser_set_language(parser, tree_sitter_cpp());
      break;
    default:
      ts_parser_delete(parser);
      return {};
  }
  auto fns = functions_complexity_file(src, parser, lang);
  ts_parser_delete(parser);
  return fns;
}

static std::vector<FunctionComplexity> analyze_file(
  const std::filesystem::path& rel, Language lang) {
  // Resolve path relative to project root (source dir). Use this source file
  // location to find the repository root regardless of current working dir.
  static const std::filesystem::path project_root =
    std::filesystem::path(__FILE__).parent_path().parent_path();
  return analyze_source(read_file(project_root / rel), lang);
}

static unsigned int total(const std::vector<FunctionComplexity>& fns) {
  unsigned int sum = 0;
  for (const auto& fn : fns) sum += fn.complexity;
  return sum;
}

static unsigned int compute_file_complexity_lang(
  const std::filesystem::path& rel, Language lang) {
  return total(analyze_file(rel, lang));
}

static unsigned int row_cost(const std::vector<FunctionComplexity>& fns,
                             unsigned int row) {
  unsigned int sum = 0;
  for (const auto& fn : fns)
    for (const auto& line : fn.lines)
      if (line.row == row) sum += line.complexity;
  return sum;
}

static std::vector<std::pair<std::string, unsigned int>> recursion_cases() {
  return {
    {R"py(def f(n):
    [x for x in n for y in (f := n)]
    return f(n)
)py",
     0},
    {R"py(def g(n):
    return [y for x in n for y in (z := g(x))] + z
)py",
     1},
    {R"py(def g():
    return f()
def f():
    def g():
        return 1
    return g()
)py",
     0},
    {R"py(if X:
    def f():
        return 1
else:
    def f():
        return g()
def g():
    return f()
)py",
     2},
    {R"py(if X:
    class C:
        def m(self):
            return self.m()
else:
    class C:
        def m(self):
            return 1
)py",
     1},
    {R"py(def helper():
    return 0
def f():
    def helper():
        return 1
    class C:
        def m(self):
            return helper()
    return C
)py",
     0},
    {R"py(def f(n):
    return lambda k: f(k)
)py",
     1},
    {R"py(def f(n):
    return lambda k=f(n): k
)py",
     1},
    {R"py(def f(n):
    def inner(x=f(n)):
        return x
    return inner
)py",
     1},
    {R"py(def f(n):
    @f(n)
    def inner():
        pass
    return inner
)py",
     1},
    {R"py(def f(n):
    class K(f(n)):
        pass
    return K
)py",
     1},
    {R"py(def f(n):
    class K: x = f(n)
    return K
)py",
     1},
    {R"py(def f(n):
    return [f(x) for x in range(n)]
)py",
     1},
    {R"py(def f(n):
    return [x for f in f(n)]
)py",
     1},
    {R"py(def f(n):
    return lambda f: f()
)py",
     0},
    {R"py(def f(n):
    return lambda *f: f()
)py",
     0},
    {R"py(def f(n):
    @f
    def inner():
        pass
    return inner
)py",
     0},
    {R"py(class C:
    def m(self):
        return lambda self: self.m()
)py",
     0},
    {R"py(def f(n):
    def inner():
        return f(n)
    return lambda inner: inner()
)py",
     0},
    {R"py(def f(fs):
    return [f(x) for f in fs]
)py",
     0},
    {R"py(def f(fs):
    return {k: f() for k, f in fs}
)py",
     0},
    {R"py(def f(n):
    class K[f](f()):
        pass
    return K
)py",
     0},
    {R"py(def T(n):
    class Box[T]:
        def m(self):
            return T(self)
    return Box.m(None)
)py",
     0},
    {R"py(def g():
    return f()
def f[g]():
    return g()
)py",
     0},
    {R"py(def other():
    return 1
def f():
    other()
    return f()
)py",
     1},
    {R"py(def g():
    return f()
def f():
    class g:
        pass
    return g()
)py",
     0},
    {R"py(def g():
    return f()
def f():
    from m import g
    return g()
)py",
     0},
    {R"py(import a.g as g
def h():
    return g()
def g():
    return h()
)py",
     2},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
class Box[K]:
    item: K
)py",
     1},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
def f():
    global K
)py",
     1},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
class O:
    global K
)py",
     1},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
xs = [K for K in range(3)]
)py",
     1},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
xs = {K: v for K, *v in []}
)py",
     1},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
def f():
    K = 1
    def g():
        nonlocal K
        K = 2
)py",
     1},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
)py",
     1},
    {R"py(class K:
    @classmethod
    def s(cls, n):
        return K.s(n - 1)
)py",
     1},
    {R"py(class K:
    def s(self, n):
        return K.s(self, n - 1)
)py",
     1},
    {R"py(def f():
    class K:
        @staticmethod
        def s(n):
            return K.s(n - 1)
    return K
)py",
     1},
    {R"py(class O:
    from m import K
class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
)py",
     1},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
class K:
    pass
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
K = object()
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
from m import K
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
def K():
    pass
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
for K in []:
    pass
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
class O:
    global K
    K = 1
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
from m import *
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
with open(p) as K:
    pass
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
del K
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
K += 1
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
type K = int
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
K: int = 1
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
xs = [(K := x) for x in []]
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
def f():
    global K
    import K
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
def f():
    global K
    def K():
        pass
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
try:
    pass
except E as K:
    pass
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
match x:
    case [P(a=(1 | {**K}))]:
        pass
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
def f():
    global K
    K = 1
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(K):
        return K.s(1)
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        K = n
        return K.s(1)
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(ks):
        return [K.s(x) for K in ks]
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        class C:
            K = 1
            x = K.s(n)
        return n
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        class C:
            def K(self):
                pass
            x = K.s(n)
        return n
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        class C:
            import K
            x = K.s(n)
        return n
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        def g(x=(K := 1)):
            pass
        return K.s(n - 1)
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        f = lambda x=(K := 1): x
        return K.s(n - 1)
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        class C((K := object)):
            pass
        return K.s(n - 1)
)py",
     0},
    {R"py(def f():
    class K:
        @staticmethod
        def s(n):
            return K.s(n - 1)
    def g():
        nonlocal K
        K = 1
)py",
     0},
    {R"py(class K:
    @staticmethod
    def t(n):
        return K.s(n)
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s[K](n):
        return K.s(n - 1)
)py",
     0},
    {R"py(class K:
    @staticmethod
    def s(n):
        return C.s(n)
class C[K]:
    @staticmethod
    def s(n):
        return K.s(n - 1)
)py",
     0},
    {R"py(class O:
    class K:
        @staticmethod
        def s(n):
            return K.s(n - 1)
)py",
     0},
    {R"py(K = None
class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
)py",
     1},
    {R"py(from m import K
class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
)py",
     1},
    {R"py(def K():
    pass
class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
)py",
     1},
    {R"py(for K in []:
    pass
class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
)py",
     1},
    {R"py(if c:
    K = None
else:
    class K:
        @staticmethod
        def s(n):
            return K.s(n - 1)
)py",
     1},
    {R"py(for x in xs:
    K = None
    class K:
        @staticmethod
        def s(n):
            return K.s(n - 1)
)py",
     1},
    {R"py(K = None
class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
K = None
)py",
     0},
    {R"py(from m import *
class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
)py",
     0},
    {R"py(for x in xs:
    class K:
        @staticmethod
        def s(n):
            return K.s(n - 1)
    K = None
)py",
     0},
    {R"py(K = None
class K:
    @staticmethod
    def s(n):
        return K.s(n - 1)
class K:
    pass
)py",
     0},
    {R"py(def f():
    K = None
    class K:
        x = K.s(1)
        @staticmethod
        def s(n):
            return f()
)py",
     0},
    {R"py(def f():
    class K:
        x = K.s(1)
        @staticmethod
        def s(n):
            return f()
)py",
     0},
    {R"py(def f():
    K = None
    @d(K.s(1))
    class K:
        @staticmethod
        def s(n):
            return f()
)py",
     0},
    {R"py(def f():
    K = None
    class K(K.s(1)):
        @staticmethod
        def s(n):
            return f()
)py",
     0},
    {R"py(def f():
    K = None
    class K:
        class C:
            x = K.s(1)
        @staticmethod
        def s(n):
            return f()
)py",
     0},
    {R"py(def f():
    K = None
    class K:
        x = [K.s(i) for i in r]
        @staticmethod
        def s(n):
            return f()
)py",
     0},
    {R"py(def f():
    class K:
        g = lambda: K.s(1)
        @staticmethod
        def s(n):
            return f()
    return K
)py",
     2},
    {R"py(class C:
    @staticmethod
    def m():
        return f()
def f():
    class K:
        class C:
            x = C.m()
)py",
     2},
    {R"py(class C:
    @staticmethod
    def m():
        return f()
def f():
    class K:
        class C(C.m()):
            pass
)py",
     2},
    {R"py(def f(n):
    class C:
        x = f(n)
        f = 1
    return C
)py",
     1},
    {R"py(def f(n):
    class C:
        f = 1
        x = f(n)
    return C
)py",
     0},
    {R"py(def f(n):
    class C:
        f = f(n)
    return C
)py",
     1},
    {R"py(def f(n):
    class C:
        if n:
            f = 1
        x = f(n)
    return C
)py",
     1},
    {R"py(def f(n):
    class C:
        f = 1
        del f
        x = f(n)
    return C
)py",
     1},
    {R"py(def outer():
    def f(n):
        class C:
            x = f(n)
            f = 1
        return C
    return f
)py",
     0},
    {R"py(def f(n):
    class C:
        def g(x):
            return f(x)
        y = g(n)
    return C
)py",
     2},
    {R"py(def f(f):
    return f()
)py",
     0},
    {R"py(def g(n):
    g = n
    return g()
)py",
     0},
    {R"py(def f(n):
    if (f := n):
        return f()
)py",
     1},
    {R"py(def f(n):
    for f in n:
        f()
)py",
     1},
    {R"py(def f(n):
    with n as f:
        return f()
)py",
     0},
    {R"py(def f(n):
    try:
        pass
    except E as f:
        f()
)py",
     1},
    {R"py(def f(n):
    match n:
        case [f]:
            f()
)py",
     1},
    {R"py(def f(n):
    del f
    return f()
)py",
     0},
    {R"py(def f(n):
    type f = int
    return f()
)py",
     0},
    {R"py(def f(n):
    import f
    return f()
)py",
     0},
    {R"py(def f(n):
    f += 1
    return f()
)py",
     0},
    {R"py(def f(n):
    f: int = n
    return f()
)py",
     0},
    {R"py(def f(n):
    f: int
    return f()
)py",
     0},
    {R"py(def f(n):
    class f:
        pass
    return f()
)py",
     0},
    {R"py(def f(n):
    def outer(f):
        def g(m):
            return f(m)
        return g(f)
    return outer(n)
)py",
     0},
    {R"py(def f(n):
    return f()
f = wrap(f)
)py",
     0},
    {R"py(def f(n):
    return f()
for f in []:
    pass
)py",
     0},
    {R"py(def f(n):
    return f()
def h():
    global f
    f = 1
)py",
     0},
    {R"py(def outer():
    def f(n):
        return f()
    def h():
        nonlocal f
        f = 1
    return f
)py",
     0},
    {R"py(def outer():
    def f(n):
        return f()
    f = wrap(f)
    return f
)py",
     0},
    {R"py(def f(n):
    return f()
class f:
    pass
)py",
     0},
    {R"py(if X:
    def f(n):
        return f()
else:
    pass
f = 1
)py",
     0},
    {R"py(try:
    def f(n):
        return f()
    g()
except E:
    f = 1
)py",
     0},
    {R"py(for x in xs:
    def f(n):
        return f()
else:
    f = 1
)py",
     0},
    {R"py(def g():
    return f()
def f():
    g = 1
    return g()
)py",
     0},
    {R"py(def g():
    return f()
def f():
    def h():
        return g()
    g = 1
    return h()
)py",
     0},
    {R"py(f = 1
def f(n):
    return f()
)py",
     1},
    {R"py(from m import f
def f(n):
    return f()
)py",
     1},
    {R"py(def f(n):
    return f()
f.cache = {}
)py",
     1},
    {R"py(@d(f := 1)
def f(n):
    return f()
)py",
     1},
    {R"py(def outer(f):
    def f(n):
        return f()
    return f
)py",
     1},
    {R"py(if X:
    f = 1
    def f(n):
        return f()
)py",
     1},
    {R"py(class f:
    pass
def f(n):
    return f()
)py",
     1},
    {R"py(def f(n=(f := 1)):
    return f()
)py",
     1},
    {R"py(try:
    f = 1
except E:
    def f(n):
        return f()
)py",
     1},
    {R"py(for x in xs:
    f = 1
else:
    def f(n):
        return f()
)py",
     1},
    {R"py(for x in (f := xs):
    def f(n):
        return f()
)py",
     1},
    {R"py(while (f := X):
    pass
else:
    def f(n):
        return f()
)py",
     1},
    {R"py(def h():
    global g
    return g
def f(n):
    return f()
)py",
     1},
    {R"py(def f(n):
    return f(n)
f = 1
def f(n):
    return f(n)
)py",
     1},
    {R"py(for x in xs:
    f = 1
    def f(n):
        return f()
)py",
     1},
    {R"py(for f in xs:
    def f(n):
        return f()
)py",
     1},
    {R"py(for x in xs:
    f = 1
    for y in ys:
        break
    def f(n):
        return f()
)py",
     1},
    {R"py(for x in xs:
    if c:
        f = 1
        def f(n):
            return f()
)py",
     1},
    {R"py(def f(n):
    return f()
f: int
)py",
     1},
    {R"py(def outer():
    def f(n):
        return f()
    f: int
    return f
)py",
     1},
  };
}

int main() {
  // Expected totals per file (mirrors complexipy tests). Paths are relative to
  // repository root.
  const std::map<std::string, unsigned int> expected = {
    {"tests/src/python/test_main.py", 0},
    {"tests/src/python/test_for.py", 5},
    {"tests/src/python/test_for_assign.py", 1},
    {"tests/src/python/test_if.py", 3},
    {"tests/src/python/test_match.py", 1},
    {"tests/src/python/test_multiple_func.py", 0},
    {"tests/src/python/test_nested_func.py", 2},
    {"tests/src/python/test_recursive.py", 1},
    {"tests/src/python/test_ternary_op.py", 1},
    {"tests/src/python/test_try.py", 3},
    {"tests/src/python/test_try_nested.py", 13},
    {"tests/src/python/test_break_continue.py", 3},
    {"tests/src/python/test_class.py", 1},
    {"tests/src/python/test_decorator.py", 1},
    {"tests/src/python/test_while.py", 1},
    {"tests/src/python/test.py", 9},
    {"tests/src/python/test_elif_else.py", 22},
    {"tests/src/python/test_finally_with.py", 2},
    {"tests/src/python/test_match_case.py", 7},
    {"tests/src/python/test_loop_try_else.py", 6},
    {"tests/src/python/test_nested_def_nesting.py", 6},
    {"tests/src/python/test_except_match_nesting.py", 6},
    {"tests/src/python/test_ternary_nesting.py", 3},
    {"tests/src/python/test_ternary_branches.py", 6},
    {"tests/src/python/test_ternary_comment.py", 2},
    {"tests/src/python/test_return_value.py", 11},
    {"tests/src/python/test_bool_sequences.py", 22},
    {"tests/src/python/test_lambda_nesting.py", 8},
    {"tests/src/python/test_expr_sites.py", 17},
    {"tests/src/python/test_case_guard.py", 6},
    {"tests/src/python/test_decorator_rule.py", 12},
    {"tests/src/python/test_definition_headers.py", 3},
    {"tests/src/python/test_function_discovery.py", 2},
    {"tests/src/python/test_nested_definitions.py", 4},
    {"tests/src/python/test_bool_paren_comment.py", 1},
    {"tests/src/python/test_recursion.py", 10},
  };

  bool ok = true;
  for (const auto& [path, exp] : expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::Python);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  // JavaScript (current implementation expectations)
  const std::map<std::string, unsigned int> js_expected = {
    {"tests/src/javascript/test_if.js", 6},
    {"tests/src/javascript/test_switch_ternary.js", 3},
    {"tests/src/javascript/test_ternary_nested.js", 5},
    {"tests/src/javascript/test_bool_ops.js", 9},
    {"tests/src/javascript/test_arrow.js", 7},
    {"tests/src/javascript/test_headers.js", 8},
    {"tests/src/javascript/test_bool_paren_comment.js", 1},
    {"tests/src/javascript/test_recursion.js", 8},
  };
  for (const auto& [path, exp] : js_expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::JavaScript);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  // TypeScript
  const std::map<std::string, unsigned int> ts_expected = {
    {"tests/src/typescript/test_if.ts", 8},
    {"tests/src/typescript/test_expr_rules.ts", 7},
    {"tests/src/typescript/test_recursion.ts", 3},
  };
  for (const auto& [path, exp] : ts_expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::TypeScript);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  // C
  const std::map<std::string, unsigned int> c_expected = {
    {"tests/src/c/test_if.c", 4},
    {"tests/src/c/test_ternary_nested.c", 5},
    {"tests/src/c/test_bool_ops.c", 9},
    {"tests/src/c/test_bool_paren_comment.c", 1},
    {"tests/src/c/test_recursion.c", 11},
  };
  for (const auto& [path, exp] : c_expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::C);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  // C++
  const std::map<std::string, unsigned int> cpp_expected = {
    {"tests/src/cpp/test_if.cpp", 8},
    {"tests/src/cpp/test_operator.cpp", 3},
    {"tests/src/cpp/test_lambda.cpp", 6},
    {"tests/src/cpp/test_lambda_sites.cpp", 6},
    {"tests/src/cpp/test_lambda_statements.cpp", 4},
    {"tests/src/cpp/test_bool_ops.cpp", 4},
    {"tests/src/cpp/test_recursion.cpp", 3},
    {"tests/src/cpp/test_switch.cpp", 6},
    {"tests/src/cpp/test_ctor_dtor.cpp", 2},
    {"tests/src/cpp/test_method_out_of_class.cpp", 2},
    {"tests/src/cpp/test_template_method.cpp", 2},
    {"tests/src/cpp/test_template_out_of_class.cpp", 3},
    {"tests/src/cpp/test_template_free.cpp", 2},
  };
  for (const auto& [path, exp] : cpp_expected) {
    unsigned int got = 0;
    try {
      got = compute_file_complexity_lang(path, Language::Cpp);
    } catch (...) {
      std::cerr << "Exception while processing: " << path << "\n";
      ok = false;
      continue;
    }
    if (got != exp) {
      std::cerr << "Mismatch for " << path << ": expected " << exp << ", got "
                << got << "\n";
      ok = false;
    }
  }

  const auto try_nested =
    analyze_file("tests/src/python/test_try_nested.py", Language::Python);
  for (const auto& [row, exp] : std::map<unsigned int, unsigned int>{
         {16, 1},
         {20, 1},
       }) {
    unsigned int got = row_cost(try_nested, row);
    if (got != exp) {
      std::cerr << "Line cost mismatch for test_try_nested.py line " << row + 1
                << ": expected " << exp << ", got " << got << "\n";
      ok = false;
    }
  }

  const auto recursion =
    analyze_file("tests/src/python/test_recursion.py", Language::Python);
  for (const auto& [row, exp] : std::map<unsigned int, unsigned int>{
         {1, 1},
         {2, 0},
         {6, 1},
         {15, 1},
       }) {
    unsigned int got = row_cost(recursion, row);
    if (got != exp) {
      std::cerr << "Line cost mismatch for test_recursion.py line " << row + 1
                << ": expected " << exp << ", got " << got << "\n";
      ok = false;
    }
  }

  for (const auto& [code, exp] : recursion_cases()) {
    unsigned int got = total(analyze_source(code, Language::Python));
    if (got != exp) {
      std::cerr << "Recursion case mismatch: expected " << exp << ", got "
                << got << "\n"
                << code << "\n";
      ok = false;
    }
  }

  {
    const std::size_t depth = 50000;
    auto repeat = [](const std::string& s, std::size_t n) {
      std::string out;
      out.reserve(s.size() * n);
      for (std::size_t i = 0; i < n; ++i) out += s;
      return out;
    };
    const std::vector<std::tuple<std::string, Language, unsigned int>> deep = {
      {"def f(a):\n    return a" + repeat("+a", depth) + "\n", Language::Python,
       0},
      {"def f(a):\n    return a" + repeat("()", depth) + "\n", Language::Python,
       0},
      {"def f(a):\n    return a" + repeat(" or a", depth) + "\n",
       Language::Python, 1},
      {"def f(a):\n    return " + repeat("[x for x in ", depth) + "a" +
         std::string(depth, ']') + "\n",
       Language::Python, 0},
      {"function f(a) {\n  return a" + repeat("+a", depth) + ";\n}\n",
       Language::JavaScript, 0},
      {"function f(a) {\n  return a" + repeat("()", depth) + ";\n}\n",
       Language::JavaScript, 0},
      {"function f(a) {\n  return a" + repeat(" || a", depth) + ";\n}\n",
       Language::JavaScript, 1},
      {"int f(int a) {\n  return a" + repeat("+a", depth) + ";\n}\n",
       Language::C, 0},
      {"int f(int a) {\n  return a" + repeat("()", depth) + ";\n}\n",
       Language::C, 0},
      {"int f(int a) {\n  return a" + repeat(" || a", depth) + ";\n}\n",
       Language::C, 1},
      {"def f(a):\n    return " + repeat("a or (a + (", depth) + "a" +
         repeat("))", depth) + "\n",
       Language::Python, static_cast<unsigned int>(depth)},
      {"function f(a) {\n  return " + repeat("a || (a + (", depth) + "a" +
         repeat("))", depth) + ";\n}\n",
       Language::JavaScript, static_cast<unsigned int>(depth)},
      {"int f(int a) {\n  return " + repeat("a || (a + (", depth) + "a" +
         repeat("))", depth) + ";\n}\n",
       Language::C, static_cast<unsigned int>(depth)},
    };
    for (const auto& [code, lang, expected] : deep) {
      if (total(analyze_source(code, lang)) != expected) {
        std::cerr << "Deep expression: wrong total for " << code.substr(0, 30)
                  << "\n";
        ok = false;
      }
    }
  }

  {
    const std::size_t count = 20000;
    std::string chain;
    for (std::size_t i = 0; i < count; ++i)
      chain += "def f" + std::to_string(i) + "():\n    return f" +
               std::to_string((i + 1) % count) + "()\n";
    if (total(analyze_source(chain, Language::Python)) != count) {
      std::cerr << "Recursion: 20000-def python cycle not fully detected\n";
      ok = false;
    }
  }

  {
    const std::size_t depth = 200000;
    std::string deep = "def f(a):\n    " + std::string(depth, '[') + "b" +
                       std::string(depth, ']') + " = a\n    match a:\n" +
                       "        case " + std::string(depth, '[') + "c" +
                       std::string(depth, ']') +
                       ":\n            pass\n    g = ";
    for (std::size_t i = 0; i < depth; ++i) deep += "lambda: ";
    deep += "a\n    return f(a)";
    for (int i = 0; i < 20000; ++i) deep += "+a";
    deep += '\n';
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_python());
    TSTree* tree = ts_parser_parse_string(parser, nullptr, deep.c_str(),
                                          static_cast<uint32_t>(deep.size()));
    if (PythonGSGBuilder()
          .recursive_defs(ts_tree_root_node(tree), deep)
          .size() != 1) {
      std::cerr << "Recursion: call under a deep python expression missed\n";
      ok = false;
    }
    ts_tree_delete(tree);
    ts_parser_delete(parser);
  }

  {
    std::string deep = "function f(a, " + std::string(200000, '[') + "b" +
                       std::string(200000, ']') + ") {\n  return f(a)";
    for (int i = 0; i < 20000; ++i) deep += "+a";
    deep += ";\n}\n";
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_javascript());
    TSTree* tree = ts_parser_parse_string(parser, nullptr, deep.c_str(),
                                          static_cast<uint32_t>(deep.size()));
    if (JavaScriptGSGBuilder()
          .recursive_defs(ts_tree_root_node(tree), deep)
          .size() != 1) {
      std::cerr << "Recursion: call under a deep js expression missed\n";
      ok = false;
    }
    ts_tree_delete(tree);
    ts_parser_delete(parser);
  }

  {
    const std::size_t count = 20000;
    std::vector<uint32_t> starts(count);
    std::vector<std::vector<CallEdge>> edges(count);
    for (std::size_t i = 0; i < count; ++i) {
      starts[i] = static_cast<uint32_t>(i);
      edges[i].push_back({.callee = (i + 1) % count, .call = SourceLoc{}});
    }
    if (recursive_defs_from_graph(starts, edges).size() != count) {
      std::cerr << "Recursion: 20000-def cycle not fully detected\n";
      ok = false;
    }
    std::vector<std::vector<CallEdge>> chain = {
      {{.callee = 1, .call = SourceLoc{.row = 1}}},
      {{.callee = 2, .call = SourceLoc{.row = 2}}},
      {}};
    if (!recursive_defs_from_graph({0, 1, 2}, chain).empty()) {
      std::cerr << "Recursion: acyclic chain reported as recursive\n";
      ok = false;
    }
    std::vector<std::vector<CallEdge>> self = {
      {{.callee = 1, .call = SourceLoc{.row = 3}},
       {.callee = 0, .call = SourceLoc{.row = 4}}},
      {}};
    auto found = recursive_defs_from_graph({0, 1}, self);
    if (found.size() != 1 || found[0].call.row != 4) {
      std::cerr << "Recursion: self call not reported on its first call row\n";
      ok = false;
    }
  }

  if (ok) {
    std::cout << "All complexity tests passed." << std::endl;
    return 0;
  }
  return 1;
}
