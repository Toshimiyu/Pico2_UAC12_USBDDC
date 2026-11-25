# Diagnostic Event Reference

[English](DIAGNOSTICS_EN.md) | [日本語](DIAGNOSTICS.md)

## USB Event Log

The bottom area displays USB communication events.

Features:

- 3 columns × 8 rows
- Up to 64 events stored (256 with CDC enabled)
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
| M | USB Mount |
| J | Initial audio state during USB initialization (IDLE) |
| P | CDC connection status change (01: USB configured, not suspended and DTR asserted; 00: otherwise) |
| L | CDC line-state request (first value: DTR, second: RTS; CDC-enabled builds only) |
| Q | USB suspend (first value: remote wakeup enabled; CDC-enabled builds only) |
| V | USB resume (CDC-enabled builds only) |
| A | Host request for Audio Streaming Interface Alt=0 |
| I | Host request for Audio Streaming Interface Alt>0 |
| O | `usb_audio_stream_open()` |
| C | `usb_audio_stream_close()` (also used for configuration descriptor requests) |
| R | `usb_audio_apply_sample_rate()` |
| W | Sample-rate switch begin / processing in `service_switch()` |
| K | `service_switch()` completed |
| G | USB Control Request Get |
| S | USB Control Request Set / audio state transition |
| U | USB Disconnect |
| D | Device descriptor request / buffer drain complete |

Some event symbols are reused for different purposes. In particular, use the following hexadecimal fields and surrounding event sequence to distinguish `C`, `S`, and `D` events. During an alternate-setting change, `A → C → I → O` represents the normal sequence of an Alt=0 request, stream close, Alt>0 request, and stream open; it does not indicate an error.

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

## Display Specification (Pico Display Pack 2.0)

At startup, the boot screen is displayed. After initialization completes, normal builds show the playback screen, while CDC-enabled builds (`USB_BRINGUP_LEVEL=3`) show the diagnostic screen. CDC-enabled builds disable the playback screen, FFT processing, and FFT input capture.
After startup completes, the lower `System Ready` message is cleared after about 0.5 seconds; the upper message remains.

The X button on Pico Display Pack 2.0 cycles through the following screens in one direction:

- Boot Screen
- Playback Screen (CDC-disabled builds only)
- Diagnostic Screen
- Feedback Monitor screen
- Playback Start Log screen (CDC-enabled builds only)

- X : Next screen (Boot → Diagnostics → Feedback Monitor → Playback Start Log → Boot with CDC enabled; Boot → Playback → Diagnostics → Feedback Monitor → Boot otherwise)

On the diagnostic screen:

- A : Scroll older USB log entries
- B : Scroll newer USB log entries
- A+B : Toggle between USB events and recent audio-state history
- Hold Y for 3 seconds : Reset diagnostic counters and the USB event log (a countdown is shown while holding)

USB event logging starts at boot, and the latest 64 events (256 in CDC-enabled builds) remain available on the Diagnostics screen. It records events such as USB bus resets/disconnects, device/configuration/string descriptor requests, configuration mount/unmount, and UAC control requests. This is event-driven logging, not a periodic playback-state log, so the display does not change while playback continues without a new event. Track changes and other USB audio state transitions may add new entries. Holding Y for 3 seconds clears the collected diagnostic values and USB event log; logging resumes with subsequent events. The O/F/S/C/U event counters are updated when events occur regardless of the active screen. Periodic diagnostic statistics, such as buffer levels and processing intervals, are collected while the Diagnostics, Feedback Monitor, or packet capture screen is active, or CDC is connected. Collected diagnostic values are retained when leaving the screen, and collection resumes from those values when it is reopened. The playback screen continues audio playback, FFT, and bit-depth/sample-rate display.

In the log header, `U:Y` means USB is configured and `U:N` means it is not configured. `C:Y` means USB is configured, not suspended, and CDC DTR is asserted; `C:N` means at least one of those conditions is false. `C:-` indicates a build without CDC. DTR depends on the host's settings, so `C:N` alone does not identify a fault or why a COM port cannot be opened. History is lost on power-off; photograph the screen before unplugging USB if needed.

#### Feedback Monitor screen

Press X on the Diagnostics screen to open `FEEDBACK MONITOR`. It refreshes approximately every 0.5 seconds and draws incrementally when audio headroom permits.

- `BUF` is the current ring fill and `DEV` is current fill minus the target, in stereo frames.
- `FB` is the requested feedback rate rounded to integer Hz. `OFF` is the internal feedback rate minus the effective I²S rate, in 0.1 Hz units. It is not just the PI integral term or a measured clock error. Both are unavailable in IDLE or SWITCHING.
- `1s` gives the minimum and maximum ring fill sampled on this screen over the last completed approximately one-second STREAMING window; `PP` is their difference. Sampling can miss instantaneous DMA extrema. Values are unavailable until a complete window after entry or after streaming resumes.
- `UF` counts low ring-plus-DMA-pool underflow detections; `OF` counts high ring level or ring-write-failure overflow detections. Hold Y on Diagnostics to reset them. Zero counts do not guarantee the absence of clicks.

#### Playback Start Log screen

In CDC-enabled builds (`USB_BRINGUP_LEVEL=3`), press X on the Feedback Monitor screen to open `PLAYBACK START LOG`. It records without opening a COM port, including when connected to Android, and can be photographed while connected. `SILENCE EVENTS` counts silence-substitution episodes and `SILENCE FRAMES` counts frames scheduled to DMA since boot; the latter includes any interrupted by a stop, so it is not necessarily the number actually played.

The newest records are shown first. Press A for older records and B for newer ones. State transitions show uptime seconds (wrapping every 100000 seconds), the previous and next state, sample rate, ring fill `R`, and DMA pool `D`. For example, `BUFF>STRM` shows the levels at playback start. `SILENCE START` marks the first silence block, `SILENCE PCM` the return to PCM, and `SILENCE STOP` a DMA stop; `F` on the end row is the number of frames scheduled in that episode. DMA pool fill is an upper bound including the block being played.

The screen retains the latest 128 records independently of CDC transmission and reports overwritten records as `HISTORY OVERWRITTEN`. Holding Y on the Diagnostics screen does not clear this history; power-off or reboot clears the history and totals, so take photos before unplugging USB.

#### USB CDC Diagnostic Log

For CDC configuration, CSV formats, logged metrics, and hardware log checks, see the [USB CDC diagnostic log guide](CDC_LOG_EN.md).

### Playback Screen

The playback screen displays the following real-time information.

#### L/R Level Meter

Displays stereo audio levels in real time.

#### Independent L/R 11-Band FFT Display

Displays left and right channel frequency spectra independently with A-weighting applied.
FFT bars are updated one channel at a time, left channel first, followed by the right channel.

#### Audio Information

Displays current:

- Bit Depth
- Sample Rate (Fs)

#### Display Notes

Note: The peak indicator of the L/R level meter updates continuously according to input signal level and may appear to flicker. This is normal behavior.

If the FFT bars and peak meters do not move while music is playing, first check the volume setting on the host device, such as a smartphone or PC. The meter display follows the software gain, so an extremely low host volume or mute may result in no visible activity on the screen.

---

### Diagnostic Screen

The X button cycles through Boot → Playback → Diagnostics → Feedback Monitor in normal builds, or Boot → Diagnostics → Feedback Monitor → Playback Start Log in CDC-enabled builds. Diagnostic builds with packet capture enabled add the packet capture screen after Playback Start Log. Y does not navigate between screens; holding it for 3 seconds on the Diagnostics screen resets the diagnostic counters and USB event log and rearms packet capture when enabled. See the [Android test procedure](ANDROID_TEST_EN.md) for operation and limitations.

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

The buffer level does not converge to a perfectly fixed value.

The PI controller continuously adjusts USB Feedback to compensate for small fluctuations in the host-side transmission rate caused by Windows, media players, USB scheduling, and other factors.

As a result, the buffer level normally exhibits small movements around the target level rather than remaining perfectly stationary.

##### Current / MIN / MAX

`c` is the current ring level; `n` and `x` are the minimum and maximum recorded during playback.

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
| F | Old frames discarded when the USB receive FIFO is near capacity |
| S | Frames discarded during sample-rate switching |
| C | Frames cleared during restart or stop processing |
| U | Output stoppage count detected during playback |
| a | Maximum Core0 processing interval (µs) |
| b | Maximum Core1 processing interval (µs) |
| T | DMA tail-block output count |
| B | BUFFERING waits of at least one second while USB audio is arriving |
| K | Clock restarts lasting at least 250 ms during rate switching or BUFFERING |
| V | Overflow detections |
| R | Returns to BUFFERING from STREAMING or a fault state |

O/F/S/C/U/B/K/V/R are displayed with four digits and values above 9999 are shown as 9999.

The current-stall indication is independent of the cumulative counters. BUFFERING is timed only while USB reception continues, and timing restarts after reception pauses. Clock restarts in BUFFERING or SWITCHING are timed from restart initiation; normal idle clock stoppage is not a stall. Startup silence is excluded; BUFFERING that began during startup is timed no earlier than the end of startup silence.

O/F/S/C/U/B/K/V/R are incremented when their events occur, regardless of the active screen, and show counts accumulated since reset. B/K are shown even without CDC, in red when nonzero. Only while a current wait is timed out, output is stopped, and DMA is silent, the log heading is drawn one character per second as `Bxxxx Kxxxx STATE BK` even with low ring headroom. The trailing `B`/`K` indicate the currently stalled wait types, or `-` when absent. Special low-headroom drawing stops when the wait ends; the normal heading returns when normal drawing is permitted. These timeouts are diagnostic only and do not force a stop or automatic recovery. T counts DMA tail-block output conditions, but is incremented only while diagnostics are enabled and audio is streaming. Periodic measurements such as a/b and buffer levels are also collected while Diagnostics, Feedback Monitor, or the packet capture screen is active, or CDC is connected.

For stable operation:

```text
O = 0
F = 0
U = 0
```

is generally expected.

---

### USB Event Log

For event symbols and anomaly codes, see the USB Event Log and Audio Anomaly Codes and State History sections in this document.

### Information to Capture When Playback Stops

If playback stops or audio dropouts occur, capture the diagnostic screen immediately if possible.

The USB event log stores up to the most recent 64 events (256 with CDC enabled).

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



## Audio Anomaly Codes and State History

Audio-state history is recorded about every 100 ms during playback. When an anomaly is detected, the approximately 4.8 seconds before and 1.6 seconds after it are retained as a frozen history for review. Detected anomalies are added to the USB log as red `E` entries: `01` buffer underflow, `02` overflow/discard, `03` old data discarded when the USB receive FIFO is near capacity, `04` USB bus reset/disconnect, `05` playback output stop, `06` Core0 loop interval overrun, `07` Core1 loop interval overrun, `08` USB OUT packet interval of at least 3 ms, `09` PIO TX FIFO stall, `0A` DMA AHB transfer error, `0B` invalid PCM format, `0C` invalid PCM frame boundary, and `0D` click candidate. Click candidates can also be normal audio. These events also record state, rate, ring fill, and DMA-pool level in the CDC log. PIO stall monitoring starts 10 ms after output begins. Each history entry shows uptime seconds, audio state, ring-buffer level, and DMA-pool level.

On `0A`, DMA is stopped and cleared, and playback re-enters buffering if necessary. Detail bit 0 indicates a READ error and bit 1 a WRITE error. `DROP_FRAMES_UPPER` is an upper bound on the DMA-pool frames discarded during recovery; it can include a partially played block. The same upper bound is added to the clear-discard counter. Anomaly logging is rate-limited, so repeated faults may not all be logged. Recovery requires hardware validation.
