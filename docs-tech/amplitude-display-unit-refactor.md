<!--
  File:    amplitude-display-unit-refactor.md
  Module:  docs-tech
  Purpose: Implementation plan for project-level amplitude display unit setting
-->

# Amplitude Display Unit Refactor

The application currently displays signal amplitude in at least four different units across
dialogs, graphs, and labels: IRE, millivolts (mV), 10-bit sample levels (0–1023), and
16-bit sample levels. This plan consolidates them behind a single, project-level preference
— IRE, mV, or 10-bit samples — and provides correct bidirectional conversion between the
chosen display unit and the canonical 10-bit storage domain.

The underlying store is always 10-bit unsigned (0–1023, matching the CVBS_U10_4FSC domain).
All numeric parameters entered in IRE or mV are rounded to the nearest 10-bit value before
storage, preserving sub-sample precision where possible (e.g., 7.5 IRE → 276 for NTSC).

Default: **IRE** for NTSC projects, **mV** for PAL and PAL-M (matching industry convention —
EBU for PAL, SMPTE for NTSC).

---

## Scope of change

Files that display or accept amplitude values and must be updated:

| File | Current unit(s) | Element type |
|---|---|---|
| `orc/gui/masklineconfigdialog.cpp/h` | IRE | Spinbox, preset labels |
| `orc/gui/qualitymetricsdialog.cpp/h` | IRE | Read-only labels |
| `orc/gui/hintsdialog.cpp/h` | 10-bit | Read-only labels |
| `orc/gui/framescopedialog.cpp/h` | mV | Y-axis, markers, cursor (handles both line and frame scope) |
| `orc/gui/waveformmonitordialog.cpp/h` + `waveformmonitorwidget.cpp/h` | mV | Y-axis, markers |
| `orc/gui/frametimingwidget.cpp/h` | mV | Y-axis tick labels, tick intervals, markers |
| `orc/gui/burstlevelanalysisdialog.cpp/h` | IRE | Y-axis title, data series |
| `orc/gui/projectpropertiesdialog.cpp/h` | — | New unit selector |
| `orc/presenters/include/metrics_presenter.h` | IRE (field in struct) | DTO field |
| `orc/plugins/stages/mask_line/mask_line_stage.cpp/h` | IRE | `ire_to_sample()` helper |

Note: `LineScopeDialog` was replaced by `FrameScopeDialog` (see `previewdialog.cpp`
comment "Create frame scope dialog (replaces LineScopeDialog)"). `showLineScope()` in
`PreviewDialog` delegates directly to `frame_scope_dialog_`. There is no separate
`LineScopeDialog` to update.

Conversion functions currently scattered across the GUI (`cvbs_sample_to_mv`,
`active_video_mv`, duplicated `ire_to_mv` constants) are centralised in a new common header
in Phase 3 and removed from their current locations.

---

## Phase 0 — Pre-implementation: migrate non-10-bit internal storage to 10-bit

**Goal:** ensure every internal data path carries amplitudes as 10-bit sample values before
the display-unit feature is built on top of them. This phase must be merged before Phase 1
begins.

**Identified non-10-bit fields that must change:**

| Location | Field / key | Current unit | Required change |
|---|---|---|---|
| `orc/common/include/common_types.h` | `FieldBurstLevelStats::median_burst_ire` | IRE double | Rename to `median_burst_10bit`; unit becomes 10-bit sample amplitude |
| `orc/common/include/common_types.h` | `FrameBurstLevelStats::median_burst_ire` | IRE double | Same |
| `orc/core/observers/burst_level_observer.cpp` | Context key `"median_burst_ire"`, stores `median_raw * ire_per_unit` | IRE | Store `median_raw` (10-bit); rename key to `"median_burst_10bit"` |
| `orc/presenters/include/metrics_presenter.h` | `double burst_level` (comment: "IRE") | IRE | Rename `burst_level_10bit`; read key `"median_burst_10bit"` |
| `orc/presenters/src/metrics_presenter.cpp` | Reads context key `"median_burst_ire"` | IRE | Read `"median_burst_10bit"` |
| `orc/plugins/stages/burst_level_analysis_sink/…_deps.cpp` | CSV column `"median_burst_ire"` | IRE | Rename column to `"median_burst_10bit"` |
| `orc/gui/burstlevelanalysisdialog.h` | `addDataPoint(int32_t, double burstLevel)` | IRE | Change `double` → `double` in 10-bit units (or `int32_t`; see note below) |
| `orc/plugins/stages/mask_line/mask_line_stage.cpp/h` | `mask_ire_`, `"maskIRE"` param, `ire_to_sample()` | IRE | See Task 0.2 |
| `orc/core/analysis/mask_line/mask_line_analysis.cpp` | `"maskIRE"` parameter descriptor | IRE | See Task 0.2 |

**Breaking changes in this phase:** project YAML files containing `maskIRE` will fail
validation after Task 0.2. CSV files from `burst_level_analysis_sink` will have a renamed
column. No backward compatibility is provided for either.

**Resolved: `kMinBurstAmplitude = 20.0` in `colour_frame_phase_observer.cpp`**

`kMinBurstAmplitude` is an accumulated quadrature demodulator threshold — the units are
`sqrt(I²+Q²)` where I and Q are sums of 40 centered 10-bit sample values. For a burst
with per-sample amplitude A, coherent accumulation over 40 samples at 4FSC gives
`amplitude = 20 × A`, so 20.0 in this domain corresponds to ~1 ADU per sample (~0.17
IRE). This is a noise-floor check ("is there any burst at all?"), not an IRE-derived
constant. No conversion is required. The constant is already correct and must not be
changed in Phase 0.

**Task 0.1 — Migrate burst level from IRE to 10-bit sample amplitude**

The burst amplitude is computed directly from 10-bit samples:

```cpp
// rms is computed from centered 10-bit sample values
double peak_amplitude = rms * std::sqrt(2.0);  // in 10-bit sample units
```

The `* ire_per_unit` conversion that follows is display-only work that does not belong in
the observer. Remove it.

Changes:
- `BurstLevelObserver::process()`: store `median_raw` (not `median_raw * ire_per_unit`)
  under context key `"median_burst_10bit"`. Remove the local `ire_per_unit` variable.
  Update the 30 IRE outlier filter: `peak_amplitude * ire_per_unit > 30.0` →
  `peak_amplitude > 30.0 / ire_per_unit` (compute threshold once from video params, then
  compare in 10-bit domain).
- Rename `FieldBurstLevelStats::median_burst_ire` and
  `FrameBurstLevelStats::median_burst_ire` to `median_burst_10bit`.
- Update `metrics_presenter.h` field name and `metrics_presenter.cpp` key string.
- Update `BurstLevelAnalysisDialog::addDataPoint()`: second parameter now carries a 10-bit
  amplitude (`double` is fine; rms computation produces a non-integer result even though
  samples are integers).
- Rename CSV column.

Note: `median_burst_10bit` is an AC amplitude (peak burst excursion in sample units),
not an absolute level. Conversion to a display unit requires dividing by
`(white_level - blanking_level)` to normalise, then multiplying by 100 (IRE) or
`active_video_mv()` (mV). This is distinct from converting an absolute level like
blanking or white.

Acceptance criterion: with `BurstLevelObserver` running on a PAL source, the stored
`median_burst_10bit` value is in the range 100–140 (matching the known ~21.5 IRE burst
amplitude in 10-bit domain for PAL: 21.5/100 × (844−256) ≈ 127 samples).

**Task 0.2 — Migrate mask_line stage parameter from IRE to 10-bit**

The `"maskIRE"` YAML parameter stores a `double` in `[0.0, 100.0]`. Change to an integer
10-bit sample level `[0, 1023]`.

Changes in `orc/plugins/stages/mask_line/mask_line_stage.cpp/h`:
- Rename YAML parameter key `"maskIRE"` → `"maskSampleLevel"`.
- Change `ParameterType::DOUBLE` to `ParameterType::INT`; update constraints min/max to
  `0` / `1023`.
- Rename member `mask_ire_` → `mask_sample_level_` (type `int32_t`).
- Remove `ire_to_sample()` helper — the mask value IS the sample value.
- Replace all `ire_to_sample(mask_ire_)` call sites with
  `static_cast<int16_t>(std::clamp(mask_sample_level_, 0, 1023))`.
- Update `get_parameters()` display name / description to remove "IRE".

Same parameter changes in `orc/core/analysis/mask_line/mask_line_analysis.cpp`.

Acceptance criterion: save a project containing a mask line stage; `"maskSampleLevel"`
appears in YAML; loading that project and triggering the stage writes the correct 10-bit
value into the output frame.

---

## Phase 1 — Core: AmplitudeDisplayUnit enum and Project storage

**Goal:** the `Project` model stores an amplitude display unit; it round-trips through YAML.

**Task 1.1 — Add `AmplitudeDisplayUnit` enum to `orc/common/include/common_types.h`**

Add after the `VideoFormat` block:

```cpp
enum class AmplitudeDisplayUnit {
  IRE,          ///< Percentage of full active-video amplitude (0 = blanking, 100 = white)
  Millivolts,   ///< mV referenced to blanking (700 mV PAL, 714.3 mV NTSC/PAL-M)
  Samples10Bit  ///< Raw 10-bit CVBS_U10_4FSC sample value (0–1023)
};

inline std::string amplitude_unit_to_string(AmplitudeDisplayUnit u) {
  switch (u) {
    case AmplitudeDisplayUnit::IRE:         return "IRE";
    case AmplitudeDisplayUnit::Millivolts:  return "mV";
    case AmplitudeDisplayUnit::Samples10Bit: return "10bit";
    default:                                return "IRE";
  }
}

inline AmplitudeDisplayUnit amplitude_unit_from_string(const std::string& s) {
  if (s == "mV")    return AmplitudeDisplayUnit::Millivolts;
  if (s == "10bit") return AmplitudeDisplayUnit::Samples10Bit;
  return AmplitudeDisplayUnit::IRE;  // default
}

/// Return the conventional display unit for a given video system.
/// NTSC convention: IRE (SMPTE 170M-2004).
/// PAL / PAL-M convention: mV (EBU Tech. 3280-E).
inline AmplitudeDisplayUnit default_amplitude_unit(VideoSystem system) {
  switch (system) {
    case VideoSystem::PAL:
    case VideoSystem::PAL_M:
      return AmplitudeDisplayUnit::Millivolts;
    default:
      return AmplitudeDisplayUnit::IRE;
  }
}
```

Acceptance criterion: `ctest -R unit` still green; `ctest -R MVPArchitectureCheck` passes.

**Task 1.2 — Add storage and I/O to `Project`**

In `orc/core/include/project.h`:
- Add private field: `AmplitudeDisplayUnit amplitude_unit_ = AmplitudeDisplayUnit::IRE;`
- Add public const getter: `AmplitudeDisplayUnit get_amplitude_unit() const { return amplitude_unit_; }`
- Add `friend` declaration for a new `project_io::set_amplitude_unit()` function.
- Declare `project_io::set_amplitude_unit(Project&, AmplitudeDisplayUnit)`.

In `orc/core/project.cpp`:
- In `serialize_project_to_yaml()` emit under the `project:` block:
  `amplitude_unit: <amplitude_unit_to_string(project.amplitude_unit_)>`
- In `load_project_from_yaml()` read the `amplitude_unit` key — this field is **required**.
  If it is missing, emit an error and fail validation (same behaviour as the `video_format`
  key: no silent defaulting).
- Implement `set_amplitude_unit()` setting `project.amplitude_unit_` and marking `is_modified_`.

Acceptance criterion: save a project, reload it, unit round-trips correctly for all three
values.

**Task 1.3 — Auto-default in `create_empty_project()`**

In `project_io::create_empty_project()` (in `project.cpp`), after setting `video_format_`:

```cpp
project.amplitude_unit_ = default_amplitude_unit(video_format);
```

Acceptance criterion: new PAL project defaults to `Millivolts`; new NTSC project defaults
to `IRE`; new PAL-M project defaults to `Millivolts`.

---

## Phase 2 — Presenter: expose amplitude unit through IProjectPresenter

**Goal:** GUI code reads and writes the amplitude unit through the presenter seam.

**Task 2.1 — Extend `IProjectPresenter` and `ProjectPresenter`**

`AmplitudeDisplayUnit` lives in `orc/common/` (`common_types.h`), which is visible to every
layer including presenters and GUI. There is no need for a parallel presenter-layer enum
(unlike `orc::presenters::VideoFormat`, which collapses the three-valued `orc::VideoSystem`
into a two-valued display format — no such simplification exists here). Use
`orc::AmplitudeDisplayUnit` directly in the presenter interface.

In `orc/presenters/include/i_project_presenter.h` add to the **Project Metadata** section:

```cpp
virtual orc::AmplitudeDisplayUnit getAmplitudeUnit() const = 0;
virtual void setAmplitudeUnit(orc::AmplitudeDisplayUnit unit) = 0;
```

Implement in `orc/presenters/src/project_presenter.cpp`:
- `getAmplitudeUnit()` calls `core_project_.get_amplitude_unit()` and returns the value
  directly (no conversion function required).
- `setAmplitudeUnit()` calls `project_io::set_amplitude_unit()` directly.

Add a mock override in any existing `MockProjectPresenter` used in tests.

Acceptance criterion: `ctest -R unit` green; `ctest -R MVPArchitectureCheck` passes.

---

## Phase 3 — Common conversion utilities

**Goal:** all amplitude conversions live in one place; scattered per-dialog functions are
removed.

**Task 3.1 — Create `orc/common/include/amplitude_conversion.h`**

This header is pure C++ with no Qt or presenter dependencies. It includes `<common_types.h>`
for `VideoSystem` and `AmplitudeDisplayUnit`.

```cpp
namespace orc {

/// Active video amplitude in mV for the given system.
/// EBU Tech. 3280-E §1.1: PAL = 700 mV.
/// SMPTE 170M-2004 §11.4: NTSC/PAL-M = 7.143 mV/IRE × 100 = 714.3 mV.
inline double active_video_mv(VideoSystem system);

/// Convert a 10-bit sample to millivolts.
/// Blanking level maps to 0 mV; white level maps to active_mv.
inline double samples10_to_mv(int32_t sample, int32_t blanking, int32_t white,
                               VideoSystem system);

/// Convert a 10-bit sample to IRE.
/// Blanking level maps to 0 IRE; white level maps to 100 IRE.
inline double samples10_to_ire(int32_t sample, int32_t blanking, int32_t white);

/// Convert millivolts to the nearest 10-bit sample (clamped to [0, 1023]).
inline int32_t mv_to_samples10(double mv, int32_t blanking, int32_t white,
                                VideoSystem system);

/// Convert IRE to the nearest 10-bit sample (clamped to [0, 1023]).
inline int32_t ire_to_samples10(double ire, int32_t blanking, int32_t white);

/// Convert a 10-bit sample to the chosen display unit.
double samples10_to_display(int32_t sample, int32_t blanking, int32_t white,
                             VideoSystem system, AmplitudeDisplayUnit unit);

/// Convert a value in the chosen display unit to the nearest 10-bit sample.
int32_t display_to_samples10(double value, int32_t blanking, int32_t white,
                              VideoSystem system, AmplitudeDisplayUnit unit);

} // namespace orc
```

All functions are implemented inline in the header or in a corresponding
`orc/common/amplitude_conversion.cpp` if needed. No external dependencies.

**Task 3.2 — Add "nice interval" helpers to `orc/common/include/amplitude_conversion.h`**

Tick marks (Y-axis grid lines at regular intervals) must always land on round numbers in
the selected unit. Simply converting existing mV tick positions to another unit produces
fractional results (e.g., 100 mV ÷ 7.0 mV/IRE = 14.3 IRE — unacceptable). Each unit has
its own canonical step sizes:

| Unit | Minor tick (grid line) | Major tick (labelled) |
|---|---|---|
| IRE | 5 IRE | 10 IRE |
| mV | 50 mV | 100 mV |
| 10-bit | 128 samples | 256 samples |

These are the defaults; the implementation may choose smaller steps when the visible range
is narrow (e.g., zoomed in) or larger steps when it is very wide.

Add to `amplitude_conversion.h`:

```cpp
/// Default minor-tick interval in display units (grid lines, not labelled).
double amplitude_minor_tick(AmplitudeDisplayUnit unit);

/// Default major-tick interval in display units (labelled grid lines).
double amplitude_major_tick(AmplitudeDisplayUnit unit);

/// Snap a value upward to the nearest multiple of step (for first visible tick).
/// Always returns an integer multiple of step; no fractional results.
inline double snap_ceil(double value, double step) {
  return std::ceil(value / step) * step;
}
```

**Two distinct label types — different rounding rules apply:**

*Tick mark labels* (regular Y-axis grid lines):
- Always at exact integer multiples of the step size (computed via `snap_ceil`).
- Labels show no decimal places — `"10 IRE"`, `"100 mV"`, `"256"`.
- The existing mV code (`std::ceil(y_min_mv_ / tick_step) * tick_step`) already does this
  correctly; the refactor must preserve this pattern for all three units.

*Signal level markers* (named threshold lines at blanking, black, white, sync tip, peak):
- Drawn at the exact converted position of the 10-bit level; do NOT snap to the nearest
  tick.
- In 10-bit mode these are always exact integers (`"256"`, `"844"`).
- In IRE or mV mode the converted value may be fractional; show one decimal place
  (`"7.5 IRE"`, `"700.0 mV"`).  Do not round to the nearest tick integer.
- This is the only place where fractional display values appear.

**Task 3.3 — Extend `orc/common/include/amplitude_conversion.h` with string formatting**

String formatting has no Qt dependency and belongs in common so that the CLI can also use
it. Extend the header with:

```cpp
namespace orc {

/// Short suffix string for a display unit: "IRE", "mV", or "" (10-bit has no suffix).
std::string amplitude_unit_suffix(AmplitudeDisplayUnit unit);

/// Input precision (decimal places): 1 for IRE, 1 for mV, 0 for 10-bit.
int amplitude_input_precision(AmplitudeDisplayUnit unit);

/// Display range [min, max] in the chosen unit, covering sync_tip to peak.
/// Requires blanking, sync_tip, white, and peak 10-bit values from OrcSourceParameters.
std::pair<double, double> amplitude_display_range(
    int32_t sync_tip, int32_t blanking, int32_t white, int32_t peak,
    VideoSystem system, AmplitudeDisplayUnit unit);

/// Format a 10-bit sample as a display string with suffix
/// (e.g., "82.3 IRE", "576.5 mV", "844").
/// Use this for signal level markers, which show an exact converted position.
std::string format_amplitude(int32_t sample10, int32_t blanking, int32_t white,
                              VideoSystem system, AmplitudeDisplayUnit unit);

/// Format a value that is already in display-unit terms (e.g., a tick position
/// produced by snap_ceil) as a label string with suffix.
/// Tick labels never need decimal places: "10 IRE", "100 mV", "256".
/// This function is distinct from format_amplitude because tick positions are
/// generated as display-unit doubles, not converted from 10-bit samples.
std::string format_tick_label(double display_value, AmplitudeDisplayUnit unit);

} // namespace orc
```

**Task 3.4 — Create `orc/gui/amplitude_unit_helpers.h`**

A thin Qt wrapper over the common functions. Depends on `<QString>` only:

```cpp
namespace orc::gui {

/// Qt-typed wrappers — delegates to orc::format_amplitude / orc::amplitude_unit_suffix.
inline QString format_amplitude_q(int32_t sample10, const orc::OrcSourceParameters& p,
                                   orc::AmplitudeDisplayUnit unit) {
  return QString::fromStdString(
      orc::format_amplitude(sample10, p.blanking_level, p.white_level, p.system, unit));
}

/// Qt wrapper for tick-label formatting. display_value is already in display-unit terms
/// (produced by snap_ceil); delegates to orc::format_tick_label.
inline QString format_tick_label_q(double display_value, orc::AmplitudeDisplayUnit unit) {
  return QString::fromStdString(orc::format_tick_label(display_value, unit));
}

inline QString amplitude_unit_suffix_q(orc::AmplitudeDisplayUnit unit) {
  return QString::fromStdString(orc::amplitude_unit_suffix(unit));
}

} // namespace orc::gui
```

All GUI call sites use the `_q` helpers; no logic lives in this file.

**Task 3.5 — Remove duplicate conversion code from GUI headers**

After the utilities above exist:
- Remove the inline `cvbs_sample_to_mv()` and `active_video_mv()` functions from
  `orc/gui/framescopedialog.h`; replace call sites in `framescopedialog.cpp`,
  `waveformmonitorwidget.cpp`, and `frametimingwidget.cpp` with the new common functions.
- Remove the local `ire_to_mv` constants from `linescopedialog.cpp` and
  `frametimingwidget.cpp`.

Acceptance criterion: no duplicate conversion logic remains in GUI; all builds pass.

---

## Phase 4 — Project properties UI: unit selector

**Goal:** the user can view and change the amplitude display unit in **Project Properties**.

**Task 4.1 — Add unit selector to `ProjectPropertiesDialog`**

In `orc/gui/projectpropertiesdialog.h`, add:
- `orc::AmplitudeDisplayUnit amplitudeUnit() const;`
- `void setAmplitudeUnit(orc::AmplitudeDisplayUnit unit);`
- Private member: `QComboBox* amplitude_unit_combo_;`

In `orc/gui/projectpropertiesdialog.cpp`, add the combo box below the description field
with items: `"IRE"`, `"mV (millivolts)"`, `"10-bit samples"`. Map to
`orc::AmplitudeDisplayUnit::IRE`, `Millivolts`, `Samples10Bit`.

**Task 4.2 — Wire the dialog into the main window**

In `MainWindow::onEditProject()` (`mainwindow.cpp`):
- Before `dialog.exec()`: populate with `presenter->getAmplitudeUnit()`.
- After `QDialog::Accepted`: call `presenter->setAmplitudeUnit(dialog.amplitudeUnit())`
  then call a new private helper `propagateAmplitudeUnit()`.

Acceptance criterion: changing the unit in Project Properties saves with the project; the
project file contains the correct `amplitude_unit` key; reopening the project restores the
selection.

**Task 4.3 — Implement live propagation of unit changes to all open dialogs**

Add `void MainWindow::propagateAmplitudeUnit()` called whenever the unit changes (from
Project Properties or after loading a project). This is the single point that pushes the
new unit to every open dialog.

The `MainWindow` has direct ownership of some dialogs and indirect ownership of others via
`preview_dialog_`:

```
MainWindow
├── quality_metrics_dialog_        → setAmplitudeUnit()
├── hints_dialog_                  → setAmplitudeUnit()
├── burst_level_analysis_dialogs_  → iterate map, setAmplitudeUnit() on each
└── preview_dialog_                → forwardAmplitudeUnit() (new method, see below)
        ├── frame_scope_dialog_        → setAmplitudeUnit()
        ├── frame_timing_dialog_       → setAmplitudeUnit() on its WaveformMonitorWidget
        └── waveform_monitor_dialog_   → setAmplitudeUnit() forwarded to WaveformMonitorWidget
```

`propagateAmplitudeUnit()` implementation sketch:

```cpp
void MainWindow::propagateAmplitudeUnit() {
  // IProjectPresenter returns orc::AmplitudeDisplayUnit directly — no conversion needed.
  const orc::AmplitudeDisplayUnit unit = project_.presenter()->getAmplitudeUnit();
  if (quality_metrics_dialog_) quality_metrics_dialog_->setAmplitudeUnit(unit);
  if (hints_dialog_)           hints_dialog_->setAmplitudeUnit(unit);
  for (auto& [id, dlg] : burst_level_analysis_dialogs_)
    if (dlg) dlg->setAmplitudeUnit(unit);
  if (preview_dialog_)         preview_dialog_->forwardAmplitudeUnit(unit);
}
```

Add `void PreviewDialog::forwardAmplitudeUnit(orc::AmplitudeDisplayUnit unit)`:

```cpp
void PreviewDialog::forwardAmplitudeUnit(orc::AmplitudeDisplayUnit unit) {
  if (frame_scope_dialog_)        frame_scope_dialog_->setAmplitudeUnit(unit);
  if (frame_timing_dialog_)       frame_timing_dialog_->setAmplitudeUnit(unit);
  if (waveform_monitor_dialog_)   waveform_monitor_dialog_->setAmplitudeUnit(unit);
}
```

**Re-render vs label-only behaviour per dialog:**

Each dialog's `setAmplitudeUnit()` stores the unit and then either refreshes labels or
triggers a full re-render:

| Dialog / widget | Stored data? | Action on `setAmplitudeUnit()` |
|---|---|---|
| `QualityMetricsDialog` | No (labels only) | Update displayed text labels |
| `HintsDialog` | No (labels only) | Update displayed text labels |
| `FrameScopeDialog` | Yes — sample vectors from `setFrameLineSamples()` | Call `updatePlotData()` to re-convert and re-plot in new unit |
| `WaveformMonitorWidget` | Yes — accumulated histogram (re-accumulates from stored `composite_samples_` / `y_samples_` in `WaveformMonitorDialog`) | `WaveformMonitorDialog::setAmplitudeUnit()` forwards to widget; widget calls `update()` to repaint with new Y scale |
| `FrameTimingDialog` / `FrameTimingWidget` | Yes — stored field samples | Trigger repaint with new Y-axis tick intervals and unit labels |
| `BurstLevelAnalysisDialog` | Yes — stored 10-bit amplitude series (after Phase 0 Task 0.1) | Re-convert all stored 10-bit data points to the new unit and replot; update Y-axis title |

`BurstLevelAnalysisDialog` stores its historical data points as 10-bit amplitude values
(following Phase 0 Task 0.1). On `setAmplitudeUnit()` it must re-convert every
accumulated point from 10-bit to the new display unit, clear the plot, and re-add all
points. Conversion requires `VideoParametersView` (blanking_level, white_level, system):
store the most recently received `VideoParametersView` as a member, following the same
pattern used by `WaveformMonitorDialog::setData()` and
`FrameScopeDialog::setFrameLineSamples()`. The burst amplitude conversion is:
`display_value = samples10_to_display(blanking + amplitude_10bit, blanking, white, system, unit)`
where `amplitude_10bit` is the stored peak burst amplitude in 10-bit sample domain units.

Also call `propagateAmplitudeUnit()` immediately after a project is loaded (in
`MainWindow::loadProject()` or equivalent) so that dialogs opened before a project reload
reflect the loaded project's unit setting.

Acceptance criterion: with `FrameScopeDialog`, waveform monitor, and burst level analysis
dialogs all open simultaneously, changing the unit in Project Properties causes all three
to update within the same UI event without requiring the user to close and reopen them.

---

## Phase 5 — Waveform and scope dialogs

**Goal:** `FrameScopeDialog`, `WaveformMonitorWidget`, and `FrameTimingWidget` display
Y-axis, markers, and cursor tooltips in the project unit. Each dialog exposes
`setAmplitudeUnit()` as required by the propagation in Phase 4, Task 4.3.

Each dialog / widget follows the same pattern:
1. Add `void setAmplitudeUnit(orc::AmplitudeDisplayUnit unit)`.
2. Replace hard-coded mV or IRE strings with calls to `amplitude_unit_helpers.h`.
3. Re-render stored data when the unit changes (see the re-render requirements in
   Phase 4, Task 4.3).
4. Apply the tick rounding rules from Phase 3, Task 3.2: tick mark positions must be
   exact integer multiples of `amplitude_major_tick()` / `amplitude_minor_tick()`; signal
   level marker lines stay at their exact converted position and may show one decimal place.

**Task 5.1 — `FrameScopeDialog`**

`FrameScopeDialog` replaces the old `LineScopeDialog` and handles both line and frame scope
views (confirmed in `previewdialog.cpp`: "Create frame scope dialog (replaces
LineScopeDialog)").

`orc/gui/framescopedialog.cpp/h`:
- Add `void setAmplitudeUnit(orc::AmplitudeDisplayUnit unit)` — stores the unit and calls
  `updatePlotData()` to re-convert and re-plot all stored samples.
- Replace Y-axis title `"mV (millivolts)"` (line 107) with the unit label from
  `amplitude_unit_suffix_q()`.
- Replace the hardcoded initial `setAxisRange(Qt::Vertical, -350, 950)` (line 108 —
  currently mV) with a range derived from `amplitude_display_range()`.
- Update the dynamic `setAxisRange` call (line 526) to use converted `y_min` / `y_max`
  in the selected unit.
- No secondary IRE axis exists in `FrameScopeDialog` — `setSecondaryYAxisEnabled(false)`
  is already present (line 533). No action required.
- Y-axis tick marks: use `amplitude_major_tick()` / `amplitude_minor_tick()` for interval
  sizes so they land on clean integers (e.g., 10 IRE, not 14.3 IRE). Tick positions
  computed with `snap_ceil(y_min, step)` — never convert mV tick positions directly.
- Signal level marker labels (blanking, sync tip, black, white, peak): stay at the exact
  converted position; format with one decimal place for IRE/mV, no decimal for 10-bit.
- Update cursor tooltip `"mV: %1  10-bit: %2"` → `format_amplitude_q()` (one decimal for
  IRE/mV, integer for 10-bit).

**Task 5.2 — `WaveformMonitorDialog` and `WaveformMonitorWidget`**

`WaveformMonitorDialog` wraps `WaveformMonitorWidget` and stores the raw sample vectors
(`composite_samples_`, `y_samples_`, `c_samples_`) so it can re-render when the channel
selector changes — the same stored data is used for unit changes.

`orc/gui/waveformmonitordialog.cpp/h`:
- Add `void setAmplitudeUnit(orc::AmplitudeDisplayUnit unit)` — stores the unit, forwards
  it to `monitor_widget_`, and calls `monitor_widget_->update()` to repaint.

`orc/gui/waveformmonitorwidget.cpp/h`:
- Add `void setAmplitudeUnit(orc::AmplitudeDisplayUnit unit)`.
- The Y-axis title is painted directly (`painter.drawText(..., "mV")`, line 399) — not via
  `setAxisTitle`. Replace the literal `"mV"` with `amplitude_unit_suffix_q()`.
- Replace the hardcoded `y_min_mv_` / `y_max_mv_` range variables with unit-aware
  equivalents computed via `amplitude_display_range()`.
- Tick lines (lines 380–389 and 407–409): replace the hardcoded `tick_step = 100.0`
  with `amplitude_major_tick(unit)` so grid lines land on clean integers in all units.
  Use `snap_ceil(y_min, step)` for the first tick — do not convert existing mV positions.
- Update signal level marker label strings (e.g., `"Blanking (0 mV)"`, line 504) via
  `format_amplitude_q()` with one decimal for IRE/mV, integer for 10-bit.

**Task 5.3 — `FrameTimingDialog` / `FrameTimingWidget`**

`orc/gui/frametimingwidget.cpp/h`:
- Add `void setAmplitudeUnit(orc::AmplitudeDisplayUnit unit)` — stores the unit and calls
  `update()` to repaint with new scale.
- The Y-axis tick labels are painted directly (not via `setAxisTitle`). Replace the `" mV"`
  suffix (line 469) with `amplitude_unit_suffix_q()`.
- The tick interval is currently hardcoded to `grid_step = 50.0` mV / `label_step = 100.0`
  mV (lines 428–431 in `frametimingwidget.cpp`). Replace with
  `amplitude_minor_tick(unit)` and `amplitude_major_tick(unit)` respectively, and compute
  `first_tick = snap_ceil(min_value, grid_step)`. This gives 5/10 IRE, 50/100 mV, or
  128/256 sample steps — never a fractional position like 14.3 IRE.
- Label format at major ticks: integer only (`"10 IRE"`, `"100 mV"`, `"256"`). The
  existing `static_cast<int>(mv_value)` (line 469) achieves this for mV; replace with
  `format_tick_label_q(tick_value, unit)` for all three units — this function always
  produces an integer label regardless of unit.
- Replace local `ire_to_mv` constant with `active_video_mv()`.
- Signal level markers (blanking, black, white etc.): kept at their exact converted
  position; label with one decimal for IRE/mV, no decimal for 10-bit.

Add a forwarding `setAmplitudeUnit()` to `FrameTimingDialog` if `MainWindow` holds a
pointer to the dialog wrapper rather than the widget directly.

**Task 5.4 — `BurstLevelAnalysisDialog`**

`orc/gui/burstlevelanalysisdialog.cpp/h`:

After Phase 0 Task 0.1, `addDataPoint()` receives a `double` 10-bit amplitude value (not
IRE). The dialog needs `VideoParametersView` (blanking_level, white_level, system) to
convert this to any display unit.

Follow the same pattern used by `WaveformMonitorDialog::setData()` and
`FrameScopeDialog::setFrameLineSamples()`: accept an
`std::optional<orc::presenters::VideoParametersView>` parameter alongside data, and cache
it as a member `cached_video_params_`.

The coordinator callback in `mainwindow_coordinator_callbacks.cpp` (currently at the
`dialog->addDataPoint(stats.frame_number, stats.median_burst_10bit)` call site after Phase 0) already has
access to `OrcSourceParameters` or can obtain `VideoParametersView` from the presenter —
follow the exact pattern used by the nearest callback that feeds similar dialogs.

Changes:
- Add `std::optional<orc::presenters::VideoParametersView> cached_video_params_` member.
- Update `addDataPoint(int32_t frameNumber, double burstLevel10bit,
    const std::optional<orc::presenters::VideoParametersView>& video_params)`.
  Cache `video_params` and store the raw 10-bit amplitude value in the data series.
- Add `void setAmplitudeUnit(orc::AmplitudeDisplayUnit unit)` — re-converts all stored
  10-bit values and replots; calls `update()`.
- In `paintEvent` / plot rendering, use `samples10_to_display(blanking + value_10bit,
  blanking, white, system, unit)` for each data point.
- Update the Y-axis title via `amplitude_unit_suffix()`.

Note: burst amplitude conversion uses `blanking + amplitude_10bit` as the sample argument
to `samples10_to_display` (the amplitude is relative to blanking, so we add blanking to
normalise it as a signal level for the conversion formula).

---

## Phase 6 — Parameter and metric dialogs

**Goal:** `MaskLineConfigDialog`, `HintsDialog`, and `QualityMetricsDialog` use the project unit.

**Task 6.1 — `MaskLineConfigDialog`**

`orc/gui/masklineconfigdialog.cpp/h`:

After Phase 0 Task 0.2, the mask stage stores a 10-bit sample level (`"maskSampleLevel"`)
and there is no longer an `ire_to_sample()` helper. The dialog must become unit-aware.

- The `mask_ire_spinbox_` stores the mask level in IRE — rename to `mask_level_spinbox_`.
- On construction / `setAmplitudeUnit()`, adjust the spinbox range and suffix:
  - IRE: range [−40.0, 120.0], suffix `" IRE"`, 1 decimal place
  - mV: range [−280.0, 840.0], suffix `" mV"`, 1 decimal place
  - 10-bit: range [0, 1023], no suffix, 0 decimal places
- Update preset labels dynamically (e.g., `"Black (0 IRE)"` / `"Black (0 mV)"` /
  `"Black (0)"`).
- The stage now stores a 10-bit integer. The dialog reads the spinbox display value and
  calls `display_to_samples10(spinbox_value, blanking, white, system, unit)` to convert
  to the 10-bit stage parameter. Requires `VideoParametersView` — follow the same
  construction-time or setter pattern used by `FrameScopeDialog` (pass
  `OrcSourceParameters` from the caller when the dialog is constructed or populated).

**Task 6.2 — `HintsDialog` and `QualityMetricsDialog`**

`orc/gui/hintsdialog.cpp/h`:
- Add `setAmplitudeUnit()`.
- Labels currently read `"Blanking Level (10-bit):"` etc. Change to use
  `amplitude_unit_suffix()` for the parenthetical.
- Values currently displayed as raw 10-bit integers; convert with `format_amplitude()`.

`orc/gui/qualitymetricsdialog.cpp/h`:
- The burst level display uses `"%1 IRE"`. After Phase 0, the presenter DTO
  (`metrics_presenter.h`) carries `burst_level_10bit` (a 10-bit amplitude). Replace the
  `"%1 IRE"` string with `format_amplitude_q(blanking + burst_level_10bit, ...)` using
  the cached video parameters, applying the same burst-amplitude normalisation used in
  Task 5.4. Requires `VideoParametersView` — follow the same pattern used by
  `BurstLevelAnalysisDialog`.

---

## Phase 7 — Tests

**Task 7.1 — Unit tests for `amplitude_conversion.h`**

Add `orc-tests/core/unit/common/amplitude_conversion_test.cpp`.

Required coverage (label: `unit`):
- `samples10_to_mv` round-trip against spec values: blanking (256 PAL) → 0 mV,
  white (844 PAL) → 700 mV; NTSC equivalents.
- `samples10_to_ire` round-trip: blanking → 0 IRE, white → 100 IRE.
- `ire_to_samples10` with NTSC 7.5 IRE pedestal: result is the blanking sample (0 IRE
  maps to blanking, black is at +7.5 IRE).
- `display_to_samples10` round-trip for all three units.
- `default_amplitude_unit` returns IRE for NTSC, Millivolts for PAL and PAL-M.
- `amplitude_major_tick` / `amplitude_minor_tick`: verify that for IRE the major step is
  10 and minor is 5; for mV major is 100 and minor is 50; for 10-bit major is 256 and
  minor is 128.
- `snap_ceil`: verify that a fractional IRE value (e.g., 14.3) snaps up to the next
  multiple of 10 (→ 20) and that an exact multiple (10.0) is returned unchanged; repeat
  for mV and 10-bit step sizes. Verify no fractional tick positions are produced.

**Task 7.2 — Presenter-layer unit test**

Extend or add to `orc-tests/core/unit/presenters/` a test for
`ProjectPresenter::getAmplitudeUnit()` / `setAmplitudeUnit()` using the existing mock
infrastructure. Verify that:
- Setting a unit marks the project modified.
- The unit is preserved across a serialize / deserialize round-trip (mock file I/O using
  `load_project_from_yaml` / `serialize_project_to_yaml`).

**Task 7.3 — GUI tier-3 smoke test for `ProjectPropertiesDialog`**

Add to `orc-tests/gui/unit/` (label: `gui-widget`):
- Instantiate `ProjectPropertiesDialog` with `QT_QPA_PLATFORM=offscreen`.
- Set each of the three unit values via `setAmplitudeUnit()`; read back via
  `amplitudeUnit()`; assert round-trip.

**Task 7.4 — GUI tier-3 smoke test for `MaskLineConfigDialog` unit switch**

Add to `orc-tests/gui/unit/` (label: `gui-widget`):
- Instantiate `MaskLineConfigDialog`, call `setAmplitudeUnit()` for each unit, verify that
  the spinbox suffix and preset label strings update correctly.

---

## Key conversion constants

For reference during implementation:

| System | Active video (mV) | mV / IRE | blanking (10-bit) | white (10-bit) |
|---|---|---|---|---|
| PAL | 700.0 | 7.0 | 256 | 844 |
| NTSC | 714.3 | 7.143 | 256 | 844 |
| PAL-M | 714.3 | 7.143 | 256 | 844 |

Spec authority:
- EBU Tech. 3280-E §1.1: PAL 700 mV peak-to-peak luma.
- SMPTE 170M-2004 §11.4: NTSC 7.143 mV/IRE × 100 IRE = 714.3 mV.
- ITU-R BT.1700-1 Annex 1 Part B: PAL-M follows NTSC amplitude.

Note: NTSC has a 7.5 IRE setup (pedestal) where picture-black sits above blanking.
`default_amplitude_unit` and all conversion helpers must use blanking (0 IRE / 0 mV) as
the reference level, not picture-black. The 7.5 IRE pedestal is accounted for by
`black_level` being above `blanking_level` in the stored `OrcSourceParameters`.
