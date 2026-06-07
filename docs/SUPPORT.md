# Support

## No Audio Reaction

Start music in another Windows app and make sure it is playing through the default output device. WASAPI loopback follows the default render endpoint selected in Windows sound settings.

## Audio File Does Not Load

PCM and IEEE-float WAV files are supported by the portable loader. Windows builds also try Media Foundation for common local formats such as MP3, AAC/M4A, WMA, and any installed-codec formats. If a compressed file does not load, confirm Windows Media Player or another Media Foundation-based app can play it on the same machine.

## Build Environment

Use the verified Visual Studio 2022 build path:

```powershell
.\scripts\build.ps1
```

For MinGW/Ninja, use `.\scripts\build.ps1 mingw-release` only when the MinGW compiler can compile a trivial source. In this environment, `C:\msys64\mingw64\lib\gcc\x86_64-w64-mingw32\15.2.0\cc1plus.exe` currently exits with `0xC0000139`.

## Captures Are Large

Recording writes lossless PPM sequences. For deterministic offline exports, pass `--mp4 output.mp4` to `VisualizerExport.exe`. For live captures made with `R`, the capture folder also receives `index.html`, `preview.bmp`, `capture_manifest.json`, and `analysis_timeline.csv` with the active look name when recording stops. If FFmpeg is available on `PATH`, the app also writes `visualizer-live-capture.mp4`; otherwise use the command shown on the share page to convert the PPM frames.

## Offline Export

Use `VisualizerExport.exe` when you need fixed-frame deterministic output from an audio file. The exporter writes PPM files, `export_manifest.txt`, and `analysis_timeline.csv`; with `--mp4`, it also encodes the frame sequence to H.264 MP4 through FFmpeg. Add `--share` to generate `index.html`, `preview.bmp`, and `share_manifest.json` for a browsable export package. Export/share manifests include a track-intelligence summary with downbeats, phrase boundaries, average bar/phrase confidence, drop/phrase/build-tension peaks, dominant style, harmonic energy, and max primitive count, which is the quickest support signal before opening the full CSV timeline. If MP4 encoding fails, confirm `ffmpeg -version` works from the same shell or pass `--ffmpeg C:\path\to\ffmpeg.exe`.

## Sync Metrics

The HUD reports flux, drop, beat phase, bar phase, downbeat state, arrangement section, and tonal key confidence in addition to RMS/BPM/style. If those values stay near zero while music is playing, verify that the selected source is audible and that the track has enough transient or harmonic content for onset, section, and key detection.

## Adaptive Audio Profiles

The app saves learned style centroids and sync calibration per source under `profiles\sources\`. Live loopback uses `live_loopback.vizaudio` and `live_loopback.vizsync`; opened audio files receive stable source-specific stems based on their path. Press `V` or click `Reset Audio AI` to reset the active source from inside the app. You can also delete a source's `.vizaudio` file to reset style adaptation, or its `.vizsync` file to reset beat and section sensitivity for that source.

The HUD shows current Audio AI style adaptation, sync adaptation, beat sensitivity, section sensitivity, phrase phase, and build tension. A live capture records the style/sync profile file names in `capture_manifest.json`, and `analysis_timeline.csv` records per-frame adaptation, sensitivity, beat phase, bar phase, downbeat, phrase, and build-tension values.

`profiles\adaptive_style_profile.vizaudio` is still accepted as a legacy fallback for live loopback, but new profile writes go to `profiles\sources\`.

On wide windows, the live inspector panel shows the active source, decoded audio format, active look, user preset library count, and exact `.vizaudio` / `.vizsync` filenames being used. Check it first when a track seems to be using the wrong learned profile, when confirming that a loaded file is advancing through playback, or when checking whether user presets were discovered.

## Environment Reactivity

Use `E` or the `Env` panel toggle to enable or disable local environmental influence. Live mode derives it from local time-of-day and pointer motion; offline export uses deterministic `--time-of-day N` so shared renders are reproducible.

## Color Customization

Use `C` or the palette buttons for base palettes, then use `U` or the `Hue Shift` slider to rotate that palette without changing the selected mode. Offline exports can reproduce the same look with `--hue-shift N`.

When Auto Scene is enabled, detected tonal key can steer hue shift from the selected base value. Export metadata records both the requested hue shift and the final rendered hue shift.

## Performance

If visuals stutter, leave adaptive quality enabled, lower the `Quality` slider, or lower `Complexity` to reduce procedural density while keeping the selected mode and palette. The live HUD separates `Core` analyzer/geometry time from `D2D` renderer-present time. Use `VisualizerBenchmark.exe` to check core analyzer and geometry throughput without the Windows renderer in the loop, and use a short live capture when you need a saved `capture_manifest.json` timing breakdown.

## Support Bundle

Use `VisualizerSupport.exe` to package the metadata needed for a bug report without copying large frame, video, or audio payloads:

```powershell
.\build\vs2022\Release\VisualizerSupport.exe --workspace . --output artifacts\support_bundle_current --max-captures 5 --max-artifacts 5
```

The bundle contains `support_manifest.json` for machine-readable diagnostics and `support_summary.txt` for a quick human pass. It copies small adaptive profiles (`.vizaudio`, `.vizsync`, `.vizpreset`), capture/export/batch manifests, and timelines up to `--max-file-bytes` bytes. PPM frame sequences, MP4 files, WAV/MP3/M4A inputs, and other media payloads are summarized by count and bytes but are not copied.

Useful options:

- `--workspace DIR`: collect diagnostics from a specific Visualizer workspace.
- `--output DIR`: write to a deterministic folder instead of `artifacts\support_bundle_YYYYMMDD_HHMMSS`.
- `--max-captures N`: limit recent live capture folders included in the manifest.
- `--max-artifacts N`: limit recent offline export/artifact folders included in the manifest.
- `--max-file-bytes N`: change the small-file copy threshold.
- `--no-copy`: write only the support manifest and summary.

## Presets

Use `B` / `N` or the `Look <` / `Look >` panel buttons to cycle built-in curated looks without opening a file. Offline exports can use the same bank with `--look "Acid Geometry"` or a short alias such as `--look 4d`, `--look weave`, `--look loom`, or `--look chladni`; `--list-looks` prints the full list.

Use `K` or the `Save User` panel button to save the current look into `profiles\presets\`. Use `[` / `]` or the `User <` / `User >` panel buttons to cycle that library during playback. The app sanitizes preset filenames and adds numeric suffixes for duplicates, so quick saves do not overwrite earlier versions. Offline and batch exports can reuse those live-saved looks with `--user-preset NAME`; `--list-user-presets` shows the display names and filenames that can be used.

`S` and `P` still use standard file dialogs for saving or loading a chosen `.vizpreset` anywhere. Presets are plain text files. They store mode, palette, hue shift, 3D depth, 3D object density, mouse depth, lighting glow, color impact, scene personality, complexity, intensity, speed, HUD, trails, interaction, environment reactivity, adaptive quality, and Auto Scene settings. If a preset contains an unknown value, the loader keeps the existing default for that field instead of failing the entire load.
