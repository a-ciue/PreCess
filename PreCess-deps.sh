#!/usr/bin/env sh

set -eu

case $0 in
    */*) SCRIPT_DIR=${0%/*} ;;
    *) SCRIPT_DIR=. ;;
esac
PYTHONUTF8=1
export PYTHONUTF8

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN=python3
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN=python
else
    echo "错误：未找到 python3 或 python" >&2
    exit 127
fi

exec "$PYTHON_BIN" "$SCRIPT_DIR/PreCess-deps.py" "$@"
