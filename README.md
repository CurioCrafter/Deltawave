# Visualizer

Visualizer is a C++20 Windows music-visualization application built around intense geometric motion for techno and adjacent electronic genres. The first vertical slice uses Win32, Direct2D, DirectWrite, WASAPI loopback capture, local audio-file playback, and a portable analysis/geometry core with no third-party runtime dependencies.

## Current Features

- Hardware-accelerated Direct2D geometric renderer.
- Decaying geometric trails for live rendering, live frame capture, and deterministic offline export.
- Thirteen procedural modes: Quantum Tunnel, Techno Mandala, Lissajous Mesh, Frequency Bloom, Fractal Cathedral, Polyrhythm Lattice, Spectral Origami, Chroma Kaleidoscope, Hyperspace Polytope, Phase Weave, Resonance Tessellation, Neural Constellation, and Cymatic Interference.
- WASAPI loopback input, so visuals react to music playing from any Windows app on the default output device.
- Local audio open and drag/drop path for user-provided music: PCM/IEEE-float WAV everywhere, plus Media Foundation-supported formats on Windows such as MP3, AAC/M4A, WMA, and installed-codec formats.
- Real-time RMS, peak, bass/mid/treble, spectrum, spectral flux, per-band onset, beat, beat phase, four-beat bar phase, downbeat confidence, 16-beat phrase phase, phrase-boundary confidence, build tension, BPM, drop, phrase, arrangement section, stereo-width, chroma/key, harmonic-energy, and style metrics.
- Source-adaptive audio profiles that learn style centroids plus beat/section sync sensitivity, with portable `.vizaudio` and `.vizsync` files under `profiles/sources/`.
- Keyboard customization for curated looks, visual mode, palette, hue shift, complexity, intensity, speed, HUD, fullscreen, source profile reset, and recording.
- Mouse-reactive geometric field that bends rings, particles, beams, and line meshes around pointer movement/clicks.
- Environment-reactive visual layer that shifts ambient geometry from local time-of-day and pointer-motion context.
- Built-in curated looks, an app-managed user preset library under `profiles/presets/`, and arbitrary `.vizpreset` file save/load for reusable visual settings.
- Clickable Direct2D control panel for source, source profile reset, mode, palette, hue shift, complexity, sliders, curated/user presets, recording, HUD, and interaction controls.
- Persistent in-window inspector for current source, file playback progress, active look, source profile files, decoded format, and capture status.
- Runtime FPS/frame-time telemetry with analysis, geometry, Direct2D presentation, and recording timing breakdowns plus adaptive geometry quality scaling for smoother high-resolution playback.
- Auto Scene director that uses detected style, BPM, arrangement sections, drops, phrase phase, build tension, stereo width, and tonal key confidence to adapt mode, palette, hue shift, speed, and intensity with beat-synced morph overlays during scene switches.
- Deterministic offline audio exporter for rendering frame sequences without relying on real-time playback timing.
- Batch audio exporter for turning a folder of user music into a consistent gallery of share packages.
- Optional FFmpeg-backed MP4 encoding from exported and live-captured frames for share-ready H.264 videos.
- Optional share packages with `index.html`, browser-viewable `preview.bmp` contact sheets, `share_manifest.json`, track-intelligence summaries, and per-frame `analysis_timeline.csv` metadata for exported visualizations.
- Support bundle generator that packages small diagnostics, adaptive audio profiles, recent capture/export manifests, build facts, and docs presence without duplicating large media/frame payloads.
- Benchmark executable for repeatable analyzer and geometry throughput checks.
- Frame-sequence export to `captures/` as PPM images, with live-capture `index.html`, `preview.bmp`, `capture_manifest.json`, `analysis_timeline.csv`, and best-effort `visualizer-live-capture.mp4` share output.
- Core tests for audio analysis, advanced sync metrics, WAV parsing, geometry generation, UI hit testing, offline export, and frame recording.

## Build

The verified build path is Visual Studio 2022:

```powershell
.\scripts\build.ps1
```

The MinGW presets are kept for portability experiments, but this machine's current MinGW `cc1plus.exe` crashes before compiling a trivial source. Use `.\scripts\build.ps1 mingw-release` only after that toolchain is repaired.

## Run

```powershell
.\build\vs2022\Release\Visualizer.exe
```

Play music in any Windows app to use live loopback analysis, or press `O` / drag an audio file onto the window.

## Offline Export

Render an audio file into a deterministic PPM frame sequence and optional MP4:

```powershell
.\build\vs2022\Release\VisualizerExport.exe --input song.wav --output captures\offline --width 1920 --height 1080 --fps 60 --look "Acid Geometry" --trails
.\build\vs2022\Release\VisualizerExport.exe --input song.mp3 --output captures\offline --mp4 captures\visualizer.mp4 --share --width 1920 --height 1080 --fps 60 --auto-scene
.\build\vs2022\Release\VisualizerExport.exe --input song.wav --output captures\calibrated --sync-profile profiles\sources\live_loopback.vizsync --style-profile profiles\sources\live_loopback.vizaudio
.\build\vs2022\Release\VisualizerExport.exe --input song.wav --output captures\library_look --user-preset "Late Set" --share
```

The exporter writes `frame_000000.ppm` files plus `export_manifest.txt` and `analysis_timeline.csv`. Manifests include a track-intelligence summary with downbeats, phrase boundaries, average bar/phrase lock, drop/phrase/build-tension peaks, dominant style, harmonic energy, and peak primitive count. When `--mp4` is provided, it also invokes FFmpeg to encode a shareable H.264 MP4. Add `--share` to write a browsable `index.html`, `preview.bmp` contact sheet, and machine-readable `share_manifest.json` beside the frame sequence.
Use `--style-profile` and `--sync-profile` to reproduce a learned live calibration during deterministic offline export.

Batch-render a folder of tracks into a browsable gallery:

```powershell
.\build\vs2022\Release\VisualizerBatch.exe --input-dir music --output captures\batch --look "Resonance Tessellation" --width 1280 --height 720 --fps 30 --seconds 30 --share
.\build\vs2022\Release\VisualizerBatch.exe --input-dir music --output captures\batch_user --user-preset late_set --width 1280 --height 720 --fps 30 --seconds 30 --share
```

The exporter and batch exporter can apply built-in looks with `--look`, arbitrary preset files with `--preset`, or saved live library entries with `--user-preset`; `--list-user-presets` prints entries discovered under `profiles\presets\`. The batch exporter writes `batch_manifest.json` and `index.html` at the batch root, including per-track preview thumbnails and style/sync intelligence, then one normal export/share folder per discovered audio file.

## Support Bundle

Generate a compact diagnostic package when reporting sync, capture, export, or performance issues:

```powershell
.\build\vs2022\Release\VisualizerSupport.exe --workspace . --output artifacts\support_bundle_current --max-captures 5 --max-artifacts 5
```

The bundle writes `support_manifest.json` and `support_summary.txt`, copies small `.vizaudio`, `.vizsync`, `.vizpreset`, manifest, and timeline files, and summarizes PPM/MP4/audio payloads by count and size instead of copying them.

## Benchmark

Run a repeatable analyzer and geometry benchmark:

```powershell
.\build\vs2022\Release\VisualizerBenchmark.exe --frames 240 --width 1920 --height 1080
```

The benchmark reports average analyzer/geometry time, separate analysis and geometry tracker timings, estimated FPS, adaptive quality scale, and primitive counts. Live HUD and capture packages add phrase/build-tension sync, Direct2D presentation, and recording timing so runtime bottlenecks can be diagnosed separately from core generation speed.

## Controls

- `O`: open an audio file
- Drag/drop audio: load and play
- `L`: return to live loopback
- `V`: reset the current source's adaptive audio AI profile
- `0` to `9`: switch visual modes
- `M`: switch to Resonance Tessellation
- `Y`: switch to Neural Constellation
- `Z`: switch to Cymatic Interference
- `B` / `N`: cycle previous/next curated look
- `[` / `]`: cycle previous/next user preset from `profiles\presets`
- `K`: save the current look to the user preset library
- `A`: toggle Auto Scene director
- `E`: toggle environment-reactive layer
- `T`: toggle decaying trails
- `C`: cycle palette
- `U`: advance hue shift
- `X`: increase geometry complexity
- `I`: toggle mouse-reactive interaction field
- `Q`: toggle adaptive quality
- Arrow keys: adjust intensity and speed
- `S`: save a `.vizpreset`
- `P`: load a `.vizpreset`
- `R`: record PPM frame sequence and best-effort MP4
- `H`: toggle HUD
- `F11`: fullscreen
- `Esc`: quit

The on-screen control panel exposes the same core controls with buttons and sliders, including previous/next curated look buttons and user preset library save/cycle buttons. On wide windows, a right-side inspector keeps source, active look, user preset count, profile, decoded-format, playback-progress, and capture metadata visible while visuals run.

## Capture

Press `R` to write frames to `captures/visualizer_YYYYMMDD_HHMMSS/`. When recording stops, the app writes `index.html`, `preview.bmp`, `capture_manifest.json`, and `analysis_timeline.csv` into the capture folder with source, active look, settings, duration, frame count, beat, key, section, runtime timing, and encoding metadata. If FFmpeg is available on `PATH`, it also writes `visualizer-live-capture.mp4`; otherwise the share page keeps a manual command:

```powershell
ffmpeg -framerate 60 -i captures\visualizer_YYYYMMDD_HHMMSS\frame_%06d.ppm -pix_fmt yuv420p visualizer.mp4
```

## Docs

- [Architecture](docs/ARCHITECTURE.md)
- [Controls](docs/CONTROLS.md)
- [Export](docs/EXPORT.md)
- [Performance](docs/PERFORMANCE.md)
- [Roadmap](docs/ROADMAP.md)
- [Support](docs/SUPPORT.md)
