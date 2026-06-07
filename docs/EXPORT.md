# Offline Export

`VisualizerExport.exe` renders an audio file into a deterministic PPM frame sequence. It can also invoke FFmpeg to encode those frames into a share-ready H.264 MP4 and can write a dependency-free `preview.bmp` contact sheet for quick browser inspection. This is better than live recording when you need reproducible output, fixed frame rate, high-resolution export independent of playback timing, or an export manifest with peak RMS, BPM, beat count, downbeat/bar/phrase-lock summary, phrase boundaries, drop/phrase/build-tension peaks, dominant style, harmonic energy, primitive peak, and detected tonal key metadata.

Live recordings use the same frame sequence and FFmpeg encoder path when you stop recording with `R`; successful captures include `visualizer-live-capture.mp4` beside `index.html`.

## Example

```powershell
.\build\vs2022\Release\VisualizerExport.exe --input song.wav --output captures\offline --width 1920 --height 1080 --fps 60 --look "Acid Geometry" --trails
.\build\vs2022\Release\VisualizerExport.exe --input song.mp3 --output captures\offline --mp4 captures\visualizer.mp4 --share --width 1920 --height 1080 --fps 60 --crf 18 --auto-scene --depth-3d 1.0 --color-impact 1.0
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
- `--hue-shift N`: shift the selected palette hue from `0.0` to `1.0`; default `0.0`.
- `--depth-3d N`: set faux-3D perspective/parallax strength from `0.0` flat to `1.0` deep; alias `--depth` also works.
- `--color-impact N`: set palette personality strength from `0.0` subtle to `1.0` intense; alias `--color` also works.
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

`--share` writes a portable `index.html` showcase, `preview.bmp` contact sheet, and `share_manifest.json` metadata file into the export folder. When `--mp4` is also used, the page embeds the MP4; without `--mp4`, the page still shows the BMP preview, summarizes the frame sequence, and links the metadata. Export and share metadata include requested and final depth/color settings, the strongest detected arrangement section, plus a `trackIntelligence` object summarizing downbeats, phrase boundaries, average bar and phrase confidence, peak downbeat/drop/phrase/build-tension intensity, average harmonic energy, dominant style, style confidence, and max primitive count.

`analysis_timeline.csv` contains one row per rendered frame with time, resolved mode/palette/hue/depth/color/intensity/speed/quality, RMS/bands/flux/beat/bar/downbeat/phrase/build-tension/BPM/drop, arrangement section, style, adaptive profile weights, beat/section sensitivity, key confidence, and primitive count. Use it to debug whether visuals are following the music as expected. Profile paths are recorded in `export_manifest.txt` and `share_manifest.json` so calibrated exports can be traced back to their source adaptation files.

## Batch Export

`VisualizerBatch.exe` renders every supported audio file in a directory into a consistent set of per-track export folders. It reuses the same renderer as `VisualizerExport.exe`, so `--look`, `--user-preset`, `--preset-library`, `--mode`, `--preset`, `--auto-scene`, `--style-profile`, `--sync-profile`, `--trails`, `--complexity`, `--hue-shift`, `--depth-3d`, `--color-impact`, `--mp4`, and FFmpeg settings behave the same way.

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
