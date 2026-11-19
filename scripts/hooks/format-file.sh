#!/usr/bin/env bash
# Claude Code PostToolUse hook: format an edited C/C++ file with the pinned
# clang-format. Never blocks; fixtures under tests/src/ are left untouched.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"

if ! command -v jq >/dev/null 2>&1; then
  echo "format-file: jq not found; skipping auto-format" >&2
  exit 0
fi

file=$(jq -r '.tool_input.file_path // empty')
[ -n "$file" ] && [ -f "$file" ] || exit 0
file=$(cd "$(dirname "$file")" && pwd -P)/$(basename "$file")
rel=${file#"$ROOT"/}

case "$rel" in
  tests/src/*) exit 0 ;;
  src/* | include/* | tests/*) ;;
  *) exit 0 ;;
esac
case "$rel" in
  *.c | *.cc | *.cpp | *.cxx | *.h | *.hh | *.hpp) ;;
  *) exit 0 ;;
esac

# shellcheck source=../lib/tools.sh
source "$ROOT/scripts/lib/tools.sh"
clang_format -i "$file" || echo "format-file: clang-format failed on $rel" >&2
exit 0
