# Raw EFM Data Sink

Extracts raw EFM t-values from the pipeline and writes them to a headerless binary file, field by field. Use this stage when you want the lowest-level EFM export for archival, offline re-decoding with different settings, or external analysis tools.

## When to use

Use this stage when you want to preserve the raw EFM t-values separately from the video pipeline — for example to archive the data for later re-decoding with updated EFM decode algorithms, to feed into external analysis or visualisation tools, or to compare EFM quality across multiple source captures before committing to a decode. If you want decoded audio or ECMA-130 sectors, use the EFM Decoder Sink stage instead.

## What it does

For each field in the pipeline, the stage reads the raw EFM t-values from the VideoFieldRepresentation and appends them sequentially to the output file. Each t-value is written as a single 8-bit unsigned integer (valid range 3–11 inclusive). There are no headers, no field boundaries, and no framing — just a flat binary stream of t-values in field order.

## Parameters

### output_path (string)
Path to the output file. The conventional extension is `.efm`. Required. The file will be created or overwritten at trigger time.

## Notes

The upstream source stage must supply EFM data — the pipeline will abort if no EFM data is present in the VideoFieldRepresentation. EFM stacking (selecting the best t-values across multiple captures) must be performed upstream via the Stacker stage before this sink is triggered.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
