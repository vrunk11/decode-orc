# CVBS Source

Reads composite video from a `.composite` file (or a `.y` / `.c` pair for Y/C captures) and presents the decoded frames as a VideoFrameRepresentation for downstream stages. By default the stage detects the sample encoding and other capture details from the `.meta` SQLite sidecar; because the CVBS file format declares metadata optional, the sample encoding can also be selected manually so that sources without a sidecar can be used. All sample values are normalised to the internal 10-bit CVBS domain before passing data downstream.

## When to use

Add CVBS Source as the first stage in any pipeline that starts from a CVBS file produced by the decode-orc capture toolchain. Only files with a signal state of `STANDARD_TBC_LOCKED` are accepted; files in any other state (for example `STANDARD_TBC_UNLOCKED` or `STANDARD_RAW`) are rejected before any sample data is read.

## Parameters

The file-path parameters offered match the project's source type: a composite project shows only **CVBS File Path**, while a Y/C project shows only the **Y (Luma)** and **C (Chroma)** paths.

| Parameter | Meaning |
|-----------|---------|
| CVBS File Path (`input_path`) | Path to the composite data file (`.composite`). Composite projects only. |
| CVBS Y (Luma) File Path (`y_path`) | Path to the luma channel file (`.y`). Y/C projects only; set together with `c_path`. |
| CVBS C (Chroma) File Path (`c_path`) | Path to the chroma channel file (`.c`). Y/C projects only; set together with `y_path`. |
| Sample Encoding (`sample_encoding`) | `From metadata` (default) reads the encoding from the `.meta` sidecar. Selecting `CVBS_U10_4FSC`, `CVBS_U16_4FSC`, `CVBS_TPG21_4FSC`, or `CVBS_S16_FSC` manually makes the sidecar optional. |

The `lock_audio` parameter has been removed: the pipeline no longer has a free-running audio regime. All audio is carried as 48 kHz synchronous (frame-locked) 24-bit stereo channel pairs, and source material at another rate is converted automatically on first access.

## What it does

With **Sample Encoding** at its default (`From metadata`), the stage opens the `.meta` sidecar at execute time and reads the video standard, sample encoding, and signal state. If the signal state is not `STANDARD_TBC_LOCKED`, or the video standard does not match the stage, the stage reports a configuration error and stops.

When a sample encoding is selected manually the `.meta` sidecar is ignored (it need not exist). The video standard comes from the stage itself, the signal is assumed to be TBC-locked, the frame count is measured from the file size, and audio channel pairs carry derived names (the `audio_track` metadata table is not read).

Each frame's sample words are read in order, applying the normalisation appropriate to the encoding:

- `CVBS_U10_4FSC` — identity transform; values are already in the 10-bit domain.
- `CVBS_U16_4FSC` — divide the 16-bit unsigned value by 64.
- `CVBS_TPG21_4FSC` — divide the signed 16-bit value by 64 and add 508.
- `CVBS_S16_FSC` — divide the signed 16-bit value by 32 and add the 10-bit blanking level.

Each frame carries a colour-frame index measured from the colour burst: 1–4 for PAL and PAL-M, 0–1 for NTSC. Frames whose burst is absent or cannot be measured carry a colour-frame index of -1.

Dropout, audio, EFM, and AC3 sidecars are loaded automatically when the corresponding files are present alongside the primary data file.

### Audio channel pairs

Every `<basename>_audio_00.wav` … `_audio_15.wav` sidecar becomes a pipeline audio channel pair, in ascending container-number order (the pipeline carries at most 8 pairs). Per-pair descriptions are read from the `.meta` file's `audio_track` table; pairs without a table row derive names of the form `Track NN`. The table's legacy `audio_locked` column is ignored — there is no free-running audio regime.

All pipeline audio is 48 kHz synchronous (frame-locked) 24-bit stereo (SMPTE 272M-1994) and always follows frame remapping (trimming, mapping, stacking) downstream. On first audio access each pair's 16-bit WAV payload is widened to 24-bit and, using the WAV header sample rate, converted to the 48 kHz frame-locked form (SoXR HQ, duration- and sync-preserving); a payload already at 48000 Hz passes through unresampled, so legacy 44.1 kHz containers are converted and current containers are served as-is.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. The source file is present and valid, and metadata is either present and valid or not required (manual sample encoding). |
| Yellow | The source file is present but its `.meta` sidecar is missing or unreadable, and the sample encoding is set to `From metadata`. Provide the sidecar or select an encoding manually. |
| Red | Not configured. No file path is set, the configured path does not point to an accessible source file, or the file's video system does not match the stage. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
