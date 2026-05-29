#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"
PYTHON="$(resolve_project_python "$PROJECT_ROOT")"
PLATFORMIO_DIR="$PROJECT_ROOT/PlatformIO"
COMPILE_DB_PATH="$PLATFORMIO_DIR/compile_commands.json"

if [[ ! -d "$PLATFORMIO_DIR" ]]; then
    echo "未找到 PlatformIO 目录: $PLATFORMIO_DIR" >&2
    exit 1
fi

echo "Project root: $PROJECT_ROOT"
echo "PlatformIO dir: $PLATFORMIO_DIR"
echo "刷新 compile_commands.json ..."

cd "$PLATFORMIO_DIR"
"$PYTHON" -m platformio run -t compiledb

if [[ -f "$COMPILE_DB_PATH" ]]; then
    echo "完成: $COMPILE_DB_PATH"
else
    echo "生成失败，未找到: $COMPILE_DB_PATH" >&2
    exit 1
fi
