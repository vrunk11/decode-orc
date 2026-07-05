# Dropout Map

Overrides the dropout hints attached to individual fields without altering the underlying video sample data. The stage can add new dropout regions that the automatic detector missed, remove false-positive regions, or adjust the boundaries of existing regions on a per-field basis. Downstream stages such as Dropout Correction see the modified hints and act accordingly.

## When to use

Use Dropout Map when the automatic dropout detection in the source stage has produced incorrect results on specific frames. Example: the automatic detector missed a physical scratch across field 42, or incorrectly flagged a valid high-chroma region on field 100 as a dropout. Add Dropout Map after the source stage, open the Dropout Editor tool to draw corrections interactively, then let Dropout Correction fix the marked regions.

## What it does

The stage wraps the incoming field representation and intercepts calls to `get_dropout_hints()`. For each field that has an entry in the `dropout_map` parameter, additions are merged into the hint list and removals are subtracted before the list is returned to the caller. Fields without an entry are forwarded with their original hints unchanged. No pixel data is read or modified by this stage.

## Parameters

### dropout_map (string)
Per-frame dropout override specification encoded as a JSON-like array. Each element identifies a frame and lists regions to add or remove. Default: `[]` (no overrides). Example: `[{frame:1,add:[{line:10,start:100,end:200}],remove:[{line:15,start:50,end:75}]}]`. Frame numbers are entered 1-based in the GUI parameter dialog, matching the preview window; the project file (YAML) stores them 0-based and the conversion is automatic. Line, start, and end values are frame-flat 0-based coordinates. Use the Dropout Editor tool to build and edit this string interactively rather than writing it by hand.

## Tools

### Dropout Editor
Opens the interactive dropout map editor. Select this tool from the stage context menu to visually draw new dropout regions or delete existing ones on a per-field basis. Changes made in the editor are saved back to the `dropout_map` parameter automatically.

## Status Indicator

The coloured dot in the top-right corner of the node shows its configuration status.

| Colour | Meaning |
|--------|---------|
| Green | Fully configured and ready to run. All required parameters are set. |
| Yellow | Partially configured. The stage can run but will use default or reduced behaviour — for example, pass-through mode or console-only output. Review the parameters for optional settings. |
| Red | Not configured. One or more required parameters are missing and the stage cannot run. |

Parameters can be set via **Edit Parameters...** in the node context menu. Some stages also provide interactive stage tools (listed under **Tools** above) that set parameters directly from within the tool.
