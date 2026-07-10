# Audio Track Import

Attaches an external WAV file to the pipeline as a new audio track. Use it to bring separately captured or restored audio — a cleaned-up soundtrack, a commentary recording, an externally decoded track — into the project so sinks can select, embed, or export it alongside the decoder-provided audio.

## When to use

Use Audio Track Import whenever audio that did not come from the video capture should travel with the video through the pipeline. The imported track is appended after all existing tracks (existing track numbers are unchanged) and reports origin `IMPORTED`. If the recording needs nudging into sync with the picture, follow with an Audio Align stage targeting the new track.

## What it does

The stage wraps the incoming frame representation and appends one audio track served directly from the WAV file. Video, dropout hints, EFM/AC3 signal data, and all existing audio tracks pass through untouched.

The WAV must be RIFF/WAVE, PCM, stereo, 16-bit — the only audio encoding permitted by the CVBS file format specification. Anything else fails validation with a reason.

The track is either frame-locked or free-running:

- **Frame-locked** — the WAV holds exactly `frame_count × pairs-per-frame` stereo pairs at the locked rate for the project's video standard (PAL: 1764 pairs/frame at 44100 Hz; NTSC/PAL-M: 1470 pairs/frame at 44100000⁄1001 Hz, header 44056). Each video frame maps to its exact WAV slice; anything past end-of-file is silence.
- **Free-running** — a 44100 Hz stream independent of frame timing, whose first sample is synchronous with the first frame of this stage's input. WAVs with any other header rate fail validation.

Appending the track counts toward the pipeline's 16-track limit; the stage fails validation if the input already carries 16 tracks.

## Parameters

### wav_path
File path, required. The WAV file to attach.

### track_name
String, default empty. Human-readable name for the track (shown by sinks, e.g. as the stream title in Video Sink). Empty uses the WAV file name.

### lock_mode
String, default `auto`. One of:

| Value | Effect |
|-------|--------|
| `auto` | Frame-locked when the WAV's length and header rate exactly match the locked layout for the video standard; otherwise free-running |
| `locked` | Force frame-locked serving (each frame reads its slice at the locked pairs-per-frame stride) |
| `free_running` | Force free-running serving (header must be 44100 Hz) |

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
