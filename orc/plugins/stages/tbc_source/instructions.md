# TBC Source

Reads one or more TBC files produced by ld-decode or vhs-decode and presents the decoded video as a frame-domain representation for downstream stages. The stage detects the video system (PAL, NTSC, PAL-M) and signal type (composite or Y/C) from the `.tbc.db` metadata database, remaps all sample levels from the ld-decode 16-bit internal domain to the 10-bit CVBS domain, and assembles full-frame buffers ready for processing.

## When to use

Add TBC Source as the first stage in any pipeline that starts from ld-decode or vhs-decode output. Select the composite `.tbc` file for a standard composite capture, or supply the `.tbcy` luma and `.tbcc` chroma files for a Y/C capture. All other sidecars (audio, EFM disc data, AC3 RF) are found automatically from the same directory.

## Parameters

| Parameter | Meaning |
|-----------|---------|
| TBC File Path (`input_path`) | Path to the composite `.tbc` file. Composite captures only. |
| Luma TBC File Path (`y_path`) | Path to the luma `.tbcy` file. Y/C captures only; set together with `c_path`. |
| Chroma TBC File Path (`c_path`) | Path to the chroma `.tbcc` file. Y/C captures only; set together with `y_path`. |
| PCM Audio File Path (`pcm_path`) | Path to the analogue audio `.pcm` sidecar (raw 16-bit stereo PCM). |
| PCM Audio Timing (`pcm_audio_timing`) | Declares the actual timing of the `.pcm` sidecar. `free_running` (default): the PCM is a free-running 44100 Hz stream, as written by ld-decode. `frame_locked`: the PCM is already at the video standard's frame-locked rate, laid out per the metadata's per-field audio sample counts, and is served directly with no resampling. |
| Lock Audio To Frames (`lock_audio`) | Free-running sidecars only (hidden when the timing is declared `frame_locked`). Enabled (default): the stream is resampled into a frame-locked track (SoXR HQ, duration- and sync-preserving) — for PAL this is a same-rate segmentation, for NTSC/PAL-M a pull-down to 44,100,000⁄1001 Hz. Disabled: the PCM is carried verbatim as a free-running 44100 Hz track. |
| EFM Data File Path (`efm_path`) | Path to the EFM t-value `.efm` sidecar. |
| AC3 RF Symbols File Path (`ac3rf_path`) | Path to the AC3 RF symbols `.ac3sym` sidecar. |

## What it does

At execute time the stage opens the `.tbc.db` metadata database (falling back to legacy `.tbc.json` metadata produced by older ld-decode/vhs-decode versions) and reads the video system, field dimensions, and signal levels. It then selects the correct converter (PAL composite, PAL Y/C, NTSC composite, NTSC Y/C, or PAL-M composite) and reads each pair of TBC fields, remapping the 16-bit ld-decode levels to the 10-bit CVBS domain. The resulting full-frame buffers are assembled in order and returned as a VideoFrameRepresentation.

Frame sizes after assembly: PAL frames contain 709,379 samples; NTSC frames contain 477,750 samples; PAL-M frames contain 477,225 samples.

For Y/C inputs the stage validates colour-frame phase alignment between the luma and chroma files at open time and rejects misaligned pairs. NTSC-J sources with a non-standard black level are detected automatically from the metadata and the remapping is adjusted accordingly. Associated audio (`.pcm`), EFM disc data (`.efm`), and AC3 RF (`.ac3sym`) sidecars are attached to each frame if the files are present alongside the primary TBC file.

When a `.pcm` sidecar is present it becomes audio track 0 (named "Analogue"). Its handling follows the **PCM Audio Timing** and **Lock Audio To Frames** parameters: with the defaults (`free_running` + lock enabled) the stream is converted to a frame-locked track exactly as in previous releases — the NTSC/PAL-M resample is deferred until audio is first read, so video-only preview never pays for it. Declaring the sidecar `frame_locked` serves it verbatim using the per-field audio sample counts from the metadata. Disabling the lock carries the sidecar verbatim as a free-running 44100 Hz track, addressed by stream position rather than per frame.

The raw `.efm` sidecar stores one byte per EFM T-value in field order; the stage locates each frame's T-values using the per-field T-value counts recorded in the `.tbc.db` metadata. (Unlike CVBS captures, TBC captures have no separate `.efm.meta` index — the counts in the video metadata are authoritative.) EFM is therefore exposed to downstream stages, such as the EFM Sink, only when both the `.efm` file exists and the metadata carries T-value counts.

The display name shown in the pipeline is resolved at load time from the detected metadata, for example "PAL TBC Composite" or "NTSC TBC YC".

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. The TBC file and its metadata are present and readable. |
| Yellow | The TBC file is present but its metadata sidecar is missing or unreadable. The stage cannot run until the metadata is available. |
| Red | Not configured. No file path is set, or the configured path does not point to an accessible TBC file. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
