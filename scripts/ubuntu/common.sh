#!/usr/bin/env bash

resolve_project_python() {
    local root="$1"
    if [[ -x "$root/.venv/bin/python3" ]]; then
        echo "$root/.venv/bin/python3"
        return
    fi
    if command -v python3 &>/dev/null; then
        command -v python3
        return
    fi
    echo "未找到 Python。请先运行: ./scripts/ubuntu/setup-venv.sh" >&2
    exit 1
}
