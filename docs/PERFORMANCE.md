# Performance

Visualizer tracks frame time, estimated FPS, primitive count, active quality scale, and a frame-time breakdown at runtime. When adaptive quality is enabled, the app lowers geometry density if average frame time rises above the 60 FPS target range, then restores density when frame time recovers.

## Runtime HUD

The HUD shows:

- `FPS`: exponentially smoothed frame rate.
- `Frame`: average frame time in milliseconds.
- `Core`: average analyzer plus geometry generation time.
- `D2D`: average Direct2D draw and present time measured around the renderer.
- `Audio AI`: current style/sync adaptation and beat/section sensitivity from the active source profile.
- `Beat` / `Bar` / `Phrase` / `Tension`: current beat phase, four-beat bar phase, 16-beat phrase phase, and build-tension estimate, with downbeat and phrase-boundary state when detected.
- `Prims`: geometry primitive and point count for the last frame.
- `Quality`: active geometry scale.
- `AUTO`: shown when adaptive quality has reduced density below 100%.

Use `Q` or the `Auto Quality` panel toggle to enable or disable adaptive quality. Drag the `Quality` slider for manual quality; manual changes disable adaptive quality.

Auto Scene (`A`) changes mode, palette, hue, depth, color impact, intensity, and speed from audio metrics but still runs through the same adaptive-quality path. If a track causes a dense mode switch, high-depth camera push, or object-heavy 3D profile on weaker hardware, keep Auto Quality enabled.

Depth (`D` or the `Depth 3D` slider) changes the true 3D camera projection and makes object parallax stronger. `Obj 3D` controls the number of emitted polyhedra, shards, nodes, links, plates, tunnel ribs, and particles; this is the 3D setting most likely to affect geometry cost. `Mouse 3D` changes interactive z impulses, and `Glow` changes projected shading/alpha intensity. Color impact (`F` or the `Color Impact` slider) changes color grading only, so it should have little cost compared with complexity, depth, and object density.

Trails (`T`) replace a full background clear with a low-cost decay pass. They are useful for dense techno motion and shareable exports; disable them when you need crisp single-frame inspection or exact geometry edges.

## Benchmark

```powershell
.\build\vs2022\Release\VisualizerBenchmark.exe --frames 240 --width 1920 --height 1080
.\build\vs2022\Release\VisualizerBenchmark.exe --frames 120 --width 1920 --height 1080 --all-modes
```

The benchmark runs the same `AudioAnalyzer` and `VisualizerEngine` path as the app with synthetic techno-like audio. Use `--mode NAME` to isolate one mode or `--all-modes` to compare every geometry generator. It reports total analyzer/geometry time plus separate tracker averages for analysis and geometry. It still does not create a Direct2D window, so use the live HUD or a live capture manifest when you need renderer-present-time evidence.

## Capture Timing

Live recordings write timing and phrase-sync fields to `capture_manifest.json` and show them on `index.html`:

- `averageFrameMs`: full live frame cost.
- `averageAnalysisMs`: source analysis cost.
- `averageGeometryMs`: procedural geometry build cost.
- `averageRenderMs`: Direct2D draw and present cost.
- `averageRecordMs`: frame recording, timeline, and package update cost while recording.
- `averagePhraseConfidence` and `peakBuildTension`: compact sync-quality signals for checking whether the analyzer was locked to phrase motion during the capture.
