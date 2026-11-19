# Cognity — agent guide

Cognity computes cognitive complexity per function for many languages. It is a
multi-language port of [Complexipy](https://github.com/rohaquinlop/complexipy)
(Python only). Complexipy is the reference: for Python, Cognity must give the
same numbers. Other languages apply the same rules to their constructs.

C++23, CMake, tree-sitter. Grammars and the tree-sitter core are fetched from
GitHub at configure time (pinned tags in `CMakeLists.txt`).

## Prerequisites

- CMake ≥ 3.16, a C++23 compiler, git
- `uv` — runs the pinned clang tools (`uvx clang-format@23.1.5`,
  `uvx clang-tidy@23.1.4`, see `scripts/lib/tools.sh`)
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

Exit codes: `0` pass, `1` a check failed, `2` usage error, `3` environment
problem (configure or fetch failed, missing tool, lint base not found — run
`git fetch origin`).

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

- `include/gsg.h`: `GSGNodeKind` (If, ElseIf, For, While, Try, Catch, Ternary,
  …), `GSGNode` (`kind`, `name`, `loc`, `addl_cost`, `children`).
- Builders map tree-sitter nodes to GSG nodes. `addl_cost` carries extra cost
  such as boolean-operator sequences.
  - `python_gsg_builder` — Python
  - `javascript_gsg_builder` — JavaScript and TypeScript/TSX
  - `c_gsg_builder` (`CLikeGSGBuilder`) — C and C++
- `compute_cognitive_complexity_gsg` holds the scoring rules (increments and
  nesting). It is language-independent. Algorithm changes go here first; a
  builder changes only when a language must emit different GSG nodes.
- `make_builder` (in `cognitive_complexity.cpp`) maps `Language` to a builder.

## Tests

- `tests/test_complexity.cpp` maps each fixture to its expected **total**
  complexity per file, per language.
- Fixtures live in `tests/src/<lang>/`. They are analyzer inputs:
  - Never edit a fixture to make a test pass.
  - Never format them (`tests/src/.clang-format` disables formatting).
- Python expected values come from Complexipy. When Cognity and Complexipy
  disagree, Cognity is wrong unless the user says otherwise.
- To add a case: add a fixture, add its expected value to the map for its
  language, run `scripts/check.sh test`.
- Change an expected value only when the scoring rules change on purpose. Name
  the rule in the commit message.

## Conventions

- Style: `.clang-format` (Google base, 80 columns, `PointerAlignment: Left`).
  The Claude Code hook formats edited files. Do not hand-format.
- Lint: `.clang-tidy` (bugprone, performance, modernize). Disabled:
  `modernize-use-trailing-return-type` (conflicts with the Google style) and
  `bugprone-easily-swappable-parameters` (noisy for `(src, name)` string
  pairs). The gate checks only changed lines. Old warnings exist; fix them
  only when you touch that code.
- New `.cpp` file: add it to every CMake target that compiles it. `cognity`
  lists all sources; `cognity_tests` lists only what the tests link (no
  `main.cpp`, `output.cpp`, `sourcing.cpp`).
- Includes use relative paths (`"../include/x.h"`).
- CI builds and tests on Linux, macOS and Windows (MSVC). Avoid POSIX-only
  APIs and compiler-specific extensions.
- Do not change user-facing CLI text unless the task is about it.

## Agent hooks (`.claude/settings.json`)

- PostToolUse (`Edit|Write|MultiEdit`): `scripts/hooks/format-file.sh` formats
  the edited C/C++ file.
- Stop: `scripts/hooks/verify.sh` runs `scripts/check.sh all` when files under
  `src/`, `include/`, `tests/`, `scripts/`, `CMakeLists.txt` or `.clang-*`
  differ from the merge-base. A failure blocks the stop once and shows the
  output. Environment problems only warn. A tree that already passed is not
  checked again.
- Permissions: `.claude/settings.json` pre-approves `scripts/check.sh`,
  `cmake`, `ctest` and the pinned `uvx clang-format` / `uvx clang-tidy` calls.
