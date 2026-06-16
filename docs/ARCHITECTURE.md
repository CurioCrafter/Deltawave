# Architecture

Visualizer is split into a portable core and a thin Windows shell.

## Portable Core

- `AudioAnalyzer`: converts interleaved float samples into normalized music metrics, spectrum bins, spectral flux, per-band onsets, beat events, beat phase, four-beat bar phase, downbeat confidence, 16-beat phrase phase, phrase-boundary confidence, build tension, BPM estimates, drop/phrase intensity, arrangement section state, stereo width, 12-bin chroma, detected key/mode confidence, harmonic energy, and style hints.
- `AdaptiveStyleModel`: classifies style using lightweight centroids and online adaptation so the app can adjust to the active stream without a heavyweight ML runtime; style profiles can be saved and loaded as portable text.
- `AudioSyncProfile`: learns beat and arrangement-section sensitivity from the active source, then persists that calibration as a portable text profile for more consistent synchronization on repeat listens or deterministic exports.
- `AudioFileLoader`: format-agnostic loader facade. It uses the portable WAV parser first and can delegate to platform decoders when available.
- `WavFile`: decodes PCM and IEEE-float WAV files into normalized interleaved float samples.
- `VisualizerEngine`: converts metrics, environment state, interaction state, and user settings into a neutral `GeometryFrame`, including music-aware palette personality, stronger whole-frame hue steering, a 3D-first scene-intent interpreter, a reusable true 3D object layer, user-controlled camera depth/object density/mouse depth/glow/scene personality, user-controlled geometry complexity, Auto Scene morph overlays, tunnel, mandala, mesh, bloom, fractal cathedral, polyrhythm lattice, spectral origami, chroma kaleidoscope, 4D hyperspace polytope projection, phase-weave flow fields, resonance tessellation lattices, neural constellation node networks, cymatic interference contours, and time-of-day ambient field generators.
- `PresetStore`: saves and loads `.vizpreset` files, exposes built-in curated looks, and scans/saves the app-managed user preset library for portable visual customization.
- `SceneDirector`: resolves manual settings into audio-adaptive settings when Auto Scene is enabled, including arrangement-aware build/drop/breakdown routing, phrase build-tension routing, harmonic high-tension routing into Cymatic Interference, wide high-flux routing into Phase Weave, bar-locked harmonic routing into Neural Constellation, key-aware hue steering, depth/color personality steering, and transient transition pulses for mode switches.
- `ControlPanel`: computes renderer-independent panel layout and hit testing for buttons and sliders, including source profile reset controls.
- `RuntimeInspector`: formats renderer-independent source, playback, active-look, profile, decoded-format, and capture-state lines for the live in-window inspector.
- `OfflineExporter`: renders decoded audio files into deterministic frame sequences using the same analyzer and geometry engine as the live app, optionally loading saved style and sync profiles first, and aggregating track-intelligence summaries from the per-frame metrics.
- `VideoEncoder`: validates exported PPM frame sequences and optionally invokes FFmpeg to create shareable H.264 MP4 output.
- `PreviewImage`: reads exported PPM frame sequences and writes dependency-free BMP contact sheets for browser-viewable package previews.
- `SharePackage`: writes a portable HTML showcase, BMP preview, JSON metadata bundle, track-intelligence summary, and links to per-frame analysis timelines for exported visualizations.
- `BatchExporter`: scans a directory of audio sources and orchestrates repeated `OfflineExporter` runs into a gallery root with `batch_manifest.json`, `index.html`, preview thumbnails, per-track intelligence summaries, and one ordinary export/share folder per track.
- `CapturePackage`: writes a portable HTML page, BMP preview, and JSON metadata bundle for live frame recordings, including timing, source profile references, timeline, and optional MP4 metadata.
- `AnalysisTimelineWriter`: streams per-frame sync metrics, adaptive profile state, resolved visual settings, scene intent, retained 2D/projected 3D primitive budgets, 3D dominance, and primitive counts to CSV for export/capture debugging and sharing.
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
5. `SceneDirector` optionally adapts mode, palette, hue shift, 3D depth, color impact, speed, and intensity from style/BPM/arrangement/drop/phrase/build-tension/key metrics while preserving user complexity and emitting scene-transition pulses.
6. `VisualizerEngine` scores the current musical intent across calm, groove, tension, drop, release, melodic, industrial, dark, bright, chaotic, spacious, heavy, and minimal. The selected intent is stored on `GeometryFrame` for HUD/timeline debugging.
7. The engine still creates legacy rings, beams, particles, and polylines for faint rhythmic context, then thins and fades that screen-space layer before projected 3D is added. `GeometryFrame` records authored 2D count/weight, retained 2D count/weight, projected 3D count/weight, and 3D dominance.
8. The shared 3D object layer maps modes into five profiles: Techno Machine, Crystal Storm, Neural Space, Dimensional Tunnel, and Cymatic Sculpture. It emits `Object3D` polyhedra, shards, tunnel ribs, nodes, links, plates, ribbons, particles, depth planes, columns, cages, wave surfaces, orbiters, and anchors with x/y/z positions, rotations, scales, velocities, glow, perspective projection, depth sorting, lighting/fog shading, and mouse-driven z impulses before rendering them back into neutral Direct2D primitives.
9. A second intent-driven 3D layer adds bass-pressure tunnel mass, sequencer architecture, ambient orbital planes, harmonic crystal constellations, breakbeat fracture fields, and dark/minimal monoliths so the same mode can feel different for different songs.
10. `RuntimeInspector` formats current source, look, user preset library count, profile, playback, and capture metadata for live display.
11. `Direct2DRenderer` draws the frame, using trail decay when enabled, then draws the HUD, control panel, and inspector.
12. `FrameRecorder` optionally writes the same geometry to disk with matching trail persistence.
13. `FramePerformanceTracker` smooths full-frame, analysis, geometry, Direct2D, and recording costs for the HUD and live capture metadata.
14. `AnalysisTimelineWriter` records one CSV row per rendered or recorded frame, including resolved 3D depth/object/glow/personality settings, scene intent, 3D dominance, and bar/downbeat/phrase/build-tension sync columns.
15. `CapturePackage` can summarize a live recording as `index.html`, `preview.bmp`, plus `capture_manifest.json`; the Windows shell also attempts a best-effort MP4 encode on recording stop.
16. `VideoEncoder` can turn deterministic exported or live-captured frames into MP4 for sharing.
17. `SharePackage` can publish the exported result as `index.html`, `preview.bmp`, plus `share_manifest.json`, including the aggregated track-intelligence summary.
18. `BatchExporter` can repeat that deterministic path across a folder of tracks and publish a root gallery manifest/page with preview and per-track intelligence columns.
19. `SupportBundle` can collect recent profile, capture, export, batch, docs, and binary metadata into `support_manifest.json` and `support_summary.txt` for troubleshooting.

## Porting Strategy

The portable core should remain reusable. Future platforms need replacement shells for audio capture, windowing/input, and rendering, while preserving the analyzer, portable WAV loading, geometry, tests, and capture path. Platform decoders should stay behind `AudioFileLoader`.
