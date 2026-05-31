#!/usr/bin/env python3
"""按稳定 USB 地址解析 Linux 串口（避免 ttyACM0/1 互换）。"""

from __future__ import annotations

import glob
import json
import os
import subprocess
import sys
from typing import Any


def _udev_props(dev: str) -> dict[str, str]:
    try:
        out = subprocess.check_output(
            ["udevadm", "info", "-q", "property", "-n", dev],
            text=True,
            stderr=subprocess.DEVNULL,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return {}
    props: dict[str, str] = {}
    for line in out.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            props[k] = v
    return props


def _readlink(path: str) -> str | None:
    try:
        return os.path.realpath(path)
    except OSError:
        return None


def scan_ports() -> list[dict[str, Any]]:
    devices: list[str] = []
    for pattern in ("/dev/ttyACM*", "/dev/ttyUSB*"):
        devices.extend(glob.glob(pattern))
    devices = sorted(set(devices), key=lambda p: (len(p), p))

    by_id_dir = "/dev/serial/by-id"
    by_path_dir = "/dev/serial/by-path"
    rows: list[dict[str, Any]] = []

    for dev in devices:
        props = _udev_props(dev)
        by_id = None
        by_path = None
        if os.path.isdir(by_id_dir):
            for name in os.listdir(by_id_dir):
                link = os.path.join(by_id_dir, name)
                if _readlink(link) == dev:
                    by_id = name
                    break
        if os.path.isdir(by_path_dir):
            for name in os.listdir(by_path_dir):
                if "usbv2" in name:
                    continue
                link = os.path.join(by_path_dir, name)
                if _readlink(link) == dev:
                    by_path = name
                    break

        rows.append(
            {
                "dev": dev,
                "by_id": by_id,
                "by_path": by_path,
                "serial": props.get("ID_SERIAL_SHORT") or props.get("ID_USB_SERIAL_SHORT"),
                "usb_path": props.get("ID_PATH"),
                "vendor": props.get("ID_VENDOR_ID"),
                "model": props.get("ID_MODEL_ID"),
            }
        )
    return rows


def _match_entry(row: dict[str, Any], entry: dict[str, Any]) -> bool:
    for key in ("by_id", "by_path", "usb_path", "serial"):
        want = entry.get(key)
        if not want:
            continue
        if key == "usb_path":
            if row.get("usb_path") == want or row.get("by_path") == want:
                return True
            continue
        if row.get(key) == want:
            return True
    return False


def resolve_selector(selector: str, ports_cfg: dict[str, Any] | None = None) -> str:
    selector = (selector or "").strip()
    if not selector:
        raise SystemExit("未指定串口选择器")

    rows = scan_ports()
    if not rows:
        raise SystemExit("未检测到串口设备")

    ports_cfg = ports_cfg or {}

    # 别名：rrc_flash / host_link
    if selector in ports_cfg and isinstance(ports_cfg[selector], dict):
        entry = ports_cfg[selector]
        for row in rows:
            if _match_entry(row, entry):
                return row["dev"]
        raise SystemExit(f"未找到别名 '{selector}' 对应的设备（配置: {entry}）")

    if selector.startswith("by-id:"):
        name = selector[6:].strip()
        path = name if name.startswith("/") else os.path.join("/dev/serial/by-id", name)
        dev = _readlink(path)
        if dev:
            return dev
        raise SystemExit(f"by-id 不存在: {path}")

    if selector.startswith("by-path:"):
        name = selector[8:].strip()
        path = name if name.startswith("/") else os.path.join("/dev/serial/by-path", name)
        dev = _readlink(path)
        if dev:
            return dev
        raise SystemExit(f"by-path 不存在: {path}")

    if selector.startswith("/dev/"):
        if os.path.exists(selector):
            return selector
        raise SystemExit(f"设备不存在: {selector}")

    # 裸序列号
    for row in rows:
        if row.get("serial") == selector:
            return row["dev"]

    raise SystemExit(f"无法解析串口选择器: {selector}")


def format_port_line(row: dict[str, Any], ports_cfg: dict[str, Any] | None = None) -> str:
    ports_cfg = ports_cfg or {}
    alias = None
    label = None
    for name, entry in ports_cfg.items():
        if isinstance(entry, dict) and _match_entry(row, entry):
            alias = name
            label = entry.get("label") or name
            break
    parts = [row["dev"]]
    if alias:
        parts.append(f"[{alias}]")
    if label and label != alias:
        parts.append(label)
    if row.get("serial"):
        parts.append(f"SN={row['serial']}")
    if row.get("by_path"):
        parts.append(f"path={row['by_path']}")
    return " ".join(parts)


def cmd_list(config_path: str | None) -> None:
    ports_cfg: dict[str, Any] = {}
    if config_path and os.path.isfile(config_path):
        with open(config_path, encoding="utf-8") as f:
            cfg = json.load(f)
        ports_cfg = cfg.get("Ports") or {}

    rows = scan_ports()
    if not rows:
        print("（无串口）")
        return
    for row in rows:
        print(format_port_line(row, ports_cfg))


def cmd_resolve(selector: str, config_path: str | None) -> None:
    ports_cfg: dict[str, Any] = {}
    if config_path and os.path.isfile(config_path):
        with open(config_path, encoding="utf-8") as f:
            cfg = json.load(f)
        ports_cfg = cfg.get("Ports") or {}
    print(resolve_selector(selector, ports_cfg))


def main() -> None:
    if len(sys.argv) < 2:
        print("用法: serial_port_resolve.py list|resolve <selector> [config.json]", file=sys.stderr)
        sys.exit(2)
    cmd = sys.argv[1]
    config_path = sys.argv[3] if len(sys.argv) > 3 else (sys.argv[2] if cmd == "list" else None)
    if cmd == "list":
        cmd_list(sys.argv[2] if len(sys.argv) > 2 else None)
    elif cmd == "resolve":
        if len(sys.argv) < 3:
            print("缺少 selector", file=sys.stderr)
            sys.exit(2)
        selector = sys.argv[2]
        cfg = sys.argv[3] if len(sys.argv) > 3 else None
        cmd_resolve(selector, cfg)
    else:
        print(f"未知命令: {cmd}", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
