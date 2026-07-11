# Audio Channel Map

Routes the left/right channels of one audio channel pair. Use it to split bilingual (dual-mono) material into two separate channel pairs, to fill both channels from one channel, or to swap channels. One operation per stage instance; chain instances for compound routing.

## When to use

LaserDisc bilingual discs and tapes with independent linear/Hi-Fi programmes carry two unrelated mono programmes as the left and right channels of a single stereo pair. Use `split_dual_mono` to turn that pair into two channel pairs, each with the programme on both channels, so downstream sinks can select or embed them independently.

The in-place operations (`left_to_both`, `right_to_both`, `swap_channels`) fix common channel faults — a one-sided mono recording or reversed wiring — without changing the channel-pair count.

The operation is a pure per-sample channel remap on the synchronous 48 kHz 24-bit pipeline audio; timing and per-frame sample counts are untouched.

## What it does

The stage wraps the incoming frame representation and remaps the channels of the target channel pair on the fly. Video, dropout hints, EFM/AC3 signal data, and all non-targeted channel pairs pass through untouched.

- `split_dual_mono` replaces the target channel pair in place with "`<name>` (L)" (left channel on both channels) and appends "`<name>` (R)" (right channel on both channels) as the last channel pair. All other channel-pair indices are unchanged. Both derived channel pairs report origin `DERIVED`. The appended channel pair counts toward the pipeline's 8-pair limit; the stage fails validation if the input already carries 8 channel pairs.
- `left_to_both` / `right_to_both` copy the chosen channel to both channels of the target channel pair, in place.
- `swap_channels` exchanges the left and right channels of the target channel pair, in place.

The stage fails validation when the target channel pair does not exist on the input.

## Parameters

### channel_pair
Integer, 0–7, default `0`. The audio channel pair the operation applies to. Channel-pair numbers are 0-based, matching the CVBS container `_audio_<n>.wav` numbering.

### operation
String, default `split_dual_mono`. One of:

| Value | Effect |
|-------|--------|
| `split_dual_mono` | Replace the channel pair with "`<name>` (L)" in place and append "`<name>` (R)" |
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
