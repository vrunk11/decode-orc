# Video Parameters

Overrides any or all of the video parameter hints that flow through the pipeline alongside field data. Parameters set to `-1` are inherited from the source unchanged, making partial overrides straightforward. Downstream decoding and export stages read these hints to determine frame geometry, IRE levels, sample ranges, and active-area boundaries.

## When to use

Use Video Parameters when your source metadata contains incorrect or non-standard values. Example: a capture made with hardware that used a different black or white level than the standard — set `black_level` and `white_level` here to correct the interpretation before downstream chroma decoding. Alternatively, use the active-line parameters to crop the visible picture area to the true active region when the source metadata describes a non-standard crop.

## What it does

The stage wraps the incoming field representation and overrides the value returned by `get_video_parameters()`. For each parameter that has been set to a value other than `-1`, the corresponding field in the `SourceParameters` structure is replaced. Parameters that remain at `-1` are copied from the source's existing parameters. The stage also updates the `has_nonstandard_values` flag when black or white level is overridden, and sets `active_area_cropping_applied` when any of the active-area parameters are changed. No pixel data is altered.

## Parameters

All parameters are `int32`. A value of `-1` means "inherit from source".

### frame_width_nominal
Override the nominal frame width in samples.

### frame_height
Override the frame height in lines.

### sync_tip_level
Override the sync tip level in the 10-bit sample domain.

### blanking_level
Override the blanking level in the 10-bit sample domain.

### black_level
Override the black level in the 10-bit sample domain. Sets `has_nonstandard_values = true` on the output parameters.

### white_level
Override the white level in the 10-bit sample domain. Sets `has_nonstandard_values = true` on the output parameters.

### peak_level
Override the peak level in the 10-bit sample domain.

### active_video_start
Override the active video start sample index. Sets `active_area_cropping_applied = true`.

### active_video_end
Override the active video end sample index. Sets `active_area_cropping_applied = true`.

### first_active_frame_line
Override the first active frame line number. Sets `active_area_cropping_applied = true`.

### last_active_frame_line
Override the last active frame line number. Sets `active_area_cropping_applied = true`.

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
