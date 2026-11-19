#!/usr/bin/env bash
# Single entrypoint for local checks, agent hooks and CI.
#
# Usage: scripts/check.sh <format|format-fix|configure|build|test|lint|all>
#
# Exit codes: 0 pass, 1 a check failed, 2 usage error,
#             3 environment problem (configure/fetch failed, tool missing,
#               lint base not resolvable).
#
# Env:
#   BUILD_DIR   build directory (default: build/check)
#   BUILD_TYPE  CMake build type (default: Debug)
#   CHECK_BASE  ref that lint compares against via merge-base
#               (default: origin/main)
#   JOBS        max parallel clang-tidy processes (default: CPU count)
#   CLANG_FORMAT, CLANG_TIDY  override the pinned uvx tools
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$ROOT"
# shellcheck source=lib/tools.sh
source scripts/lib/tools.sh

BUILD_DIR=${BUILD_DIR:-build/check}
BUILD_TYPE=${BUILD_TYPE:-Debug}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}
ENV_FAIL=3
CXX_RE='\.(c|cc|cpp|cxx|h|hh|hpp)$'

log() { echo "[check] $*" >&2; }

require() {
  if ! command -v "$1" >/dev/null 2>&1; then
    log "missing tool: $1"
    exit $ENV_FAIL
  fi
}

require_clang_tools() {
  if ! has_clang_tools; then
    log "missing tool: uvx (install uv, or set CLANG_FORMAT and CLANG_TIDY)"
    exit $ENV_FAIL
  fi
}

# Fixtures under tests/src/ are analyzer inputs and are never checked.
project_files() {
  {
    git ls-files -- src include tests
    git ls-files --others --exclude-standard -- src include tests
  } | grep -E "$CXX_RE" | grep -v '^tests/src/' | sort -u |
    while IFS= read -r f; do [ -f "$f" ] && echo "$f"; done
}

cmd_configure() {
  require cmake
  mkdir -p "$BUILD_DIR"
  if ! cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DFETCHCONTENT_UPDATES_DISCONNECTED=ON >"$BUILD_DIR/configure.log" 2>&1; then
    tail -n 20 "$BUILD_DIR/configure.log" >&2
    log "configure failed (grammars are fetched from GitHub on first run)"
    exit $ENV_FAIL
  fi
}

cmd_build() {
  cmd_configure
  log "build ($BUILD_TYPE, $BUILD_DIR)"
  cmake --build "$BUILD_DIR" --parallel || return 1
}

cmd_test() {
  cmd_build
  require ctest
  log "test"
  ctest --test-dir "$BUILD_DIR" --output-on-failure || return 1
}

cmd_format() {
  require_clang_tools
  local files=()
  while IFS= read -r f; do files+=("$f"); done < <(project_files)
  log "format check (${#files[@]} files)"
  clang_format --dry-run --Werror "${files[@]}" || return 1
}

cmd_format_fix() {
  require_clang_tools
  local files=()
  while IFS= read -r f; do files+=("$f"); done < <(project_files)
  clang_format -i "${files[@]}"
}

resolve_base() {
  local ref=${CHECK_BASE:-origin/main}
  if ! git rev-parse --verify -q "${ref}^{commit}" >/dev/null; then
    log "cannot resolve lint base '$ref'; run: git fetch origin"
    exit $ENV_FAIL
  fi
  if ! git merge-base "$ref" HEAD; then
    log "no merge-base between '$ref' and HEAD"
    exit $ENV_FAIL
  fi
}

changed_files() {
  local base=$1
  {
    git diff --name-only --diff-filter=d "$base" -- src include tests
    git ls-files --others --exclude-standard -- src include tests
  } | grep -E "$CXX_RE" | grep -v '^tests/src/' | sort -u || true
}

line_filter_json() {
  local base=$1
  shift
  local entries=() f ranges
  for f in "$@"; do
    if git ls-files --error-unmatch -- "$f" >/dev/null 2>&1; then
      ranges=$(git diff -U0 "$base" -- "$f" |
        sed -nE 's/^@@ -[0-9,]+ \+([0-9]+)(,([0-9]+))? @@.*/\1 \3/p' |
        awk '{ n = ($2 == "") ? 1 : $2; if (n > 0) printf "%s[%d,%d]", (c++ ? "," : ""), $1, $1 + n - 1 }')
    else
      ranges="[1,1000000]"
    fi
    [ -n "$ranges" ] && entries+=("{\"name\":\"$f\",\"lines\":[$ranges]}")
  done
  local IFS=,
  echo "[${entries[*]}]"
}

cmd_lint() {
  require_clang_tools
  local base
  base=$(resolve_base)
  local changed=()
  while IFS= read -r f; do changed+=("$f"); done < <(changed_files "$base")
  if [ ${#changed[@]} -eq 0 ]; then
    log "lint: no changed C/C++ files vs $(git rev-parse --short "$base")"
    return 0
  fi
  if [ ! -f "$BUILD_DIR/compile_commands.json" ] ||
    [ CMakeLists.txt -nt "$BUILD_DIR/compile_commands.json" ]; then
    cmd_configure
  fi

  local tus=() header_changed=0 f
  for f in "${changed[@]}"; do
    case "$f" in *.h | *.hh | *.hpp) header_changed=1 ;; esac
  done
  if [ $header_changed -eq 1 ]; then
    while IFS= read -r f; do
      case "$f" in *.h | *.hh | *.hpp) ;; *) tus+=("$f") ;; esac
    done < <(project_files)
  else
    tus=("${changed[@]}")
  fi

  local filter tmp
  filter=$(line_filter_json "$base" "${changed[@]}")
  if [ "$filter" = "[]" ]; then
    log "lint: only deleted lines; nothing to check"
    return 0
  fi
  tmp=$(mktemp -d)
  log "lint: ${#changed[@]} changed file(s), ${#tus[@]} translation unit(s)"
  local pids=() i=0 next=0 fail=0
  for f in "${tus[@]}"; do
    clang_tidy -p "$BUILD_DIR" --quiet --warnings-as-errors='*' \
      --line-filter="$filter" "$f" >"$tmp/$i.log" 2>&1 &
    pids+=($!)
    i=$((i + 1))
    if [ $((i - next)) -ge "$JOBS" ]; then
      reap_tidy "$tmp" "${pids[$next]}" "$next" || fail=1
      next=$((next + 1))
    fi
  done
  while [ $next -lt $i ]; do
    reap_tidy "$tmp" "${pids[$next]}" "$next" || fail=1
    next=$((next + 1))
  done
  rm -rf "$tmp"
  return $fail
}

reap_tidy() {
  if wait "$2"; then
    return 0
  fi
  grep -v -E '^[0-9]+ warnings? (and [0-9]+ errors? )?generated\.$' \
    "$1/$3.log" >&2 || true
  return 1
}

usage() {
  sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
}

case "${1:-}" in
  format) cmd_format ;;
  format-fix) cmd_format_fix ;;
  configure) cmd_configure ;;
  build) cmd_build ;;
  test) cmd_test ;;
  lint) cmd_lint ;;
  all)
    cmd_format
    cmd_test
    cmd_lint
    ;;
  -h | --help) usage ;;
  *)
    usage >&2
    exit 2
    ;;
esac
