# Plugin SDK Header Inventory and Allowlist

Deliverable for Phase 4 of the
[plugin SDK hardening plan](./plugin-sdk-hardening-plan.md) (Tasks 4.1 and
4.2). This document inventories every non-SDK header included by code under
`orc/plugins/stages/`, assigns each a disposition, and defines the allowlisted
SDK contract surface that the Phase 5 enforcement gate will implement.

Method: every `#include` in `orc/plugins/stages/**/*.{h,cpp}` was resolved the
way the compiler resolves it (quoted includes against the including file's
directory first, then the plugin target's include path in CMake order), so
each entry below names the file that is actually included, not just the
spelling used.

## 1. Inventory (Task 4.1)

Usage count = number of `#include` lines across all plugin sources that
resolve to the header. Dispositions:

- **(a) contract** — belongs in the SDK contract; re-homed (moved, not
  copied) into `orc/sdk/include/orc/stage/`.
- **(b) via SDK** — already reachable through an existing SDK header; the
  direct include is removed or replaced.
- **(c) private** — host-only; plugin usage migrated away.

### orc-core `orc/core/include/`

| Header | Uses | Disposition |
|---|---|---|
| `logging.h` | 74 | (a) contract — re-homed as `<orc/stage/logging.h>`; see §3.1 |
| `video_frame_representation.h` | 28 | (a) contract |
| `stage_parameter.h` | 21 | (a) contract |
| `preview_helpers.h` | 15 | (a) contract |
| `observation_context_interface.h` | 10 | (a) contract |
| `field_id.h` | 10 | (a) contract (consolidates the near-duplicate copy in `orc/common/include/`) |
| `frame_line_util.h` | 8 | (a) contract |
| `frame_descriptor.h` | 6 | (a) contract |
| `preview_renderer.h` | 5 | (c) private — host rendering engine; plugins only wanted `PreviewImage`, which lives in `orc_rendering.h`; direct includes removed |
| `lru_cache.h` | 5 | (a) contract — generic utility container; see deviation note §3.2 |
| `frame_id.h` | 5 | (a) contract (consolidates the near-duplicate copy in `orc/common/include/`) |
| `dropout_run.h` | 5 | (a) contract |
| `artifact.h` | 5 | (a) contract |
| `buffered_file_io.h` | 4 | (c) private — concrete writer implementation; plugins migrated to `IStageServices` writer factories |
| `file_io_interface.h` | 3 | (a) contract — `IFileWriter<T>` mocking seam used by sink dependency interfaces |
| `eia608_decoder.h` | 3 | (a) contract — closed-caption decoding utility used by cc/ffmpeg sinks (implementation stays in orc-core) |
| `dropout_util.h` | 3 | (a) contract |
| `video_metadata_types.h` | 2 | (a) contract |
| `stage_custom_preview_renderer.h` | 1 | (a) contract (preview mixin interface) |
| `observation_schema.h` | 1 | (a) contract |
| `dropout_decision.h` | 1 | (a) contract |
| `colour_preview_conversion.h` | 1 | (a) contract (implementation stays in orc-core) |

Also re-homed because SDK headers or moved headers depend on them (no direct
plugin includes): `observation_context.h` (required by `stage.h`/DAGStage
signatures), `preview_stage_types.h`, `stage_preview_capability.h`,
`colour_preview_provider.h` (required by `orc_stage_preview.h`).

### orc-core `orc/core/stages/`

| Header | Uses | Disposition |
|---|---|---|
| `triggerable_stage.h` | 22 | (a) contract |
| `analysis_sink_results.h` | 3 | (a) contract |
| `stage.h` (via SDK umbrella) | — | (a) contract — DAGStage base class |

`stage_factories.h` / `stage_factories_interface.h` remain host-only.

### orc-core `orc/core/observers/`

| Header | Uses | Disposition |
|---|---|---|
| `closed_caption_observer.h` | 3 | (a) contract (provisional; see §3.3) |
| `white_snr_observer.h` | 1 | (a) contract (provisional) |
| `burst_level_observer.h` | 1 | (a) contract (provisional) |
| `black_psnr_observer.h` | 1 | (a) contract (provisional) |
| `biphase_observer.h` | 1 | (a) contract (provisional) |
| `observer.h` | — | (a) contract (base class of the above) |

Re-homed under `orc/sdk/include/orc/stage/observers/`. Observers not used by
plugins (`colour_frame_phase`, `field_quality`, `fm_code`, `white_flag`)
stay in `orc/core/observers/`.

### orc-core `orc/core/analysis/`

| Header | Uses | Disposition |
|---|---|---|
| `vectorscope/vectorscope_analysis.h` | 1 | (c) private — the single consumer (`chroma_sink_stage.cpp`) used only the static `extractFromComponentFrame()` helper, which itself depended on a plugin-owned type (`ComponentFrame`), i.e. a core→plugin reverse dependency. The extraction helper moved into the chroma sink plugin (`sinks/common/decoders/vectorscope_extract.{h,cpp}`) and was removed from orc-core. |

### orc-common `orc/common/include/`

| Header | Uses | Disposition |
|---|---|---|
| `cvbs_signal_constants.h` | 23 | (a) contract |
| `node_type.h` | 12 | (a) contract |
| `common_types.h` | 12 | (a) contract |
| `error_types.h` | 7 | (a) contract |
| `parameter_types.h` (via SDK umbrella) | — | (a) contract |
| `node_id.h` (via `orc_rendering.h`) | — | (a) contract |

Implementation files (`node_id.cpp`, `parameter_util.cpp`, `node_type.cpp`)
stay in their owning modules; only the headers move. `amplitude_conversion.h`,
`line_numbering.h`, `crash_handler.h` and the application logging header
`logging.h` remain in `orc/common/include/` (not plugin-facing).

The `field_id.h` / `frame_id.h` copies that existed in **both**
`orc/common/include/` and `orc/core/include/` (identical except for the
`Module:` comment — a latent ODR footgun) are consolidated into the single
re-homed `orc/stage/` copy.

### orc-view-types `orc/view-types/`

| Header | Uses | Disposition |
|---|---|---|
| `orc_source_parameters.h` | 16 | (a) contract — `VideoFrameRepresentation` (contract) embeds it |
| `orc_rendering.h` (via SDK preview header) | — | (a) contract (`PreviewImage`, render result types) |
| `orc_preview_types.h` (via SDK preview header) | — | (a) contract |
| `orc_preview_carriers.h` (via SDK preview header) | — | (a) contract |
| `orc_vectorscope.h` (via `orc_preview_carriers.h`) | — | (a) contract |

Remaining view-types headers (`orc_preview_views.h`, `orc_analysis.h`,
`orc_histogram.h`, `orc_cvbs_source_parameters.h`) stay in `orc/view-types/`
(presenter/GUI-facing only).

### Plugin-local and plugin-shared headers

All quoted includes that resolve inside the including plugin's own directory
tree (~150 headers: `plugin.h`, stage headers, `efm-lib/`, `ac3rf-lib/`,
`sinks/common/`, deps interfaces, …) are **plugin-local** and always
permitted.

Two sanctioned **plugin-shared** cases exist and are permitted by the
allowlist:

- `orc/plugins/stages/sinks/common/` — shared implementation for the raw and
  FFmpeg video sink plugins (declared via each sink's CMake include path).
- `orc/plugins/stages/tbc_source/tbc_metadata_types.h` — shared ld-decode
  field-metadata structures used by `ld_sink` (declared in
  `ld_sink/CMakeLists.txt`).

### Standard library and third-party headers

- C/C++ standard library: always permitted (578 includes).
- `fmt` / `spdlog`: permitted (the SDK logging surface itself uses them).
- Plugin-specific third-party libraries are permitted when the plugin
  declares the dependency in its own CMakeLists: FFmpeg (`libav*`,
  `libswscale`) in the ffmpeg video sink, `fftw3.h` (Transform PAL),
  `sqlite3.h` and `soxr.h` (tbc_source, ld_sink, cvbs_source), `ezpwd/*`
  (efm_sink), and platform headers (`sys/*`, `unistd.h`, `fcntl.h`, `io.h`,
  `share.h`, `windows.h`).

## 2. Allowlisted SDK contract surface (Task 4.2)

Plugins may include, and the Phase 5 gate will permit, exactly:

1. **`<orc/plugin/...>`** — the plugin ABI/services surface (unchanged):
   `orc_plugin_sdk.h`, `orc_plugin_abi.h`, `orc_stage_api.h`,
   `orc_stage_runtime.h`, `orc_stage_preview.h`, `orc_stage_services.h`,
   `orc_stage_tooling.h`, `orc_plugin_services.h`,
   `orc_plugin_services_helpers.h`.
2. **`<orc/stage/...>`** — the re-homed contract tree:
   - Stage model: `stage.h`, `triggerable_stage.h`, `stage_parameter.h`,
     `parameter_types.h`, `node_type.h`, `node_id.h`, `artifact.h`,
     `analysis_sink_results.h`.
   - Frame/signal model: `video_frame_representation.h`,
     `video_metadata_types.h`, `frame_descriptor.h`, `frame_id.h`,
     `field_id.h`, `frame_line_util.h`, `common_types.h`,
     `cvbs_signal_constants.h`, `orc_source_parameters.h`, `dropout_run.h`,
     `dropout_util.h`, `dropout_decision.h`.
   - Observation model: `observation_context.h`,
     `observation_context_interface.h`, `observation_schema.h`,
     `observers/observer.h`, `observers/biphase_observer.h`,
     `observers/black_psnr_observer.h`, `observers/burst_level_observer.h`,
     `observers/closed_caption_observer.h`,
     `observers/white_snr_observer.h`.
   - Preview contract: `stage_preview_capability.h`,
     `stage_custom_preview_renderer.h`, `colour_preview_provider.h`,
     `colour_preview_conversion.h`, `preview_helpers.h`,
     `preview_stage_types.h`, `orc_preview_types.h`,
     `orc_preview_carriers.h`, `orc_rendering.h`, `orc_vectorscope.h`.
   - Utilities: `logging.h`, `lru_cache.h`, `file_io_interface.h`,
     `eia608_decoder.h`, `error_types.h`.
3. **Plugin-local headers** — any header inside the including plugin's own
   directory, plus the two sanctioned plugin-shared cases in §1.
4. **Standard library headers.**
5. **Third-party headers** — `fmt`/`spdlog` unconditionally; other
   third-party libraries when declared by the plugin's own CMake target.

Host-only headers that stay in `orc/core/` and are **forbidden** to plugins
include (non-exhaustive): `dag_executor.h`, `dag_frame_renderer.h`,
`dag_serialization.h`, `preview_renderer.h`, `preview_view_registry.h`,
`stage_registry.h`, `stage_plugin_loader.h`, `stage_plugin_registry.h`,
`plugin_remote_loader.h`, `plugin_safe_call.h`, `project.h`,
`project_to_dag.h`, `pipeline_validator.h`, `observation_cache.h`,
`observer_config.h`, `buffered_file_io.h`, `stage_factories*.h`, the
`orc/core/analysis/` tree, and the vbi decoder internals (`vbi_decoder.h`,
`vbi_types.h`, `vbi_utilities.h`).

SDK umbrella changes made in this phase:

- `orc_stage_runtime.h` no longer includes `dag_executor.h` (which exposed
  the host DAG executor, LRU cache, and scheduling internals to every
  plugin); it now includes `<orc/stage/stage.h>`,
  `<orc/stage/observation_context.h>` and
  `<orc/stage/triggerable_stage.h>`.
- `orc_stage_api.h` and `orc_stage_preview.h` include their dependencies by
  `<orc/stage/...>` path only.

## 3. Decision notes and deviations

### 3.1 Logging surface

The sanctioned plugin logging surface is the re-homed
`<orc/stage/logging.h>` (the `ORC_LOG_*` macro family backed by the host's
spdlog logger, linked from orc-core like every other contract symbol). The
services-table path (`ORC_PLUGIN_LOG_*` in
`<orc/plugin/orc_plugin_services_helpers.h>`) remains available and is the
recommended surface for out-of-tree plugins that want to avoid a direct
spdlog dependency. Rewriting the ~820 in-tree `ORC_LOG_*` call sites to the
services macros was evaluated and rejected for this phase: it is a large,
behaviour-affecting change (logs would silently no-op wherever stages run
without plugin services, e.g. direct in-process test construction) with no
boundary-hygiene gain while plugins link orc-core anyway.

The application logging header `orc/common/include/logging.h`
(`get_app_logger()`) is unrelated, stays put, and remains the GUI/CLI
surface; the previous situation where two different `logging.h` files could
be picked up depending on include-path order is resolved by the re-homing.

### 3.2 `lru_cache.h`

The hardening plan listed `lru_cache.h` as host-only, but four plugins
(`cvbs_source`, `dropout_correct`, `stacker`, `tbc_source`) legitimately use
it for frame caching. It is a self-contained generic container with no host
coupling, so it is allowlisted as `<orc/stage/lru_cache.h>` rather than
forcing four plugins to vendor copies. Deviation recorded here per the plan's
Task 4.1 acceptance criteria.

### 3.3 Observer headers (provisional contract)

The analysis sinks instantiate concrete host observers (`WhiteSNRObserver`,
`BlackPSNRObserver`, `BurstLevelObserver`, `BiphaseObserver`,
`ClosedCaptionObserver`) to (re)compute observations at trigger time. Their
headers are re-homed so the allowlist can be enforced, but they are flagged
**provisional**: a cleaner long-term design is a host-side observation
service (candidate for a later SDK expansion), after which these headers
would return to host-only status.

### 3.4 `IStageServices` expansion

`AudioSinkStage` wrote 16-bit signed PCM through the private
`BufferedFileWriter<int16_t>`. Migrating it to services required a genuinely
missing capability, so `IStageServices` gained
`create_buffered_file_writer_int16()` (with `IFileWriterInt16`), appended
after the existing factories per the append-only services convention. Mocked
unit tests and the plugin-sdk.md host-services documentation were updated in
the same change (AGENTS.md §9 sync table).

### 3.5 API compatibility

Re-homing moves headers without changing declarations: it is not an API
break, and `plugin_api_version` is unchanged. The `IStageServices` addition
is append-only and is likewise not a break under the exact-match version
policy. All ABI-affecting changes remain batched for Phase 6.
