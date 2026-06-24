# Analogue Audio Sink

Extracts the analogue audio tracks from the processed video pipeline and writes them to a standard WAV file, synchronised to the processed video timeline. Use this stage when you want the audio independently from the full TBC output.

## When to use

Add this sink in parallel with the LD Sink stage when your LaserDisc or tape source carries analogue audio tracks and you need the audio as a standalone file — for example to process it in a DAW, verify audio/video sync, or archive it separately. Because it is a parallel sink, adding it does not affect the video output.

## What it does

The stage reads 16-bit signed little-endian stereo PCM samples at 44,100 Hz from the VideoFieldRepresentation, which are populated by the upstream source stage from the `.pcm` sidecar file. It then wraps the samples in a standard RIFF WAV container and writes the result to the specified path. The audio is aligned to the processed field sequence, so any frame trimming or reordering performed upstream is reflected in the output.

## Parameters

### output (string)
Path to the output WAV file. Required. The file will be created or overwritten at trigger time.

## Notes

This stage handles analogue audio only. Digital audio carried as EFM (CD-quality stereo) or AC3 RF (Dolby Digital) must be extracted with the EFM Decoder Sink or AC3 RF Sink stages respectively. Audio track selection or stacking (when processing multiple source captures) must be performed upstream, for example via the Stacker stage's `audio_stacking` parameter.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
