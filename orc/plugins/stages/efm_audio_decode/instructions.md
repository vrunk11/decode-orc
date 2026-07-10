# EFM Audio Decode

Decodes the EFM t-value stream carried by the pipeline (LaserDisc digital audio) to CD audio and appends it to the representation as a new audio track. The appended track is free-running: an exactly 44100 Hz stereo stream, independent of video frame timing, whose first sample is synchronous with the first frame of this stage's input. The decoded audio is never resampled.

## When to use

Use EFM Audio Decode on ld-decode LaserDisc sources that carry an `.efm` sidecar when you want the disc's digital audio available as a pipeline track — for example so the Video Sink embeds it alongside the analogue audio, or so the Audio Sink or CVBS Sink can export it. If you only want a standalone WAV file of the digital audio, the EFM Decoder Sink does that without adding a track.

Place this stage after dropout correction and stacking so it decodes the best available EFM stream, and after any frame mapping or source alignment so the decoded track is born aligned to the output timeline.

This transform is audio-only. EFM data discs (ECMA-130 sectors) cannot become an audio track; on a data disc the decode fails and the appended track is empty (use the EFM Decoder Sink in data mode instead).

## What it does

The stage wraps the incoming frame representation and appends one audio track (`EFM digital audio`, origin EFM, free-running, 44100 Hz). Video, dropout hints, undecoded EFM/AC3 signal data, and all existing audio tracks pass through untouched.

EFM decoding is whole-stream sequential (the CIRC interleaving spans sectors), so it cannot run frame-by-frame. The decode therefore runs lazily, at most once, on the first access to the appended track's audio: the stage gathers the t-values across the input's full frame range, runs the shared EFM decode pipeline, and caches the decoded PCM in a scratch file on disk (decoded audio is roughly 635 MB per hour). Video-only preview and project validation never trigger the decode.

EFM decode start-up (sync acquisition) makes the track's time origin approximate; if the digital audio needs nudging relative to the video, apply a per-track sync adjustment downstream.

Appending the track counts toward the pipeline's 16-track limit; the stage fails validation if the input already carries 16 tracks.

## Parameters

### no_timecodes
Boolean, default `false`. Disable timecode verification during decode. Needed for early CAV discs that pre-date the EFM timecode specification.

### no_audio_concealment
Boolean, default `false`. Disable interpolation-based audio error concealment; uncorrectable samples are left as decoded instead of being concealed.

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
