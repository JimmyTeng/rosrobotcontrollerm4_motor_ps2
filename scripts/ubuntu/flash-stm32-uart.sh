#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"
PYTHON="$(resolve_project_python "$PROJECT_ROOT")"
CONFIG_PATH="$SCRIPT_DIR/flash-stm32-uart.config.json"
DEFAULT_INIT_SEQUENCE="dtr=0,rts=1,-100,dtr=1"
FLYMCU_INIT_LABEL="DTR低电平复位 RTS高电平进入BootLoader"

PORT=""
PORT_SELECTOR=""
LIST_PORTS=false
BAUD=115200
INIT_SEQUENCE=""
FIRMWARE=""
ENVIRONMENT="release"
BUILD=false
ONLY_UNPROTECT=false
ONLY_ERASE=false
SKIP_UNPROTECT=false
SKIP_VERIFY=false
NO_RUN=false
MENU=false
AUTO=false
NO_INIT=false
USE_INIT=true

usage() {
    cat <<EOF
用法: $(basename "$0") [选项]

  --port PORT           串口：别名(rrc_flash/host_link)、by-id:...、by-path:... 或 /dev/ttyACM*
                        rrc_flash = USART1 115200（烧录 + printf 调试，勿与 host_link 混用）
  --list-ports          列出串口及稳定地址映射后退出
  --baud RATE           波特率 (默认 115200)
  --init SEQUENCE       初始化序列
  --firmware PATH       固件路径
  --env NAME            PlatformIO 环境 (默认 release)
  --build               烧录前先编译
  --auto                自动扫描串口并烧录
  --menu                交互菜单 (默认无参数时)
  --only-unprotect      仅解除读保护
  --only-erase          仅擦除
  --skip-unprotect      跳过解除读保护
  --skip-verify         跳过校验
  --no-run              烧录后不运行
  --no-init             关闭 DTR/RTS 初始化
  -h, --help            显示帮助
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --port) PORT_SELECTOR="$2"; shift 2 ;;
        --list-ports) LIST_PORTS=true; shift ;;
        --baud) BAUD="$2"; shift 2 ;;
        --init) INIT_SEQUENCE="$2"; shift 2 ;;
        --firmware) FIRMWARE="$2"; shift 2 ;;
        --env) ENVIRONMENT="$2"; shift 2 ;;
        --build) BUILD=true; shift ;;
        --auto) AUTO=true; shift ;;
        --menu) MENU=true; shift ;;
        --only-unprotect) ONLY_UNPROTECT=true; shift ;;
        --only-erase) ONLY_ERASE=true; shift ;;
        --skip-unprotect) SKIP_UNPROTECT=true; shift ;;
        --skip-verify) SKIP_VERIFY=true; shift ;;
        --no-run) NO_RUN=true; shift ;;
        --no-init) NO_INIT=true; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "未知参数: $1" >&2; usage; exit 1 ;;
    esac
done

load_config() {
    if [[ -f "$CONFIG_PATH" ]]; then
        "$PYTHON" - "$CONFIG_PATH" <<'PY'
import json, sys
path = sys.argv[1]
try:
    with open(path, encoding="utf-8") as f:
        print(json.dumps(json.load(f)))
except Exception:
    print("{}")
PY
    else
        echo "{}"
    fi
}

save_config() {
    local port_selector="$1" baud="$2" init="$3" use_init="$4" firmware="$5" mode="$6"
    "$PYTHON" - "$CONFIG_PATH" "$port_selector" "$baud" "$init" "$use_init" "$firmware" "$mode" <<'PY'
import json, sys
path, port_selector, baud, init, use_init, firmware, mode = sys.argv[1:8]
data = {}
try:
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
except (OSError, json.JSONDecodeError):
    pass
data["Port"] = port_selector or None
data["Baud"] = int(baud)
data["InitSequence"] = init
data["UseInit"] = use_init == "true"
data["LastFirmware"] = firmware
data["LastMode"] = mode
with open(path, "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2, ensure_ascii=False)
    f.write("\n")
PY
}

apply_saved_config() {
    local cfg
    cfg="$(load_config)"
    if [[ -z "$INIT_SEQUENCE" ]]; then
        INIT_SEQUENCE="$("$PYTHON" -c "import json,sys; c=json.loads(sys.argv[1]); print(c.get('InitSequence') or '')" "$cfg")"
        [[ -z "$INIT_SEQUENCE" ]] && INIT_SEQUENCE="$DEFAULT_INIT_SEQUENCE"
    fi
    if [[ -z "$PORT_SELECTOR" ]]; then
        PORT_SELECTOR="$("$PYTHON" -c "import json,sys; c=json.loads(sys.argv[1]); print(c.get('Port') or '')" "$cfg")"
    fi
    if [[ "$NO_INIT" != true ]]; then
        local saved_use
        saved_use="$("$PYTHON" -c "import json,sys; c=json.loads(sys.argv[1]); print('true' if c.get('UseInit', True) else 'false')" "$cfg")"
        USE_INIT="$saved_use"
    fi
    if [[ "$NO_INIT" == true ]]; then
        USE_INIT=false
    fi
}

resolve_stm32flash() {
    if command -v stm32flash &>/dev/null; then
        command -v stm32flash
        return
    fi
    cat >&2 <<EOF
未找到 stm32flash。

请先运行:
  ./scripts/ubuntu/install-stm32flash.sh
EOF
    exit 1
}

serial_port_list() {
    "$PYTHON" "$SCRIPT_DIR/serial_port_resolve.py" list "$CONFIG_PATH"
}

serial_port_resolve() {
    local selector="$1"
    "$PYTHON" "$SCRIPT_DIR/serial_port_resolve.py" resolve "$selector" "$CONFIG_PATH"
}

# 将别名/by-id/路径 解析为当前 /dev/ttyACM*
resolve_port_selector() {
    local selector="$1"
    [[ -z "$selector" ]] && return 1
    if [[ "$selector" == /dev/* ]] && [[ -e "$selector" ]]; then
        echo "$selector"
        return 0
    fi
    serial_port_resolve "$selector"
}

get_serial_ports() {
    serial_port_list | awk '{print $1}'
}

get_config_port_aliases() {
    [[ -f "$CONFIG_PATH" ]] || return 0
    "$PYTHON" -c "
import json
with open('$CONFIG_PATH', encoding='utf-8') as f:
    ports = json.load(f).get('Ports') or {}
for name in sorted(ports):
    print(name)
"
}

# 解析 PORT_SELECTOR -> PORT；失败时保留原值供后续扫描
finalize_port_resolution() {
    if [[ -z "$PORT_SELECTOR" ]]; then
        PORT=""
        return 0
    fi
    if resolved="$(resolve_port_selector "$PORT_SELECTOR" 2>/dev/null)"; then
        PORT="$resolved"
        return 0
    fi
    if [[ "$PORT_SELECTOR" == /dev/* ]]; then
        PORT="$PORT_SELECTOR"
        return 0
    fi
    echo "警告: 无法解析串口 '$PORT_SELECTOR'，将尝试自动扫描。" >&2
    PORT=""
    return 1
}

resolve_firmware_path() {
    local input="${1:-}"
    local root="$PROJECT_ROOT"
    local p candidate

    if [[ -n "$input" ]]; then
        for p in "$input" "$root/$input"; do
            if [[ -f "$p" ]]; then
                readlink -f "$p"
                return
            fi
        done
        echo "固件文件不存在: $input" >&2
        exit 1
    fi

    local default_hex="$root/MDK-ARM/RosRobotControllerM4/RosRobotControllerM4.hex"
    if [[ -f "$default_hex" ]]; then
        readlink -f "$default_hex"
        return
    fi

    candidate="$(find "$root" -type f \( -name '*.hex' -o -name '*.bin' \) -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)"
    if [[ -n "$candidate" && -f "$candidate" ]]; then
        readlink -f "$candidate"
        return
    fi

    echo "未找到可用固件（*.hex/*.bin），请通过 --firmware 指定。" >&2
    exit 1
}

is_dtr_rts_sequence() {
    [[ "$1" =~ [Dd][Tt][Rr][[:space:]]*= || "$1" =~ [Rr][Tt][Ss][[:space:]]*= ]]
}

invoke_serial_dtr_rts() {
    local port_name="$1" baud="$2" sequence="$3"
    "$PYTHON" - "$port_name" "$baud" "$sequence" <<'PY'
import sys, time
try:
    import serial
except ImportError:
    sys.exit("需要 pyserial: pip install pyserial")

port_name, baud, sequence = sys.argv[1], int(sys.argv[2]), sys.argv[3]
if not sequence.strip():
    sys.exit(0)

port = serial.Serial()
port.port = port_name
port.baudrate = baud
port.bytesize = serial.EIGHTBITS
port.parity = serial.PARITY_EVEN
port.stopbits = serial.STOPBITS_ONE
port.timeout = 0.5
port.write_timeout = 0.5
port.dtr = False
port.rts = False
port.open()
try:
    for step in sequence.split(","):
        step = step.strip()
        if not step:
            continue
        if step.lower().startswith("dtr="):
            level = int(step.split("=", 1)[1])
            # Linux pyserial 直接控制信号线电平
            port.dtr = (level == 1)
        elif step.lower().startswith("rts="):
            level = int(step.split("=", 1)[1])
            port.rts = (level == 1)
        elif step.startswith("-") and step[1:].isdigit():
            time.sleep(int(step[1:]) / 1000.0)
        else:
            print(f"未知 DTR/RTS 步骤: {step}", file=sys.stderr)
    time.sleep(0.05)
finally:
    port.close()
PY
}

get_base_flash_args() {
    local args=("-b" "$BAUD")
    if [[ "$USE_INIT" == true && -n "$INIT_SEQUENCE" ]] && ! is_dtr_rts_sequence "$INIT_SEQUENCE"; then
        args+=("-i" "$INIT_SEQUENCE")
    fi
    printf '%s\0' "${args[@]}"
}

invoke_stm32flash() {
    local exe="$1" allow_fail="${2:-false}"
    shift 2
    local -a flash_args=("$@")

    echo ">>> $exe ${flash_args[*]}"
    set +e
    local output
    output="$("$exe" "${flash_args[@]}" 2>&1)"
    local code=$?
    set -e
    [[ -n "$output" ]] && echo "$output"
    if [[ "$code" -ne 0 && "$allow_fail" != true ]]; then
        echo "stm32flash 失败，退出码: $code" >&2
        exit "$code"
    fi
    return "$code"
}

invoke_stm32flash_on_port() {
    local exe="$1" port_name="$2" allow_fail="${3:-false}"
    shift 3
    local -a extra_args=("$@")
    local attempt max_attempts=3
    local output code

    for ((attempt = 1; attempt <= max_attempts; attempt++)); do
        if [[ "$attempt" -gt 1 ]]; then
            echo "  重试 $attempt/$max_attempts: 重新进入 BootLoader..."
            sleep 0.5
        fi

        if [[ "$USE_INIT" == true ]] && is_dtr_rts_sequence "$INIT_SEQUENCE"; then
            invoke_serial_dtr_rts "$port_name" "$BAUD" "$INIT_SEQUENCE"
            sleep 0.2
        fi

        local -a base_args=()
        while IFS= read -r -d '' arg; do base_args+=("$arg"); done < <(get_base_flash_args)

        echo ">>> $exe ${base_args[*]} ${extra_args[*]} $port_name"
        set +e
        output="$("$exe" "${base_args[@]}" "${extra_args[@]}" "$port_name" 2>&1)"
        code=$?
        set -e
        [[ -n "$output" ]] && echo "$output"

        if [[ "$code" -eq 0 || "$allow_fail" == true ]]; then
            return "$code"
        fi

        if [[ "$output" == *"Failed to init device"* ]]; then
            continue
        fi

        echo "stm32flash 失败，退出码: $code" >&2
        exit "$code"
    done

    echo "stm32flash 失败，退出码: $code" >&2
    exit "$code"
}

wait_after_mcu_reset() {
    local reason="${1:-MCU 复位}"
    echo "  等待 ${reason}..."
    sleep 1.2
    if [[ "$USE_INIT" == true ]] && is_dtr_rts_sequence "$INIT_SEQUENCE"; then
        invoke_serial_dtr_rts "$PORT" "$BAUD" "$INIT_SEQUENCE"
        sleep 0.3
    fi
}

test_bootloader_port() {
    local exe="$1" port_name="$2"
    local attempt result code

    for attempt in 1 2 3; do
        [[ "$attempt" -gt 1 ]] && echo "  重试 $attempt/3..." && sleep 0.3
        set +e
        result="$(invoke_stm32flash_on_port "$exe" "$port_name" true)"
        code=$?
        set -e
        if [[ "$code" -eq 0 ]] && [[ "$result" =~ BootLoader|Version[[:space:]]*[:=]|Device[[:space:]]ID|STM32 ]]; then
            return 0
        fi
    done
    return 1
}

find_bootloader_port() {
    local exe="$1"
    local -a all_ports=()
    local p

    while IFS= read -r p; do
        [[ -n "$p" ]] && all_ports+=("$p")
    done < <(get_serial_ports)

    if [[ ${#all_ports[@]} -eq 0 ]]; then
        echo "未检测到任何串口，请检查 USB 连接与驱动。" >&2
        exit 1
    fi

    local -a try_list=()
    local alias
    while IFS= read -r alias; do
        [[ -n "$alias" ]] || continue
        if dev="$(resolve_port_selector "$alias" 2>/dev/null)"; then
            try_list+=("$dev")
        fi
    done < <(get_config_port_aliases)
    [[ -n "$PORT" ]] && try_list+=("$PORT")
    try_list+=("${all_ports[@]}")
    local -a unique=()
    local seen=""
    for p in "${try_list[@]}"; do
        [[ "$seen" == *"|$p|"* ]] && continue
        seen+="|$p|"
        unique+=("$p")
    done

    echo "自动扫描串口: $(IFS=', '; echo "${all_ports[*]}")"
    echo "按顺序尝试连接 BootLoader..."

    for p in "${unique[@]}"; do
        local found=false
        for ap in "${all_ports[@]}"; do [[ "$ap" == "$p" ]] && found=true; done
        [[ "$found" != true ]] && continue
        echo -n "  尝试 $p ..."
        if test_bootloader_port "$exe" "$p"; then
            echo " 成功"
            PORT="$p"
            return
        fi
        echo " 无响应"
    done

    cat >&2 <<EOF
未找到可用的 BootLoader 串口。请确认：
  1. 板子已供电，串口未被其他程序占用
  2. DTR/RTS 模式与 FlyMCU 一致：$FLYMCU_INIT_LABEL
  3. 若仍失败：手动将 BOOT0 置高 -> 按 RST -> 再试（设置里可关闭 DTR/RTS 自动进 Boot）
EOF
    exit 1
}

invoke_platformio_build() {
    local platformio_dir="$PROJECT_ROOT/PlatformIO"
    if [[ ! -d "$platformio_dir" ]]; then
        echo "未找到 PlatformIO 目录: $platformio_dir" >&2
        exit 1
    fi
    echo "构建固件: env=$ENVIRONMENT"
    cd "$platformio_dir"
    "$PYTHON" -m platformio run -e "$ENVIRONMENT"
}

invoke_flash_workflow() {
    local exe="$1" mode="$2" auto_pick="${3:-false}" do_build="${4:-false}"

    finalize_port_resolution || true
    [[ "$do_build" == true ]] && invoke_platformio_build

    local firmware_path
    firmware_path="$(resolve_firmware_path "$FIRMWARE")"

    if [[ "$auto_pick" == true || -z "$PORT" ]]; then
        find_bootloader_port "$exe"
    else
        local available
        available="$(get_serial_ports | paste -sd', ' -)"
        if [[ -n "$available" ]] && [[ ",$available," != *",$PORT,"* ]]; then
            echo "警告: 指定串口 $PORT 当前不在系统列表中（$available），仍将尝试。" >&2
        fi
    fi

    local init_desc
    if [[ "$USE_INIT" != true ]]; then
        init_desc="(关闭)"
    elif is_dtr_rts_sequence "$INIT_SEQUENCE"; then
        init_desc="DTR/RTS - $INIT_SEQUENCE"
    else
        init_desc="GPIO - $INIT_SEQUENCE"
    fi

    echo ""
    echo "======== 烧录参数 ========"
    echo "串口     : $PORT$( [[ -n "$PORT_SELECTOR" && "$PORT_SELECTOR" != "$PORT" ]] && echo " ($PORT_SELECTOR)" )"
    echo "波特率   : $BAUD"
    echo "初始化   : $init_desc"
    echo "固件     : $firmware_path"
    echo "模式     : $mode"
    echo "=========================="
    echo ""

    case "$mode" in
        Unprotect)
            echo "[1/1] 解除读保护"
            invoke_stm32flash_on_port "$exe" "$PORT" false "-k"
            ;;
        Erase)
            echo "[1/1] 擦除 Flash"
            invoke_stm32flash_on_port "$exe" "$PORT" false "-o"
            ;;
        WriteOnly)
            echo "[1/1] 写入固件"
            local -a write_args=("-w" "$firmware_path")
            [[ "$SKIP_VERIFY" != true ]] && write_args+=("-v")
            [[ "$NO_RUN" != true ]] && write_args+=("-g" "0x0")
            invoke_stm32flash_on_port "$exe" "$PORT" false "${write_args[@]}"
            ;;
        Full)
            if [[ "$SKIP_UNPROTECT" != true ]]; then
                echo "[1/3] 解除读保护"
                invoke_stm32flash_on_port "$exe" "$PORT" false "-k"
                wait_after_mcu_reset "解除读保护后 MCU 复位"
            else
                echo "[1/3] 跳过解除读保护"
            fi
            echo "[2/3] 擦除 Flash"
            invoke_stm32flash_on_port "$exe" "$PORT" false "-o"
            wait_after_mcu_reset "擦除完成后"
            echo "[3/3] 写入固件"
            local -a write_args=("-w" "$firmware_path")
            [[ "$SKIP_VERIFY" != true ]] && write_args+=("-v")
            [[ "$NO_RUN" != true ]] && write_args+=("-g" "0x0")
            invoke_stm32flash_on_port "$exe" "$PORT" false "${write_args[@]}"
            ;;
    esac

    local save_selector="${PORT_SELECTOR:-$PORT}"
    save_config "$save_selector" "$BAUD" "$INIT_SEQUENCE" "$USE_INIT" "$firmware_path" "$mode"
    echo ""
    echo "完成。"
}

show_main_menu() {
    local exe="$1"
    while true; do
        clear || true
        local ports fw_name port_info
        ports="$(serial_port_list | paste -sd' | ' -)"
        [[ -z "$ports" ]] && ports="无"
        if [[ -n "$PORT_SELECTOR" ]]; then
            port_info="$PORT_SELECTOR -> ${PORT:-未解析}"
        else
            port_info="${PORT:-(自动)}"
        fi
        fw_name="(未找到，请先编译或设置)"
        if fw_path="$(resolve_firmware_path "$FIRMWARE" 2>/dev/null)"; then
            fw_name="$(basename "$fw_path")"
        fi

        echo "========================================"
        echo "   STM32 串口烧录 (stm32flash)"
        echo "========================================"
        echo "检测到串口: $ports"
        echo "当前串口  : $port_info"
        echo "波特率    : $BAUD"
        echo "固件      : $fw_name"
        echo "DTR/RTS   : $( [[ "$USE_INIT" == true ]] && echo "FlyMCU - $FLYMCU_INIT_LABEL ($INIT_SEQUENCE)" || echo "关" )"
        echo "----------------------------------------"
        echo "  1. 完整烧录 (解除保护 + 擦除 + 写入 + 校验 + 运行)"
        echo "  2. 仅解除读保护"
        echo "  3. 仅擦除"
        echo "  4. 仅写入固件 (跳过解除保护)"
        echo "  5. 自动模式 (扫描串口并连接后完整烧录)"
        echo "  6. 先 PlatformIO 编译再完整烧录"
        echo "  7. 设置..."
        echo "  8. 安装 stm32flash"
        echo "  0. 退出"
        echo "----------------------------------------"

        local choice
        read -r -p "请选择: " choice || return
        case "$choice" in
            0) return ;;
            1) invoke_flash_workflow "$exe" Full "$([[ -z "$PORT" ]] && echo true || echo false)" false || true; read -r -p "按 Enter 继续..." _ ;;
            2) invoke_flash_workflow "$exe" Unprotect "$([[ -z "$PORT" ]] && echo true || echo false)" false || true; read -r -p "按 Enter 继续..." _ ;;
            3) invoke_flash_workflow "$exe" Erase "$([[ -z "$PORT" ]] && echo true || echo false)" false || true; read -r -p "按 Enter 继续..." _ ;;
            4) SKIP_UNPROTECT=true; invoke_flash_workflow "$exe" WriteOnly "$([[ -z "$PORT" ]] && echo true || echo false)" false || true; read -r -p "按 Enter 继续..." _ ;;
            5) local saved_sel="$PORT_SELECTOR" saved_port="$PORT"; PORT_SELECTOR=""; PORT=""; invoke_flash_workflow "$exe" Full true false || true; PORT_SELECTOR="$saved_sel"; PORT="$saved_port"; read -r -p "按 Enter 继续..." _ ;;
            6) invoke_flash_workflow "$exe" Full "$([[ -z "$PORT" ]] && echo true || echo false)" true || true; read -r -p "按 Enter 继续..." _ ;;
            7) show_settings_menu ;;
            8) "$SCRIPT_DIR/install-stm32flash.sh" || true; read -r -p "按 Enter 继续..." _ ;;
            *) echo "无效选项"; sleep 1 ;;
        esac
    done
}

show_settings_menu() {
    while true; do
        local fw_name port_info
        fw_name="(未设置)"
        if fw_path="$(resolve_firmware_path "$FIRMWARE" 2>/dev/null)"; then
            fw_name="$(basename "$fw_path")"
        fi
        if [[ -n "$PORT_SELECTOR" ]]; then
            port_info="$PORT_SELECTOR -> ${PORT:-未解析}"
        else
            port_info="${PORT:-(自动)}"
        fi

        echo ""
        echo "--- 设置 ---"
        echo "  1. 串口      : $port_info"
        echo "  2. 固件      : $fw_name"
        echo "  3. 波特率    : $BAUD"
        echo "  4. 初始化序列: $INIT_SEQUENCE"
        echo "  5. 使用 DTR/RTS 自动进 Boot: $( [[ "$USE_INIT" == true ]] && echo 是 || echo 否 )"
        echo "  6. 跳过解除读保护: $( [[ "$SKIP_UNPROTECT" == true ]] && echo 是 || echo 否 )"
        echo "  7. 跳过校验  : $( [[ "$SKIP_VERIFY" == true ]] && echo 是 || echo 否 )"
        echo "  8. 烧录后不运行: $( [[ "$NO_RUN" == true ]] && echo 是 || echo 否 )"
        echo "  9. 恢复默认初始化序列"
        echo "  0. 返回主菜单"

        local choice
        read -r -p "请选择: " choice
        case "$choice" in
            0) return ;;
            1)
                echo "--- 选择串口（按稳定地址，非 ACM 编号）---"
                serial_port_list
                echo ""
                local -a aliases=()
                while IFS= read -r a; do [[ -n "$a" ]] && aliases+=("$a"); done < <(get_config_port_aliases)
                local i
                for i in "${!aliases[@]}"; do
                    local mark=""
                    [[ "${aliases[$i]}" == "$PORT_SELECTOR" ]] && mark=" <-- 当前"
                    echo "  $((i + 1)). 别名 ${aliases[$i]}$mark"
                done
                local base=$(( ${#aliases[@]} + 1 ))
                local -a devs=()
                while IFS= read -r p; do [[ -n "$p" ]] && devs+=("$p"); done < <(get_serial_ports)
                for i in "${!devs[@]}"; do
                    local mark=""
                    [[ "${devs[$i]}" == "$PORT" && -z "$PORT_SELECTOR" ]] && mark=" <-- 当前"
                    echo "  $((base + i)). 设备 ${devs[$i]}$mark"
                done
                echo "  A. 自动扫描并连接"
                echo "  0. 返回"
                read -r -p "请选择: " c
                if [[ "$c" == "0" ]]; then continue; fi
                if [[ "$c" =~ ^[aA]$ ]]; then PORT_SELECTOR=""; PORT=""; continue; fi
                if [[ "$c" =~ ^[0-9]+$ ]] && (( c >= 1 && c <= ${#aliases[@]} )); then
                    PORT_SELECTOR="${aliases[$((c - 1))]}"
                    finalize_port_resolution || true
                    continue
                fi
                if [[ "$c" =~ ^[0-9]+$ ]] && (( c > ${#aliases[@]} && c - base < ${#devs[@]} )); then
                    PORT="${devs[$((c - base))]}"
                    PORT_SELECTOR="$PORT"
                    continue
                fi
                ;;
            2)
                read -r -p "固件完整路径: " FIRMWARE
                ;;
            3)
                read -r -p "波特率 (默认 115200): " new_baud
                [[ -n "$new_baud" ]] && BAUD="$new_baud"
                ;;
            4)
                read -r -p "DTR/RTS 序列 (如 dtr=0,rts=1,-100,dtr=1) 或 GPIO -i 序列: " INIT_SEQUENCE
                ;;
            5) USE_INIT=$([[ "$USE_INIT" == true ]] && echo false || echo true) ;;
            6) SKIP_UNPROTECT=$([[ "$SKIP_UNPROTECT" == true ]] && echo false || echo true) ;;
            7) SKIP_VERIFY=$([[ "$SKIP_VERIFY" == true ]] && echo false || echo true) ;;
            8) NO_RUN=$([[ "$NO_RUN" == true ]] && echo false || echo true) ;;
            9) INIT_SEQUENCE="$DEFAULT_INIT_SEQUENCE" ;;
        esac
    done
}

is_cli_mode() {
    [[ -n "$PORT_SELECTOR" || -n "$FIRMWARE" || -n "$INIT_SEQUENCE" || "$BUILD" == true || "$AUTO" == true \
        || "$LIST_PORTS" == true \
        || "$ONLY_UNPROTECT" == true || "$ONLY_ERASE" == true || "$SKIP_UNPROTECT" == true \
        || "$SKIP_VERIFY" == true || "$NO_RUN" == true || "$NO_INIT" == true ]]
}

apply_saved_config

if [[ "$LIST_PORTS" == true ]]; then
    echo "串口列表（稳定地址）:"
    serial_port_list
    exit 0
fi

finalize_port_resolution || true

STM32FLASH="$(resolve_stm32flash)"

if [[ "$MENU" == true ]] || { ! is_cli_mode && [[ "$AUTO" != true ]]; }; then
    show_main_menu "$STM32FLASH"
    exit 0
fi

if [[ "$AUTO" == true ]]; then
    mode=Full
    [[ "$ONLY_UNPROTECT" == true ]] && mode=Unprotect
    [[ "$ONLY_ERASE" == true ]] && mode=Erase
    invoke_flash_workflow "$STM32FLASH" "$mode" true "$BUILD"
    exit 0
fi

if [[ "$ONLY_UNPROTECT" == true && "$ONLY_ERASE" == true ]]; then
    echo "-OnlyUnprotect 与 -OnlyErase 不能同时使用。" >&2
    exit 1
fi

mode=Full
[[ "$ONLY_UNPROTECT" == true ]] && mode=Unprotect
[[ "$ONLY_ERASE" == true ]] && mode=Erase
[[ "$SKIP_UNPROTECT" == true && "$ONLY_UNPROTECT" != true && "$ONLY_ERASE" != true ]] && mode=WriteOnly

finalize_port_resolution || true
invoke_flash_workflow "$STM32FLASH" "$mode" "$([[ -z "$PORT" ]] && echo true || echo false)" "$BUILD"
