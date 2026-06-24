# Raw Video Sink

Writes the decoded video stream to an uncompressed raw file for use in external tools. Supports RGB48, YUV444P16, and Y4M output formats. Does not support audio — use FFmpeg Video Sink if you need embedded audio.

## When to use

Use this sink when you need an uncompressed output for integration with external tools such as FFmpeg, VirtualDub, or image-processing scripts. Choose `y4m` for maximum compatibility with tools that understand Y4M headers, or `rgb`/`yuv` for direct integration with image processing pipelines.

## What it does

Applies the selected chroma decoder to convert the incoming TBC video stream to colour video, then writes the raw decoded frames to a file without any compression. The output format determines the pixel layout and whether a Y4M header is prepended.

## Parameters

### output_path (string)
Output file path. Use `.rgb`, `.yuv`, or `.y4m` to match the chosen format. Required.

### decoder_type (string)
Chroma decoder to apply. PAL: `pal2d`, `transform2d`, `transform3d`. NTSC: `ntsc1d`, `ntsc2d`, `ntsc3d`, `ntsc3dnoadapt`. Other: `mono`.

### output_format (string)
Raw output format. Values: `rgb` (RGB48, 16-bit per channel), `yuv` (YUV444P16, planar), `y4m` (YUV444P16 with Y4M header).

### chroma_gain (double)
Chroma gain multiplier applied before output. Range: 0.0–10.0. Default: `1.0`.

### chroma_phase (double)
Chroma phase rotation in degrees. Range: -180 to 180. Default: `0`.

### luma_nr (int)
Luma noise reduction level. Higher values reduce luminance noise at the cost of sharpness.

### chroma_nr (int)
Chroma noise reduction level. Higher values reduce colour noise at the cost of chroma resolution.

### ntsc_phase_comp (bool)
Enable NTSC phase compensation. Applies to NTSC sources only.

### simple_pal (bool)
Enable simple PAL mode. Applies to PAL sources only.

### threads (int)
Number of worker threads. Default: `auto` (uses all available cores).

### output_padding (int)
Alignment padding added to each output frame. Default: `8`.

## Notes

- This sink does not support audio embedding. Use FFmpeg Video Sink if you need audio in the output.
- Raw output files can be very large; ensure sufficient disk space before triggering.
- The `y4m` format adds a Y4M header to the file, making it directly readable by tools such as FFmpeg and rav1e without specifying pixel format manually.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
