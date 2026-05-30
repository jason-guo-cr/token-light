from __future__ import annotations

import json
import time
from typing import Any


DEFAULT_PORT = "auto"
BAUD_RATE = 115200
ESPRESSIF_VID = 0x303A
ESP32_USB_JTAG_PID = 0x1001


def encode_snapshot_line(snapshot: dict[str, Any]) -> bytes:
    return (json.dumps(snapshot, separators=(",", ":"), ensure_ascii=True) + "\n").encode("utf-8")


def write_snapshot(port, snapshot: dict[str, Any]) -> None:
    port.write(encode_snapshot_line(snapshot))
    port.flush()


def resolve_serial_port(port_name: str = DEFAULT_PORT) -> str:
    if port_name != "auto":
        return port_name

    ports = detected_esp32_ports()
    if not ports:
        raise RuntimeError("No Espressif USB serial port detected")
    return ports[0]


def open_serial_port(port_name: str = DEFAULT_PORT, settle_seconds: float = 3.0):
    import serial

    port = serial.Serial(resolve_serial_port(port_name), BAUD_RATE, timeout=1)
    if settle_seconds > 0:
        time.sleep(settle_seconds)
    return port


def detected_serial_ports() -> list[str]:
    try:
        from serial.tools import list_ports
    except ModuleNotFoundError:
        return []

    return [port.device for port in list_ports.comports() if port.device.startswith("/dev/cu.")]


def detected_esp32_ports() -> list[str]:
    try:
        from serial.tools import list_ports
    except ModuleNotFoundError:
        return []

    matches = []
    for port in list_ports.comports():
        if port.vid == ESPRESSIF_VID and port.pid == ESP32_USB_JTAG_PID:
            matches.append(port.device)
            continue
        text = " ".join(
            value
            for value in (port.manufacturer, port.product, port.description)
            if value
        ).lower()
        if "espressif" in text or "usb jtag" in text:
            matches.append(port.device)
    return matches
