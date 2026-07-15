# webOS Starfish/ALSA audio sync experiment

Date: 2026-07-15

## Configuration

- Video output: Starfish
- Audio output: direct ALSA `hw:0,7`
- Original sound output: `bt_soundbar`
- Test outputs: `tv_speaker`, `external_arc`, `bt_soundbar`
- Test signal: silent stereo PCM, 48 kHz, opened directly on `hw:0,7`
- The original `bt_soundbar` output was restored after the test.

The experiment subscribed to
`luna://com.webos.service.audio/getSoundOutput`, changed the output with
`setSoundOutput`, and sampled these ALSA controls both idle and while PCM was
active:

- `Adec Lipsync Offset` (`numid=62`)
- `Audio Latency Time` (`numid=96`)
- `Delay InputOutput` (`numid=81`)
- ARC/eARC/SPDIF/Sndout controls (`numid=82`, `83`, `87`, `90`, `91`, `95`,
  `98`, and `99`)

## Sound-output event log

All requested transitions returned `returnValue: true` and generated a
subscription event:

1. `bt_soundbar` -> `tv_speaker`
2. `tv_speaker` -> `external_arc`
3. `external_arc` -> `bt_soundbar`

The final query returned:

```json
{"returnValue":true,"lastSoundOutput":"external_arc","soundOutput":"bt_soundbar","settingsSoundOutputs":["bt_soundbar"]}
```

## Measured controls

The values were identical at idle and while direct PCM was active.

| Sound output | `Audio Latency Time` values | Trailing value | `Adec Lipsync Offset` |
| --- | --- | ---: | ---: |
| `tv_speaker` | `1,1,4,1,0,0,0,74` | 74 | 150 |
| `external_arc` | `1,1,4,32,0,0,0,53` | 53 | 150 |
| `bt_soundbar` | `1,1,4,8,0,0,0,42` | 42 | 150 |

`Delay InputOutput` remained zero for every enumerated path. The inspected
Sndout ARC/eARC/Atmos controls also remained unchanged during this direct-ALSA
test.

The OLED platform table at
`/etc/palm/audiooutputd-hwdata/delay/inOutDelayExt_OLED.xml` contains MEDIA
delays of 45-75 ms for `tv_speaker`, 35-60 ms for `headphone`, and zero for
`bt_soundbar` and `external_optical`.

## Interpretation

The split-clock synchronizer drives:

```text
audio_pts - starfish_video_pts + audio_delay = 0
```

Therefore a positive mpv `audio-delay` makes audio later. If `V` is the delay
from the Starfish-reported presentation point to photons on the panel and `L`
is downstream audio latency not represented by ALSA's PCM buffer, the required
setting is:

```text
audio_delay = V - L
```

The observed settings are internally consistent with approximately 160-170 ms
of display-side latency:

- Bluetooth: `+20 ms` plus roughly 145-150 ms output latency gives about
  165-170 ms.
- eARC PCM: `+100-120 ms` plus roughly 50-55 ms output latency gives about
  150-175 ms.

The experiment strengthens this explanation:

- The invariant 150 value in `Adec Lipsync Offset` is close to the inferred
  panel-side delay.
- The ARC `Audio Latency Time` value of 53 predicts a base delay of
  `150 - 53 = +97 ms`, close to the observed `+100-120 ms`.
- The Bluetooth value of 42 cannot be end-to-end Bluetooth latency. Using it
  would predict `+108 ms`, while the observed correction is only `+20 ms`.
  It appears to describe an internal pipeline stage and excludes Bluetooth
  encoding/link/device latency.
- mpv's ALSA AO derives delay from `snd_pcm_status_get_delay()` or queued PCM
  samples. It does not read these platform controls and cannot see latency
  after the direct PCM device.

## Recommended sync change

Add a small webOS audio-output monitor in the app:

1. Subscribe to `getSoundOutput` and log every payload.
2. At playback start and after every output change, read and log controls 62
   and 96 through ALSA's control API.
3. Store the user's trim per sound output instead of globally.
4. Apply an automatic base delay per output, then add the user's trim.

For ARC and speakers, `Adec Lipsync Offset - Audio Latency Time` is a useful
initial automatic base. Bluetooth needs a calibrated route value because the
driver's 42 ms value excludes most of its real latency. Initial values for this
TV would be approximately:

```text
external_arc:  +97 ms base, then user trim
bt_soundbar:   +20 ms base, then user trim
tv_speaker:    +76 ms provisional base, then calibrate
```

Picture mode changes panel latency, so the automatic base must remain
user-adjustable. A later experiment should compare Standard and Game modes;
all three required bases should move together if the Starfish clock is
upstream of picture processing.

## Compressed passthrough

There is no supported compressed-bitstream route through direct ALSA on this
TV:

- `aplay -L` exposes only `null`, `pulse`, and `sysdefault:CARD=lg115x`; there
  is no `iec958` or HDMI passthrough PCM.
- `/dev/snd` and `/sys/class/sound` contain no ALSA compress-offload device.
- Active `hw:0,7` reports ordinary PCM with `subformat: STD`.
- The sound-output bitstream controls belong to the platform Sndout path, not
  the direct ALSA AO.

Starfish is the viable encoded-audio route. The local Starfish context already
maps AC3 and EAC3 (`AC3 PLUS`) formats and has
`starfish_ctx_configure_audio_passthrough()`, but `ao_starfish` never selects
that path: it currently always feeds decoded PCM or AAC produced by its own
encoder.

The next implementation experiment should add an encoded mode to
`ao_starfish` that accepts mpv's AC3/EAC3 special formats, configures the
Starfish context with `starfish_ctx_configure_audio_passthrough()`, and feeds
the demuxed access units without decoding or re-encoding. EAC3/JOC is the best
first target for Atmos. TrueHD/MAT should be treated as unsupported until the
app-facing Starfish pipeline is verified to accept it; codec names in platform
delay tables alone do not prove that capability.
