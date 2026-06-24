# Stacker

Combines between 1 and 16 frame-aligned input sources into a single superior output by selecting or averaging the best pixel data available across all sources. When only one input is connected the stage acts as a passthrough. With multiple inputs, corresponding pixels from each source are compared and the most plausible value is selected, recovering pixels that are in dropout in one source but valid in another.

## When to use

Use Stacker after Source Align when you have multiple captures of the same LaserDisc that contain dropouts. Example: three captures with heavy surface damage — Stacker overlaps each capture's pixel data and, using differential dropout detection, can recover a pixel that is corrupted in one or two captures as long as at least one source has a clean reading. Three or more sources are required for differential dropout detection.

## What it does

For each output field, Stacker fetches the corresponding field from every input source and applies the selected stacking mode pixel by pixel. In `Auto` mode the stage selects the most appropriate algorithm for the number of sources available. Smart modes use a configurable threshold to exclude outlier values before averaging. Differential dropout detection compares dropout flags across sources; if a pixel is flagged as a dropout in fewer than all sources, valid data from the clean sources is used instead. Audio and EFM data can be combined independently of video using their own stacking settings. The stage caches stacked fields in an LRU cache to avoid redundant recomputation during preview navigation.

## Parameters

### mode (string)
Stacking algorithm. Values: `Auto`, `Mean`, `Median`, `Smart Mean`, `Smart Neighbor`, `Neighbor`. Default: `Auto`. `Auto` selects the most appropriate mode for the number of available sources.

### smart_threshold (int32)
Pixel-value threshold used by the `Smart Mean` and `Smart Neighbor` modes to exclude outliers before averaging. Range: 0–128. Default: `15`. Has no effect for other modes.

### no_diff_dod (bool)
Disable differential dropout detection. When `true`, dropout status from individual sources is not used to guide pixel selection. Default: `false`.

### passthrough (bool)
When `true`, pixels that are in dropout across all sources are passed through unchanged rather than being replaced. Default: `false`.

### audio_stacking (string)
Method used to combine audio samples from multiple sources. Values: `Disabled`, `Mean`, `Median`. Default: `Mean`. When `Disabled`, audio from the source with the fewest dropouts is used.

### efm_stacking (string)
Method used to combine EFM t-values from multiple sources. Values: `Disabled`, `Mean`, `Median`. Default: `Mean`. When `Disabled`, EFM from the source with the fewest dropouts is used.

## Tools

This stage has no interactive tools. A Stacker Configuration stage report is generated automatically after execution.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
