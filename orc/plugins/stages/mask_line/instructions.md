# Mask Line

Blanks (overwrites with a fixed IRE level) specified lines in the field stream based on field parity and line number ranges. The stage does not affect any other lines or the audio/EFM streams. It is typically placed at the end of a processing pipeline to clean up lines that should not appear in the final output.

## When to use

Use Mask Line when specific lines contain data that must be removed before encoding or export. Example: you want to black out the VBI region (lines 0–22 of both fields) before producing a final video file, or you need to suppress the NTSC closed-caption line (field-1 line 21) because it interferes with a downstream analysis tool. Add Mask Line after all other processing stages, specify the target lines in `lineSpec`, or use the Mask Line Config tool to choose a preset.

## What it does

The stage wraps the incoming field representation and intercepts `get_line()` calls. For each requested line, it checks whether the line falls within any of the specified ranges for the correct field parity. If it does, the returned pointer points to a buffer filled with the sample value that corresponds to `maskIRE`. All other lines are forwarded from the source unchanged. YC dual-channel sources are handled independently: both the luma and chroma buffers for a masked line are filled with the mask value.

## Parameters

### lineSpec (string)
Comma-separated list of line ranges to mask. Each element is `PARITY:LINE` or `PARITY:START-END`. Parity codes: `F` = field 1 only, `S` = field 2 only, `A` = both fields. Line numbers are 0-based frame-flat indices. Default: `""` (no masking). Examples: `F:21` masks field-1 line 21; `S:6-22` masks field-2 lines 6 through 22; `A:0-22,F:21` masks the VBI region of both fields and also field-1 line 21.

### maskIRE (double)
IRE level written to all masked lines. Range: 0.0–100.0. Default: `0.0` (black level). Set to `100.0` to fill with white, or any intermediate value for a mid-grey reference.

## Tools

### Mask Line Config
Opens a preset helper dialog that provides common masking patterns (for example, the VBI region or the NTSC closed-caption line) without requiring you to write the `lineSpec` string manually. Select this tool from the stage context menu to open the dialog and apply a preset.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
