from __future__ import annotations

import json
import time
from typing import Any


DEFAULT_PORT = "/dev/cu.usbmodem3101"
BAUD_RATE = 115200


def encode_snapshot_line(snapshot: dict[str, Any]) -> bytes:
    return (json.dumps(snapshot, separators=(",", ":"), ensure_ascii=True) + "\n").encode("utf-8")


def write_snapshot(port, snapshot: dict[str, Any]) -> None:
    port.write(encode_snapshot_line(snapshot))
    port.flush()


def open_serial_port(port_name: str = DEFAULT_PORT, settle_seconds: float = 3.0):
    import serial

    port = serial.Serial(port_name, BAUD_RATE, timeout=1)
    if settle_seconds > 0:
        time.sleep(settle_seconds)
    return port


def detected_serial_ports() -> list[str]:
    try:
        from serial.tools import list_ports
    except ModuleNotFoundError:
        return []

    return [port.device for port in list_ports.comports() if port.device.startswith("/dev/cu.")]
