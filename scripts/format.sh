#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:---write}"

if [[ "$MODE" != "--write" && "$MODE" != "--check" ]]; then
    echo "Usage: scripts/format.sh [--write|--check]" >&2
    exit 2
fi

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format is required. Install clang-format and retry." >&2
    exit 1
fi

if ! command -v npx >/dev/null 2>&1; then
    echo "Node.js/npm (npx) is required for frontend formatting." >&2
    exit 1
fi

cd "$ROOT_DIR"

mapfile -d '' C_FILES < <(
    find backend-c esp32 -type f \( \
        -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' -o -name '*.ino' \
    \) -print0
)

if [[ "$MODE" == "--check" ]]; then
    clang-format --dry-run --Werror "${C_FILES[@]}"
    npx --yes prettier@3.3.3 --check \
        "frontend/**/*.js" "frontend/**/*.html" \
        ".github/workflows/*.yml" "*.json"
else
    clang-format -i "${C_FILES[@]}"
    npx --yes prettier@3.3.3 --write \
        "frontend/**/*.js" "frontend/**/*.html" \
        ".github/workflows/*.yml" "*.json"
fi

echo "Formatting ${#C_FILES[@]} C/C++ source and header files completed ($MODE)."
