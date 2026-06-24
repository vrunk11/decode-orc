# TBC Source

Reads one or more TBC files produced by ld-decode or vhs-decode and presents the decoded video as a frame-domain representation for downstream stages. The stage detects the video system (PAL, NTSC, PAL-M) and signal type (composite or Y/C) from the `.tbc.json.db` metadata, remaps all sample levels from the ld-decode 16-bit internal domain to the 10-bit CVBS domain, and assembles full-frame buffers ready for processing.

## When to use

Add TBC Source as the first stage in any pipeline that starts from ld-decode or vhs-decode output. Select the composite `.tbc` file for a standard composite capture, or supply the `.tbcy` luma and `.tbcc` chroma files for a Y/C capture. All other sidecars (audio, EFM disc data, AC3 RF) are found automatically from the same directory.

## What it does

At execute time the stage opens the `.tbc.json.db` metadata database and reads the video system, field dimensions, and signal levels. It then selects the correct converter (PAL composite, PAL Y/C, NTSC composite, NTSC Y/C, or PAL-M composite) and reads each pair of TBC fields, remapping the 16-bit ld-decode levels to the 10-bit CVBS domain. The resulting full-frame buffers are assembled in order and returned as a VideoFrameRepresentation.

Frame sizes after assembly: PAL frames contain 709,379 samples; NTSC frames contain 477,750 samples; PAL-M frames contain 477,225 samples.

For Y/C inputs the stage validates colour-frame phase alignment between the luma and chroma files at open time and rejects misaligned pairs. NTSC-J sources with a non-standard black level are detected automatically from the metadata and the remapping is adjusted accordingly. Associated audio (`.pcm`), EFM disc data (`.efm`), and AC3 RF (`.ac3sym`) sidecars are attached to each frame if the files are present alongside the primary TBC file.

The display name shown in the pipeline is resolved at load time from the detected metadata, for example "PAL TBC Composite" or "NTSC TBC YC".

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
