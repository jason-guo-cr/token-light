import unittest
from unittest.mock import patch

from token_light.serial_writer import encode_snapshot_line, resolve_serial_port, write_snapshot


class FakeSerial:
    def __init__(self):
        self.data = b""
        self.flushed = False

    def write(self, chunk):
        self.data += chunk

    def flush(self):
        self.flushed = True


class SerialWriterTests(unittest.TestCase):
    def test_encode_snapshot_line_is_compact_json_with_newline(self):
        line = encode_snapshot_line({"type": "snapshot", "status": "live"})

        self.assertEqual(line, b'{"type":"snapshot","status":"live"}\n')

    def test_write_snapshot_flushes_serial_port(self):
        fake = FakeSerial()

        write_snapshot(fake, {"type": "snapshot"})

        self.assertEqual(fake.data, b'{"type":"snapshot"}\n')
        self.assertTrue(fake.flushed)

    def test_resolve_auto_port_uses_detected_esp32_port(self):
        with patch("token_light.serial_writer.detected_esp32_ports", return_value=["/dev/cu.usbmodem1234"]):
            self.assertEqual(resolve_serial_port("auto"), "/dev/cu.usbmodem1234")

    def test_resolve_auto_port_reports_missing_esp32(self):
        with patch("token_light.serial_writer.detected_esp32_ports", return_value=[]):
            with self.assertRaisesRegex(RuntimeError, "No Espressif"):
                resolve_serial_port("auto")


if __name__ == "__main__":
    unittest.main()
