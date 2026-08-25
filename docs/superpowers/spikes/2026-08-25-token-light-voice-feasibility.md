# Token Light Voice Feasibility Spike

**Date:** 2026-08-25
**Decision:** Do not implement the full PTT path in v0.1.

## Gate

The companion specification requires the board to enter `SENDING` within three seconds after release and show a result within ten seconds end to end. Audio must remain in RAM, must not be written to TF storage, and must not interfere with the dashboard serial channel.

## Evidence

- The current dashboard uses the ESP32-S3 USB Serial/JTAG channel at 115200 baud. Fifteen seconds of 16 kHz, mono, 16-bit PCM is about 480 KB. The UART-rate upper bound is about 11.5 KB/s before framing overhead, so uncompressed transfer alone would exceed 40 seconds.
- The board exposes ES8311 playback and ES7210 microphone capture on a shared audio path. The official board mapping is I2C SDA 13 / SCL 14 and I2S MCLK 16 / BCLK 9 / WS 45 / DIN 10 / DOUT 8, with PA on GPIO46.
- USB Audio Class would require changing the current USB descriptor/runtime and validating coexistence with USB Serial/JTAG. The current Arduino/PlatformIO configuration does not provide that path.
- A LAN transport could satisfy throughput, but it adds Wi-Fi provisioning, authenticated transport, request cancellation, RAM lifecycle, and a new privacy boundary that v0.1 does not otherwise require.
- An ESP32-S3 USB Serial/JTAG device is visible on the development Mac, but no firmware was flashed and no microphone capture was performed because device mutation was not authorized in this task.

Primary hardware references:

- <https://github.com/waveshareteam/ESP32-S3-RLCD-4.2/tree/main/02_Example/Arduino/07_Audio_Test>
- <https://docs.waveshare.com/ESP32-S3-RLCD-4.2>

## Decision

The latency, coexistence, and privacy gates are not demonstrated, so v0.1 keeps the long-press fallback (`VOICE OFF`) and does not implement recording or audio transport. A future spike should compare compressed USB CDC and authenticated LAN transfer on the physical board, measure p50/p95 release-to-`SENDING` and end-to-end latency, and verify that buffers are zeroed after completion or failure.
