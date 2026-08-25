# token-light

Token Light is a small desk dashboard for the Waveshare ESP32-S3-RLCD-4.2. A Mac host process reads local Codex/OpenAI state, builds a compact JSON snapshot, and sends it to the board over USB serial.

The board currently provides three local pages:

- `OVERVIEW`: clock, weather, battery, weekly quota, 60-minute token burn, quota forecast, onboard temperature/humidity, and the token pet.
- `CODEX NOW`: a fixed privacy-safe Codex activity state, elapsed time, pet pose, burn, quota, and forecast.
- `FOCUS`: a board-local 25/5-minute focus timer that keeps running without the Mac.

The GPIO18 KEY is active-low. Short press cycles pages, double press starts/pauses/resumes focus and opens the focus page, and long press resets focus. Long press on the other pages shows `VOICE OFF`; push-to-talk remains gated by the voice feasibility spike.

Codex completion plays one short non-blocking ES8311/I2S tone, except during the default Shanghai quiet period from 22:00 through 07:59. The board reads SHTC3 temperature and humidity every 60 seconds and falls back from cached to stale after five minutes.

Credentials stay on the Mac. The ESP32 only receives rendered snapshot data over serial.
Companion activity is restricted to fixed state, label, detail, elapsed-seconds, and completion-sequence fields. Prompts, answers, commands, tool input/output, paths, and Codex identifiers are never included.

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

Use `--audio-always` to override quiet hours or `--no-audio` to disable completion sound. Quota samples are retained for 14 days at `~/.cache/token-light/quota-history.jsonl`; override the location with `--quota-history`.

Important intervals:

- `--interval 60`: sends a fresh screen snapshot every minute, keeping the clock current.
- `--usage-interval 600`: requests current Codex quota-window data every 10 minutes.
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

### Codex Account Usage

The host uses the documented Codex App Server method
[`account/rateLimits/read`](https://learn.chatgpt.com/docs/app-server#6-rate-limits-chatgpt).
Codex owns authentication and token refresh; Token Light does not read or send
the access token itself.

The board renders the general `codex` bucket returned by the current account
instead of assuming that every plan has both a five-hour and weekly limit. It
uses one full-width card:

- `CODEX WEEK`: remaining percentage, used percentage, and reset date.

Model-specific preview buckets such as `GPT-5.3-Codex-Spark` are intentionally
not sent to the board. If the service returns more than one general window, the
dashboard prefers the longest window.

`SYNC HH:MM` is the last successful account sync time. After a temporary sync
failure, the board keeps the last good metrics and marks them `CACHE`.

The host finds `codex` on `PATH` and also checks the common Homebrew paths. To
select it explicitly:

```bash
TOKEN_LIGHT_CODEX_BIN=/opt/homebrew/bin/codex \
  .venv/bin/python -m token_light.cli --stdout --once
```

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

Run the pure firmware state-machine tests:

```bash
.venv/bin/pio test -d firmware -e native
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
