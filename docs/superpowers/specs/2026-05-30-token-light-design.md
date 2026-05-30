# Token Light Design

## Goal

Build a polished dashboard for the Waveshare ESP32-S3-RLCD-4.2 that shows current date/time, Codex usage-limit remaining percentages, reset times, and small status details in a black-and-white desk-display UI.

## Hardware

- Board: Waveshare ESP32-S3-RLCD-4.2
- MCU: ESP32-S3-WROOM-1-N16R8
- Display: 4.2 inch reflective LCD, 300 x 400 native resolution
- UI orientation: landscape, 400 x 300
- Host connection: USB serial through the Espressif USB JTAG/serial interface
- Current detected serial device: `/dev/cu.usbmodem3101`

## Data Source

The Mac host will fetch the same Codex usage-limit information shown in the Codex app usage menu. The ESP32 firmware will not store credentials or call ChatGPT/OpenAI endpoints directly.

Required host configuration:

- `TOKEN_LIGHT_PORT`: optional serial port override, defaulting to `/dev/cu.usbmodem3101`.
- `TOKEN_LIGHT_AUTH_FILE`: optional Codex auth file override, defaulting to `~/.codex/auth.json`.

The host agent reads the existing Codex ChatGPT login token from the local Codex auth file and calls `https://chatgpt.com/backend-api/wham/usage`. The endpoint returns `rate_limit.primary_window` and `rate_limit.secondary_window`, including `used_percent`, `limit_window_seconds`, and `reset_at`. The display computes remaining percent as `100 - used_percent`.

## Architecture

The system has two parts:

1. Mac sync agent
   - Reads the existing Codex ChatGPT login token from the local Codex auth file.
   - Calls the Codex usage endpoint used by the desktop app.
   - Extracts primary and secondary usage windows.
   - Sends compact JSON snapshots over USB serial once per minute and immediately at startup.

2. ESP32 display firmware
   - Initializes the ST7305 reflective LCD using Waveshare-compatible display settings.
   - Receives newline-delimited JSON snapshots over serial.
   - Keeps local time ticking between host updates.
   - Renders a high-contrast 1-bit dashboard optimized for reflective LCD readability.
   - Shows stale/offline state when host data stops arriving.

## Serial Protocol

The Mac sends newline-delimited JSON:

```json
{
  "type": "snapshot",
  "sent_at": "2026-05-30T15:50:00+08:00",
  "date": "05/30",
  "weekday": "SAT",
  "time": "15:50",
  "plan_type": "prolite",
  "primary": {
    "label": "5H LIMIT",
    "remaining_percent": 97,
    "used_percent": 3,
    "reset_label": "20:36",
    "reset_at": 1780144603,
    "window_minutes": 300
  },
  "secondary": {
    "label": "WEEK LIMIT",
    "remaining_percent": 96,
    "used_percent": 4,
    "reset_label": "05/31",
    "reset_at": 1780217709,
    "window_minutes": 10080
  },
  "status": "live"
}
```

If the host cannot fetch Codex usage data, it sends:

```json
{
  "type": "snapshot",
  "sent_at": "2026-05-30T15:50:00+08:00",
  "date": "05/30",
  "weekday": "SAT",
  "time": "15:50",
  "status": "api_error",
  "message": "Codex usage request failed"
}
```

The firmware ignores unknown fields so the host protocol can grow without requiring immediate firmware changes.

## UI Direction

The dashboard follows the provided reference image: monochrome, dense, technical, and clock-forward.

Landscape layout:

- Top status bar
  - Date and weekday on the left.
  - Sync/Wi-Fi-style indicator and data freshness on the right.
  - Battery indicator when battery voltage is available.

- Main clock panel
  - Large monospaced time in the center.
  - Thin rectangular frame and corner markers.
  - Local time continues ticking between host sync messages.

- Middle metric cards
  - Left card: `5H LIMIT`, remaining percent, horizontal progress bar, reset time.
  - Right card: `WEEK LIMIT`, remaining percent, horizontal progress bar, reset date.

- Bottom status strip
  - Temperature and humidity if the onboard sensor is supported in the selected firmware stack.
  - If sensor support is not ready in the first implementation, show host sync age and serial status.
  - A small black-and-white Codex pet-inspired pixel character appears near the lower right if it fits without reducing metric legibility.

## Error Handling

- Missing Codex auth file or ChatGPT token: Mac agent exits with a clear setup message and never sends fake live data.
- Codex usage request failure: Mac agent sends an `api_error` snapshot with the current time.
- Serial port unavailable: Mac agent lists detected `/dev/cu.*` candidates.
- Stale data on device: firmware displays `STALE` when no snapshot arrives for more than 5 minutes.
- Bad JSON: firmware ignores the line and keeps the last valid snapshot.

## Testing

Host agent tests:

- Codex auth parsing reads `tokens.access_token` without logging secrets.
- Usage response parsing handles primary and secondary windows.
- Remaining percent is computed as `100 - used_percent` and clamped between 0 and 100.
- Window labels map 300 minutes to `5H LIMIT` and 10080 minutes to `WEEK LIMIT`.
- Reset labels use local Asia/Shanghai dates and times.
- Serial message formatting produces one compact JSON object per line.

Firmware tests/build checks:

- Compile for ESP32-S3 with PSRAM enabled.
- Render mock snapshots for live, stale, and API error states.
- Verify that the screen layout uses fixed dimensions and does not overlap at 400 x 300.

Manual verification:

- Flash firmware to the detected ESP32-S3 serial device.
- Run host agent with mock data and confirm UI appears.
- Run host agent with the existing local Codex ChatGPT login token.
- Confirm the device updates at startup and then once per minute.

## Non-Goals

- Store Codex or ChatGPT credentials on the ESP32.
- Use Wi-Fi for the first version.
- Display OpenAI API billing/cost information.
- Build a full settings UI on the device.
