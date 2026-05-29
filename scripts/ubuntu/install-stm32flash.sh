#!/usr/bin/env bash
set -euo pipefail

FORCE=false
if [[ "${1:-}" == "--force" || "${1:-}" == "-f" ]]; then
    FORCE=true
fi

if command -v stm32flash &>/dev/null && [[ "$FORCE" != true ]]; then
    echo "stm32flash 已安装: $(command -v stm32flash)"
    echo "使用 --force 可重新安装。"
    exit 0
fi

if command -v apt-get &>/dev/null; then
    echo "通过 apt 安装 stm32flash ..."
    if [[ "$(id -u)" -ne 0 ]]; then
        sudo apt-get update
        sudo apt-get install -y stm32flash
    else
        apt-get update
        apt-get install -y stm32flash
    fi
elif command -v dnf &>/dev/null; then
    echo "通过 dnf 安装 stm32flash ..."
    if [[ "$(id -u)" -ne 0 ]]; then
        sudo dnf install -y stm32flash
    else
        dnf install -y stm32flash
    fi
elif command -v pacman &>/dev/null; then
    echo "通过 pacman 安装 stm32flash ..."
    if [[ "$(id -u)" -ne 0 ]]; then
        sudo pacman -S --noconfirm stm32flash
    else
        pacman -S --noconfirm stm32flash
    fi
else
    cat >&2 <<'EOF'
未找到支持的包管理器 (apt/dnf/pacman)。

请手动安装 stm32flash，例如从源码编译:
  https://sourceforge.net/projects/stm32flash/
EOF
    exit 1
fi

if command -v stm32flash &>/dev/null; then
    echo ""
    echo "完成: $(command -v stm32flash)"
    echo "运行 ./scripts/ubuntu/flash-stm32-uart.sh 进行烧录。"
else
    echo "安装后仍未找到 stm32flash 命令。" >&2
    exit 1
fi
