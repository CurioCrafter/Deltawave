# Roadmap

## Next

- Add GPU shader and post-processing backends for heavier procedural effects.
- Add MP3, AAC, and FLAC decode support behind a narrow decoder interface.
- Add richer in-window panels for file metadata, export progress, and deeper preset management such as rename/delete workflows.
- Add GPU-side render diagnostics once shader or post-processing backends exist.

## Audio Intelligence

- Add richer profile browser/editor UI for comparing, exporting, and pruning per-source adaptation files.
- Expand phrase intelligence from current 16-beat phase/build-tension tracking into longer-form breakdown/build/drop prediction.

## Cross-Platform

- Introduce SDL or GLFW once the Windows-native prototype stabilizes.
- Add CoreAudio and PipeWire/PulseAudio capture backends.
- Keep `visualizer_core` dependency-light unless a library materially improves quality.
