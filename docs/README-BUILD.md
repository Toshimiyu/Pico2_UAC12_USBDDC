# Build Guide - USB_BRINGUP_LEVEL Configuration

This guide explains how to build Pico2 UAC12 USBDDC with different USB startup modes and analyze RAM usage.

## Overview

The firmware supports four USB startup modes controlled by `USB_BRINGUP_LEVEL`:

| Level | Mode | CDC | FFT | Display | Purpose | RAM Usage |
|-------|------|-----|-----|---------|---------|-----------|
| 2 | Audio Only | ✗ | ✓ | ✓ | Standard audio playback with spectrum display | ~43% |
| 3 | CDC Diagnostic | ✓ | ✗ | ✗ | USB diagnostics logging, no audio processing | ~25% |
| 4 | Max Load | ✓ | ✓ | ✓ | Diagnostic with full audio + FFT (high CPU load) | ~53% |

**Note:** Level 0 (Off), Level 1 (Enumeration) are diagnostic modes not covered here.

---

## Configuration Source

Build configuration is defined in:
- `CMakeLists.txt` - Default level (2) and build system integration
- `src/usb_audio_config.h` - Feature selection based on level:
  ```c
  #define USB_CDC_ENABLED (USB_BRINGUP_LEVEL >= 3)
  #define USB_FFT_ENABLED (USB_BRINGUP_LEVEL != 3)
  ```

---

## Build Process

### Prerequisites

1. **Environment Setup**
   - Raspberry Pi Pico SDK 2.3.1
   - Toolchain: arm-none-eabi (15_2_Rel1)
   - Ninja build system (v1.13.2)
   - CMake 3.13+

2. **TinyUSB**
   - Repository must include `tinyusb/` subdirectory with UAC2 support
   - Checked during CMake configuration

### Important: Clean Build Before Each Configuration

**Always delete the old build directory before building a different level:**

```powershell
# Manual clean
Remove-Item -Recurse -Force build-level4

# Then run cmake/ninja
cmake -S . -B build-level4 -G Ninja -DUSB_BRINGUP_LEVEL=4
cmake --build build-level4
```

The build scripts do this automatically with the `-NoClean` flag.

---

## Building with Scripts

### Option 1: Build All Levels at Once

```powershell
cd C:\Users\toshi\Pico2_UAC12_USBDDC
.\tools\build-all-levels.ps1
```

This builds Level 2, 3, and 4 sequentially with clean directories, logging each result to `build-logs/`.

### Option 2: Build Individual Level

```powershell
# Build Level 2 (Audio Only)
.\tools\build-level.ps1 -Level 2 -OutputDir build-normal

# Build Level 3 (CDC Diagnostic)
.\tools\build-level.ps1 -Level 3 -OutputDir build-cdc

# Build Level 4 (Max Load)
.\tools\build-level.ps1 -Level 4 -OutputDir build-level4
```

### Script Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `-Level` | 2 | USB_BRINGUP_LEVEL (0, 1, 2, 3, 4) |
| `-OutputDir` | `build-level$Level` | Build output directory |
| `-ToolchainPath` | SDK default | Path to arm-none-eabi toolchain |
| `-NinjaPath` | SDK default | Path to Ninja executable |
| `-NoClean` | (not set) | Skip removing old build dir |
| `-NoLog` | (not set) | Don't save log file |

### Manual Build (Without Scripts)

```powershell
cd C:\Users\toshi\Pico2_UAC12_USBDDC

# Remove old build
Remove-Item -Recurse -Force build-level4

# Configure
cmake -S . -B build-level4 -G Ninja -DUSB_AUDIO_CLASS_VERSION=2 -DUSB_BRINGUP_LEVEL=4

# Build
cmake --build build-level4

# Analyze size
arm-none-eabi-size build-level4\src\Pico2_UAC12_USBDDC_UAC2.elf
```

---

## Analyzing Build Results

### Quick Check: Existing Builds

```powershell
.\tools\analyze-builds.ps1
```

Output example:
```
=== Build Configuration Analysis ===

[build-normal]
  Level: 2
  Features: CDC=OFF, FFT=ON, Display=ON
  ELF: Pico2_UAC12_USBDDC_UAC2.elf
  Memory: text=211608 B, data=0 B, bss=196940 B
  RAM: 196940 B (42.9%) | Free: 261812 B

[build-cdc]
  Level: 3
  Features: CDC=ON, FFT=OFF, Display=OFF
  ELF: Pico2_UAC12_USBDDC_UAC2.elf
  Memory: text=211608 B, data=0 B, bss=114864 B
  RAM: 114864 B (25.0%) | Free: 343888 B

[build-level4]
  Level: 4
  Features: CDC=ON, FFT=ON, Display=ON
  ELF: Pico2_UAC12_USBDDC_UAC2.elf
  Memory: text=211608 B, data=0 B, bss=242268 B
  RAM: 242268 B (52.8%) | Free: 216484 B

=== Summary Table ===

Dir         | Level | CDC | FFT | Display | BSS (B)  | RAM %  | Free (B)
------------|-------|-----|-----|---------|----------|--------|----------
build-normal|   2   | OFF | ON  | ON      | 00196940 | 42.9%  | 00261812
build-cdc   |   3   | ON  | OFF | OFF     | 00114864 | 25.0%  | 00343888
build-level4|   4   | ON  | ON  | ON      | 00242268 | 52.8%  | 00216484
```

### Verify USB_BRINGUP_LEVEL

```powershell
# Check CMakeCache.txt
Select-String -Path build-level4\CMakeCache.txt -Pattern "USB_BRINGUP_LEVEL"
# Output: USB_BRINGUP_LEVEL:STRING=4
```

### Detailed Size Breakdown

```powershell
# Manual size analysis
arm-none-eabi-size -A build-level4\src\Pico2_UAC12_USBDDC_UAC2.elf
```

---

## Build Logs

Each build run generates a log file in `build-logs/`:

**Format:** `build-info-Level<N>-YYYY-MM-DD-HHmmss.txt`

**Example:**
```
build-logs/
├── build-info-Level2-2026-11-25-213000.txt
├── build-info-Level3-2026-11-25-213015.txt
└── build-info-Level4-2026-11-25-213030.txt
```

**Log Contents:**
- Timestamp and configuration
- CMake command line
- Build configuration (Level, CDC/FFT status)
- Size output (text, data, bss)
- RAM usage summary
- Build duration

**To disable logging:**
```powershell
.\tools\build-level.ps1 -Level 4 -NoLog
```

---

## RAM Layout (RP2350 SRAM: 448 KiB)

```
Total SRAM: 0x20000000 ~ 0x20070000 (458,752 B)

Level 2 (Audio Only):
  .bss: 196,940 B (42.9%)
  Free: 261,812 B (57.1%)  ✓ Plenty of headroom

Level 3 (CDC Diagnostic):
  .bss: 114,864 B (25.0%)
  Free: 343,888 B (75.0%)  ✓ Minimal audio processing

Level 4 (Max Load):
  .bss: 242,268 B (52.8%)
  Free: 216,484 B (47.2%)  ⚠ Tight, but stable
```

**Components:**
- FFT buffers: ~118 KiB (Level 2, 4 only)
- CDC logging/diagnostics: ~35 KiB (Level 3, 4 only)
- Display/UI: ~20 KiB
- Audio pipeline: ~30 KiB
- Other: ~15 KiB

---

## Troubleshooting

### "ELF file not found"
- Verify build completed successfully
- Check output directory and ELF path
- Run build command manually to see errors

### "USB_BRINGUP_LEVEL not found in CMakeCache.txt"
- CMake configuration may have failed
- Delete build directory and retry
- Check CMake error messages

### RAM Usage Mismatch
- Verify `-DUSB_BRINGUP_LEVEL` matches intended level
- Check CMakeCache.txt: `USB_BRINGUP_LEVEL:STRING=X`
- Confirm compiler optimization flags (`-O3`, etc.) are applied

### Large BSS Variance Between Builds

**Expected ranges:**
- Level 2: 190-210 KiB (FFT processing)
- Level 3: 110-120 KiB (CDC logging minimal)
- Level 4: 240-250 KiB (Full features)

Large deviations may indicate:
- Different compiler version
- Modified source code
- Optimization flag changes

---

## References

- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf) - Memory architecture
- [Pico SDK Documentation](https://raspberrypi.com/documentation/microcontrollers/c_sdk.html)
- [TinyUSB UAC2 Audio Class](https://github.com/hathach/tinyusb/tree/master/src/class/audio)

---

## Revision History

| Date | Level | Changes |
|------|-------|---------|
| 2026-11-25 | Initial | Build script infrastructure, Level 2/3/4 configuration |

