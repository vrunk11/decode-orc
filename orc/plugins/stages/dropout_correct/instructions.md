# Dropout Correction

Replaces corrupted sample regions (dropouts) with substitute data drawn from neighbouring lines or fields. The corrector reads the dropout hints attached to each field by upstream stages (source stages or Dropout Map) and overwrites the flagged sample ranges with the best replacement data it can locate within the configurable search distance. After correction, the output field's dropout hints are cleared because all flagged regions have been processed.

## When to use

Use Dropout Correction whenever your TBC source contains dropouts from disc damage, tape degradation, or other signal loss. Place it after the source stage for single-source pipelines, or after Stacker for multi-source pipelines where stacking alone did not recover all dropouts. Enable `highlight_corrections` first to see exactly which regions will be corrected before committing to the final output.

## What it does

For each flagged dropout region the stage searches inward from the damaged line in both directions, evaluating nearby lines for suitability as replacement data. A quality metric based on sample variance guides selection. By default the search crosses field boundaries (interfield correction) when the same-field search exhausts its distance limit; setting `intrafield_only` restricts the search to the same field. For PAL sources, `match_chroma_phase` ensures the replacement line has compatible chroma phase to avoid colour errors. The `overcorrect_extension` parameter widens each dropout boundary by a fixed number of samples before correction, which helps on heavily damaged sources where the flagged region slightly underestimates the true damage extent.

## Parameters

### overcorrect_extension (uint32)
Extend each dropout region by this many samples on each side before applying correction. Useful for heavily damaged sources where the detected boundary is slightly too narrow. Range: 0–48. Default: `0`.

### intrafield_only (bool)
When `true`, replacement data is only taken from lines within the same field; interfield replacement from the opposite field block is disabled. Default: `false`.

### max_replacement_distance (uint32)
Maximum number of lines away from the damaged line to search for replacement data. Larger values find more candidates but may introduce visible artefacts if the replacement is far from the original. Range: 1–50. Default: `10`.

### match_chroma_phase (bool)
When `true`, only replacement lines whose chroma phase is compatible with the damaged line are considered. Prevents colour phase errors on PAL sources. Has no effect on NTSC composite sources. Default: `true`.

### highlight_corrections (bool)
When `true`, corrected regions are filled with white level (100 IRE) instead of replacement data. Use this to visualise which regions will be corrected before performing the final export. Default: `false`.

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
