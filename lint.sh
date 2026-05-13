#!/bin/bash
# XNet lint helper — 用 clang-format 检查/修复代码格式
# 用法: ./lint.sh [check|fix]

set -euo pipefail
cd "$(dirname "$0")"

MODE="${1:-check}"
CLANG_FORMAT=""

# 自动查找 clang-format
for cmd in clang-format-18 clang-format-17 clang-format-16 clang-format; do
  if command -v "$cmd" &>/dev/null; then
    CLANG_FORMAT="$cmd"
    break
  fi
done

if [ -z "$CLANG_FORMAT" ]; then
  echo "ERROR: clang-format not found. Install it via:"
  echo "  sudo apt install clang-format-18"
  exit 1
fi

echo "Using: $($CLANG_FORMAT --version)"

# 收集所有源文件和头文件
SOURCES=$(find src ports examples tests -name '*.cc' -o -name '*.h' | sort)
HEADERS=$(find include/xnet -name '*.h' | sort)
FILES="$SOURCES $HEADERS"

if [ "$MODE" = "fix" ]; then
  echo "--- Auto-formatting all sources with $CLANG_FORMAT ---"
  # shellcheck disable=SC2086
  $CLANG_FORMAT -i -style=file $FILES
  echo "--- Formatting complete! ---"
elif [ "$MODE" = "check" ]; then
  echo "--- Running clang-format lint (Google style) ---"
  FAILED=0
  # shellcheck disable=SC2086
  $CLANG_FORMAT --dry-run -Werror -style=file $FILES || FAILED=1
  if [ "$FAILED" -eq 0 ]; then
    echo "--- All files pass lint check! ---"
  else
    echo "--- Some files need formatting fixes. Run: ./lint.sh fix ---"
    exit 1
  fi
else
  echo "Usage: $0 [check|fix]"
  exit 1
fi
