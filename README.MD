# Pico2 UAC12 USB DDC (TinyUSB UAC1/UAC2 Selectable)

## Description

This project is a major update of `Pico2_UAC2_USBDDC_VerFinal_11FFT_NoOverSampling`.

The USB audio subsystem has been redesigned around TinyUSB, allowing USB Audio Class 1.0 and 2.0 selection at build time while sharing a common audio-processing core.

### What's New

- UAC1 / UAC2 selectable build configuration
- TinyUSB-based USB audio layer
- Common Audio State / RingBuffer / Feedback architecture
- Improved diagnostic screen with buffer monitoring
- USB event logging (up to 24 events)
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
- Keep the USB input stream as close as possible to the original PCM data
- Improve maintainability by separating USB Audio Class specific code
- Share a common audio engine across UAC1 and UAC2
- Provide real-time diagnostics for troubleshooting

Software oversampling and FIR filtering were removed because PCM5100A / PCM5102A already implement internal oversampling and digital filtering.

As a result, the firmware outputs incoming PCM data directly through the I²S interface without additional DSP processing.

---

## Change History

### Major Changes from Pico2_UAC2_USBDDC_VerFinal_11FFT_NoOverSampling

- Added UAC1/UAC2 build-time selection using `USB_AUDIO_CLASS_VERSION`
- Migrated USB audio layer to TinyUSB
- Separated UAC1/UAC2 class-specific implementation
- Shared common Audio State, RingBuffer and Feedback architecture
- Added diagnostic screen with buffer monitor
- Added USB event log viewer (24 events)
- Improved sample-rate switching sequence
- Enhanced stream restart handling
- Removed unused GPIO debug functionality
- Fixed operation in high-power clock mode
- Maintained RAM execution architecture

---

## 概要

- 本プロジェクトは、RP2350（Raspberry Pi Pico 2）上で動作するUSB-DDCです。
- TinyUSBのUSB Audio Class 1.0 / 2.0をビルド時に選択できます。
- UAC1/UAC2で共通のAudio State、Ring Buffer、非同期Feedback制御、PCM処理、FFT/表示、I2S/DAC制御を使用します。
- クラス差分は各USB Audio層に分離しています。
- USB経由で入力された2ch PCMオーディオ信号をオーバーサンプリングやFIRフィルタ処理を行わず、32bit I²S PCMとして出力します。
- ハン窓およびA特性補正を適用した11バンドFFT表示を搭載しています。
- バッファー水位計を搭載しています。
- 音声クリッピング対策を実装しています。

---

## 特長

- USB Audio Class 1.0 / 2.0 切替対応
- Async Feedback Endpointによる非同期フィードバック制御
- リアルタイムバッファー水位計
- A特性補正付き11バンドFFT表示
- No Oversampling (NOS)
- No FIR Filtering
- Direct PCM Output
- DMA + PIOによる低遅延I²S転送
- Dual Core RP2350対応
- PICO_AUDIO_PACK対応
- Pico Display Pack 2.0対応

---

## Measurement Results

### Firmware Comparison

Ver6 → Ver7

Hardware configuration, measurement environment, and test conditions were identical.

Only the firmware was changed.

### Ver7 Changes

- Oversampling-related code removed
- FIR-related code removed
- ES-series DAC support code removed
- I2C output code removed
- Audio processing path simplified

### FFT Measurements

#### 100Hz

| Item | Ver6 | Ver7 |
|------|------|------|
| THD | 0.00104% | 0.00030% |
| THD+N | 0.00743% | 0.00563% |

#### 1kHz

| Item | Ver6 | Ver7 |
|------|------|------|
| THD | 0.00231% | 0.00141% |
| THD+N | 0.00696% | 0.00508% |

#### 10kHz

| Item | Ver6 | Ver7 |
|------|------|------|
| THD | 0.00996% | 0.00988% |
| THD+N | 0.01889% | 0.01854% |

#### 20kHz

| Item | Ver6 | Ver7 |
|------|------|------|
| THD | 0.00971% | 0.01585% |
| THD+N | 0.01650% | 0.02701% |

#### IMD (19kHz + 20kHz)

| Item | Ver6 | Ver7 |
|------|------|------|
| THD | 0.01086% | 0.01040% |
| THD+N | 0.04472% | 0.04454% |

IMD Difference Frequency Component (1kHz)

-118.71dB → -121.09dB

### Listening Test

Compared with Ver6:

- Reduced sense of congestion
- More open and natural sound character

### Notes

The following factors were not evaluated independently:

- Signal-path shortening
- Reduction of branch conditions
- Difference between LUFA and TinyUSB implementations

Therefore, it cannot be concluded that:

- Signal-path shortening directly improved measurements
- Measurement improvements directly caused listening improvements

However, hardware configuration, measurement environment, and test conditions remained unchanged.

The observed differences were therefore caused by firmware changes.

Which internal firmware changes contributed to the results has not yet been isolated.

Although 20kHz THD and THD+N increased compared to Ver6, no audible issues were identified during listening tests.

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

The X and Y buttons on Pico Display Pack 2.0 can be used to switch between:

- Boot Screen
- Playback Screen
- Diagnostic Screen

Button assignment:

- X : Previous screen
- Y : Next screen

On the diagnostic screen:

- A : Scroll older USB log entries
- B : Scroll newer USB log entries

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

The peak indicator of the L/R level meter updates continuously according to input signal level and may appear to flicker. This is normal behavior.

---

### Diagnostic Screen

Press the X button on the playback screen to switch to the diagnostic screen.

X/Y buttons can be used to switch to adjacent screens.

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
- Up to 24 events stored
- Newest event at upper-left
- Oldest event at lower-right

Navigation:

- A button → Older events
- B button → Newer events

Displayed timestamps indicate seconds after boot.

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

```text
+
```

Success

```text
-
```

Failure

---

### Information to Capture When Playback Stops

If playback stops or audio dropouts occur, capture the diagnostic screen immediately if possible.

The USB event log stores only the most recent 24 events.

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

Do not share personal information such as serial numbers.

Diagnostic information is not overlaid on the playback screen.

---

## Supported Operating Systems

- Windows 11

Verified on:

```text
Windows 11 26H2
```

UAC1 mode is intended primarily for compatibility with:

- Onkyo HF Player (Android)

Recognition and playback compatibility should be confirmed on actual Android devices.

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

- TI PCM5100
- TI PCM5102A

Verified using:

- PICO_AUDIO_PACK

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

#### 16bit

- 44.1kHz
- 48kHz
- 88.2kHz
- 96kHz
- 176.4kHz
- 192kHz

#### 24bit

- 44.1kHz
- 48kHz
- 88.2kHz
- 96kHz

#### 32bit

- 44.1kHz
- 48kHz
- 88.2kHz
- 96kHz

### Notes

RP2350 USB Device Controller supports:

```text
Full-Speed USB only
```

Current Full-Speed limitations:

| Format | Maximum Rate |
|----------|----------|
| UAC2 16bit | 192kHz |
| UAC2 24bit | 96kHz |
| UAC2 32bit | 96kHz |

24bit/32bit 176.4kHz and 192kHz exceed the Full-Speed USB packet-size limit and are therefore not supported.

Compatibility with Onkyo HF Player should be verified on actual Android hardware.

---

## I²S Output Specifications

- I²S
- Stereo
- 32bit PCM Format
- Input Sample Rate Follow Mode
- No Sample Rate Conversion
- No Oversampling
- Direct PCM Output

### Notes

Some playback environments may apply digital volume control.

Configure volume handling appropriately for your playback chain.

---

## Playback Notes

- Audio dropouts may occur depending on Windows audio settings.
- Stability depends on playback software, host performance, and system load.
- Certain sample-rate combinations may be more stable than others.

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
- DAC mute control
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
| 44.1 / 88.2kHz | 112MHz |
| 48 / 96kHz | 120MHz |

---

## Pin Assignment

### I²S Interface

| Function | GPIO |
|----------|------|
| I²S DATA | GP9 |
| I²S BCLK | GP10 |
| I²S LRCK | GP11 |

### I²C Interface

| Function | GPIO |
|----------|------|
| SDA | GP6 |
| SCL | GP7 |

### DAC Control

| Function | GPIO |
|----------|------|
| DAC ENABLE | GP5 |

### Pico Display Pack 2.0

| Function | GPIO |
|----------|------|
| SPI / Control Interface | GP16 - GP20 |

### Navigation Buttons

| Button | GPIO | Usage |
|----------|------|------|
| X | GP14 | Previous Screen |
| Y | GP15 | Next Screen |

Internal pull-up enabled.

### Diagnostic Log Buttons

| Button | GPIO | Usage |
|----------|------|------|
| A | GP12 | Older Log Entries |
| B | GP13 | Newer Log Entries |

Internal pull-up enabled.

### RGB LED

The Pico Display Pack RGB LED is intentionally disabled to avoid GPIO resource conflicts.

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

PCM5100A and PCM5102A already contain internal oversampling and digital-filter processing.

For this reason, the firmware was redesigned to output incoming PCM data through I²S with minimal modification.

The RP2350 is operated in an overclocked configuration to improve processing performance.

The RP2350 is operated in an overclocked configuration.

The verified test system was equipped with a heatsink during evaluation.

## Tested Environment

### Hardware

- Raspberry Pi Pico 2 (RP2350)
- Pico Audio Pack (PCM5100A / PCM5102A)
- Pico Display Pack 2.0

### Software

- Windows 11 26H2
- Foobar2000
- ASIO4ALL 2.22

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
