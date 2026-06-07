# Architecture

Visualizer is split into a portable core and a thin Windows shell.

## Portable Core

- `AudioAnalyzer`: converts interleaved float samples into normalized music metrics, spectrum bins, spectral flux, per-band onsets, beat events, beat phase, four-beat bar phase, downbeat confidence, 16-beat phrase phase, phrase-boundary confidence, build tension, BPM estimates, drop/phrase intensity, arrangement section state, stereo width, 12-bin chroma, detected key/mode confidence, harmonic energy, and style hints.
- `AdaptiveStyleModel`: classifies style using lightweight centroids and online adaptation so the app can adjust to the active stream without a heavyweight ML runtime; style profiles can be saved and loaded as portable text.
- `AudioSyncProfile`: learns beat and arrangement-section sensitivity from the active source, then persists that calibration as a portable text profile for more consistent synchronization on repeat listens or deterministic exports.
- `AudioFileLoader`: format-agnostic loader facade. It uses the portable WAV parser first and can delegate to platform decoders when available.
- `WavFile`: decodes PCM and IEEE-float WAV files into normalized interleaved float samples.
- `VisualizerEngine`: converts metrics, environment state, interaction state, and user settings into a neutral `GeometryFrame`, including normalized palette hue shifting, user-controlled geometry complexity, Auto Scene morph overlays, tunnel, mandala, mesh, bloom, fractal cathedral, polyrhythm lattice, spectral origami, chroma kaleidoscope, 4D hyperspace polytope projection, phase-weave flow fields, resonance tessellation lattices, neural constellation node networks, cymatic interference contours, and time-of-day ambient field generators.
- `PresetStore`: saves and loads `.vizpreset` files, exposes built-in curated looks, and scans/saves the app-managed user preset library for portable visual customization.
- `SceneDirector`: resolves manual settings into audio-adaptive settings when Auto Scene is enabled, including arrangement-aware build/drop/breakdown routing, phrase build-tension routing, harmonic high-tension routing into Cymatic Interference, wide high-flux routing into Phase Weave, bar-locked harmonic routing into Neural Constellation, key-aware hue steering, and transient transition pulses for mode switches.
- `ControlPanel`: computes renderer-independent panel layout and hit testing for buttons and sliders, including source profile reset controls.
- `RuntimeInspector`: formats renderer-independent source, playback, active-look, profile, decoded-format, and capture-state lines for the live in-window inspector.
- `OfflineExporter`: renders decoded audio files into deterministic frame sequences using the same analyzer and geometry engine as the live app, optionally loading saved style and sync profiles first, and aggregating track-intelligence summaries from the per-frame metrics.
- `VideoEncoder`: validates exported PPM frame sequences and optionally invokes FFmpeg to create shareable H.264 MP4 output.
- `PreviewImage`: reads exported PPM frame sequences and writes dependency-free BMP contact sheets for browser-viewable package previews.
- `SharePackage`: writes a portable HTML showcase, BMP preview, JSON metadata bundle, track-intelligence summary, and links to per-frame analysis timelines for exported visualizations.
- `BatchExporter`: scans a directory of audio sources and orchestrates repeated `OfflineExporter` runs into a gallery root with `batch_manifest.json`, `index.html`, preview thumbnails, per-track intelligence summaries, and one ordinary export/share folder per track.
- `CapturePackage`: writes a portable HTML page, BMP preview, and JSON metadata bundle for live frame recordings, including timing, source profile references, timeline, and optional MP4 metadata.
- `AnalysisTimelineWriter`: streams per-frame sync metrics, adaptive profile state, resolved visual settings, and primitive counts to CSV for export/capture debugging and sharing.
- `SupportBundle`: scans a workspace for adaptive audio profiles, recent capture/export metadata, build outputs, and docs presence, then writes a compact diagnostics bundle while intentionally avoiding large media/frame copies.
- `FramePerformanceTracker`: tracks frame time, FPS, analysis/geometry/render/record timing, primitive count, and adaptive quality scale.
- `FrameRecorder`: rasterizes `GeometryFrame` output into PPM image sequences, with optional deterministic trail persistence.

## Windows Shell

- `WindowsApp`: window lifecycle, message loop, controls, drag/drop, audio-file playback, fullscreen, renderer-present-time profiling, and capture timing.
- `MediaFoundationAudioDecoder`: Windows-only decoder that turns Media Foundation-supported local files into normalized float samples.
- `WasapiLoopbackCapture`: captures the default Windows output endpoint in loopback mode.
- `Direct2DRenderer`: renders neutral geometry primitives, live trail decay, HUD text, the clickable control panel, and the source/profile/capture inspector panel.
- `VisualizerExport`: console executable for offline audio-to-frame rendering.
- `VisualizerBatch`: console executable for rendering a folder of tracks into a gallery of consistent share packages.
- `VisualizerBenchmark`: console executable for repeatable analyzer and geometry throughput checks.
- `VisualizerSupport`: console executable for generating support bundles from the current workspace or an explicit workspace path.

## Data Flow

1. Audio enters from WASAPI loopback or a loaded audio file.
2. `AudioAnalyzer` emits frame metrics.
3. `AdaptiveStyleModel` and `AudioSyncProfile` refine style labels, beat sensitivity, and section sensitivity using any loaded source profile while learning from the current stream.
4. Manual settings, loaded `.vizpreset` files, built-in curated looks, or user preset library entries provide the base `VisualSettings`.
5. `SceneDirector` optionally adapts mode, palette, hue shift, speed, and intensity from style/BPM/arrangement/drop/phrase/build-tension/key metrics while preserving user complexity and emitting scene-transition pulses.
6. `VisualizerEngine` generates rings, beams, particles, polylines, onset bursts, bar/downbeat rings and spokes, phrase-boundary rings, build-tension spokes, drop rings, phrase contours, build/drop/breakdown section accents, Auto Scene morph overlays, fractal arch structures, rhythm lattices, chroma-reactive origami fold shards, harmonic kaleidoscope prisms, 4D polytope wireframes, beat-phase flow-field streamlines, spectral/chroma tessellation cells, neural node constellations, cymatic nodal contours, and environment-reactive ambient orbits, optionally bending them around mouse interaction.
7. `RuntimeInspector` formats current source, look, user preset library count, profile, playback, and capture metadata for live display.
8. `Direct2DRenderer` draws the frame, using trail decay when enabled, then draws the HUD, control panel, and inspector.
9. `FrameRecorder` optionally writes the same geometry to disk with matching trail persistence.
10. `FramePerformanceTracker` smooths full-frame, analysis, geometry, Direct2D, and recording costs for the HUD and live capture metadata.
11. `AnalysisTimelineWriter` records one CSV row per rendered or recorded frame, including bar/downbeat/phrase/build-tension sync columns.
12. `CapturePackage` can summarize a live recording as `index.html`, `preview.bmp`, plus `capture_manifest.json`; the Windows shell also attempts a best-effort MP4 encode on recording stop.
13. `VideoEncoder` can turn deterministic exported or live-captured frames into MP4 for sharing.
14. `SharePackage` can publish the exported result as `index.html`, `preview.bmp`, plus `share_manifest.json`, including the aggregated track-intelligence summary.
15. `BatchExporter` can repeat that deterministic path across a folder of tracks and publish a root gallery manifest/page with preview and per-track intelligence columns.
16. `SupportBundle` can collect recent profile, capture, export, batch, docs, and binary metadata into `support_manifest.json` and `support_summary.txt` for troubleshooting.

## Porting Strategy

The portable core should remain reusable. Future platforms need replacement shells for audio capture, windowing/input, and rendering, while preserving the analyzer, portable WAV loading, geometry, tests, and capture path. Platform decoders should stay behind `AudioFileLoader`.
