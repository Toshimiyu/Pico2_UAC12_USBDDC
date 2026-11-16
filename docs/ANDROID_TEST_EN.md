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

Reducing the feedback limits tended to improve the symptoms, but the cause remains unconfirmed. The roles of the app, audio effects, and feedback control need further investigation. The Foobar2000 for Android observation is a pre-V2.1 reference test and is outside current support. Disabling DSEE Ultimate was also only a possible workaround at the time. Android's standard driver is outside the V2.1 support scope. Long-term stability with these settings has not been verified; overly narrow correction limits can allow the ring level to drift and cause dropouts.

#### Status After the Current Adjustments

- After adjusting the capacity, target level, feedback, and other settings, large drops in the ring level have rarely been observed, but occasional clicks remain. Because several settings changed, their individual effects have not been isolated.
- TinyUSB converts UAC1 feedback to 10.14 format for transmission. Rejected UAC1 volume `SET_RES` requests were identified in diagnostic logs and support was added.
- Rate changes within the same clock family skip PLL reconfiguration, but stop and reinitialize the PIO. The PIO is also reinitialized when output starts.
- The change in click frequency, occurrence of OVERFLOW / UNDERFLOW, and whether feedback correction remains at its limit after the current adjustments have not been confirmed.
- BCLK, LRCK, DATA, and DAC analog output waveforms during a click have not been measured. The cause has not been isolated to the host, device, or DAC. Future options include recording the output, observing the signals simultaneously, and comparing another host.

---
