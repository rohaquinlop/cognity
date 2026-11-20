# Cognity — agent guide

Cognity computes cognitive complexity per function for many languages. It is a multi-language port of [Complexipy](https://github.com/rohaquinlop/complexipy) (Python only). Complexipy is the reference: for Python, Cognity must give the same numbers. Other languages apply the same rules to their constructs.

C++23, CMake, tree-sitter. Grammars and the tree-sitter core are fetched from GitHub at configure time (pinned tags in `CMakeLists.txt`). The tests also fetch nlohmann/json (pinned version and hash).

## Prerequisites

- CMake ≥ 3.16, a C++23 compiler, git
- `uv` — runs the pinned clang tools (`uvx clang-format@23.1.5`, `uvx clang-tidy@23.1.4`, see `scripts/lib/tools.sh`)
- `jq` — used by the Claude Code hooks

## Commands

Always use `scripts/check.sh`. CI, hooks and humans run the same commands.

| Command | What it does |
|---|---|
| `scripts/check.sh format` | clang-format check on `src/`, `include/`, `tests/` |
| `scripts/check.sh format-fix` | apply clang-format to the same files |
| `scripts/check.sh build` | configure + build in `build/check` (Debug) |
| `scripts/check.sh test` | build + `ctest` |
| `scripts/check.sh lint` | clang-tidy on lines changed vs merge-base with `origin/main` |
| `scripts/check.sh all` | format, test, lint |

Exit codes: `0` pass, `1` a check failed, `2` usage error, `3` environment problem (configure or fetch failed, missing tool, lint base not found — run `git fetch origin`).

Run the binary: `build/check/cognity <paths> [-l py,js] [-mx N]`.

**Done means `scripts/check.sh all` exits 0.**

## Architecture

```
file → sourcing.cpp (language by extension, .gitignore, excludes)
     → tree-sitter parse
     → per-language builder (src/builders/*) → GSG (General Syntax Graph, include/gsg.h)
     → compute_cognitive_complexity_gsg (src/cognitive_complexity.cpp) → score per function
     → output.cpp (text / JSON / CSV)
```

- `include/gsg.h`: `GSGNodeKind` (If, ElseIf, For, While, Try, Catch, Ternary, …), `GSGNode` (`kind`, `name`, `loc`, `addl_cost`, `children`).
- Builders map tree-sitter nodes to GSG nodes. `addl_cost` carries extra cost such as boolean-operator sequences.
  - `python_gsg_builder` — Python
  - `javascript_gsg_builder` — JavaScript and TypeScript/TSX
  - `c_gsg_builder` (`CLikeGSGBuilder`) — C and C++
- `gsg_builder_base` holds the shared builder rules, driven by a per-language spec (node types, field names, operator tokens). `ExprBuilderBase` has the expression rules (boolean sequences, parentheses); `StmtBuilderBase` adds the statement rules (if/else-if, loops, switch, try, ternary, jumps, walk by default). JS and C/C++ derive from `StmtBuilderBase`; Python uses only `ExprBuilderBase`. A builder keeps only its real special cases.
- `compute_cognitive_complexity_gsg` holds the scoring rules (increments and nesting). It is language-independent. Algorithm changes go here first; a builder changes only when a language must emit different GSG nodes.
- `src/languages.cpp` is the language registry: one entry per `Language` with its CLI/config tokens, file extensions, grammar and builder. A `static_assert` fails the build when a `Language` value has no full entry.

## Tests

- `tests/test_complexity.cpp` has three tables:
  - `file_totals` (in `main()`): each fixture's expected **total** complexity, with its language.
  - `rule_cases()`: the rule fixtures `tests/src/<lang>/test_rules.<ext>`, one function per rule, with the expected score **per function**. A function that is missing, unexpected, duplicated or wrongly scored fails the test.
  - `coverage_cases()`: the coverage gate. For each language it reads the statement types from the grammar's `node-types.json` (statement supertypes, expanded) and puts each one in `mapped` (the builder has a rule for it) or `no_score` (it has none). A type in neither list, in both, or not in the grammar fails the test. Only statement supertype members go in the lists. The lists are kept by hand: the gate checks that they cover the grammar, not what the builder does. A new language adds a row here, its grammar to the `foreach(PAIR ...)` list in `CMakeLists.txt`, and its `COGNITY_NODE_TYPES_*` macro in the row.
- Fixtures live in `tests/src/<lang>/`. They are analyzer inputs:
  - Never edit a fixture to make a test pass.
  - Never format them (`tests/src/.clang-format` disables formatting).
- Python expected values come from Complexipy. When Cognity and Complexipy disagree, Cognity is wrong unless the user says otherwise.
- To add a case: add a fixture, add its row to `file_totals` (or, for a rule fixture function, to its `rule_cases()` map), run `scripts/check.sh test`.
- Change an expected value only when the scoring rules change on purpose. Name the rule in the commit message.

## Conventions

- Style: `.clang-format` (Google base, 80 columns, `PointerAlignment: Left`). The Claude Code hook formats edited files. Do not hand-format.
- Lint: `.clang-tidy` (bugprone, performance, modernize). Disabled: `modernize-use-trailing-return-type` (conflicts with the Google style) and `bugprone-easily-swappable-parameters` (noisy for `(src, name)` string pairs). The gate checks only changed lines. Old warnings exist; fix them only when you touch that code.
- New `.cpp` file: add it to every CMake target that compiles it. `cognity` lists all sources; `cognity_tests` lists only what the tests link (no `main.cpp`, `output.cpp`, `sourcing.cpp`).
- Includes use relative paths (`"../include/x.h"`).
- CI builds and tests on Linux, macOS and Windows (MSVC). Avoid POSIX-only APIs and compiler-specific extensions.
- Do not change user-facing CLI text unless the task is about it.

## Agent hooks (`.claude/settings.json`)

- PostToolUse (`Edit|Write|MultiEdit`): `scripts/hooks/format-file.sh` formats the edited C/C++ file.
- Stop: `scripts/hooks/verify.sh` runs `scripts/check.sh all` when files under `src/`, `include/`, `tests/`, `scripts/`, `CMakeLists.txt`, `.clang-format` or `.clang-tidy` differ from the merge-base. A failure blocks the stop once and shows the output. If the retry still fails, the hook does not block again; it shows the failure to the user. Environment problems only warn. A tree that already passed is not checked again.
- Permissions: `.claude/settings.json` pre-approves `scripts/check.sh`, `cmake`, `ctest` and the pinned `uvx clang-format` / `uvx clang-tidy` calls.

## Skills

- `.agents/skills/` holds the real skill files (`git-commit`, `create-pr`). `.claude/skills` is a symlink to `../.agents/skills`: do not replace it with a copy. On Windows, clone with `git clone -c core.symlinks=true` (needs Developer Mode or admin rights); otherwise the link is a plain file and Claude Code does not find the skills.
