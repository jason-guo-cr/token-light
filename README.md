# token-light

Token Light is a small desk dashboard for the Waveshare ESP32-S3-RLCD-4.2. A Mac host process reads local Codex/OpenAI state, builds a compact JSON snapshot, and sends it to the board over USB serial.

The board currently shows:

- Date, weekday, live/stale status, and Mac battery.
- Beijing weather, for example `BJ CLD 29C`.
- Current time.
- `QUERY HH:MM`, the last successful Codex usage-limit API query.
- 5-hour and weekly Codex usage-limit remaining percentages.
- Local Codex token consumption for today and this week.

Credentials stay on the Mac. The ESP32 only receives rendered snapshot data over serial.

## Hardware

- Board: Waveshare `ESP32-S3-RLCD-4.2`
- Display driver: ST7305 via U8g2
- Connection: USB Serial/JTAG

The host auto-detects Espressif USB Serial/JTAG ports. You usually do not need to pass a port manually after moving the board to a different USB socket.

## Host Setup

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -U pip
.venv/bin/python -m pip install -r requirements-dev.txt
```

The package also installs a `token-light` console entrypoint, but the examples below use `python -m token_light.cli` so they work directly inside the repo.

## Build And Flash

Bootstrap the Waveshare display driver once:

```bash
./scripts/bootstrap_waveshare_driver.sh
```

Build firmware:

```bash
.venv/bin/pio run -d firmware
```

Flash firmware. Replace the port only if you want to force a specific device:

```bash
.venv/bin/pio run -d firmware -t upload --upload-port /dev/cu.usbmodem1101
```

If the background sync service is running, stop it before flashing so it does not hold the serial port:

```bash
launchctl bootout gui/$(id -u) /Users/jasonguo/Library/LaunchAgents/com.token-light.sync.plist 2>/dev/null || true
```

## Run Live

Default live sync:

```bash
.venv/bin/python -m token_light.cli
```

Recommended Beijing weather setup:

```bash
.venv/bin/python -m token_light.cli \
  --interval 60 \
  --usage-interval 600 \
  --weather-lat 39.9042 \
  --weather-lon 116.4074 \
  --weather-label BJ
```

Important intervals:

- `--interval 60`: sends a fresh screen snapshot every minute, keeping the clock current.
- `--usage-interval 600`: requests Codex 5H/WEEK usage-limit data every 10 minutes.
- `--weather-interval 1800`: requests weather every 30 minutes.

The host still sends a screen snapshot every minute even when usage-limit data is cached. If the usage API is rate-limited, failed attempts also wait for `--usage-interval` before retrying.

## Background Sync

This machine uses a LaunchAgent at:

```text
/Users/jasonguo/Library/LaunchAgents/com.token-light.sync.plist
```

Current intended arguments:

```text
/Users/jasonguo/code/mycode/token-light/.venv/bin/python
-m
token_light.cli
--interval
60
--usage-interval
600
--weather-lat
39.9042
--weather-lon
116.4074
--weather-label
BJ
```

Restart the service:

```bash
: > /tmp/token-light.log
launchctl bootout gui/$(id -u) /Users/jasonguo/Library/LaunchAgents/com.token-light.sync.plist 2>/dev/null || true
launchctl bootstrap gui/$(id -u) /Users/jasonguo/Library/LaunchAgents/com.token-light.sync.plist
launchctl kickstart -k gui/$(id -u)/com.token-light.sync
```

Check service status and logs:

```bash
launchctl print gui/$(id -u)/com.token-light.sync | rg -n "state =|pid =|--interval|--usage-interval|--weather"
tail -40 /tmp/token-light.log
```

## Snapshot Data Sources

### Codex Usage Limits

The host reads the existing Codex Desktop auth token from:

```text
~/.codex/auth.json
```

It calls the ChatGPT backend usage endpoint and parses:

- Primary window: shown as `5H LIMIT`
- Secondary window: shown as `WEEK LIMIT`
- Reset labels in each card's top-right corner

`QUERY HH:MM` is the last successful usage-limit API query time.

### Token Consumption

Today/week token consumption is local-only. It is summed from Codex session JSONL logs under:

```text
~/.codex/sessions
~/.codex/archived_sessions
```

The screen displays compact totals:

- `TOKEN DAY 18.8M`
- `WEEK 95.9M`

### Battery

Mac battery is read with `pmset` and displayed in the top-right area.

### Weather

Weather uses Open-Meteo's forecast API and does not require an API key.

For Beijing:

```text
latitude: 39.9042
longitude: 116.4074
label: BJ
```

Weather display labels:

- `SUN`: clear
- `PCLD`: partly cloudy
- `CLD`: cloudy
- `FOG`: fog
- `RAIN`: rain
- `SNOW`: snow
- `STORM`: thunderstorm
- `WX`: unknown weather code

## Mock And Debug

Print one JSON snapshot with mock usage-limit data:

```bash
.venv/bin/python -m token_light.cli \
  --mock tests/fixtures/wham_usage.json \
  --weather-lat 39.9042 \
  --weather-lon 116.4074 \
  --weather-label BJ \
  --stdout \
  --once
```

Send one mock snapshot to the board:

```bash
.venv/bin/python -m token_light.cli \
  --mock tests/fixtures/wham_usage.json \
  --weather-lat 39.9042 \
  --weather-lon 116.4074 \
  --weather-label BJ \
  --once
```

Force a serial port only when auto-detection is not desired:

```bash
TOKEN_LIGHT_PORT=/dev/cu.usbmodem1101 .venv/bin/python -m token_light.cli
```

Disable optional data sources:

```bash
.venv/bin/python -m token_light.cli --no-battery --no-token-usage --no-weather
```

## Tests

Run all host-side tests:

```bash
.venv/bin/python -m unittest discover -s tests -v
```

Build firmware:

```bash
.venv/bin/pio run -d firmware
```

## Troubleshooting

### The Board Shows No Data

Check the sync service:

```bash
launchctl print gui/$(id -u)/com.token-light.sync | rg -n "state =|pid ="
tail -80 /tmp/token-light.log
```

If the serial port changed, the host should auto-detect it. To inspect detected ESP32 ports:

```bash
.venv/bin/python -c 'from token_light.serial_writer import detected_esp32_ports; print(detected_esp32_ports())'
```

### Upload Fails Because The Port Is Busy

Stop the background sync service before flashing:

```bash
launchctl bootout gui/$(id -u) /Users/jasonguo/Library/LaunchAgents/com.token-light.sync.plist 2>/dev/null || true
```

### Usage API Is Rate-Limited

The service intentionally separates screen refresh and usage polling:

- Screen snapshots: every 60 seconds.
- Codex usage-limit API calls: every 600 seconds.

If the API returns an error, the board can still show cached limit data when available, and the host waits 10 minutes before retrying.

### Weather Is Missing

Weather is optional. Ensure the service has coordinates:

```bash
--weather-lat 39.9042 --weather-lon 116.4074 --weather-label BJ
```

If the weather request fails, the host keeps the last successful weather snapshot instead of crashing.
