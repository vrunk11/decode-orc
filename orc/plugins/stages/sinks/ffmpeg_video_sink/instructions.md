# FFmpeg Video Sink

Encodes the processed video stream to a compressed output file using FFmpeg. Supports H.264/MP4 for broad compatibility and FFV1/MKV for lossless archival, with optional audio, closed captions, and chapter metadata.

## When to use

Use this sink at the end of your pipeline when you want a playable, distributable, or archival video file. Choose `mp4-h264` for wide device compatibility or `mkv-ffv1` for lossless preservation. Use the FFmpeg Preset Config tool to quickly apply well-tested decoder and encoder combinations for PAL or NTSC sources.

## What it does

Applies the selected chroma decoder to convert the incoming TBC video stream to colour video, then encodes it using FFmpeg into the chosen container and codec. Optionally embeds analogue audio, closed captions (as mov_text subtitles, MP4 only), and chapter markers derived from VBI data.

## Parameters

### output_path (string)
Output file path. Use `.mp4` for H.264 or `.mkv` for FFV1. Required.

### decoder_type (string)
Chroma decoder to apply. PAL: `pal2d`, `transform2d`, `transform3d`. NTSC: `ntsc1d`, `ntsc2d`, `ntsc3d`, `ntsc3dnoadapt`. Other: `mono`.

### output_format (string)
Container and codec. Values: `mp4-h264` (H.264 in MP4), `mkv-ffv1` (FFV1 lossless in MKV).

### chroma_gain (double)
Chroma gain multiplier applied before encoding. Range: 0.0–10.0. Default: `1.0`.

### chroma_phase (double)
Chroma phase rotation in degrees. Range: -180 to 180. Default: `0`.

### luma_nr (int)
Luma noise reduction level. Higher values reduce luminance noise at the cost of sharpness.

### chroma_nr (int)
Chroma noise reduction level. Higher values reduce colour noise at the cost of chroma resolution.

### ntsc_phase_comp (bool)
Enable NTSC phase compensation. Applies to NTSC sources only.

### simple_pal (bool)
Enable simple PAL mode. Applies to PAL sources only. Uses a simpler chroma decoding path.

### threads (int)
Number of worker threads. Default: `auto` (uses all available cores).

### output_padding (int)
Padding added for codec alignment requirements. Default: `8`.

### encoder_preset (string)
Encoder speed/quality trade-off. Values: `fast`, `medium`, `slow`, `veryslow`. Slower presets produce smaller files at the same quality level.

### encoder_crf (int)
Constant Rate Factor for quality-based encoding. Range: 0–51; lower values produce higher quality and larger files. Default: `18`. Used when `encoder_bitrate` is `0`.

### encoder_bitrate (int)
Target bitrate in bits per second. When non-zero, overrides CRF mode. Default: `0` (use CRF).

### embed_audio (bool)
Embed analogue audio from the source into the output file. Requires audio data to be present in the pipeline. Default: `false`.

### embed_closed_captions (bool)
Embed closed captions as mov_text subtitles. MP4 output only; converts EIA-608 captions from VBI. Default: `false`.

### embed_chapter_metadata (bool)
Write chapter markers derived from VBI data into the output file. Default: `false`.

## Tools

### FFmpeg Preset Config
Opens a preset helper dialog that lets you select common encoder and decoder configurations without manually setting each parameter. Invoke from the Stage Tools menu. Applies well-tested PAL and NTSC combinations for quick setup.

## Notes

- Closed caption embedding (`embed_closed_captions`) is only supported in MP4 output (`mp4-h264`).
- CRF and bitrate modes are mutually exclusive; set `encoder_bitrate` to a non-zero value to switch from CRF mode.
- Audio embedding requires analogue audio data to be present in the upstream pipeline.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
