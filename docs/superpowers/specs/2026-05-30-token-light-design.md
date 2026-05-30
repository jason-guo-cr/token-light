# Token Light Design

## Goal

Build a polished dashboard for the Waveshare ESP32-S3-RLCD-4.2 that shows current date/time, real OpenAI usage and cost data, and small status details in a black-and-white desk-display UI.

## Hardware

- Board: Waveshare ESP32-S3-RLCD-4.2
- MCU: ESP32-S3-WROOM-1-N16R8
- Display: 4.2 inch reflective LCD, 300 x 400 native resolution
- UI orientation: landscape, 400 x 300
- Host connection: USB serial through the Espressif USB JTAG/serial interface
- Current detected serial device: `/dev/cu.usbmodem3101`

## Data Source

The Mac host will fetch real OpenAI organization usage and cost data. The ESP32 firmware will not store OpenAI credentials or call OpenAI directly.

Required host configuration:

- `OPENAI_ADMIN_KEY`: an OpenAI Admin API key with access to organization usage/cost endpoints.
- `TOKEN_LIGHT_PORT`: optional serial port override, defaulting to `/dev/cu.usbmodem3101`.
- `TOKEN_LIGHT_ORG_ID`: optional organization id if the API call or account setup requires explicit organization selection.

The display does not invent a remaining-balance value. OpenAI exposes organization usage and cost records via API, so the first implementation displays current month cost, today cost, token totals, and API freshness. If OpenAI later exposes an official balance or credit-grant endpoint for the user's account type, the host agent can add it as another snapshot field.

## Architecture

The system has two parts:

1. Mac sync agent
   - Reads configuration from environment variables or a local ignored `.env`.
   - Calls OpenAI organization usage/cost endpoints.
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
  "month_reset": "06/01 00:00",
  "month_cost_usd": 12.34,
  "today_cost_usd": 1.23,
  "input_tokens": 1234567,
  "output_tokens": 234567,
  "total_tokens": 1469134,
  "status": "live"
}
```

If the host cannot fetch OpenAI data, it sends:

```json
{
  "type": "snapshot",
  "sent_at": "2026-05-30T15:50:00+08:00",
  "date": "05/30",
  "weekday": "SAT",
  "time": "15:50",
  "status": "api_error",
  "message": "OpenAI request failed"
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
  - Left card: `MONTH COST`, current month cost, today cost, reset time.
  - Right card: `TOKEN USED`, current month token total, input/output split, last update.

- Bottom status strip
  - Temperature and humidity if the onboard sensor is supported in the selected firmware stack.
  - If sensor support is not ready in the first implementation, show host sync age and serial status.
  - A small black-and-white Codex pet-inspired pixel character appears near the lower right if it fits without reducing metric legibility.

## Error Handling

- Missing OpenAI Admin key: Mac agent exits with a clear setup message and never sends fake live data.
- OpenAI API failure: Mac agent sends an `api_error` snapshot with the current time.
- Serial port unavailable: Mac agent lists detected `/dev/cu.*` candidates.
- Stale data on device: firmware displays `STALE` when no snapshot arrives for more than 5 minutes.
- Bad JSON: firmware ignores the line and keeps the last valid snapshot.

## Testing

Host agent tests:

- Month window uses local Asia/Shanghai dates unless configured otherwise.
- OpenAI API response parsing handles empty buckets, multiple result rows, and missing optional token fields.
- Cost parsing computes current month and current day totals from official cost buckets.
- Serial message formatting produces one compact JSON object per line.

Firmware tests/build checks:

- Compile for ESP32-S3 with PSRAM enabled.
- Render mock snapshots for live, stale, and API error states.
- Verify that the screen layout uses fixed dimensions and does not overlap at 400 x 300.

Manual verification:

- Flash firmware to the detected ESP32-S3 serial device.
- Run host agent with mock data and confirm UI appears.
- Run host agent with a real OpenAI Admin key.
- Confirm the device updates at startup and then once per minute.

## Non-Goals

- Store OpenAI credentials on the ESP32.
- Use Wi-Fi for the first version.
- Invent an account-balance or remaining-quota number when no official source is available.
- Build a full settings UI on the device.
