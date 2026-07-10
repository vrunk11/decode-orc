# Audio Align

Shifts one audio track in time relative to the video to correct audio/video synchronisation. Positive offsets delay the audio (insert lead-in); negative offsets advance it (trim from the start). Only the target track moves — video, all other tracks, and undecoded EFM/AC3 signal data are untouched.

## When to use

Use Audio Align when a track is consistently early or late against the picture — for example an EFM digital audio track whose decode start-up (sync acquisition) left it slightly offset, or an imported WAV that was captured with a different lead-in than the video. Add one instance per track that needs adjustment; multiple instances may target different tracks.

## What it does

The stage wraps the incoming frame representation and serves the target track shifted by the requested offset, converted to a whole number of stereo pairs at the track's exact rational sample rate.

- **Frame-locked tracks**: each frame's audio window is assembled from the neighbouring frames' samples, with silence filling past either end of the frame range. Per-frame pair counts are unchanged, so the track remains spec-conformant locked audio.
- **Free-running tracks**: the stream origin shifts — positive offsets prepend silence pairs, negative offsets trim pairs from the start, and the reported stream length adjusts accordingly.

Sample values and the sample rate are never changed; the shift is pure placement, no resampling.

The stage fails validation when the target track does not exist on the input. A zero offset passes the input through unchanged.

## Parameters

### track
Integer, 0–15, default `0`. The audio track to shift. Track numbers are 0-based, matching the CVBS container `_audio_NN.wav` numbering.

### offset_ms
Floating point milliseconds, default `0.0`. Positive delays the audio relative to the video; negative advances it. The offset is rounded to the nearest whole stereo pair at the track's sample rate (about 0.023 ms resolution at 44.1 kHz).

## Tools

This stage has no interactive tools.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
