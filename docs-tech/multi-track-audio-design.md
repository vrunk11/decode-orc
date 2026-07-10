# Multi-Track Audio Design

Design for carrying up to 16 stereo audio tracks through the decode-orc
stage/DAG pipeline, including EFM-decoded digital audio, bilingual channel
routing, per-track sync adjustment, and multi-track embedding at the sinks.

---

## 1. Motivation and Current State

The CVBS file format specification permits up to 16 stereo audio tracks
([spec — Audio Data](cvbs-file-format-specification/docs/index.md)), but the
runtime implements exactly one:

- The `VideoFrameRepresentation` (VFR) SDK contract
  (`orc/sdk/include/orc/stage/video_frame_representation.h`) exposes a single
  implicit stereo track: `has_audio()`, `audio_locked()`,
  `get_audio_sample_count(FrameID)`, `get_audio_samples(FrameID)` — always
  interleaved stereo `int16_t`, sample rate implied by the video system and
  never carried in the model.
- `tbc_source` always treats the `.pcm` sidecar as a free-running 44100 Hz
  stream and unconditionally converts it to a frame-locked track (lazy
  NTSC/PAL-M resample-and-segment; PAL slices at the same rate). There is no
  way to declare that the sidecar is already frame-locked, nor to keep it
  free-running.
- `cvbs_source` reads only `<basename>_audio_00.wav` and refuses to serve
  free-running audio per-frame; `cvbs_sink` writes only track 00 and only when
  the audio is frame-locked. There is no way to convert a free-running track
  to frame-locked anywhere in the pipeline.
- `video_sink`'s FFmpeg backend hard-codes one stereo 44100 Hz stream
  (`sinks/common/ffmpeg_output_backend.cpp`, `setupAudioEncoder()`).
- EFM digital audio can only leave the pipeline via `efm_sink` as a standalone
  WAV; it can never re-enter the pipeline as a track.
- LaserDisc bilingual (dual-mono) analogue audio is carried verbatim as one
  stereo pair; the VBI sound-mode metadata is informational only.

Goals of this design:

1. Track-aware audio in the core VFR model (up to 16 stereo tracks).
2. Sources expose every container-provided track.
3. EFM decode as a transform stage whose output becomes a free-running track
   (never downsampled).
4. External WAV files attachable as tracks via an import transform.
5. Channel-routing transform for bilingual/dual-mono material.
6. Per-track sync (position) adjustment transform.
7. Source-side timing control: declare whether a TBC `.pcm` sidecar is
   free-running or already frame-locked, and optionally resample
   free-running source audio (TBC or CVBS) into frame-locked tracks.
8. Track selection at `audio_sink`; multi-track embedding at `video_sink`;
   full multi-track read/write at `cvbs_source`/`cvbs_sink`.
9. CVBS file format specification extended with per-track metadata.

Decisions already made (with the project owner):

| Decision | Choice |
|----------|--------|
| EFM audio decode placement | Dedicated transform stage; decode pipeline moves to a shared library |
| CVBS spec | Extended with a per-track metadata table; file-level `audio_locked` removed (the spec describes only the current version) |
| External audio | New import transform stage (not source-stage parameters) |
| SDK compatibility | Clean replacement of the single-track VFR accessors; host ABI bump (5 → 6) |

---

## 2. Core Model

### 2.1 Track invariants

Every track, everywhere in the pipeline, is:

- **Stereo**, interleaved `int16_t` pairs (L, R, L, R, …) — matching the CVBS
  spec's only permitted audio encoding. Mono material occupies both channels
  or one channel of a stereo pair; there is no mono track type.
- Identified by **index** `0 … audio_track_count() - 1` within a
  representation. Track order is stable through the DAG; transforms that add
  tracks append.
- Capped at **16 tracks** (`kMaxAudioTracks = 16`), the CVBS container limit.
  The cap is enforced at every track-adding stage, not just at `cvbs_sink`,
  so a DAG that validates also exports.
- Either **frame-locked** or **free-running**, per track:
  - *Frame-locked* — exact integer stereo pairs per video frame
    (PAL 1764 @ 44100 Hz; NTSC/PAL-M 1470 @ 44100000⁄1001 Hz), addressed
    per-`FrameID`. Frame remapping (frame_map, source_align, stacker)
    remaps the audio in lockstep, exactly as today.
  - *Free-running* — a 44100 Hz stream independent of frame timing,
    addressed by stream sample offset. Sample 0 is synchronous with the
    first sample of the representation's first frame (the CVBS spec origin
    convention). Frame-remapping stages remap free-running tracks in the
    **time domain** (§4.5): a mapping that is a contiguous source range
    becomes an exact stream slice; arbitrary mappings stitch nominal
    per-frame time windows. Stages that *combine* sources (stacker) never
    combine free-running tracks (§4.5.3).

### 2.2 New SDK types

New header `orc/sdk/include/orc/stage/audio_track.h`:

```cpp
// Exact rational sample rate in Hz (e.g. 44100/1, 44100000/1001).
struct AudioSampleRate {
  uint32_t num;
  uint32_t den;
};

enum class AudioTrackOrigin : uint8_t {
  ANALOGUE,   // decoder-provided analogue audio (LD analogue, tape linear)
  HIFI,       // tape Hi-Fi stereo
  EFM,        // decoded EFM digital audio
  IMPORTED,   // attached external WAV
  DERIVED,    // produced by a transform (e.g. bilingual split)
  UNKNOWN,
};

struct AudioTrackDescriptor {
  std::string name;            // human-readable, e.g. "Analogue", "EFM digital"
  AudioTrackOrigin origin;
  bool locked;                 // frame-locked vs free-running
  AudioSampleRate sample_rate; // authoritative rate; WAV headers approximate
};

// Stream pair offset corresponding to the START of frame |frame_index| for a
// free-running track, computed with exact 64-bit rational arithmetic:
//   round(frame_index × rate / frame_rate)
// Every stage and sink that maps frames to free-running stream positions MUST
// use this helper so window boundaries agree pipeline-wide (cumulative form:
// per-frame counts are NOT constant — NTSC yields 1471/1472-pair windows).
uint64_t audio_stream_pair_offset(uint64_t frame_index,
                                  AudioSampleRate rate,
                                  VideoSystem system);
```

### 2.3 VFR contract changes (ABI 6)

The four single-track accessors in `video_frame_representation.h` are
**replaced** (no deprecated shims) by:

```cpp
// --- Audio tracks -----------------------------------------------------------

// Number of stereo audio tracks (0 = no audio). Max kMaxAudioTracks.
virtual size_t audio_track_count() const { return 0; }

// Descriptor for track |track|; nullopt when out of range.
virtual std::optional<AudioTrackDescriptor> get_audio_track_descriptor(
    size_t track) const { return std::nullopt; }

// Frame-locked access. Meaningful only when the track's descriptor reports
// locked == true; free-running tracks return 0 / {}.
virtual uint32_t get_audio_sample_count(size_t track, FrameID id) const {
  return 0;
}
virtual std::vector<int16_t> get_audio_samples(size_t track,
                                               FrameID id) const {
  return {};
}

// Free-running (stream) access, in stereo pairs. Meaningful only when
// locked == false; locked tracks return 0 / {}.
virtual uint64_t get_audio_stream_pair_count(size_t track) const { return 0; }
virtual std::vector<int16_t> get_audio_stream_samples(
    size_t track, uint64_t first_pair, uint32_t pair_count) const {
  return {};
}
```

`has_audio()` is retained as a non-virtual convenience
(`audio_track_count() > 0`). The per-file `audio_locked()` accessor is
removed — lock state is per-track.

`VideoFrameRepresentationWrapper` forwards all six accessors to `source_`
(same pattern as today). The header's pass-through contract comment is
updated: a stage that remaps frame IDs must override the **locked** per-frame
accessors; free-running stream accessors forward untouched.

The EFM (`get_efm_samples`) and AC3-RF accessors are unchanged — they carry
undecoded signal data, not audio tracks.

Per SDK rules (AGENTS.md §9), `kStagePluginHostAbiVersion` bumps 5 → 6 and
`docs/technical/plugin-sdk.md` / `plugin-architecture.md` are updated in the
same change.

### 2.4 Why per-frame + stream dual addressing

A single addressing scheme cannot serve both regimes without lying:

- Locked audio has exact per-frame boundaries and must survive frame
  remapping sample-exactly (the existing frame_map/stacker/source_align
  behaviour).
- Free-running audio has no defined per-frame boundary (spec: "per-frame
  audio boundaries cannot be determined from the video frame index alone").
  Sinks that need time alignment compute it:
  `first_pair = round(frame_index / frame_rate × 44100)`.

This also fixes an existing gap: `cvbs_source` currently detects free-running
audio but cannot serve it at all.

---

## 3. Source Stages

### 3.1 Source audio timing: declaration and conversion

Sources handle two orthogonal concerns, each user-controllable:

1. **What the sidecar/container audio actually is** (free-running or
   frame-locked). CVBS files declare this in metadata; TBC sidecars carry no
   such flag, so `tbc_source` gains a declaration parameter (today it always
   assumes free-running).
2. **What the pipeline track should be**. A free-running source track may be
   resampled into a frame-locked track at the source via a conversion
   parameter (SoXR HQ, duration- and sync-preserving, to the preset locked
   rational rate). Frame-locked source audio is never converted back to
   free-running at the source (`audio_sink`'s `free_running_44100` export
   mode covers that need at the boundary).

The free-running↔locked resamplers currently duplicated in
`tbc_source/audio_resampler.*` (`NtscPalMAudioResampler`) and
`audio_sink/free_running_resampler.*` (`FreeRunningAudioResampler`)
consolidate into a shared plugin-side static library
`orc/plugins/stages/common/audio-resample/`, used by tbc_source,
cvbs_source and audio_sink.

### 3.2 tbc_source

- The `.pcm` sidecar becomes **track 0**
  (`{name: "Analogue", origin: ANALOGUE, rate: per lock state}`).
- New parameter `pcm_audio_timing` (`free_running` | `frame_locked`, default
  `free_running`) declares the sidecar's actual timing:
  - `free_running` — the PCM is a 44100 Hz stream (the current assumption).
  - `frame_locked` — the PCM is already at the preset locked rate; samples
    are served directly using the per-field `audio_sample_count` metadata
    offsets, with no resampling.
- New parameter `lock_audio` (bool, default `true`) — when the sidecar is
  declared free-running, resample it into a frame-locked track. The default
  (`free_running` + `lock_audio = true`) reproduces today's behaviour,
  reusing the existing lazy resample-and-segment path
  (`ensure_ntsc_palM_audio()`; PAL is a same-rate segmentation). With
  `lock_audio = false` the PCM is served verbatim as a free-running track
  via the stream accessors. The parameter is hidden when
  `pcm_audio_timing = frame_locked`.
- No other tracks are produced at the source; EFM stays a t-value stream
  until decoded by the transform (§4.1).

### 3.3 cvbs_source

- Probes `<basename>_audio_00.wav` … `_audio_15.wav`; each existing file
  becomes a track, preserving container track numbers as pipeline indices.
- Per-track descriptions and lock modes come from the extended `.meta`
  `audio_track` table (§7) — timing is declared per track, so no declaration
  parameter is needed. All CVBS tracks report `origin: UNKNOWN` (the
  container carries no origin metadata). Tracks without a table row —
  including manual-encoding mode, where no metadata is read — default to
  free-running with `name: "Track NN"`. No backwards compatibility with
  pre-v1.2.0 metadata: only the current spec is read.
- Free-running tracks are now served via the stream accessors
  (44-byte-header WAV, seek to `first_pair × 4` bytes), removing the current
  early-return.
- New parameter `lock_audio` (bool, default `false`) — resample every
  free-running track into a frame-locked track (shared resampler, lazy on
  first access per track). Already-locked tracks are unaffected. The default
  preserves the container's timing as-is.

---

## 4. Transform Stages

All four new transforms are standard `VideoFrameRepresentationWrapper`
decorators: video, dropout hints, EFM, AC3 and non-targeted audio tracks pass
through untouched.

### 4.1 efm_audio_decode

Decodes the VFR's EFM t-value stream to CD audio and appends it as a track:
`{name: "EFM digital audio", origin: EFM, locked: false, rate: 44100/1}`.

- **Decode machinery**: `EfmProcessor` and its `decoders/`, `efm-lib/`,
  `writers/` subtree move from `orc/plugins/stages/efm_sink/` into a shared
  static library `orc/plugins/stages/common/efm-decode/`, linked by both
  `efm_sink` (unchanged behaviour) and this transform. No SDK surface change;
  this stays plugin-side code.
- **Lazy, cached decode**: EFM decode is whole-stream sequential (CIRC
  interleaving spans sectors) and cannot run per-frame. On first audio access
  (guarded by `std::once_flag`, mirroring the tbc_source lazy-resample
  pattern) the stage:
  1. Accumulates t-values across the wrapped VFR's full frame range (the
     two-pass loop currently in `efm_sink_stage_deps.cpp`).
  2. Runs the decode pipeline writing headerless 44.1 kHz stereo PCM to a
     scratch cache file (decoded audio is ~635 MB/hour — disk, not RAM).
  3. Serves `get_audio_stream_samples()` by seeking into that file.
  Video-only preview and DAG validation therefore never pay for the decode.
- **Sample rate is sacrosanct**: the decoded stream is exactly 44100 Hz and
  is never resampled by this stage or by any sink that can represent it.
- **Time origin**: pair 0 of the decoded stream is treated as synchronous
  with the first frame of the transform's input representation. EFM decode
  start-up (sync acquisition) makes this approximate; `audio_align` (§4.4)
  is the corrective tool.
- **Parameters** (subset of efm_sink's decode options): `no_timecodes`
  (early CAV discs), `no_audio_concealment`. This transform is
  **audio-only**: only audio can become a track. EFM *data* content
  (ECMA-130 sectors) continues to be extracted by `efm_sink` in
  `decode_mode = data` (§5.4), which is unchanged by this design. On a
  data disc this transform fails decode and surfaces an error observation;
  it is not applicable there.
- Placed after dropout correction / stacking so it benefits from the best
  available EFM stream (stacker already selects best-source EFM).

### 4.2 audio_track_import

Attaches an external WAV file as a new track.

- **Parameters**: `wav_path` (string, required); `track_name` (string);
  `lock_mode` (`auto` | `locked` | `free_running`, default `auto`).
- Validation: RIFF/WAVE, PCM, stereo, 16-bit (the CVBS-permitted encoding).
  `auto` selects `locked` when the WAV's total pair count equals
  `frame_count × pairs-per-frame` for the project's video standard **and**
  the header rate matches the locked rate; otherwise `free_running`
  (44100 Hz required).
- Appends `{origin: IMPORTED}`; fails validation when the 16-track cap
  would be exceeded.

### 4.3 audio_channel_map

Channel routing for bilingual / dual-mono material and general channel fixes.
One operation per stage instance; chain instances for compound routing.

- **Parameters**: `track` (int, target track); `operation`:
  - `split_dual_mono` — replaces the target with two tracks:
    `"<name> (L)"` = L→both channels, `"<name> (R)"` = R→both channels
    (`origin: DERIVED`). This is the bilingual-LD case and the tape
    linear/Hi-Fi separation case.
  - `left_to_both` / `right_to_both` — in-place mono fill.
  - `swap_channels` — in-place L/R swap.
- Works identically on locked and free-running tracks (pure per-sample
  channel remap of whichever accessor family the track answers).
- `split_dual_mono` enforces the 16-track cap.

### 4.4 audio_align

Per-track sync (position) adjustment to line audio up with video.

- **Parameters**: `track` (int); `offset_ms` (float, ±; positive delays the
  audio relative to video, i.e. inserts lead-in).
- Locked tracks: the offset is converted to whole stereo pairs at the
  track's exact rational rate; `get_audio_samples(track, id)` assembles each
  frame's window from the neighbouring frames' samples, silence-filling past
  either end of the range. Per-frame pair counts are unchanged, so the track
  stays spec-conformant locked audio.
- Free-running tracks: the stream origin shifts — positive offsets prepend
  silence pairs, negative offsets trim from the start;
  `get_audio_stream_pair_count()` adjusts accordingly.
- Multiple instances may target different tracks.

### 4.5 Frame-manipulating transforms (frame_map, source_align, stacker)

Two data regimes flow through frame-manipulating stages, each with a defined
remapping rule:

- **Per-frame data** — video, dropout hints, EFM t-values, AC3 symbols, and
  *locked* audio tracks — remaps by frame ID, exactly as today, generalised
  to loop over every locked track.
- **Free-running tracks** remap in the **time domain**: each stage derives
  the output stream from its frame mapping using
  `audio_stream_pair_offset()` (§2.2), so the audio stays synchronised with
  the manipulated video rather than passing through unmapped (the current
  behaviour, which desynchronises audio as soon as a lead-in is trimmed).

Free-running remapping is *placement*, never *resampling*: samples are
sliced, stitched, silence-filled, or dropped — their values and rate are
untouched (EFM audio stays bit-exact).

#### 4.5.1 frame_map

- Locked tracks: ID remapping with per-track locked-size silence for padding
  frames (current behaviour, per track).
- Free-running tracks, by mapping shape:
  - **Contiguous mapping** (a single monotonic source range — the dominant
    case: head/tail trim, sub-range selection): the output stream is one
    exact slice of the source stream,
    `[offset(first_source_frame), offset(last_source_frame + 1))`. Sample
    accurate, no artefacts.
  - **General mapping** (reordering, duplication, gaps): the output stream
    is the concatenation of each output frame's source time window
    `[offset(m), offset(m + 1))` (silence for padding frames). Window joins
    at mapping discontinuities are not phase-continuous; the stage emits an
    info observation naming the affected tracks ("free-running track
    remapped with N discontinuities — audible clicks possible; lock audio
    at the source for gapless stacking/reordering").
- Both shapes are served lazily through `get_audio_stream_samples()` by
  translating requested pair ranges — no stream is materialised.

#### 4.5.2 source_align

- Locked tracks: the existing shift/pad decorators remap every locked track.
- Free-running tracks: a whole-stream time shift —
  `audio_stream_pair_offset(frame_offset)` pairs are trimmed from (positive
  shift) or prepended as silence to (pad variant) the stream. Equivalent to
  an implicit `audio_align`, so a source aligned by ±N frames keeps its
  free-running tracks in sync.

#### 4.5.3 stacker

- Locked tracks: `audio_stacking` (mean/median/disabled) applies per locked
  track index present in **all** inputs; tracks not common to all inputs
  pass through from the selected best source.
- Free-running tracks are **never combined**. Independent captures'
  free-running streams share no sample clock — they start at different
  instants and drift at different rates — so sample-wise mean/median would
  comb-filter the audio. The stacker passes free-running tracks through
  from the best/reference source unchanged and emits a warning observation
  when other inputs' free-running tracks are discarded. Users who want
  stacked analogue audio enable `lock_audio` at the sources (§3.1) so the
  tracks become locked and stack in frame lockstep.
- EFM t-value stacking (best-source selection) is unchanged — which is why
  `efm_audio_decode` belongs *downstream* of the stacker (§4.6).

### 4.6 Transform ordering for EFM audio

EFM t-values are per-frame data, so they remap and stack correctly through
every frame-manipulating stage. Placing `efm_audio_decode` **after** all
frame manipulation (stacker, frame_map, source_align) means it decodes the
already-remapped t-value stream, and the resulting free-running track is
born aligned to the output timeline — no time-domain remapping needed. If a
frame-manipulating stage is nevertheless placed downstream of the decode,
its free-running track is handled by the §4.5 rules like any other. The
quick-project wiring (§6) follows this ordering.

---

## 5. Sink Stages

### 5.1 audio_sink

- New parameter `track` (int, default 0) selecting which track to write.
- Locked tracks: unchanged behaviour, including `sample_rate_mode`
  (`locked_44056` | `free_running_44100`) for NTSC/PAL-M.
- Free-running tracks: the stream is written verbatim at 44100 Hz
  (`sample_rate_mode` hidden — nothing to resample), fixing the current
  free-running early-return in `audio_sink_stage_deps.cpp`.

### 5.2 video_sink (FFmpeg backend)

- `embed_audio` (bool) is retained as the master switch; a new
  `audio_tracks` parameter (string, default `"all"`, else comma-separated
  track indices, e.g. `"0,2"`) selects which tracks to embed, gated on
  `embed_audio` via the existing `ParameterDependency` mechanism.
- **One output audio stream per selected track.** `setupAudioEncoder()` /
  `encodeAudioForFrame()` generalise to per-track encoder contexts, buffers,
  and cursors. Each stream carries `title` metadata from the track name.
- **Per-stream sample rate honesty** (fixes a latent defect):
  - Locked PAL → 44100 Hz.
  - Locked NTSC/PAL-M → declared 44056 Hz (nearest integer to
    44100000⁄1001; 1.3 ppm error ≈ 4.6 ms/hour) instead of today's
    hard-coded 44100 (999 ppm ≈ 3.6 s/hour audible drift).
  - Free-running (incl. EFM) → 44100 Hz, fed by its own sample cursor
    rather than the video frame cursor; when the export starts at frame
    *k* > 0, the stream read begins at `audio_stream_pair_offset(k)`
    (§2.2). The stream is silence-padded or truncated at flush to the
    video duration.
  - The `48000.0` silence-fallback estimate in `encodeAudioForFrame()` is
    replaced by the track's declared rate.
- **EFM is never resampled**: no `swr`/rate conversion is introduced; the
  encoder consumes 44100 Hz input directly. Codec selection keeps today's
  video-codec-derived mapping (FFV1→FLAC, ProRes/v210/v410/MPEG-2→PCM
  S24LE, else AAC); AAC is lossy but performs no rate change. A per-track
  codec override is out of scope.
- `audio_gain_db` applies uniformly to all embedded tracks (a per-track gain
  transform can be added later if needed).

### 5.3 cvbs_sink

- Writes **every** pipeline track to `<basename>_audio_NN.wav`, NN = pipeline
  track index, both locked and free-running (free-running at 44100 Hz with
  authoritative header, per spec).
- Writes the new per-track `audio_track` metadata table (§7). The removed
  file-level `cvbs_file.audio_locked` column is not written.

### 5.4 efm_sink, raw_efm_sink, ac3rf_sink

Unchanged. `efm_sink` remains the offline decoder (audio *and* ECMA-130 data
modes) and now links the shared `efm-decode` library.

---

## 6. Quick Project Wiring

The GUI quick-project builder (`orc/gui/mainwindow.cpp`, `build_downstream`)
gains: when an ld-decode source has an `.efm` sidecar, insert
`efm_audio_decode` between the dropout corrector and `video_sink`, so the
default project produces a video file with both analogue and EFM audio
tracks embedded. The parallel `efm_sink` node remains for users wanting a
standalone WAV.

---

## 7. CVBS File Format Specification Extension

The specification lives in its own repository
(`github.com/simoninns/cvbs-file-format-specification`), included here as
the git sub-module `docs-tech/cvbs-file-format-specification` (currently
pinned at v1.1.0). The extension below is authored **in that repository**,
released as a new spec version tag (v1.2.0), and the decode-orc sub-module
pin is then advanced — the spec change must be tagged before the
cvbs_source/cvbs_sink implementation that cites it lands. Extension
content:

- New `.meta` table `audio_track`:

  | Column | Type | Meaning |
  |--------|------|---------|
  | `track_number` | INTEGER PK (0–15) | Matches `_audio_NN.wav` naming |
  | `description` | TEXT | Human-readable track description |
  | `audio_locked` | BOOLEAN | Per-track lock mode |

  The container carries no origin column — `AudioTrackOrigin` is a
  pipeline-model concept only; tracks read from CVBS files report
  `origin: UNKNOWN`.

- Per-track `audio_locked` semantics match the previous file-level
  definition (locked = preset rational rate, exact pairs/frame; free-running
  = 44100 Hz, header authoritative; shared frame-0 origin).
- File-level `cvbs_file.audio_locked` is **removed** — the spec describes
  only the current version, so the column is gone from the `cvbs_file`
  schema and writers no longer emit it. Readers validate against the
  current spec only (no backwards compatibility with pre-v1.2.0 metadata);
  tracks without an `audio_track` row are treated as free-running.
- Track files need not be contiguous, but `track_number` must match the file
  suffix. Stereo 16-bit PCM remains the only permitted encoding.

---

## 8. Presentation Layer

- Track-selection parameters (`track`, `audio_tracks`) are plain stage
  parameters — the generic `StageParameterDialog` handles them. Track
  indices are presented **0-based**, matching the CVBS `_audio_NN.wav`
  naming (the 1-based UI convention applies to frames/lines, not container
  track numbers).
- Each new/changed stage updates its `instructions.md` in the same PR
  (AGENTS.md §9.1), including documenting track numbering.
- No new presenters are required for the core feature; audio remains
  non-previewed. (A future audio-preview/scope tool is out of scope.)

---

## 9. Testing

Per AGENTS.md §4 (unit-first, mocked, deterministic):

- **SDK/contract**: wrapper-forwarding tests for all six accessors;
  `audio_stream_pair_offset()` exactness (PAL constant windows, NTSC
  1471/1472 alternation, no cumulative drift over long ranges); a mock VFR
  with N tracks exercised through every remapping wrapper (frame_map,
  source_align, stacker) verifying locked remap and time-domain
  free-running remap (§4.5). Labels: `unit`, `contracts`.
- **Sources**: mocked-deps tests for cvbs_source track enumeration,
  no-table free-running defaults, free-running stream reads; tbc_source
  track-0 descriptor. Labels: `unit`, `sources`.
- **Transforms**: efm_audio_decode with a mocked decode dependency
  (t-value gathering, cache-file serving, once-only decode); channel-map
  routing math; align window assembly incl. edge silence; import validation.
  Labels: `unit`, `transforms`.
- **Sinks**: audio_sink track selection; video_sink per-track rate/codec
  configuration via mocked backend deps; cvbs_sink multi-file + metadata
  emission via mocked writer deps. Labels: `unit`, `sinks`.
- Full-pipeline EFM decode correctness stays covered by the existing
  functional EFM golden-sample baselines
  ([efm-golden-sample-baseline.md](efm-golden-sample-baseline.md)).
- SDK gates (`-L sdk`) and `MVPArchitectureCheck` must pass; the efm-decode
  library move must not introduce private-include violations.

---

## 10. Implementation Phases

### Phase 1 — SDK track model

1. Add `audio_track.h` (`AudioTrackDescriptor`, `AudioSampleRate`,
   `AudioTrackOrigin`, `kMaxAudioTracks`, `audio_stream_pair_offset()` with
   exact 64-bit rational rounding); replace the VFR audio accessors
   with the track-indexed API; update `VideoFrameRepresentationWrapper`
   forwarding and the pass-through contract comment.
   *Acceptance*: SDK headers compile standalone; wrapper forwards all six
   accessors; unit tests cover defaults and forwarding.
2. Bump `kStagePluginHostAbiVersion` to 6; update `plugin-sdk.md` and
   `plugin-architecture.md` version tables and header list.
   *Acceptance*: ABI static_asserts pass; docs updated in same PR.
3. Mechanically port every in-tree VFR implementation and consumer
   (tbc_source, cvbs_source, frame_map, source_align, stacker, dropout
   stages' wrappers, audio_sink, video_sink, cvbs_sink) to the new API as a
   single track 0, preserving current behaviour.
   *Acceptance*: full build green; existing unit + functional suites pass
   unchanged; `-L sdk` and MVP gates pass.

### Phase 2 — Sources

1. Extend the CVBS file format specification in its own repository
   (`cvbs-file-format-specification`) with the `audio_track` table and
   removal of file-level `audio_locked` (§7); tag the new spec version;
   advance the `docs-tech/cvbs-file-format-specification` sub-module pin in
   decode-orc. Ordered first: the cvbs_source work below (and cvbs_sink in
   Phase 5) cites the tagged spec.
   *Acceptance*: spec repo builds/reads coherently and is tagged v1.2.0;
   sub-module pin in decode-orc references the tagged commit.
2. Consolidate `NtscPalMAudioResampler` and `FreeRunningAudioResampler` into
   the shared `orc/plugins/stages/common/audio-resample/` library; relink
   tbc_source and audio_sink.
   *Acceptance*: existing resampler unit tests pass against the shared lib;
   SDK gates pass.
3. cvbs_source: enumerate `_audio_00..15.wav` into tracks; serve
   free-running tracks via stream accessors; no-table free-running default;
   add
   `lock_audio` (free-running → locked resample, lazy per track);
   `instructions.md`.
   *Acceptance*: mocked-deps unit tests for enumeration, descriptors,
   locked/free-running reads, and lock conversion.
4. tbc_source: emit the track-0 descriptor; add `pcm_audio_timing`
   declaration and `lock_audio` conversion parameters (defaults reproduce
   current behaviour); serve declared-locked PCM via per-field metadata
   offsets and unlocked PCM via stream accessors; `instructions.md`.
   *Acceptance*: unit tests for all four timing/conversion combinations;
   lazy NTSC path unaffected under defaults.

### Phase 3 — EFM decode as a track

1. Move `EfmProcessor` + `decoders/` + `efm-lib/` + `writers/` to
   `orc/plugins/stages/common/efm-decode/`; relink efm_sink.
   *Acceptance*: efm_sink behaviour and functional golden hashes unchanged;
   SDK gates pass.
2. Implement `efm_audio_decode` transform (lazy once-only decode to scratch
   cache, stream serving, error observation on decode failure, parameters,
   `instructions.md`).
   *Acceptance*: unit tests with mocked decode/file deps; track appended
   with correct descriptor; video-only access triggers no decode.
3. Quick-project wiring: insert the transform when an `.efm` sidecar exists.
   *Acceptance*: GUI model/coordinator test at presenter boundary.

### Phase 4 — Track transforms

1. `audio_track_import` (validation, auto lock detection, descriptor,
   `instructions.md`).
2. `audio_channel_map` (operations in §4.3, dual-mono split naming,
   `instructions.md`).
3. `audio_align` (locked window assembly, free-running origin shift,
   `instructions.md`).
4. Generalise frame_map/source_align/stacker locked-audio handling to all
   locked tracks; implement time-domain free-running remapping (§4.5):
   frame_map contiguous slice + general window stitching with discontinuity
   observation, source_align stream shift, stacker no-combine pass-through
   with discard warning.
   *Acceptance (all)*: per-stage mocked unit tests incl. edge cases (cap
   exceeded, out-of-range track, negative offsets, contiguous vs reordering
   mappings, NTSC non-integer window boundaries agreeing with
   `audio_stream_pair_offset()`); contract suites updated.

### Phase 5 — Sinks

1. audio_sink: `track` parameter; free-running write path;
   `instructions.md`.
2. video_sink: `audio_tracks` parameter; per-track encoder contexts and
   cursors; per-stream true sample rates (44056 fix); free-running feed with
   sub-range start offset; stream titles; silence-fallback rate fix;
   `instructions.md`.
3. cvbs_sink: write all tracks + `audio_track` metadata;
   `instructions.md`.
   *Acceptance (all)*: mocked unit tests per sink; a functional multi-track
   round-trip (cvbs_sink → cvbs_source) verifying descriptors and samples.
