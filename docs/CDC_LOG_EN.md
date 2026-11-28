# USB CDC Diagnostic Log

[English](CDC_LOG_EN.md) | [日本語](CDC_LOG.md)

## CDC Diagnostic Log

At `USB_BRINGUP_LEVEL=3`, the diagnostic screen follows the boot screen; the playback screen and FFT processing are disabled. `FFT_STATIC_BYTES` and FFT timing statistics are zero. Audio playback and the Playback Start Log screen remain available.

> **✅ Hardware Test on Windows 11 26H2 (UAC2 mode)**  
> From v2.7 onwards, testing with the implemented change-detection logic confirms that no noise has been detected in Windows 11 26H2 environments running in UAC2 mode. Long-duration log capture (15.8 minutes) also confirms that log-size reduction (98.8%) and preservation of diagnostic information work together effectively.

Logging still starts automatically on COM connection. After starting capture, send `LOG_START` followed by LF or CR+LF to start a new log session and reissue the headers beginning with `#FW_VERSION`. Session time and session counters are reset, but already transmitted or queued old log data is not discarded; treat the last `#FW_VERSION` in the file as the start of the new session. Pending STATE, ANOMALY and OFFSET events are cleared when the new session starts, but boot-lifetime counters and audio operation are not reset. Earlier TRACE or USB event history may follow the headers. If the clock is already synchronized, the existing `#TIME_SYNC` is also reissued; otherwise a separate `TIME` command is required.

## v2.7: Change-Detection Logic

Starting in v2.7, CDC diagnostic logs employ **change-detection logic** to reduce logging volume while preserving diagnostic value. Log size is reduced by approximately **98.8% to 99.0%** (measured in hardware: 48.5 MB → 0.59 MB) while keeping sufficient diagnostic detail.

### Periodic Sampling

**Interval**: 100 ms (detection) + 1-second forced output

Periodic logs are recorded only when one of these conditions is met:

- Host rate (`HOST_RATE`) changes by ≥ 50 Hz
- Feedback rate (`FEEDBACK_RATE`) changes by ≥ 50 Hz
- Ring level (`RING_NOW`) changes by ≥ 200 stereo frames
- Underflow counter (`UNDERFLOW_COUNT`) increments
- USB state transitions

Additionally, one row is forced out every 1 second to maintain minimum visibility.

**Related constants:**
- `CDC_LOG_FORCE_PERIOD_US`: 1,000,000 µs (1 second)
- `CDC_SAMPLE_INTERVAL_US`: 100,000 µs (100 ms)

### FAST Sampling

**Interval**: 1 ms (detection) + 500 ms forced output

FAST logs are recorded only when one of these conditions is met:

- Ring level changes by ≥ 200 stereo frames
- 500 ms since the last FAST output

Measured output behavior: average interval ~248 ms, median ~63 ms (indicating ring-change detection is the primary trigger, contributing ~53% of rows). Forced outputs contribute ~47%.

**Related constants:**
- `CDC_FAST_FORCE_PERIOD_US`: 500,000 µs (500 ms)
- `CDC_FAST_CHANGE_THRESHOLD_RING_FRAMES`: 200 stereo frames
- `CDC_FAST_SAMPLE_INTERVAL_US`: 1,000 µs (1 ms)

### High-frequency logging and clock synchronization

Continuing below...

In CDC-enabled builds, a `FAST,UPTIME_US,UNIX_US,STATE,RING,FB_LEVEL_X10,FEEDBACK_Q16,FB_INTEG_MILLIHZ,USB_OUT_AGE_US,MISSED,DROPPED,FB_DIAG_MODE,FB_DIAG_AGE_US,FB_P_MILLIHZ,FB_HF_MILLIHZ,FB_CORR_PPM_X10,FB_LIMIT_PPM_X10,FB_OUT_SAT,FB_I_SAT` row is also attempted approximately every 1 ms; the interval is not guaranteed. `UPTIME_US` is the device uptime at sampling, and `UNIX_US` is the synchronized UTC Unix time in microseconds, or blank before synchronization. State values 0–5 mean IDLE, BUFFERING, STREAMING, OVERFLOW_FATAL, UNDERFLOW_FATAL, and SWITCHING, respectively. `RING` is in stereo frames, `FB_LEVEL_X10` is the filtered level times 10, and `FB_INTEG_MILLIHZ` is the integrator in millihertz. These values are not sampled atomically. `FEEDBACK_Q16` is 16.16 samples/ms; multiply by 1000/65536 for Hz. It is zero in IDLE and SWITCHING. `USB_OUT_AGE_US` is blank when unavailable.

`MISSED` counts sample periods missed due to scheduling delays, and `DROPPED` counts rows that could not be queued, for example when the CDC transmit FIFO is full. Both are cumulative for the log session and are visible in the next successfully queued FAST row. Unsent rows are lost on disconnect.

The added fields describe the latest feedback-control update. `FB_DIAG_MODE` is `PI`, `BUFFERING`, or `FIXED` for a fixed-feedback test. All added fields are blank before an update or immediately after a state transition. `FB_DIAG_AGE_US` is the elapsed time between the update and the FAST sample; the ring and other fields are not measured atomically with it.

`FB_P_MILLIHZ` is the level-error proportional term and `FB_HF_MILLIHZ` the host-rate-difference term. The existing `FB_INTEG_MILLIHZ` is the I term. `FB_CORR_PPM_X10` is the clipped correction divided by the nominal sample rate, in tenths of a ppm. It is applied relative to the actual I2S rate, not the nominal rate; `FEEDBACK_Q16` adds quantization. `FB_LIMIT_PPM_X10` is the actual output-correction limit in tenths of a ppm. Integer conversion truncates toward zero.

`FB_OUT_SAT` and `FB_I_SAT` are `1` at or above the positive output or integrator limit, `-1` at or below the negative limit, and `0` otherwise. The P, host-difference, PI limit and saturation fields are blank in `BUFFERING` and `FIXED`. The integrator can continue changing while output is saturated, so the two flags are distinct.

On Windows, with a CDC-enabled firmware and no other terminal using the port, run:

```powershell
.\tools\capture_cdc.ps1 -Port COM3 -Output .\capture.csv
```

To retry automatically after a disconnect on the same COM port, run instead:

```powershell
.\tools\capture_cdc_auto.ps1 -Port COM3 -OutputDir .\captures
```

Each connection attempt uses a separate CSV and never overwrites an existing file. `capture_events.log` appends PC local timestamps with time-zone offsets for `START` (capture attempt), `END` (end reason), and `PORT_BACK` (port detected after being observed absent, with wait duration). The duration is measured from the preceding capture end, or from wrapper startup on the first attempt. After an error, if the port is already present, `PORT_PRESENT` records that its absence was not observed. The wrapper waits one second after an attempt ends before checking the port and approximately 0.5 seconds after detection before retrying; errors are shown on screen and in the log. If the COM number changes, the wrapper continues waiting for the old number: stop it with Ctrl+C and restart with the new number. Port enumeration does not guarantee the physical disconnect/reconnect time or a successful reconnection. Stop with Ctrl+C.

The tool refuses to overwrite an existing file, opens the port, sends `TIME,<UTC Unix microseconds>` followed by a newline, and records the received bytes. The device associates that value with its uptime when processing the command and replies `#TIME_SYNC,<UNIX_US>,<UPTIME_US>`. The tool fails if the command is rejected or no acknowledgement arrives within 10 seconds. Stop with Ctrl+C. The 115200 port setting does not set the USB transfer speed.

The CSV is created only after the COM port opens successfully. Failed open attempts remain in the event log but do not create CSVs. A failure after opening, such as a time-synchronization failure, can leave an incomplete CSV.

The PC time is taken immediately before transmission, so synchronization includes PC, USB, and device processing delays and depends on PC clock accuracy. It is not hardware synchronization; clock drift and later PC clock changes are not automatically corrected. Existing `MS` and `TRACE` timestamps remain unchanged for compatibility; use `#TIME_SYNC` and `#TRACE_SESSION_START_US` to correlate them within the same device boot.

High-frequency output increases CPU, USB, and PC write load and can affect playback or lose records. Check `MISSED`, `DROPPED`, other loss notifications and loop timings, and compare with CDC disabled. This logging does not directly measure spinlock hold time.

The diagnostic virtual COM port is controlled by the CMake option `USB_BRINGUP_LEVEL`. Level `3` enables the CDC class, interface, endpoints, and diagnostic log output; levels `0`, `1`, and `2` disable CDC, so no virtual COM port is enumerated and no CDC communication occurs. Open the device's virtual COM port on the PC to receive CSV logs with elapsed milliseconds since the log session started. Metadata records the firmware version, prefill threshold, UAC mode, and a `#MEM` row with the `.data`-to-`.bss` static RAM span, ring allocation size, mutable FFT static data size, and LCD work-buffer size. `FFT_STATIC_BYTES` and `LCD_BUFFER_BYTES` are included in `SRAM_DATA_BSS_BYTES`, not additional amounts; LCD work buffers exclude flash-resident fonts. The ring is heap-allocated and reported separately. Free heap and stack usage are not measured.

The `#FB_CONFIG` row records the target fill, base Kp and Ki, initial transient gain scale and duration, boost start and full-boost errors, base and maximum correction limits, and whether fixed-feedback testing is enabled. Kp is in µHz/frame and Ki in µHz/(frame·second). After reset, feedback updates advance the transient period and gradually restore Kp and Ki from the initial scale to unity. Boost depends on positive filtered level error, not elapsed time. `FB_LIMIT_PPM_X10` is the actual limit after the additional Hz cap.

The four `FB_EXPERIMENTAL_*` feedback options and `USB_AUDIO_ENABLE_OFFSET_SEED` default to `ON` in a fresh configuration. Existing CMake cache values are not automatically updated: explicitly set all five to `ON` or configure a new build directory. The old `FB_EXPERIMENTAL_OFFSET_SEED` value is carried over only when the new option is unset. Set `-DUSB_AUDIO_ENABLE_OFFSET_SEED=OFF` to compare without offset reuse. Earlier comparison options remain available; dependent options must be disabled before their prerequisites. With all five `OFF`, the legacy target remains 3072 frames. The default configuration targets 50% of ring capacity (2048 frames). Lowering the target reduces underflow headroom, so validate ring levels, saturation, and silence insertion at each rate on hardware.

With `FB_EXPERIMENTAL_LEVEL_SEED`, after STREAMING begins, Core1 reports when it has queued new PCM and filled the DMA pool; the next feedback update seeds the filtered level from the ring once. Before that notification the existing filter and PI control continue, and initialization waits if the pool never fills. No integrator freeze or reset is added.

`FB_EXPERIMENTAL_ANTI_WINDUP` enables conditional integration. When the candidate output including the proportional and host-difference terms reaches a limit and the integral increment would push further in that direction, that increment is withheld. Integration in the opposite direction remains allowed.

With `FB_EXPERIMENTAL_RATE_GAINS`, base Kp is 0.47 ppm of nominal sample rate per frame and base Ki is base Kp squared divided by four. Gains follow sample-rate changes, using the existing 64 ms filtered level without another filter. The existing transient ramp scales both gains equally, so during the ramp effective Ki need not equal effective Kp squared divided by four. `#FB_CONFIG` reports base Kp and Ki for `GAIN_REFERENCE_HZ`, rounded to microhertz only for reporting; control uses the unrounded values. `GAIN_MODE=RATE_SCALED`, `KP_NPPM_PER_FRAME=470`, and `KI_FORMULA=KP_SQUARED_DIV4` specify the other rates. Hardware must validate saturation, levels and settling, particularly slow integration at low rates.

The main control parameters are:

| Parameter | Value / meaning |
|---|---|
| Kp | 0.47 ppm per frame per nominal sample rate; at 44.1 kHz, 20,727 µHz/frame |
| Ki | Base Kp squared divided by four |
| Base correction limit | 100 ppm |
| Boost | The limit increases when the filtered level is more than 128 frames below target, reaching 1000 ppm at a deficit of 512 frames |
| Offset estimation window | Ignore the first 5 seconds after STREAMING starts, then estimate in 30-second windows; the first result is available at about 35 seconds |
| Valid saved estimate | Within ±80 ppm; an out-of-range estimate also invalidates the saved value |
| Seed expiry | Checked at the next STREAMING start when at least 60 minutes have elapsed since the previous stream stopped |
| Disable only OFFSET_SEED | Set `-DUSB_AUDIO_ENABLE_OFFSET_SEED=OFF` when configuring CMake |

With `USB_AUDIO_ENABLE_OFFSET_SEED`, each 30-second window estimates the next stream's initial integral from mean limited feedback correction minus the level slope in ppm, using the difference between the two 15-second filtered-level averages. The first valid estimate is saved directly; later estimates update it with 70% old and 30% new. Estimates at least 15 ppm from the saved value are accepted only after two consecutive windows are within 5 ppm of each other. Boost, output saturation exceeding 50%, stream interruption, rate change, fault or USB suspend discard the window. A feedback update gap over 20 ms also discards it and restarts the five-second wait. USB reset/unplug invalidation is implemented, but has not been verified with a USB bus reset that leaves the Pico powered. The 60-minute expiry is also implemented but has not been verified on hardware. Storage is RAM-only and is lost on power-off or reboot; across rates, saved ppm is converted to hertz at the new rate. Estimation and settling depend on host feedback behavior and require hardware validation.

The normal build displays `v2.6`; the CDC diagnostic build uses `v2.6T` on the display and in CDC metadata. Builds with comparison options disabled may include configuration details in the version name.

At startup, I²S outputs zero data for 7 seconds and USB audio is discarded during that period. I²S clocks stop after one second without reception, once the remaining audio has faded and the DAC zero-detection interval has elapsed. After reception resumes, zero output and the zero-detection interval precede normal prefill. Startup and IDLE zero-DMA blocks are not included in the playback `SILENCE_*` log.

| Row / column | Meaning |
|---|---|
| `OFFSET_SEED,UPTIME_US,RATE,VALID,SAVED_PPM,STOP_AGE_MS` | Records the saved value at STREAMING start. `UPTIME_US` is uptime in microseconds, `RATE` is the sample rate in Hz, `VALID` indicates whether the saved value is valid (1/0), and `SAVED_PPM` is the stored correction. `STOP_AGE_MS` is milliseconds since the previous stream stopped; it is blank if no stop has been recorded. Ignore `SAVED_PPM` when `VALID=0`. |
| `OFFSET_UPDATE,UPTIME_US,RATE,ESTIMATE_PPM,CORRECTION_PPM,SLOPE_FRAMES_S,SATURATION_PCT,REASON,SAVED_VALID,SAVED_PPM` | Reports the estimation-window result. `ESTIMATE_PPM` is the estimate, `CORRECTION_PPM` is mean correction during the window, `SLOPE_FRAMES_S` is the buffer-level slope, and `SATURATION_PCT` is the fraction of the window spent saturated. `SAVED_VALID` and `SAVED_PPM` report the resulting saved state. |
| `OFFSET_DROPPED,COUNT` | Number of OFFSET events dropped from the CDC output queue. Events are not retained while CDC is disconnected. |

`REASON` describes the estimation result or window status:

| Value | Meaning |
|---|---|
| `INITIAL` | No valid saved value existed; the first estimate was saved |
| `SMOOTHED` | Saved value was updated with 70% old value and 30% new estimate |
| `BOOST` | Estimate rejected because boost occurred in the window |
| `SATURATION` | Estimate rejected because output saturation exceeded 50% of the window |
| `RANGE` | Estimate exceeded ±80 ppm and invalidated the saved value |
| `PENDING` | Estimate differed from the saved value by at least 15 ppm; awaiting confirmation by the next window |
| `GAP` | Window discarded, for example after a feedback-update gap over 20 ms; estimate-related fields are blank |

A normal row is emitted every 100 ms with columns `MS,FREQ,BIT_DEPTH,USB_MODE,HOST_RATE,FEEDBACK_RATE,HF_DIFF_NOW,RING_NOW,RING_MIN,RING_AVG,RING_MAX,MAX_HF_DIFF_SESSION,MAX_HF_DIFF_STREAM,STATE,UNDERFLOW_COUNT,USB_GAP_MAX_US,USBBUF_CAPACITY,USBBUF_FREE_NOW,USBBUF_FREE_MIN_STREAM,USBBUF_FREE_MAX_STREAM,USBBUF_FREE_MIN_PCT_STREAM,LCD_MAX_US,LCD_AVG_US,LCD_COUNT,FFT_MAX_US,FFT_AVG_US,FFT_COUNT,RATE_CHANGE_COUNT,STREAM_START_COUNT,STREAM_STOP_COUNT,LOOP0_LT1MS,LOOP0_LT2MS,LOOP0_LT5MS,LOOP0_GE5MS,LOOP1_LT1MS,LOOP1_LT2MS,LOOP1_LT5MS,LOOP1_GE5MS`. `MS` is elapsed milliseconds since the CDC log session started, not uptime; `ANOMALY` rows use the same time base. `USB_MODE` is `UAC1` or `UAC2` according to the build configuration. `HOST_RATE` is estimated from received frames over a 250 ms window. `HF_DIFF_NOW` is the signed `HOST_RATE - FEEDBACK_RATE` when both rates are valid; it is blank if either rate is zero (not yet measured or unavailable). `MAX_HF_DIFF_SESSION` is the maximum absolute difference during the CDC session. It is updated only from valid measurements while STREAMING, excluding estimation windows that overlap the first second after each STREAMING start. `MAX_HF_DIFF_STREAM` is the maximum during the current STREAMING period and follows the same warmup exclusion; it remains zero until a valid measurement is available. `RING_MIN`/`RING_MAX` are the minimum/maximum ring levels observed during each log interval; `RING_AVG` is the mean of samples taken at approximately 1 ms intervals. State transitions use `STATE,MS,FROM,TO,RATE,RING,DMA_POOL,USB_MODE` rows; anomalies use `ANOMALY,MS,CODE,DETAIL,USB_MODE,STATE,RATE,RING,DMA_POOL`, including state, sample rate, ring fill, and frames in the DMA pool. Transition events are recorded on Core0 when the state changes, giving finer timing than periodic samples. Normal rows do not include calendar time; correlate their timestamps with the clock synchronization information. Periodic diagnostic statistics are collected even when the Diagnostics screen is not open while CDC is connected.

The normal row continues with `FB_LEVEL,FB_INTEG_HZ,USB_OUT_AGE_US,USB_RESET_COUNT,USB_SUSPEND_COUNT,USB_RESUME_COUNT,DTR_ASSERT_COUNT,DTR_DEASSERT_COUNT,BUFFERING_TIMEOUT_COUNT,CLOCK_WAIT_TIMEOUT_COUNT,RESUME_FAILURE_COUNT,USB_AUDIO_TASK_MAX_GAP_US`. The first three are the filtered feedback ring level, integrator value in Hz, and time in microseconds since the last USB OUT receive callback. The age is blank before a receive timestamp is available or outside STREAMING. `BUFFERING_TIMEOUT_COUNT` counts buffering waits of at least one second while USB OUT packets continue, `CLOCK_WAIT_TIMEOUT_COUNT` counts I²S clock restarts lasting at least 250 ms during rate switching or BUFFERING, and `RESUME_FAILURE_COUNT` counts failed stream reopen attempts on USB resume. These wait timeouts are diagnostic only and do not force a stop or state transition. `USB_AUDIO_TASK_MAX_GAP_US` is the maximum interval in microseconds between `usb_audio_task()` calls during a STREAMING period when timing diagnostics are enabled. `USB_GAP_MAX_US` is the largest previously observed USB OUT receive interval, not the length of an ongoing gap. CSV analysis tools must follow the header.

The following columns `SAMPLE_INTERVAL_US,TASK_GAP_MAX_INTERVAL_US,TASK_GAP_MAX_END_STATE,TASK_GAPS_OVER_1MS_INTERVAL,USB_RX_NONEMPTY_INTERVAL` cover the period since the previous normal row was successfully queued, or since session start for the first row. `SAMPLE_INTERVAL_US` is its duration; `TASK_GAP_MAX_INTERVAL_US` and `TASK_GAPS_OVER_1MS_INTERVAL` are the maximum `usb_audio_task()` interval and the number of intervals over 1 ms across all states. `TASK_GAP_MAX_END_STATE` is the state observed at the end of the longest gap, not necessarily throughout it; it is blank if no gap was observed. A gap crossing a sample boundary is counted in the row containing its end. `USB_RX_NONEMPTY_INTERVAL` counts nonempty USB OUT receive callbacks, not missing packets: silence, stream stops, host behavior and the receive path also affect it. If a normal row cannot be queued, the measurements are retained until the next successful row. The final `LOG_ROWS_DROPPED_TOTAL` counts normal rows that could not be queued during the CDC session; it excludes dropped event and FAST rows.

`UNDERFLOW_COUNT` and the five added counters are cumulative since boot and survive COM reconnection and the diagnostics-screen reset. Older firmware reported `UNDERFLOW_COUNT` relative to the COM session. `USB_RESET_COUNT` counts TinyUSB bus reset completion notifications; `USB_SUSPEND_COUNT` and `USB_RESUME_COUNT` count their TinyUSB callbacks. The DTR counters count changes observed in CDC line-state requests. A bus reset completion or unplug notification clears the tracked DTR state but is not counted as an explicit DTR deassertion. Unreported physical disconnects and implicit state changes are not counted. Counters wrap at 32 bits and are lost on power-off or reboot. Collecting additional events and counters adds diagnostic overhead.

At underflow detection, a separate `UNDERFLOW,MS,RING,DMA_POOL,RING_POOL,HOST_RATE,FEEDBACK_RATE,FB_LEVEL,FB_INTEG_HZ,USB_GAP_MAX_US,USB_OUT_AGE_US` row captures values before the state changes. DMA pool fill is an upper bound including the playing block; the levels are not an atomic measurement. A full event queue reports lost records via `EVENT_QUEUE_DROPPED`.

The `HOST_RATE` measurement also feeds feedback control and continues regardless of diagnostics being enabled or reset. Packet statistics use a separate measurement window.

USB events are emitted as `USB_EVENT,UPTIME_US32,GENERATION,OVERWRITTEN_BEFORE,EVENT`. When the COM port connects, the latest 256 retained events are replayed oldest first, followed by new events. Older events are overwritten if the capacity is exceeded. `UPTIME_US32` is the low 32 bits of uptime in microseconds and wraps about every 71.6 minutes; `GENERATION` changes when diagnostics are reset. `OVERWRITTEN_BEFORE` counts unsent events overwritten before that row. Unsent transmit FIFO data is lost on CDC disconnect. This is an event log, not a capture of all USB packets.

`P0000+` is an event indicating that the CDC connected predicate became false; it is not specific to DTR deassertion and can also result from an unconfigured USB device or suspend. Use the `L` event to check DTR and RTS in an explicit line-state request. `Q` and `V` indicate suspend and resume callbacks; `B02000+` indicates bus reset completion. The absence of events alone does not establish that USB communication continued.

Despite their names, `USBBUF_CAPACITY` and `USBBUF_FREE_*` describe audio ring capacity and free space, not the TinyUSB receive FIFO.

The 32 KiB CDC transmit FIFO retains logs during temporary PC read stalls. If the FIFO runs out of space during a longer stall, periodic rows are dropped and reported as `LOG_ROWS_DROPPED` when transmission resumes. Ring, LCD, and FFT statistics in the next periodic row accumulate since the last successfully queued periodic row. Unsent FIFO data is discarded when CDC disconnects. This FIFO uses RAM only in CDC-enabled builds and is separate from the audio buffer.

In CDC-enabled builds, state transitions and playback DMA silence substitution are recorded in RAM even before the COM port opens, then emitted as `TRACE,UPTIME_US,EVENT,FROM,TO,RATE,RING,DMA_POOL,SILENCE_FRAMES` rows. `STATE` levels are sampled just before the transition; the DMA pool is an upper bound including the block being played. `SILENCE_START` marks the first scheduled silence block, `SILENCE_RESUME` the return to PCM DMA, and `SILENCE_STOP` a DMA stop before PCM resumes. Startup and IDLE zero-DMA blocks are not included in `SILENCE_*`. The end row's `SILENCE_FRAMES` counts frames scheduled to DMA, including any interrupted by a stop, not necessarily frames actually played. An ongoing silence episode has no end row yet. Times are microseconds since boot; subtract `#TRACE_SESSION_START_US` to compare with CDC session time. The queue retains the latest 128 unsent records and reports overwritten records as `TRACE_DROPPED`. Records are removed from the pending queue when queued to the CDC transmit FIFO; unsent FIFO data is lost on CDC disconnect and RAM history is lost on power-off or reboot. Display history is kept separately from the pending queue. Silence substitution alone cannot account for every audible click.

`PCM_START` marks scheduling a PCM DMA block, `PCM_END` the completion of the last PCM DMA block when no next PCM block is scheduled, and `PCM_ABORT` an interrupted and cleared PCM DMA transfer. `RATE` is the sample rate when the event is recorded. Consecutive PCM blocks and repeated clears while stopped do not generate duplicate transitions. These and `SILENCE_*` timestamps are software times near DMA operations, not the actual start or end of analog output: PIO FIFO and DAC delays intervene. Recording these events also runs in the DMA path and can affect timing.

To compare a recording, use the same-boot `#TIME_SYNC,<sync UNIX_US>,<sync UPTIME_US>` to calculate each trace's UTC time as `sync UNIX_US + TRACE UPTIME_US - sync UPTIME_US`. Use a signed difference for events before synchronization. Check the recording's start time and recording-path latency, and compare against a continuous test signal. PC/device synchronization error, clock drift, recording start-time accuracy, and PIO, DAC, and recording delays mean the timestamps do not necessarily align exactly.

USB startup levels are `0` (USB off), `1` (TinyUSB initialization only, no CDC), `2` (full operation, no CDC), and `3` (full operation with CDC diagnostic logging). The default is `2`. Specify `-DUSB_BRINGUP_LEVEL=3` only when diagnostic logging is needed. If an existing CMake cache still contains `3`, pass `-DUSB_BRINGUP_LEVEL=2` or clear the cache before reconfiguring.

Normal playback starts or resumes after the greater of approximately 6 ms of audio at the current sample rate or 3072 frames has accumulated in the ring. For example, this is 3072 frames (about 32 ms) at 96 kHz and 3072 frames (about 16 ms) at 192 kHz. While buffering, no new ring data is moved into DMA, but already queued DMA data is drained. CSV metadata reports the time-based threshold as `PREFILL_MS` and the minimum frame count as `PREFILL_MIN_FRAMES`. The existing CMake option `USB_AUDIO_PREFILL_FRAMES` (reported as `PREFILL_FRAMES`) is the legacy start threshold for draining queued data in non-buffering states, not the normal playback prefill amount.

The ring holds 4096 stereo frames, with a default feedback target of 50% capacity (2048 frames). The normal feedback correction limit is 100 ppm; when the filtered level falls more than 128 frames below the target, the limit increases progressively, reaching 1000 ppm at a deficit of 512 frames or more. During buffering, the request adds 500 ppm of the nominal sample rate to the reference output rate. These are configuration values and do not guarantee that the host follows them.

See [diagnostic event details](DIAGNOSTICS_EN.md) for anomaly codes and audio-state history.

##### Hardware Log Checks (During V2.2 Development)

The following logs were captured with CDC-enabled builds (`USB_BRINGUP_LEVEL=3`). These are individual short tests, not guarantees for every host or playback condition. The host OS and playback software used for the tests after the fix have not been confirmed.

- UAC1: A CDC session of about 2 minutes 40 seconds included 96 kHz/24-bit and 48 kHz/24-bit. The first row had `MS=0`; the recorded `UNDERFLOW_COUNT` stayed at zero, with no `FIFO_TRIM` or DMA silence-substitution rows.
- UAC2: A CDC session of about 2 minutes included 192 kHz/16-bit and 176.4 kHz/16-bit. The first row had `MS=0`; the recorded `UNDERFLOW_COUNT` stayed at zero, with no `FIFO_TRIM` or DMA silence-substitution rows.
- In a separate UAC2 test, 141 events preceding the COM connection were retained from the initial `J0000 +` event and emitted with `OVERWRITTEN_BEFORE=0`. In another test exceeding capacity, 256 events were emitted and 244 overwritten events were reported.

No UNDERFLOW occurred in these logs, so hardware output of the detailed UNDERFLOW row has not been verified. Retaining more than 64 events on UAC1, and simultaneous CDC capture and long-duration testing on Android, also remain unverified. An absence of recorded anomalies does not guarantee click-free playback or long-term stability.

##### Long-Duration Playback Records (CDC Diagnostic Builds)

Long-duration playback logs include runs of approximately 22, 37, and 42 minutes. Across the recorded formats, these runs included sample rates from 44.1 to 192 kHz and 16-, 24-, and 32-bit audio. Excluding two disconnects attributed to a loose connection, the recorded `UNDERFLOW_COUNT`, `MISSED`, and `DROPPED` values were all zero.

These records were captured with CDC diagnostic builds, which stop FFT calculation and display. They do not establish long-duration behavior in normal builds with FFT enabled. Zero counters also do not guarantee the absence of audible noise or stability with other hosts and playback conditions.
