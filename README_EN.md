# Pico2 UAC12 USB DDC (TinyUSB UAC1/UAC2 Selectable)

[English](README_EN.md) | [日本語](README.md)

## Description

This project is a major update of `Pico2_UAC2_USBDDC_VerFinal_11FFT_NoOverSampling`.

The USB audio subsystem has been redesigned around TinyUSB, allowing USB Audio Class 1.0 and 2.0 selection at build time while sharing a common audio-processing core.

### What's New

- UAC1 / UAC2 selectable build configuration
- TinyUSB-based USB audio layer
- Common Audio State / RingBuffer / Feedback architecture
- Improved diagnostic screen with buffer monitoring
- USB event logging (up to 64 events)
- Enhanced stream restart and sample-rate switching handling
- Removal of unused GPIO debug functions
- High-power clock mode fixed operation
- Shared audio-processing core across UAC1 and UAC2

Audio processing remains:

- No Oversampling (NOS)
- No FIR Filtering
- No Sample Rate Conversion

Display features such as the 11-band FFT analyzer, real-time level meters, and buffer water-level monitor have been retained and improved.

**Build Verified**

- Raspberry Pi Pico SDK v2.3.1
- Toolchain 15_2_Rel1

---

## Design Goals

This project was developed with the following goals:

- Simplify the audio signal path
- Remove unnecessary software DSP processing
- Avoid oversampling, FIR filtering, and sample-rate conversion in the signal path
- Improve maintainability by separating USB Audio Class specific code
- Share a common audio engine across UAC1 and UAC2
- Provide real-time diagnostics for troubleshooting

Software oversampling and FIR filtering were removed because the PCM5100A already implements internal oversampling and digital filtering.

This project does not claim bit-perfect playback. It does not perform oversampling, FIR filtering, or sample-rate conversion, but it always applies a fixed output gain ratio of 0.80 and also applies UAC1/UAC2 host volume settings (0 to -64 dB) and mute in software.

---

## Change History

### Major Changes from Pico2_UAC2_USBDDC_VerFinal_11FFT_NoOverSampling

- Added UAC1/UAC2 build-time selection using `USB_AUDIO_CLASS_VERSION`
- Migrated USB audio layer to TinyUSB
- Separated UAC1/UAC2 class-specific implementation
- Shared common Audio State, RingBuffer and Feedback architecture
- Added diagnostic screen with buffer monitor
- Added USB event log viewer (up to 64 events)
- Improved sample-rate switching sequence
- Enhanced stream restart handling
- Removed unused GPIO debug functionality
- Fixed operation in high-power clock mode
- Maintained RAM execution architecture

---

## Measurement Results

### Firmware Comparison

Ver7 → Ver8

The hardware configuration, measurement environment, and measurement conditions were identical; only the firmware was changed.

Ver7 is `Pico2_UAC2_USBDDC_VerFinal_11FFT_NoOverSampling`, and Ver8 is `Pico2_UAC12_USBDDC`.

### Ver7

- TinyUSB-based UAC2
- UAC2-only configuration
- Audio State, Ring Buffer, and Feedback control implemented as a UAC2-specific configuration
- Oversampling-related code removed
- FIR-related code removed
- ES-series DAC support code removed
- I2C output code removed

### Ver8

- Build-time selection between UAC1 and UAC2
- TinyUSB-based USB audio layer
- Audio State, Ring Buffer, and Feedback control unified
- Added a diagnostic screen with buffer monitoring
- Added a USB event log (up to 64 events)
- Improved stream resume handling
- Improved sample-rate switching
- Removed unused GPIO debugging functionality
- Fixed high-clock operation
- Adopted a common audio core for UAC1 and UAC2

### FFT Measurements

#### 100Hz

| Item | Ver7 | Ver8 |
|------|------:|------:|
| THD | 0.00030% | 0.00033% |
| THD+N | 0.00563% | 0.00564% |

#### 1kHz

| Item | Ver7 | Ver8 |
|------|------:|------:|
| THD | 0.00141% | 0.00104% |
| THD+N | 0.00508% | 0.00484% |

#### 10kHz

| Item | Ver7 | Ver8 |
|------|------:|------:|
| THD | 0.00988% | 0.00991% |
| THD+N | 0.01854% | 0.01868% |

#### 20kHz

| Item | Ver7 | Ver8 |
|------|------:|------:|
| THD | 0.01585% | 0.01611% |
| THD+N | 0.02701% | 0.02712% |

#### IMD (19kHz + 20kHz)

| Item | Ver7 | Ver8 |
|------|------:|------:|
| THD | 0.01040% | 0.01046% |
| THD+N | 0.04454% | 0.04489% |

IMD difference-frequency component (1kHz):

- Ver7: -121.09dB
- Ver8: -121.96dB

### Summary of Measurement Results

- THD and THD+N at 100Hz, 10kHz, and 20kHz were nearly the same for Ver7 and Ver8.
- At 1kHz, THD decreased from 0.00141% to 0.00104%, and THD+N also improved slightly.
- In the IMD measurement, THD and THD+N were nearly the same, while the 1kHz difference-frequency component improved by approximately 0.9dB.
- The no-signal spectrum was at a level comparable to Ver7; the unwanted component near 1kHz measured -114.70dB for Ver7 and -114.89dB for Ver8.

### Listening Test

Compared with Ver7:

- No clear difference in sound quality was identified.
- No unusual changes were identified after adding diagnostic and event-log features.
- Evaluation will continue.

### Notes

- The measurement records are averages of 300 measurements each for Ver7 and Ver8.
- These measurements were performed at 24-bit/96kHz using a modified SB-1240 loopback setup; results may vary depending on the playback environment and measurement system.
- Since multiple changes were made simultaneously in Ver8, the causal relationship between any specific change and the measurement results, as well as each change's individual contribution, was not evaluated.
- The FFT measurements produced results at a level comparable to Ver7.

*The records contain one measurement series each for Ver7 and Ver8; variation across independent repeat measurements is not documented.*

### Format Switching During YouTube Playback (November 1, 2026)

With YouTube playing on Windows 11 26H2 and Google Chrome 156.0.8078.80 (official 64-bit build), the supported output formats were selected and checked on both UAC1 and UAC2 firmware builds. The checked combinations were 16/24-bit at 44.1/48/88.2/96kHz in UAC1, and 16/24/32-bit at those rates in UAC2. Checks covered both stable and unsettled buffer-water-level conditions and lasted approximately 30 seconds; exact per-check durations were not recorded.

Note: During these checks, O (ring-buffer overflow discards), F (USB receive FIFO resynchronization discards), and U (playback output stops) all remained at zero. These counters describe how the device handled received data and do not detect losses on the host or USB bus. This is an observation on the stated setup, not a guarantee for other environments or playback conditions.

---

## Hardware Photo

### Complete System

#### Boot Screen

![Boot Screen](images/system_photo_01.png)

#### Playback Screen

![Playback Screen](images/system_photo_02.png)

#### Diagnostic Screen

![Diagnostic Screen](images/system_photo_03.png)

Pico2 + Pico Audio Pack (PCM5100A) + Pico Display Pack 2.0

---

## Display Specification (Pico Display Pack 2.0)

At startup, the boot screen is displayed. After initialization completes, the playback screen is shown.

The X button on Pico Display Pack 2.0 cycles through the following screens in one direction:

- Boot Screen
- Playback Screen
- Diagnostic Screen

- X : Next screen (Boot → Playback → Diagnostics → Boot)

On the diagnostic screen:

- A : Scroll older USB log entries
- B : Scroll newer USB log entries
- A+B : Toggle between USB events and recent audio-state history
- Hold Y for 3 seconds : Reset diagnostic counters and the USB event log (a countdown is shown while holding)

USB event logging starts at boot, and the latest 64 events remain available on the Diagnostics screen. It records events such as USB bus resets/disconnects, device/configuration/string descriptor requests, configuration mount/unmount, and UAC control requests. This is event-driven logging, not a periodic playback-state log, so the display does not change while playback continues without a new event. Track changes and other USB audio state transitions may add new entries. Holding Y for 3 seconds clears the collected diagnostic values and USB event log; logging resumes with subsequent events. The O/F/S/C/U event counters are updated when events occur regardless of the active screen. Periodic diagnostic statistics, such as buffer levels and processing intervals, are collected only while the Diagnostics screen is active. Collected diagnostic values are retained when leaving the screen, and collection resumes from those values when it is reopened. The playback screen continues audio playback, FFT, and bit-depth/sample-rate display.

Audio-state history is recorded about every 100 ms during playback. When an anomaly is detected, the approximately 4.8 seconds before and 1.6 seconds after it are retained as a frozen history for review. Detected anomalies are added to the USB log as red `E` entries: `01` buffer underflow, `02` overflow/discard, `03` USB receive FIFO resynchronization, `04` USB bus reset/disconnect, `05` playback output stop, `06` Core0 loop interval overrun, and `07` Core1 loop interval overrun. Each history entry shows uptime seconds, audio state, ring-buffer level, and DMA-pool level.

### Playback Screen

The playback screen displays the following real-time information.

#### L/R Level Meter

Displays stereo audio levels in real time.

#### Independent L/R 11-Band FFT Display

Displays left and right channel frequency spectra independently with A-weighting applied.

#### Audio Information

Displays current:

- Bit Depth
- Sample Rate (Fs)

#### Display Notes

Note: The peak indicator of the L/R level meter updates continuously according to input signal level and may appear to flicker. This is normal behavior.

---

### Diagnostic Screen

Press the X button on the playback screen to switch to the diagnostic screen.

The X button cycles through Boot → Playback → Diagnostics → Boot. Y does not navigate between screens; holding it for 3 seconds on the Diagnostics screen resets the diagnostic counters and USB event log.

#### Audio Status Information

The upper section displays current USB audio and buffer status.

##### STATE

| State | Description |
|---------|---------|
| IDLE | Stopped |
| BUFF | Buffering |
| STRM | Streaming |
| SWCH | Sample-Rate Switching |
| OVF! | Buffer Overflow |
| UDF! | Buffer Underflow |

##### HOST / FB

- HOST: Host-requested sample rate
- FB: Feedback frequency reported to host

##### Bit Depth / Sample Rate

Displayed in the format:

```text
24bit 96.0kHz
```

##### RING / LEVEL

Displays:

- Current ring-buffer fill level
- Percentage of total capacity

The white marker indicates the target buffer level.

##### MIN / MAX

Minimum and maximum buffer levels recorded during playback.

Unit:

```text
Sample Frames
```

---

### Diagnostic Counters

The right side of the diagnostic screen displays processing statistics.

| Counter | Description |
|---------|---------|
| O | Frames discarded due to ring-buffer overflow |
| F | Frames discarded during USB receive FIFO resynchronization |
| S | Frames discarded during sample-rate switching |
| C | Frames cleared during restart or stop processing |
| U | Output stoppage count detected during playback |
| a | Maximum Core0 processing interval (µs) |
| b | Maximum Core1 processing interval (µs) |
| T | DMA tail-block output count |

O/F/S/C/U are incremented when their events occur, regardless of the active screen, and show counts accumulated since reset. T counts DMA tail-block output conditions, but is incremented only while diagnostics are enabled and audio is streaming. Periodic measurements such as a/b and buffer levels are also collected only while the Diagnostics screen is active.

For stable operation:

```text
O = 0
F = 0
U = 0
```

is generally expected.

---

### USB Event Log

The bottom area displays USB communication events.

Features:

- 3 columns × 8 rows
- Up to 64 events stored
- Newest event at upper-left
- Oldest event at lower-right

Navigation:

- A button → Older events
- B button → Newer events

Displayed timestamps indicate seconds after boot.

The USB log is event-driven, not periodic. Holding Y for 3 seconds clears the diagnostic values and event log; subsequent events are recorded normally.

#### Event Symbols

| Symbol | Meaning |
|---------|---------|
| G | USB Control Request Get |
| S | USB Control Request Set |
| I | Audio Interface Start |
| C | Audio Interface Stop |
| U | USB Disconnect |
| W | Sample-Rate Switch Begin |
| D | Buffer Drain Complete |
| K | Clock Switch Complete |

Result suffix:

Success

```text
+
```

Failure

```text
-
```

---

### Information to Capture When Playback Stops

If playback stops or audio dropouts occur, capture the diagnostic screen immediately if possible.

The USB event log stores up to the most recent 64 events.

Please record:

- STATE
- HOST
- FB
- RING / LEVEL
- MIN / MAX
- O/F/S/C/U counters
- a/b/T counters
- Entire USB event log
- Current sample rate
- Current bit depth
- PC operating system
- Playback software
- Timestamp of occurrence
- Actions immediately before failure
- Whether playback resumed automatically
- Whether USB reconnection was required

These records help correlate:

- USB communication status
- Buffer behavior
- Audio output activity

and assist troubleshooting.

The recorded information may also be used when consulting AI services regarding fault analysis.

Note: Do not share personal information such as serial numbers.

Diagnostic information is not overlaid on the playback screen.

---

## Supported Operating Systems

- Windows 11 (verified on Windows 11 26H2)
- Android 16 (verified on a Sony XQ-ES44 smartphone with the paid version of Onkyo HF Player v2.15.2 and Foobar2000 for Android using the OS standard audio driver)

Onkyo HF Player supports UAC1 mode only. This project has been verified with HF Player in UAC1 mode on the Android device listed above.

---

## USB Audio Class Selection

UAC1 and UAC2 can be selected at build time using:

```cmake
USB_AUDIO_CLASS_VERSION
```

| Value | Mode |
|---------|---------|
| 1 | UAC1 |
| 2 | UAC2 |
| Not specified | UAC2 |

If you don't have a build environment, download the UAC1 or UAC2 firmware from the release notes, flash it, and use it.

### Build Commands (Verified: 2026-11-01)

Build from the repository root directory.

#### UAC1 Build

```powershell
cd <repository_directory>\Pico2_UAC12_USBDDC

cmake -S . -B build -DUSB_AUDIO_CLASS_VERSION=1

cmake --build build
```

#### UAC2 Build

```powershell
cd <repository_directory>\Pico2_UAC12_USBDDC

cmake -S . -B build -DUSB_AUDIO_CLASS_VERSION=2

cmake --build build
```

After changing the setting, re-run CMake configuration and rebuild the project.

When using VS Code CMake Tools, change the CMake cache value:

```text
USB_AUDIO_CLASS_VERSION
```

to either:

```text
1
```

or

```text
2
```

and then reconfigure the project.

If the cache value is not changed, the previous configuration remains active.

---

### Building from Windows PowerShell

If PowerShell reports:

```text
'cmake' is not recognized as an internal or external command
```

verify whether CMake is available:

```powershell
where.exe cmake
```

If no result is returned, install CMake:

```powershell
winget install Kitware.CMake
```

Restart PowerShell after installation.

If CMake is already installed but not found, add the installation directory to PATH:

```powershell
$env:Path += ";C:\Program Files\CMake\bin"
```

To make the change permanent, add the CMake installation directory to the Windows user PATH environment variable.

Verify installation:

```powershell
cmake --version
```

If a version number is displayed, configure and build the project:

```powershell
cd C:\path\to\Pico2_UAC12_USBDDC

cmake -S . -B build -DUSB_AUDIO_CLASS_VERSION=1

cmake --build build
```

For UAC2 builds, change:

```text
USB_AUDIO_CLASS_VERSION=1
```

to

```text
USB_AUDIO_CLASS_VERSION=2
```

If SDK, compiler, or Ninja-related errors occur, install the Raspberry Pi Pico extension for Visual Studio Code and use:

- Pico SDK v2.3.1
- Toolchain 15_2_Rel1

---

### Generated Firmware Files

Successful builds generate the following UF2 files:

#### UAC1

```text
build/src/Pico2_UAC12_USBDDC_11FFT_NoOverSampling_UAC1.uf2
```

#### UAC2

```text
build/src/Pico2_UAC12_USBDDC_11FFT_NoOverSampling_UAC2.uf2
```

To program the device:

1. Hold the BOOTSEL button.
2. Connect Pico 2 to the PC.
3. The RP2350 drive appears.
4. Copy the generated UF2 file onto the drive.

---

### Mode Specifications

#### UAC1 (`USB_AUDIO_CLASS_VERSION=1`)

- USB Audio Class 1.0
- PCM 16bit / 24bit
- 44.1kHz
- 48kHz
- 88.2kHz
- 96kHz

#### UAC2 (`USB_AUDIO_CLASS_VERSION=2`)

- USB Audio Class 2.0
- PCM 16bit / 24bit / 32bit
- Sample-rate limits depend on Full-Speed USB bandwidth

UAC-specific descriptors and control handlers are switched according to selected mode.

The following modules are shared:

- Audio Engine
- Ring Buffer
- Async Feedback
- FFT Processing
- Display System
- I²S Output

---

## Supported DAC

- Pico Audio Pack (PCM5100A)

Verified using:

- PICO_AUDIO_PACK

Note: I also own a PCM5102A-based DAC board, but it has not been tested because it is outside the scope of this project. I can check it if requested.

---

## USB Input Specifications

- USB Audio Class 1.0 or 2.0
- Async Feedback Endpoint
- Stereo PCM

### UAC1

| Bit Depth | Sample Rate |
|------------|------------|
| 16bit | 44.1 / 48 / 88.2 / 96kHz |
| 24bit | 44.1 / 48 / 88.2 / 96kHz |

### UAC2

| Bit Depth | Sample Rate |
|------------|-------------|
| 16bit | 44.1 / 48 / 88.2 / 96 / 176.4 / 192kHz |
| 24bit | 44.1 / 48 / 88.2 / 96kHz |
| 32bit | 44.1 / 48 / 88.2 / 96kHz |

### Notes

Note: The RP2350 USB Device Controller supports:

```text
Full-Speed USB only
```

Current Full-Speed limitations:

| Format | Maximum Rate |
|----------|----------|
| UAC2 16bit | 192kHz |
| UAC2 24bit | 96kHz |
| UAC2 32bit | 96kHz |

Note: 24bit/32bit 176.4kHz and 192kHz exceed the Full-Speed USB packet-size limit and are therefore not supported.

---

## I²S Output Specifications

- I²S
- Stereo
- 32bit PCM Format
- Input Sample Rate Follow Mode
- No Sample Rate Conversion
- No Oversampling
- I²S output without software oversampling, FIR filtering, or sample-rate conversion

### Notes

Note: The firmware applies a fixed output gain ratio of 0.80, and also applies UAC1/UAC2 volume settings (0 to -64 dB) and mute received from the host through software gain. The host volume setting therefore affects the output level.

---

## Playback Notes

- Note: Audio dropouts may occur depending on Windows audio settings
- Note: Stability depends on playback software, host performance, and system load
- Note: Stability may vary depending on the sample-rate combination

If dropouts occur:

- Switch between ASIO and WASAPI
- Reduce playback sample rate
- Reduce Windows output sample rate settings

---

## FFT Spectrum Display & Buffer Monitor

### Real-Time 11-Band FFT Spectrum Analyzer

Display Device:

- Pico Display Pack 2.0

Frequency Bands:

- 45Hz
- 90Hz
- 125Hz
- 360Hz
- 700Hz
- 1.4kHz
- 2.8kHz
- 5.6kHz
- 9.8kHz
- 18kHz
- 22kHz

### FFT Configuration

- FFT_LEN = 2048
- Window Function: Hann Window
- Weighting: A-Weighting
- RingBuffer Water-Level Monitoring
- Feedback-Control Visualization

The display system provides a real-time FFT spectrum visualization and buffer-status indication optimized for RP2350 performance and low display latency.

---

## Architecture

### Hardware

- Raspberry Pi Pico 2 (RP2350)

### Development Environment

**Build Verified**

- Raspberry Pi Pico SDK v2.3.1
- Toolchain 15_2_Rel1
- Visual Studio Code

### Display and Analysis Hardware

- Pico Display Pack 2.0
- Connected through a daughterboard

### Data Path

- TinyUSB UAC1/UAC2 class layer
- Shared audio-processing core
- DMA + PIO low-latency I²S output

### Multi-Core Processing

#### Core0

- LCD rendering
  - FFT visualization
  - Buffer water-level display
- Clock switching control
  - 44.1kHz family: 336MHz
  - 48kHz family: 360MHz
- Software audio mute (output gain control)
- Timer management

#### Core1

- I²S output processing
- DMA transfer handling
- FFT processing
  - Hann Window
  - A-Weighting calculation

### USB Control

- TinyUSB UAC1/UAC2 selectable layer
- Shared Async Feedback clock synchronization control

### Timing Control

- Repeating Timer based periodic scheduling

### RAM Execution

The firmware is copied to RAM and executed from RAM during startup.

Flash clock divider setting:

```text
Divider = 3
```

Observed values during operation:

| Sample Rate Family | Flash Clock |
|-------------------|------------|
| 44.1 / 88.2 / 176.4kHz | 112MHz |
| 48 / 96 / 192kHz | 120MHz |

---

## Pin Assignment

### I²S Interface

| Function | GPIO |
|----------|------|
| I²S DATA | GP9 |
| I²S BCLK | GP10 |
| I²S LRCK | GP11 |

### Pico Display Pack 2.0

| Function | GPIO |
|----------|------|
| SPI / Control Interface | GP16 - GP20 |

### Navigation Buttons

| Button | GPIO | Usage |
|----------|------|------|
| X | GP14 | Next Screen |
| Y | GP15 | Hold for 3 seconds on the diagnostic screen to reset diagnostics |

Internal pull-up enabled.

### Diagnostic Log Buttons

| Button | GPIO | Usage |
|----------|------|------|
| A | GP12 | Older Log Entries |
| B | GP13 | Newer Log Entries |

Internal pull-up enabled.

### RGB LED

The Pico Display Pack RGB LED is intentionally kept off; its active-low channels on GP6-GP8 are driven high. The current firmware does not use I²C output or a DAC ENABLE signal on GP5.

---

## Build and Usage

1. Connect all hardware through the daughterboard.
2. Install the Raspberry Pi Pico Extension for Visual Studio Code.
3. Clone this repository.
4. Build the project in Visual Studio Code.
5. Hold the Pico 2 BOOTSEL button while connecting to the PC.
6. The RP2350 mass-storage device appears.
7. Copy the generated UF2 file to the device.
8. After reboot, the system is recognized as the selected USB Audio Class device (UAC1 or UAC2).

Recommended Environment:

- Pico SDK v2.3.1
- Toolchain 15_2_Rel1

---

## Development Background

The project was redesigned to simplify the audio-processing path and reduce unnecessary software processing.

Changes include:

- Removal of software oversampling
- Removal of software FIR filtering
- Simplification of signal and processing paths
- Migration from LUFA to TinyUSB
- Shared UAC1/UAC2 architecture
- Enhanced diagnostic capabilities
- Maintenance of FFT visualization and clipping protection

The PCM5100A already contains internal oversampling and digital-filter processing.

For this reason, the firmware avoids software oversampling, FIR filtering, and sample-rate conversion. However, software gain control is applied for volume and mute, so bit-perfect playback is not guaranteed or claimed.

The RP2350 is operated in an overclocked configuration.

The verified test system was equipped with a heatsink during evaluation.

## Tested Environment

### Hardware

- Raspberry Pi Pico 2 (RP2350)
- Pico Audio Pack (PCM5100A)
- Pico Display Pack 2.0
- Sony XQ-ES44 smartphone

### Software

**Windows**

- Windows 11 26H2
- Foobar2000 v2.26.1 (x64)
- ASIO4ALL 2.22

**Android**

- Android 16
- Onkyo HF Player v2.15.2
- Foobar2000 for Android v2.26.1 (ARM, OS standard audio driver)

---

## License

MIT License

Copyright (c) 2026 Toshimiyu

---

## References

- USB Audio Class 1.0 Specification
- USB Audio Class 2.0 Specification
- TinyUSB Project
- Raspberry Pi Pico SDK v2.3.1
- RP2350 Documentation
- PCM5100A Datasheet
- PCM5102A Datasheet
- With gratitude to ArqAlice (Pico2UltraHiResUSBDDC), whose work served as a reference
