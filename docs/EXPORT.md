# Offline Export

`VisualizerExport.exe` renders an audio file into a deterministic PPM frame sequence. It can also invoke FFmpeg to encode those frames into a share-ready H.264 MP4 and can write a dependency-free `preview.bmp` contact sheet for quick browser inspection. This is better than live recording when you need reproducible output, fixed frame rate, high-resolution export independent of playback timing, or an export manifest with peak RMS, BPM, beat count, downbeat/bar/phrase-lock summary, phrase boundaries, drop/phrase/build-tension peaks, dominant style, harmonic energy, primitive peak, and detected tonal key metadata.

Live recordings use the same frame sequence and FFmpeg encoder path when you stop recording with `R`; successful captures include `visualizer-live-capture.mp4` beside `index.html`.

## Example

```powershell
.\build\vs2022\Release\VisualizerExport.exe --input song.wav --output captures\offline --width 1920 --height 1080 --fps 60 --look "Acid Geometry" --trails
.\build\vs2022\Release\VisualizerExport.exe --input song.mp3 --output captures\offline --mp4 captures\visualizer.mp4 --share --width 1920 --height 1080 --fps 60 --crf 18 --auto-scene --depth-3d 1.0 --object-density-3d 0.9 --lighting-glow 0.85 --color-impact 1.0 --scene-personality 0.9 --motion-style Hyperspace --response-3d 0.88 --motion-stability 0.82 --pattern-clarity 0.9
.\build\vs2022\Release\VisualizerExport.exe --input song.wav --output captures\calibrated --sync-profile profiles\sources\live_loopback.vizsync --style-profile profiles\sources\live_loopback.vizaudio
.\build\vs2022\Release\VisualizerExport.exe --list-looks
.\build\vs2022\Release\VisualizerExport.exe --list-user-presets
.\build\vs2022\Release\VisualizerExport.exe --input song.wav --output captures\library_look --user-preset "Late Set" --share
.\build\vs2022\Release\VisualizerBatch.exe --input-dir music --output captures\batch --look "Resonance Tessellation" --width 1280 --height 720 --fps 30 --seconds 30 --share
```

## Options

- `--input FILE`: PCM or IEEE-float WAV source, or a Windows Media Foundation-supported audio file on Windows.
- `--output frames`: output directory for `frame_000000.ppm` files, `export_manifest.txt`, and `analysis_timeline.csv`.
- `--width N`: output width, default `1280`.
- `--height N`: output height, default `720`.
- `--fps N`: output frame rate, default `60`.
- `--seconds N`: export only the first `N` seconds.
- `--mp4 FILE`: encode the exported frame sequence to H.264 MP4 using FFmpeg.
- `--style-profile FILE`: load an adaptive `.vizaudio` style profile before analysis.
- `--sync-profile FILE`: load an adaptive `.vizsync` beat/section sensitivity profile before analysis.
- `--share`: write `index.html` and `share_manifest.json` beside the exported frames.
- `--ffmpeg FILE`: FFmpeg executable path, default `ffmpeg`.
- `--crf N`: H.264 quality from `0` to `51`; lower is higher quality, default `18`.
- `--video-preset P`: FFmpeg x264 preset, default `medium`.
- `--look NAME`: apply a built-in curated look before rendering. Supported names include `Warehouse Strobe`, `Acid Geometry`, `4D Hyperspace`, `Harmonic Glass`, `Fractal Cathedral`, `Breakbeat Origami`, `Deep Bloom`, `Phase Weave`, `Stereo Loom`, `Resonance Tessellation`, `Neural Constellation`, `Cymatic Interference`, and `Auto DJ Director`; short aliases such as `warehouse`, `acid`, `4d`, `origami`, `weave`, `flow`, `loom`, `tess`, `mosaic`, `neural`, `network`, `cymatic`, `chladni`, and `auto` also work.
- `--list-looks`: print the built-in curated looks and exit.
- `--user-preset NAME`: apply a saved user preset from `profiles\presets\`, matching either the display name or `.vizpreset` filename/stem.
- `--preset-library DIR`: read user presets from a custom directory instead of `profiles\presets\`.
- `--list-user-presets`: print saved user presets and exit.
- `--mode NAME`: `QuantumTunnel`, `TechnoMandala`, `LissajousMesh`, `FrequencyBloom`, `FractalCathedral`, `PolyrhythmLattice`, `SpectralOrigami`, `ChromaKaleidoscope`, `HyperspacePolytope`, `PhaseWeave`, `ResonanceTessellation`, `NeuralConstellation`, or `CymaticInterference`.
- `--palette NAME`: `NeonVoltage`, `InfraredChrome`, `AcidAurora`, `MonochromeLaser`, or `OceanicPulse`.
- `--motion-style NAME`: `Smooth`, `Mechanical`, `Liquid`, `Hyperspace`, `HeavyBass`, `AmbientDrift`, or `Breakbeat`; alias `--motion` also works.
- `--hue-shift N`: shift the selected palette hue from `0.0` to `1.0`; default `0.0`.
- `--depth-3d N`: set true 3D camera depth from `0.0` flat to `1.0` deep; alias `--depth` also works.
- `--object-density-3d N`: set 3D object density from `0.0` sparse to `1.0` packed; alias `--objects` also works.
- `--interaction-depth N`: set mouse/depth interaction strength from `0.0` off to `1.0` strong; alias `--mouse-depth` also works.
- `--lighting-glow N`: set 3D lighting and glow strength from `0.0` matte to `1.0` luminous; alias `--glow` also works.
- `--color-impact N`: set palette personality strength from `0.0` subtle to `1.0` intense; alias `--color` also works.
- `--scene-personality N`: set scene-specific motion/personality bias from `0.0` restrained to `1.0` extreme; alias `--personality` also works.
- `--response-3d N`: set music-to-3D response gain from `0.0` restrained to `1.0` intense; aliases `--response` and `--reactivity` also work.
- `--motion-stability N`: smooth camera/object jitter from `0.0` wild to `1.0` stable; alias `--stability` also works.
- `--pattern-clarity N`: preserve readable geometry from `0.0` chaotic to `1.0` crisp; alias `--clarity` also works.
- `--complexity N`: set artistic geometry density from `0.35` sparse to `1.8` dense; default `1.0`.
- `--auto-scene`: adapt mode, palette, hue shift, 3D depth, color impact, intensity, and speed from audio metrics while exporting.
- `--environment`: enable deterministic environmental visual influence.
- `--no-environment`: disable environmental visual influence.
- `--time-of-day N`: set the environment phase from `0.0` midnight to `1.0` next midnight; default `0.5`.
- `--trails`: keep decaying geometry trails in exported frames.
- `--no-trails`: force crisp per-frame clears even when a loaded preset enables trails.
- `--preset FILE`: load a `.vizpreset` before rendering.

## Manual MP4 Encoding

```powershell
ffmpeg -framerate 60 -i captures\offline\frame_%06d.ppm -pix_fmt yuv420p visualizer.mp4
```

## Share Package

`--share` writes a portable `index.html` showcase, `preview.bmp` contact sheet, and `share_manifest.json` metadata file into the export folder. When `--mp4` is also used, the page embeds the MP4; without `--mp4`, the page still shows the BMP preview, summarizes the frame sequence, and links the metadata. Export and share metadata include requested and final motion style, depth, object density, mouse depth, lighting/glow, color, scene-personality, 3D-response, motion-stability, and pattern-clarity settings, the strongest detected arrangement section, plus a `trackIntelligence` object summarizing downbeats, phrase boundaries, average bar and phrase confidence, peak downbeat/drop/phrase/build-tension intensity, average harmonic energy, dominant style, style confidence, and max primitive count.

`analysis_timeline.csv` contains one row per rendered frame with time, resolved mode/palette/motion-style/hue/depth/object-density/mouse-depth/glow/color/scene-personality/response/stability/clarity/intensity/speed/quality, RMS/bands/flux/beat/bar/downbeat/phrase/build-tension/BPM/drop, arrangement section, persistent song-arc values (`songArc3D`, anticipation, impact, recovery, and continuity), cinematic camera continuity values (`cameraMotion3D` and `cameraContinuity3D`), style, adaptive profile weights, beat/section sensitivity, key confidence, scene name, scene intent, authored 2D primitive count, retained 2D primitive count, projected 3D primitive count, 3D dominance, role spread/balance/vocabulary/silhouette contrast, role crosstalk, role legibility, and total primitive count. Use it to debug whether visuals are following the music as expected, whether projected 3D is actually carrying the frame, whether the camera is easing rather than snapping, and whether separate musical parts look like different 3D object families instead of one generic swarm. Profile paths are recorded in `export_manifest.txt` and `share_manifest.json` so calibrated exports can be traced back to their source adaptation files.

## Batch Export

`VisualizerBatch.exe` renders every supported audio file in a directory into a consistent set of per-track export folders. It reuses the same renderer as `VisualizerExport.exe`, so `--look`, `--user-preset`, `--preset-library`, `--mode`, `--motion-style`, `--preset`, `--auto-scene`, `--style-profile`, `--sync-profile`, `--trails`, `--complexity`, `--hue-shift`, `--depth-3d`, `--object-density-3d`, `--interaction-depth`, `--lighting-glow`, `--color-impact`, `--scene-personality`, `--response-3d`, `--motion-stability`, `--pattern-clarity`, `--mp4`, and FFmpeg settings behave the same way.

```powershell
.\build\vs2022\Release\VisualizerBatch.exe --input-dir music --output captures\batch --recursive --look "Auto DJ Director" --seconds 45 --width 1920 --height 1080 --fps 60 --mp4
.\build\vs2022\Release\VisualizerBatch.exe --input-dir music --output captures\batch_user --user-preset late_set --seconds 30 --share
```

The batch root receives:

- `index.html`: browsable gallery linking each rendered track.
- `batch_manifest.json`: machine-readable summary with discovered/exported/failed counts, per-track metadata, and each track's `trackIntelligence` style/sync/phrase summary.
- `index.html`: browsable gallery with one preview thumbnail per exported track when share package generation is enabled, plus style, sync, and phrase columns.
- One deterministic per-track folder such as `001_first_track\`, containing ordinary `frame_000000.ppm`, `export_manifest.txt`, `analysis_timeline.csv`, and optional share/MP4 files.

Useful batch-specific options:

- `--input-dir DIR`: directory containing audio files.
- `--recursive`: scan nested folders.
- `--max-files N`: cap the number of rendered tracks for quick previews.
- `--mp4`: encode one MP4 per track using FFmpeg.
- `--share` / `--no-share`: enable or disable per-track share pages.

## 3D Scene QA Contact Sheet

Run the deterministic scene QA script when checking that Deltawave still renders a 3D-first scene family across the required music profiles:

```powershell
.\scripts\export_scene_qa.ps1
```

The script generates eight short WAV profiles (`silence`, `low_volume`, `techno`, `bass_drop`, `ambient`, `melodic`, `breakbeat`, and `dark_minimal`), exports each through `VisualizerExport.exe` with high-clarity 3D-first settings, writes per-profile share folders, and creates a root `scene_contact_sheet.bmp`, `scene_qa_summary.csv`, and `index.html`. It fails if any profile keeps retained 2D primitives, has weak projected 3D coverage, has weak material-surface share, drifts too far off-center, lacks explicit music-role 3D geometry, does not spread role districts clearly through depth, lets musical parts share the same role vocabulary/silhouette language, or misses the persistent song-arc response needed to make builds, drops, phrase lifts, and recoveries read as different 3D structures.

The QA also locks representative scene identity for the strongest profiles: techno must resolve to mechanical rhythm architecture, bass drops must stay `Quantum Tunnel` / `Heavy Bass`, breakbeat cuts must resolve to `Spectral Origami` / `Breakbeat`, ambient must expose a dominant space role, melodic material must expose melody/harmony roles, and dark minimal material must expose a shadow/monolith role. Use `scene_qa_summary.csv` and the per-profile `analysis_timeline.csv` files to compare final mode, motion style, role maxima, explicit role share, district spread, role vocabulary, role silhouette contrast, role crosstalk, role legibility, camera continuity, and song-arc anticipation/impact/recovery peaks.
