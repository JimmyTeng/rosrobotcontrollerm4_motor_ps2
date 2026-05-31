#!/usr/bin/env python3
"""Read live IMU fused telemetry from RRC (V2 protocol)."""
import math
import sys
import time

sys.path.insert(0, '/home/jimmy/project/tank/ros_robot_controller_ros2/src/ros_robot_controller')

from ros_robot_controller.ros_robot_controller_sdk import Board


def probe_port(port: str, seconds: float = 3.0):
    print(f'--- {port} ({seconds:.0f}s) ---')
    try:
        board = Board(device=port, baudrate=1000000, timeout=0.5, protocol_version='v2')
    except Exception as e:
        print(f'  open failed: {e}')
        return False

    board.enable_reception()

    samples = []
    t0 = time.time()
    while time.time() - t0 < seconds:
        imu = board.get_imu_fused()
        if imu is not None:
            qw, qx, qy, qz = imu['orientation']
            qnorm = math.sqrt(qw*qw + qx*qx + qy*qy + qz*qz)
            samples.append({
                'roll_deg': math.degrees(imu['roll']),
                'pitch_deg': math.degrees(imu['pitch']),
                'yaw_deg': math.degrees(imu['yaw']),
                'qnorm': qnorm,
            })
        time.sleep(0.05)

    board.port.close()

    if not samples:
        print('  no telemetry frames')
        return False

    rolls = [s['roll_deg'] for s in samples]
    pitches = [s['pitch_deg'] for s in samples]
    qnorms = [s['qnorm'] for s in samples]
    print(f'  frames: {len(samples)}')
    print(f'  roll  deg: min={min(rolls):+.2f} max={max(rolls):+.2f} mean={sum(rolls)/len(rolls):+.2f}')
    print(f'  pitch deg: min={min(pitches):+.2f} max={max(pitches):+.2f} mean={sum(pitches)/len(pitches):+.2f}')
    print(f'  q norm:    min={min(qnorms):.4f} max={max(qnorms):.4f}')
    last = samples[-1]
    print(f'  last: roll={last["roll_deg"]:+.2f} pitch={last["pitch_deg"]:+.2f} yaw={last["yaw_deg"]:+.2f}')
    ok = 0.99 <= min(qnorms) <= 1.01 and max(qnorms) <= 1.01
    print(f'  quaternion normalized: {"OK" if ok else "FAIL"}')
    return ok


def main():
    ports = sys.argv[1:] or ['/dev/ttyACM0', '/dev/ttyACM1']
    any_ok = False
    for p in ports:
        if probe_port(p):
            any_ok = True
    return 0 if any_ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
