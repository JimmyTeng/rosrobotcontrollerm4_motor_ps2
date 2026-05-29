#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"
PYTHON="$(resolve_project_python "$PROJECT_ROOT")"
ENVIRONMENT="${1:-release}"
PLATFORMIO_DIR="$PROJECT_ROOT/PlatformIO"

if [[ ! -d "$PLATFORMIO_DIR" ]]; then
    echo "未找到 PlatformIO 目录: $PLATFORMIO_DIR" >&2
    exit 1
fi

echo "Project root: $PROJECT_ROOT"
echo "PlatformIO dir: $PLATFORMIO_DIR"
echo "开始下载到 ST-Link, 环境: $ENVIRONMENT"

cd "$PLATFORMIO_DIR"
"$PYTHON" -m platformio run -e "$ENVIRONMENT" -t upload
