#!/usr/bin/env bash
# 无交互：自动扫描串口并完整烧录
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/flash-stm32-uart.sh" --auto "$@"
