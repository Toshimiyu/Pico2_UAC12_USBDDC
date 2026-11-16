# Changelog (V1.0-V1.9)

[English](CHANGELOG_EN.md) | [日本語](CHANGELOG.md)

## V1.9

#### USB Reception and Diagnostics

- Add maximum USB OUT receive gap, ring-buffer headroom, and underrun count to the 100 ms CDC summary
- Add a 1 ms sampled ring-water average and report static RAM, ring, FFT, and LCD buffer memory figures in CDC metadata
- Record Core0/Core1 loop-interval distributions and stream start/stop and rate-change counts
- `USB_GAP_MAX_US` is the maximum interval between USB OUT receive callbacks while streaming, not a Windows-only delay measurement
- LCD draw-operation and per-channel FFT times are reported as 100 ms maximum/average values; loop distributions accumulate during streaming
- Expand the CDC transmit FIFO to 32 KiB to reduce log loss during temporary PC read stalls
- Add columns to periodic CDC rows; existing CSV analysis tools need to be updated for the new header

#### Display and Load

- Limit spectrum rendering to 30 FPS and make peak-marker fall speed time-based
- Complete bar decay after input changes stop and reduce blocking SPI drawing frequency without changing ring capacity

### V1.8

#### USB Reception and Diagnostics

- Validate PCM sample width and stereo frame boundaries before writing to the ring buffer; clear the USB receive FIFO to resynchronize when invalid data is detected
- Record PCM format and frame-boundary errors in diagnostic events and counters
- Add diagnostics for abrupt input PCM changes as click candidates without modifying playback audio; normal signals can also trigger a candidate

#### Playback and Mute

- Unify playback startup and rebuffering around approximately 6 ms of audio at the current sample rate, fixing premature DMA consumption before prefill completes
- Stop reading the ring during rebuffering and drain data already queued to DMA before waiting
- Add an approximately 5 ms gain ramp for USB mute and unmute; volume changes remain immediate and automatic mute on faults is not implemented
- Add `PREFILL_MS`, the normal playback prefill time, to CDC log metadata

### V1.7

#### Diagnostics

- Record the host's Audio Streaming Interface Alt=0 request as `A`, distinct from the stream-close event `C`, in both UAC1 and UAC2
- Add and clarify event records for alternate-setting requests, stream open/close, sample-rate application, and audio state transitions

#### Playback and Display

- Replace the 250 µs repeating timer with elapsed-time monitoring in the Core0 main loop for detecting audio inactivity (500 ms)
- Move silent-period buffer cleanup to the main loop, reducing periodic interrupt load
- Split FFT bar rendering into separate update passes for the left and right channels

### V1.6

- Record USB OUT packet intervals of 3 ms or longer as events
- Detect PIO TX FIFO stalls and DMA AHB transfer errors, recording state, ring, and DMA levels
- Disabled the virtual COM port (CDC) and diagnostic log output in the normal build; diagnostic logging mode is enabled only when `-DUSB_BRINGUP_LEVEL=3` is specified

### V1.5

- Changed underflow detection to consider total frames in the ring and DMA pool, rather than ring fill alone

### V1.4

- Reduced the USB CDC diagnostic sample interval from 1 second to 100 ms
- Logged state transitions as events with millisecond timestamps
- Added firmware version and prefill threshold to log metadata
- Recorded maximum feedback-rate differences per session and stream
- Added state, ring, and DMA-pool snapshots to anomaly events

### V1.3

- Moved the I²S DMA transmit buffers into a dedicated linker section at the start of SRAM7 (`0x20070000`)
- Limited the regular RAM region to 448 KiB and reserved the upper 64 KiB for DMA buffers
- Increased the audio ring buffer from 1024 to 1536 stereo frames (about 8 ms at 192 kHz)

### V1.2

- Removed unnecessary files from the `src` folder

### V1.1

- Updated measurement data

### V1.0

Major changes from `Pico2_UAC2_USBDDC_VerFinal_11FFT_NoOverSampling`:
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
