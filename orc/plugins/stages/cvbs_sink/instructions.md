# CVBS Sink

Writes processed CVBS video data to the CVBS file format for archival, interchange, or round-trip testing. The output consists of a `.composite` binary file and a `.meta` SQLite sidecar, and can be re-opened by the CVBS Source stage.

## When to use

Use this sink at the end of a CVBS processing pipeline when you want to save the result in the native CVBS file format — for example, after stacking multiple captures or applying corrections, and you want to store the result for later re-processing or sharing.

## What it does

Writes the incoming video stream to a `.composite` binary file (or `.y`/`.c` pair for Y/C signal type) and a `.meta` SQLite sidecar database. The sidecar is written with `sample_encoding_preset = CVBS_U10_4FSC` and `signal_state_preset = STANDARD_TBC_LOCKED`; these values are fixed and not user-configurable. Associated dropout, audio, EFM, and AC3 data are written automatically as sidecar files when present in the stream.

## Parameters

### output_path (string)
Base path for output files, without extension. The stage appends `.composite` (or `.y`/`.c`) and `.meta` automatically. Required.

### signal_type (string)
Signal type to write. Use `composite` for standard CVBS output, or `yc` to write separate Y and C component files. Default: `composite`.

### capture_notes (string)
Optional free-text notes written to the `.meta` file. When left empty, no notes field is written. Default: `""`.

## Notes

- The `.meta` SQLite sidecar uses fixed encoding and signal-state presets; these cannot be changed via parameters.
- Sidecar files for dropout, audio, EFM, and AC3 data are written automatically alongside the main output when those data streams are present.
- The output is compatible with the CVBS Source stage for round-trip workflows.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
