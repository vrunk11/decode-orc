# CVBS Source

Reads composite video from a `.composite` file (or a `.y` / `.c` pair for Y/C captures) and its `.meta` SQLite sidecar, then presents the decoded frames as a VideoFrameRepresentation for downstream stages. The stage detects the video system and sample encoding from the sidecar and normalises all sample values to the internal 10-bit CVBS domain before passing data downstream.

## When to use

Add CVBS Source as the first stage in any pipeline that starts from a CVBS file produced by the decode-orc capture toolchain. Only files with a signal state of `STANDARD_TBC_LOCKED` are accepted; files in any other state (for example `STANDARD_TBC_UNLOCKED` or `STANDARD_RAW`) are rejected before any sample data is read.

## What it does

At execute time the stage opens the `.meta` sidecar and reads the video standard, sample encoding, and signal state. If the signal state is not `STANDARD_TBC_LOCKED` the stage reports a configuration error and stops. Otherwise it computes the frame count from the file size and reads each frame's sample words in order, applying the normalisation appropriate to the declared encoding:

- `CVBS_U10_4FSC` — identity transform; values are already in the 10-bit domain.
- `CVBS_U16_4FSC` — divide the 16-bit unsigned value by 64.
- `CVBS_TPG21_4FSC` — divide the signed 16-bit value by 64 and add 508.
- `CVBS_S16_FSC` — divide the signed 16-bit value by 32 and add the 10-bit blanking level.

Each frame carries a colour-frame index measured from the colour burst: 1–4 for PAL and PAL-M, 0–1 for NTSC. Frames whose burst is absent or cannot be measured carry a colour-frame index of -1.

Dropout, audio, EFM, and AC3 sidecars are loaded automatically when the corresponding files are present alongside the primary data file.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
