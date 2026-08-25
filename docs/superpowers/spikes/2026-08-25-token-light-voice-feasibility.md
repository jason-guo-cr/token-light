# Token Light Voice Feasibility Spike

**Date:** 2026-08-25
**Decision:** NO-GO for v0.1 because the hardware gates remain unproven; this document records a static investigation, not a completed feasibility measurement.

## Gate

The companion specification requires the board to enter `SENDING` within three seconds after release and show a result within ten seconds end to end. Audio must remain in RAM, must not be written to TF storage, and must not interfere with the dashboard serial channel.

## Evidence

- The current `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1` configuration maps `Serial` to the ESP32-S3 hardware USB CDC / USB Serial-JTAG path, not a 115200-baud UART. Arduino-ESP32's `HWCDC::begin(baud)` does not configure a baud rate, and its implementation explicitly reports that USB Serial/JTAG has no configurable baud rate. Therefore `115200 / 10 = 11.5 KB/s` and the earlier derived 40-second transfer time are not valid evidence for this build. Fifteen seconds of 16 kHz, mono, 16-bit PCM is still about 480 KB, but USB CDC throughput and p50/p95 transfer latency must be measured on the physical board. See the [Arduino-ESP32 HWCDC implementation](https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/HWCDC.cpp) and [ESP-IDF USB Serial/JTAG guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/usb-serial-jtag-console.html).
- The board exposes ES8311 playback and ES7210 microphone capture on a shared audio path. The official board mapping is I2C SDA 13 / SCL 14 and I2S MCLK 16 / BCLK 9 / WS 45 / DIN 10 / DOUT 8, with PA on GPIO46.
- USB Audio Class would require changing the current USB descriptor/runtime and validating coexistence with USB Serial/JTAG. The current Arduino/PlatformIO configuration does not provide that path.
- A LAN transport could satisfy throughput, but it adds Wi-Fi provisioning, authenticated transport, request cancellation, RAM lifecycle, and a new privacy boundary that v0.1 does not otherwise require.
- An ESP32-S3 USB Serial/JTAG device is visible on the development Mac, but no firmware was flashed and no microphone capture was performed because device mutation was not authorized in this task.

Primary hardware references:

- <https://github.com/waveshareteam/ESP32-S3-RLCD-4.2/tree/main/02_Example/Arduino/07_Audio_Test>
- <https://docs.waveshare.com/ESP32-S3-RLCD-4.2>

## Decision

The latency, microphone capture, dashboard coexistence, memory clearing, and stability gates have not been measured, so the Phase 5 threshold is **unproven** and the correct v0.1 decision remains `NO-GO`. v0.1 keeps the long-press fallback (`VOICE OFF`) and does not implement recording or audio transport. A future hardware spike should compare USB CDC, USB Audio Class, and authenticated LAN transfer on the physical board; measure p50/p95 release-to-`SENDING`, transfer, and end-to-end latency; exercise concurrent dashboard traffic; and verify that RAM buffers are cleared after completion or failure.
