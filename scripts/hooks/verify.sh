#!/usr/bin/env bash
# Claude Code Stop hook: run `scripts/check.sh all` when sources or build
# config differ from the merge-base with origin/main.
# Exit 2 returns the failure to the agent. Environment problems (offline,
# fetch failure, missing tools) only warn, because the agent cannot fix them.
# A tree that already passed is not checked again.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT" || exit 0
BUILD_DIR=${BUILD_DIR:-build/check}
STAMP="$BUILD_DIR/verified.stamp"

input=$(cat)
if ! command -v jq >/dev/null 2>&1; then
  echo "verify: jq not found; skipping checks" >&2
  exit 0
fi
# One retry per stop: a second failure goes to the user instead of looping.
[ "$(jq -r '.stop_hook_active // false' <<<"$input")" = "true" ] && exit 0

base=$(git merge-base "${CHECK_BASE:-origin/main}" HEAD 2>/dev/null) || base=HEAD
watched='^(src/|include/|tests/|scripts/|CMakeLists\.txt$|\.clang-(format|tidy)$)'
changed=$({
  git diff --name-only "$base"
  git ls-files --others --exclude-standard
} | grep -E "$watched")
[ -n "$changed" ] || exit 0

key=$({
  git rev-parse HEAD
  git diff HEAD
  git ls-files --others --exclude-standard -z | xargs -0 cat 2>/dev/null
} | git hash-object --stdin)
[ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$key" ] && exit 0

out=$(scripts/check.sh all 2>&1)
rc=$?
case $rc in
  0)
    mkdir -p "$BUILD_DIR" && echo "$key" >"$STAMP"
    exit 0
    ;;
  3)
    echo "verify: environment problem, checks did not run:" >&2
    tail -n 15 <<<"$out" >&2
    exit 0
    ;;
  *)
    echo "verify: scripts/check.sh all failed (exit $rc). Fix it, then stop again:" >&2
    tail -n 60 <<<"$out" >&2
    exit 2
    ;;
esac
