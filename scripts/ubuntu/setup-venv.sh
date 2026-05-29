#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VENV_DIR="$PROJECT_ROOT/.venv"
REQUIREMENTS="$PROJECT_ROOT/requirements.txt"

if ! command -v python3 &>/dev/null; then
    echo "未找到 python3，请先安装 Python 3。" >&2
    exit 1
fi

if [[ ! -d "$VENV_DIR" ]]; then
    echo "创建虚拟环境: $VENV_DIR"
    python3 -m venv "$VENV_DIR"
fi

echo "安装依赖: $REQUIREMENTS"
"$VENV_DIR/bin/pip" install -U pip
"$VENV_DIR/bin/pip" install -r "$REQUIREMENTS"

echo ""
echo "完成。虚拟环境: $VENV_DIR"
echo "PlatformIO: $("$VENV_DIR/bin/python3" -m platformio --version)"
echo ""
echo "可直接运行:"
echo "  ./scripts/ubuntu/build-platformio.sh"
