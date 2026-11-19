# Android Click Investigation

[English](ANDROID_TEST_EN.md) | [日本語](ANDROID_TEST.md)

## Historical Test Notes

Similar clicks occurred in this project, and the feedback limits were previously changed to `FB_BASE_LIMIT_PPM=50` and `FB_BOOST_MAX_PPM=500`. The historical results below are from the derived [Pico2_UAC12_USBDDC_Weather](https://github.com/Toshimiyu/Pico2_UAC12_USBDDC_Weather) project, not measurements of this project with its current settings. The Foobar2000 for Android observation predates V2.1 and is not evidence of current support.

On November 13, 2026, playback was observed on a Sony XQ-ES44 running Android 16 with the standard audio driver, using UAC2 at 48 kHz / 32-bit. Windows 11 26H2 was also tested at 48 kHz / 32-bit.

- YouTube and the Music app on Android produced intermittent short clicks, including after the ring level settled around 50%. Changing the Android volume did not change the click volume.
- No clicks were observed while playing a silent WAV on Android. The USB format during silent playback was not confirmed.
- With the test settings `FB_BASE_LIMIT_PPM=50` and `FB_BOOST_MAX_PPM=500`, the Windows ring level reached 49.9% after about 3 minutes 40 seconds without observed dropouts. In a pre-V2.1 reference test with Foobar2000 for Android, the ring level reached 50% after about 2 minutes 27 seconds without observed clicks. Clicks remained with the Music app under the same settings.
- Turning DSEE Ultimate off in the Music app greatly reduced click frequency, but intermittent clicks and brief muddy-sounding audio remained.
- The diagnostic screen showed O/F/S/C/U at zero and no anomaly events. This does not rule out every kind of audio problem.

Reducing the feedback limits tended to improve the symptoms, but the cause remains unconfirmed. The roles of the app, audio effects, and feedback control need further investigation. The Foobar2000 for Android observation is a pre-V2.1 reference test and is outside current support. Disabling DSEE Ultimate was also only a possible workaround at the time. After V2.2 changed the Output Terminal to `0x0301`, UAC1 recognition by Android's standard driver and playback with Onkyo HF Player were verified on hardware. Noise during playback through the standard driver remains under investigation, and long-term stability has not been verified. UAC1 operation with Onkyo HF Player's USB driver was also verified on hardware. Overly narrow correction limits can allow the ring level to drift and cause dropouts.

#### Status After the Current Adjustments

- After adjusting the capacity, target level, feedback, and other settings, large drops in the ring level have rarely been observed, but occasional clicks remain. Because several settings changed, their individual effects have not been isolated.
- TinyUSB converts UAC1 feedback to 10.14 format for transmission. Rejected UAC1 volume `SET_RES` requests were identified in diagnostic logs and support was added.
- Rate changes within the same clock family skip PLL reconfiguration, but stop and reinitialize the PIO. The PIO is also reinitialized when output starts.
- The change in click frequency, occurrence of OVERFLOW / UNDERFLOW, and whether feedback correction remains at its limit after the current adjustments have not been confirmed.
- BCLK, LRCK, DATA, and DAC analog output waveforms during a click have not been measured. The cause has not been isolated to the host, device, or DAC. Future options include recording the output, observing the signals simultaneously, and comparing another host.

#### Status and Validation Plan as of November 19, 2026

##### Observed facts

- In a comparison using the same HF Player and audio source, clicks were observed with Android's standard driver but not with the HF Player USB Driver. The number of trials and quantitative click frequency were not recorded.
- V2.2 seems to have substantially reduced clicks by listening, but no quantitative comparison has been made.
- V2.2 includes multiple changes, including feedback-control changes, so the effect of each change has not been isolated. The absence of a clear UNDERFLOW or other diagnostic event alone does not establish that no clicks occurred.
- The final V2.2 target is 2048 frames, 50% of the 4096-frame ring. The 2150-frame value was suggested by Claude for testing, not specified by the user as a design target. There is no evidence that changing from 2150 to 2048 improved the symptoms.
- A reply to TinyUSB Discussion #4547 is pending.

##### Hypotheses (unconfirmed)

- Even when both host and device operate within the specification, the interaction between Feedback changes or response and buffer control may cause an interoperability issue with a particular host-device combination.
- V2.2 control changes may have made tracking of momentary fluctuations more measured, possibly reducing the symptoms. However, there are no measurements yet to establish a relationship to PI control or explain the improvement.
- Do not attribute the cause to either the host or device, or treat any explanation for the improvement as established.

##### Measurements to evaluate interoperability

Where possible, compare Android's standard driver and the HF Player USB Driver across repeated trials using the same device, connection, HF Player, audio source, sample rate, bit depth, volume, audio effects, and playback duration. Record the firmware version, build settings, driver, test conditions, and duration for each trial, and correlate click timestamps and counts with a recording.

- **Audio symptoms:** Count and timestamp clicks; record the output waveform and, if possible, simultaneously observe the DAC analog output and BCLK / LRCK / DATA. The absence of a visible digital anomaly does not establish that there was no audible symptom.
- **Buffer and anomalies:** `RING_NOW` / `RING_MIN` / `RING_AVG` / `RING_MAX`, UNDERFLOW, OVERFLOW, and whether and when DMA silence substitution occurred.
- **Feedback control:** Compare `FEEDBACK_RATE`, `HOST_RATE`, `HF_DIFF_NOW`, and `FB_LEVEL` / `FB_INTEG_HZ`; where available, also compare `FB_CORR_PPM_X10`, `FB_LIMIT_PPM_X10`, `FB_OUT_SAT`, and `FB_I_SAT` for each trial. CDC logging may add diagnostic load, so also check whether symptoms differ with CDC enabled and disabled.
- **USB receive timing:** Relate `USB_OUT_AGE_US` and `USB_GAP_MAX_US` and receive intervals to symptom timestamps. These values alone do not identify the host as the cause.

##### Separate host-side and device-side options

- **Host side:** Record the driver, application, audio effects, and requested format. If available, inspect USB traces or host logs for Feedback reception and application. Changes to Android's standard driver cannot be controlled by this device, so share observations with the relevant implementers.
- **Device side:** Compare one factor at a time, including target level, PI control, Anti-Windup, rate-scaled gain, and reuse of the estimated clock difference between streams. Evaluate buffer level, correction variation and saturation, and dropouts as well as clicks; confirm that reducing clicks does not compromise underflow headroom.

These are proposed validation steps, not evidence that either host or device violates the specification or that any particular mitigation will work.

---
