#!/usr/bin/env python3
"""IMU static + vibration test: mean, std, 3x3 covariance (ROS Imu layout)."""
from __future__ import annotations

import argparse
import math
import sys
import time
from typing import Iterable

sys.path.insert(0, '/home/jimmy/project/tank/ros_robot_controller_ros2/src/ros_robot_controller')

from ros_robot_controller.ros_robot_controller_sdk import Board


def cov3(samples: list[tuple[float, float, float]]) -> list[float]:
    """Sample covariance of (x,y,z); returns 9-element row-major matrix."""
    n = len(samples)
    if n < 2:
        return [0.0] * 9
    mx = sum(s[0] for s in samples) / n
    my = sum(s[1] for s in samples) / n
    mz = sum(s[2] for s in samples) / n
    c = [[0.0] * 3 for _ in range(3)]
    for x, y, z in samples:
        v = [x - mx, y - my, z - mz]
        for i in range(3):
            for j in range(3):
                c[i][j] += v[i] * v[j]
    denom = n - 1
    flat = [c[i][j] / denom for i in range(3) for j in range(3)]
    return flat


def stats1(values: Iterable[float]) -> tuple[float, float, float]:
    vals = list(values)
    n = len(vals)
    if n == 0:
        return 0.0, 0.0, 0.0
    mean = sum(vals) / n
    var = sum((v - mean) ** 2 for v in vals) / max(n - 1, 1)
    return mean, math.sqrt(var), var


def fmt_cov(label: str, mat: list[float]) -> None:
    print(f'  {label} (row-major 3x3):')
    for row in range(3):
        cells = ' '.join(f'{mat[row * 3 + col]:+.6e}' for col in range(3))
        print(f'    [{cells}]')
    print(f'    diag (var): roll/x={mat[0]:.6e}  pitch/y={mat[4]:.6e}  yaw/z={mat[8]:.6e}')


def collect(board: Board, seconds: float, period: float = 0.01) -> list[dict]:
    out: list[dict] = []
    t_end = time.time() + seconds
    while time.time() < t_end:
        cache = board._telemetry_cache
        if cache and 'imu' in cache:
            imu = cache['imu']
            ax, ay, az = imu['linear_acceleration']
            gx, gy, gz = imu['angular_velocity']
            out.append({
                'roll': imu['roll'],
                'pitch': imu['pitch'],
                'yaw': imu['yaw'],
                'acc': (ax, ay, az),
                'gyro': (gx, gy, gz),
                'qnorm': math.sqrt(sum(q * q for q in imu['orientation'])),
            })
        time.sleep(period)
    return out


def summarize(name: str, samples: list[dict]) -> None:
    if not samples:
        print(f'[{name}] no samples')
        return

    rolls = [math.degrees(s['roll']) for s in samples]
    pitches = [math.degrees(s['pitch']) for s in samples]
    yaws = [math.degrees(s['yaw']) for s in samples]
    rpy_rad = [(s['roll'], s['pitch'], s['yaw']) for s in samples]
    acc = [s['acc'] for s in samples]
    gyro = [s['gyro'] for s in samples]
    qnorms = [s['qnorm'] for s in samples]

    print(f'\n=== {name} ({len(samples)} frames) ===')
    for label, vals in ('roll°', rolls), ('pitch°', pitches), ('yaw°', yaws):
        mean, std, var = stats1(vals)
        print(f'  {label:7s} mean={mean:+.3f}  std={std:.4f}  var={var:.6e}')

    print(f'  |q|     min={min(qnorms):.4f}  max={max(qnorms):.4f}')

    ori_cov = cov3(rpy_rad)
    acc_cov = cov3(acc)
    gyro_cov = cov3(gyro)
    fmt_cov('orientation_covariance (rad²)', ori_cov)
    fmt_cov('linear_acceleration_covariance (m²/s⁴)', acc_cov)
    fmt_cov('angular_velocity_covariance (rad²/s²)', gyro_cov)

    print('  ROS node suggestion:')
    print(f"    orientation_covariance = {ori_cov}")
    print(f"    angular_velocity_covariance = {gyro_cov}")
    print(f"    linear_acceleration_covariance = {acc_cov}")


def main() -> int:
    parser = argparse.ArgumentParser(description='IMU flat + vibration covariance test')
    parser.add_argument('--port', default='/dev/ttyACM0')
    parser.add_argument('--warmup', type=float, default=3.0, help='wait after open (gyro static cal + AHRS init)')
    parser.add_argument('--static', type=float, default=10.0, help='static sample seconds')
    parser.add_argument('--vibrate', type=float, default=5.0, help='vibration sample seconds')
    parser.add_argument('--buzzer-hz', type=int, default=4000)
    args = parser.parse_args()

    print(f'Port: {args.port}')
    board = Board(device=args.port, baudrate=1000000, timeout=0.5, protocol_version='v2')
    board.enable_reception()

    print(f'Warmup {args.warmup:.0f}s (gyro bias cal on boot, keep still)...')
    time.sleep(args.warmup)

    print(f'Static capture {args.static:.0f}s (keep flat & still)...')
    static_samples = collect(board, args.static)

    print(f'Vibration capture {args.vibrate:.0f}s (buzzer {args.buzzer_hz} Hz)...')
    board.set_buzzer(args.buzzer_hz, 60.0, 0.0, 1)
    vib_samples = collect(board, args.vibrate)
    board.set_buzzer(0, 0, 0, 1)

    board.enable_recv = False
    time.sleep(0.2)
    board.port.close()

    summarize('STATIC (flat)', static_samples)
    summarize('VIBRATION (buzzer)', vib_samples)

    if static_samples:
        sr = [math.degrees(s['roll']) for s in static_samples]
        sp = [math.degrees(s['pitch']) for s in static_samples]
        print('\n=== PASS CRITERIA (static flat) ===')
        ok_roll = abs(sum(sr) / len(sr)) < 1.0 and max(abs(v - sum(sr) / len(sr)) for v in sr) < 2.0
        ok_pitch = abs(sum(sp) / len(sp)) < 1.0
        print(f'  roll  mean within ±1°: {"OK" if ok_roll else "FAIL"}')
        print(f'  pitch mean within ±1°: {"OK" if ok_pitch else "FAIL"}')
        return 0 if ok_roll and ok_pitch else 1
    return 1


if __name__ == '__main__':
    raise SystemExit(main())
