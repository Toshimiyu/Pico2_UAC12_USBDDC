# Build Configuration & RAM Analysis - Verified Facts

**Last Updated**: 2026-11-25 22:24 JST  
**Analysis Basis**: Clean builds (Levels 2, 3, 4) + source code inspection

---

## 1. Build Results Summary (All Levels Confirmed)

### RAM Configuration Base
- **Total RAM**: 458,752 B (448 KiB)
- **ARM NONE EABI Size Output**: BSD format (text, data, bss, dec, hex)

| Level | Label | Features | Text | BSS | RAM % | Free | Build Time |
|-------|-------|----------|------|-----|-------|------|-----------|
| **2** | build-normal | FFT+Display | 194,620 B | 196,940 B | 42.9% | 261,812 B | 10.33 s |
| **3** | build-cdc | CDC Diagnostic | 121,864 B | 113,328 B | 24.7% | 345,424 B | 10.35 s |
| **4** | build-level4 | CDC+FFT+Display | 211,608 B | 242,268 B | 52.8% | 216,484 B | 10.4 s |

---

## 2. EVT_LOG_N Configuration & Impact

### Source Definition
**File**: `src/usb_device_control.c` (lines 83-87)

```c
#if USB_CDC_ENABLED
#define EVT_LOG_N 128  // Reduced from 256 for memory efficiency (RAM optimization P1)
#else
#define EVT_LOG_N 64
#endif
```

### Memory Impact
- **Array Structure**: `static char evt_log[EVT_LOG_N][8]` (1 element = 8 B)  
  `static uint32_t evt_time_us[EVT_LOG_N]` (1 element = 4 B)

| Config | EVT_LOG_N | evt_log Size | evt_time_us Size | Total |
|--------|-----------|--------------|------------------|-------|
| Level 2 (CDC OFF) | 64 | 512 B | 256 B | 768 B |
| Level 3 (CDC ON) | 128 | 1,024 B | 512 B | 1,536 B |
| Level 4 (CDC ON) | 128 | 1,024 B | 512 B | 1,536 B |

### Level 3 BSS Variance
- **Previous Measurement** (EVT_LOG_N=256): 114,864 B  
- **Current Measurement** (EVT_LOG_N=128): 113,328 B  
- **Delta**: -1,536 B (matches expected array reduction: 256→128)

---

## 3. Conditional Feature Compilation

### Feature Flags (usb_audio_config.h)
```c
USB_CDC_ENABLED:  (USB_BRINGUP_LEVEL >= 3)   // Level 3, 4 → ON  |  Level 2 → OFF
USB_FFT_ENABLED:  (USB_BRINGUP_LEVEL != 3)   // Level 2, 4 → ON  |  Level 3 → OFF
```

### Impact Verification
✅ **Level 2**: CDC OFF, FFT ON → Text 194,620 B (smallest)  
✅ **Level 3**: CDC ON, FFT OFF → Text 121,864 B (CDC overhead ~73 KB)  
✅ **Level 4**: CDC ON, FFT ON → Text 211,608 B (combined features)

---

## 4. Memory Headroom Analysis

### Available Free Space
| Level | Usage | Free | Safety Margin |
|-------|-------|------|---------------|
| Level 2 | 42.9% | 261,812 B | 57.1% (safe for FFT+Display) |
| Level 3 | 24.7% | 345,424 B | 75.3% (safe for CDC diagnostics) |
| Level 4 | 52.8% | 216,484 B | 47.2% (operational, monitor heap) |

**Level 4 Consideration**: At 52.8% RAM usage with only 216 KB free (~0.47%), stack/heap contention must be monitored during CDC logging operations.

---

## 5. Build Configuration Verification

### CMake Parameters Applied
```bash
# Level 2
cmake -S . -B build-normal -G Ninja \
  -DUSB_AUDIO_CLASS_VERSION=2 \
  -DUSB_BRINGUP_LEVEL=2

# Level 3
cmake -S . -B build-cdc -G Ninja \
  -DUSB_AUDIO_CLASS_VERSION=2 \
  -DUSB_BRINGUP_LEVEL=3

# Level 4
cmake -S . -B build-level4 -G Ninja \
  -DUSB_AUDIO_CLASS_VERSION=2 \
  -DUSB_BRINGUP_LEVEL=4
```

### Build Consistency
- **Compilation Time**: ~10.3-10.4 s (ninja parallel, 3-116 targets)
- **Linker Output**: All Level builds use `Ninja` / `Linking CXX executable`
- **ELF Generation**: All successful with no errors/warnings

---

## 6. Next Verification Steps

### Outstanding Items
- [ ] Real-time CDC capture validation (`capture_cdc_auto.ps1` / `capture_cdc.ps1`)
- [ ] Level 3・4 firmware behavior on actual hardware
- [ ] Heap usage monitoring under CDC logging load

### Documentation Accuracy
- ✅ Level definitions (CDC/FFT feature matrix) confirmed
- ✅ RAM calculations verified against source arrays
- ⚠ "2,077 lines duplicate consolidation" should be revised to actual measured value

---

**Prepared by**: Copilot Code Analysis  
**Verification Method**: arm-none-eabi-size + CMakeCache.txt + source inspection
