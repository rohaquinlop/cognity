# Pinned clang tooling, shared by scripts/check.sh and the agent hooks.
# Override with CLANG_FORMAT / CLANG_TIDY (path to a binary) if needed.

CLANG_FORMAT_VERSION=23.1.5
CLANG_TIDY_VERSION=23.1.4

clang_format() {
  if [ -n "${CLANG_FORMAT:-}" ]; then
    "$CLANG_FORMAT" "$@"
  else
    uvx -q "clang-format@${CLANG_FORMAT_VERSION}" "$@"
  fi
}

clang_tidy() {
  if [ -n "${CLANG_TIDY:-}" ]; then
    "$CLANG_TIDY" "$@"
  else
    uvx -q "clang-tidy@${CLANG_TIDY_VERSION}" "$@"
  fi
}

has_clang_tools() {
  if [ -n "${CLANG_FORMAT:-}" ] && [ -n "${CLANG_TIDY:-}" ]; then
    return 0
  fi
  command -v uvx >/dev/null 2>&1
}
