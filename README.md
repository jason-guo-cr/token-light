# token-light

## Token Light

Token Light displays Codex usage-limit remaining percentages on a Waveshare ESP32-S3-RLCD-4.2 over USB serial.

### Setup

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -U pip
.venv/bin/python -m pip install -r requirements-dev.txt
```

### Build and flash

```bash
./scripts/bootstrap_waveshare_driver.sh
.venv/bin/pio run -d firmware
.venv/bin/pio run -d firmware -t upload --upload-port /dev/cu.usbmodem3101
```

### Test with mock data

```bash
.venv/bin/python -m token_light.cli --mock tests/fixtures/wham_usage.json --stdout --once
.venv/bin/python -m token_light.cli --mock tests/fixtures/wham_usage.json --once
```

### Run live

```bash
.venv/bin/python -m token_light.cli
```

The host reads the existing Codex Desktop auth token from `~/.codex/auth.json` and sends only usage snapshots to the ESP32. Credentials stay on the Mac.
The sync CLI auto-detects the Espressif USB Serial/JTAG port by default. Set `TOKEN_LIGHT_PORT=/dev/cu.usbmodemXXXX` or pass `--port /dev/cu.usbmodemXXXX` only when you want to force a specific port.
