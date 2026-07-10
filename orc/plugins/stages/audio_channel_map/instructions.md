# Audio Channel Map

Routes the left/right channels of one audio track. Use it to split bilingual (dual-mono) material into two separate tracks, to fill both channels from one channel, or to swap channels. One operation per stage instance; chain instances for compound routing.

## When to use

LaserDisc bilingual discs and tapes with independent linear/Hi-Fi programmes carry two unrelated mono programmes as the left and right channels of a single stereo pair. Use `split_dual_mono` to turn that pair into two tracks, each with the programme on both channels, so downstream sinks can select or embed them independently.

The in-place operations (`left_to_both`, `right_to_both`, `swap_channels`) fix common channel faults — a one-sided mono recording or reversed wiring — without changing the track count.

The operation is a pure per-sample channel remap, so it works identically on frame-locked and free-running tracks; timing, sample rate, and lock state are untouched.

## What it does

The stage wraps the incoming frame representation and remaps the channels of the target track on the fly. Video, dropout hints, EFM/AC3 signal data, and all non-targeted audio tracks pass through untouched.

- `split_dual_mono` replaces the target track in place with "`<name>` (L)" (left channel on both channels) and appends "`<name>` (R)" (right channel on both channels) as the last track. All other track indices are unchanged. Both derived tracks report origin `DERIVED` and keep the target's lock state and sample rate. The appended track counts toward the pipeline's 16-track limit; the stage fails validation if the input already carries 16 tracks.
- `left_to_both` / `right_to_both` copy the chosen channel to both channels of the target track, in place.
- `swap_channels` exchanges the left and right channels of the target track, in place.

The stage fails validation when the target track does not exist on the input.

## Parameters

### track
Integer, 0–15, default `0`. The audio track the operation applies to. Track numbers are 0-based, matching the CVBS container `_audio_NN.wav` numbering.

### operation
String, default `split_dual_mono`. One of:

| Value | Effect |
|-------|--------|
| `split_dual_mono` | Replace the track with "`<name>` (L)" in place and append "`<name>` (R)" |
| `left_to_both` | Copy the left channel to both channels, in place |
| `right_to_both` | Copy the right channel to both channels, in place |
| `swap_channels` | Exchange the left and right channels, in place |

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
