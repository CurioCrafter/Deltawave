#include "TestHarness.hpp"
#include "Visualizer/Export/BatchExporter.hpp"
#include "Visualizer/Export/CapturePackage.hpp"
#include "Visualizer/Export/OfflineExporter.hpp"
#include "Visualizer/Export/VideoEncoder.hpp"
#include "Visualizer/Performance/FramePerformanceTracker.hpp"
#include "Visualizer/UI/ControlPanel.hpp"
#include "Visualizer/UI/RuntimeInspector.hpp"
#include "Visualizer/Visualization/FrameRecorder.hpp"
#include "Visualizer/Visualization/PresetStore.hpp"
#include "Visualizer/Visualization/SceneDirector.hpp"
#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace viz::tests {

void analyzerReportsSilence();
void analyzerFindsBassAndStereo();
void analyzerDetectsChromaAndKey();
void wavLoaderReadsGeneratedPcm();
void audioFileLoaderReadsGeneratedWav();
void adaptiveStyleModelPredictsTechno();
void adaptiveStyleModelPersistsProfile();
void audioAnalyzerLoadsAndSavesStyleProfile();
void audioSyncProfilePersistsSensitivity();
void audioAnalyzerLoadsAndSavesSyncProfile();
void analyzerReportsAdvancedSyncMetrics();
void analyzerTracksBarPhaseAndDownbeats();
void analyzerDetectsArrangementSections();
void supportBundleWritesDiagnosticsWithoutCopyingLargeMedia();

namespace {

constexpr float kPi = 3.14159265358979323846f;

void writeU16(std::ofstream& output, std::uint16_t value)
{
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
}

void writeU32(std::ofstream& output, std::uint32_t value)
{
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
    output.put(static_cast<char>((value >> 16U) & 0xFFU));
    output.put(static_cast<char>((value >> 24U) & 0xFFU));
}

void writeExportTestWav(const std::filesystem::path& path, int sampleRate, int frames)
{
    constexpr int channels = 2;
    constexpr int bits = 16;
    constexpr int blockAlign = channels * bits / 8;
    const int dataBytes = frames * blockAlign;

    std::ofstream output(path, std::ios::binary);
    output.write("RIFF", 4);
    writeU32(output, 36U + static_cast<std::uint32_t>(dataBytes));
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    writeU32(output, 16);
    writeU16(output, 1);
    writeU16(output, channels);
    writeU32(output, static_cast<std::uint32_t>(sampleRate));
    writeU32(output, static_cast<std::uint32_t>(sampleRate * blockAlign));
    writeU16(output, blockAlign);
    writeU16(output, bits);
    output.write("data", 4);
    writeU32(output, static_cast<std::uint32_t>(dataBytes));

    for (int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const auto left = static_cast<std::int16_t>(std::sin(2.0f * kPi * 128.0f * t) * 19000.0f);
        const auto right = static_cast<std::int16_t>(std::sin(2.0f * kPi * 512.0f * t) * 15000.0f);
        writeU16(output, static_cast<std::uint16_t>(left));
        writeU16(output, static_cast<std::uint16_t>(right));
    }
}

std::uint64_t sumPpmPixelBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::string magic;
    int width = 0;
    int height = 0;
    int maximum = 0;
    input >> magic >> width >> height >> maximum;
    input.get();
    require(magic == "P6", "test helper expected a binary PPM frame");
    require(width > 0 && height > 0 && maximum == 255, "test helper expected a valid PPM header");

    std::uint64_t sum = 0;
    char value = 0;
    while (input.get(value)) {
        sum += static_cast<unsigned char>(value);
    }
    return sum;
}

float colorDistance(ColorRGBA a, ColorRGBA b)
{
    return std::fabs(a.r - b.r) + std::fabs(a.g - b.g) + std::fabs(a.b - b.b);
}

float averageRingRadius(const GeometryFrame& frame)
{
    if (frame.rings.empty()) {
        return 0.0f;
    }
    float total = 0.0f;
    for (const Ring& ring : frame.rings) {
        total += ring.radius;
    }
    return total / static_cast<float>(frame.rings.size());
}

float averagePolylinePointDistance(const GeometryFrame& frame, Vec2 center)
{
    float total = 0.0f;
    int count = 0;
    for (const Polyline& polyline : frame.polylines) {
        for (Vec2 point : polyline.points) {
            const float dx = point.x - center.x;
            const float dy = point.y - center.y;
            total += std::sqrt(dx * dx + dy * dy);
            ++count;
        }
    }
    return count > 0 ? total / static_cast<float>(count) : 0.0f;
}

AudioMetrics syntheticMetrics()
{
    AudioMetrics metrics{};
    metrics.rms = 0.42f;
    metrics.peak = 0.9f;
    metrics.bass = 0.7f;
    metrics.lowMid = 0.45f;
    metrics.mid = 0.36f;
    metrics.highMid = 0.52f;
    metrics.treble = 0.6f;
    metrics.spectralCentroid = 0.34f;
    metrics.stereoWidth = 0.4f;
    metrics.onset = 0.2f;
    metrics.beatConfidence = 0.8f;
    metrics.bpm = 128.0f;
    metrics.beat = true;
    for (std::size_t i = 0; i < metrics.spectrum.size(); ++i) {
        metrics.spectrum[i] = 0.15f + 0.75f * (static_cast<float>(i % 9) / 8.0f);
    }
    return metrics;
}

void geometryModesProduceShapes()
{
    VisualizerEngine engine;
    VisualSettings settings;
    const AudioMetrics metrics = syntheticMetrics();

    const VisualMode modes[] = {
        VisualMode::QuantumTunnel,
        VisualMode::TechnoMandala,
        VisualMode::LissajousMesh,
        VisualMode::FrequencyBloom,
        VisualMode::FractalCathedral,
        VisualMode::PolyrhythmLattice,
        VisualMode::SpectralOrigami,
        VisualMode::ChromaKaleidoscope,
        VisualMode::HyperspacePolytope,
        VisualMode::PhaseWeave,
        VisualMode::ResonanceTessellation,
        VisualMode::NeuralConstellation,
        VisualMode::CymaticInterference
    };

    for (VisualMode mode : modes) {
        settings.mode = mode;
        const GeometryFrame frame = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 1.25);
        const std::size_t primitiveCount = frame.rings.size() + frame.beams.size() +
                                           frame.particles.size() + frame.polylines.size();
        require(primitiveCount > 20, "mode should generate a dense geometry frame");
        require(frame.background.a == 1.0f, "background should be opaque");
        require(frame.flash > 0.0f, "beat frame should include flash");
    }
}

void advancedModesReactToSyncMetrics()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.interactiveField = false;
    settings.qualityScale = 0.8f;

    AudioMetrics calm = syntheticMetrics();
    calm.beat = false;
    calm.beatConfidence = 0.0f;
    calm.bandOnsets = {};
    calm.dropIntensity = 0.0f;
    calm.phraseIntensity = 0.0f;
    calm.spectralFlux = 0.0f;

    AudioMetrics intense = calm;
    intense.beat = true;
    intense.beatConfidence = 0.92f;
    intense.bandOnsets = {0.65f, 0.44f, 0.28f, 0.58f, 0.35f};
    intense.dropIntensity = 0.72f;
    intense.phraseIntensity = 0.66f;
    intense.phraseBoundary = true;
    intense.phrasePhase = 0.93f;
    intense.phraseConfidence = 0.76f;
    intense.buildTension = 0.82f;
    intense.spectralFlux = 0.48f;

    const VisualMode modes[] = {
        VisualMode::FractalCathedral,
        VisualMode::PolyrhythmLattice,
        VisualMode::PhaseWeave,
        VisualMode::ResonanceTessellation,
        VisualMode::NeuralConstellation,
        VisualMode::CymaticInterference
    };

    for (VisualMode mode : modes) {
        settings.mode = mode;
        const GeometryFrame calmFrame = engine.buildFrame(calm, settings, 1280.0f, 720.0f, 2.0);
        const GeometryFrame intenseFrame = engine.buildFrame(intense, settings, 1280.0f, 720.0f, 2.0);
        require(countPrimitives(intenseFrame) > countPrimitives(calmFrame),
                "advanced modes should add geometry when sync metrics intensify");
        require(intenseFrame.flash > calmFrame.flash,
                "advanced modes should preserve drop/beat flash response");
    }
}

void sceneDirectorAdaptsVisualSettings()
{
    SceneDirector director;
    VisualSettings base;
    base.mode = VisualMode::LissajousMesh;
    base.palette = Palette::MonochromeLaser;
    base.hueShift = 0.1f;
    base.intensity = 1.0f;
    base.speed = 1.0f;

    AudioMetrics techno = syntheticMetrics();
    techno.style = AudioStyle::Techno;
    techno.styleConfidence = 0.9f;
    techno.bpm = 132.0f;
    techno.beatConfidence = 0.86f;
    techno.phraseIntensity = 0.2f;
    techno.keyIndex = 4;
    techno.keyMode = MusicalMode::Major;
    techno.keyConfidence = 0.9f;
    techno.harmonicEnergy = 0.52f;

    VisualSettings manual = director.resolve(base, techno, 0.0);
    require(manual.mode == base.mode, "scene director should preserve manual mode when auto scene is off");
    require(manual.palette == base.palette, "scene director should preserve manual palette when auto scene is off");
    require(std::fabs(manual.hueShift - base.hueShift) < 0.001f,
            "scene director should preserve manual hue shift when auto scene is off");

    base.autoScene = true;
    (void)director.resolve(base, techno, 0.1);
    VisualSettings directed = director.resolve(base, techno, 0.8);
    require(directed.mode == VisualMode::PolyrhythmLattice,
            "techno metrics should direct toward the polyrhythm lattice");
    require(directed.palette == Palette::NeonVoltage || directed.palette == Palette::AcidAurora,
            "techno metrics should direct toward an electronic palette");
    require(directed.intensity > base.intensity, "high-energy techno should raise intensity");
    require(directed.hueShift > base.hueShift + 0.02f,
            "confident tonal key should steer auto scene hue shift");

    AudioMetrics hyperTechno = syntheticMetrics();
    hyperTechno.style = AudioStyle::Techno;
    hyperTechno.spectralFlux = 0.62f;
    hyperTechno.stereoWidth = 0.58f;
    hyperTechno.beatConfidence = 0.88f;
    SceneDirector hyperDirector;
    (void)hyperDirector.resolve(base, hyperTechno, 0.1);
    directed = hyperDirector.resolve(base, hyperTechno, 0.9);
    require(directed.mode == VisualMode::HyperspacePolytope,
            "wide high-flux techno should direct toward hyperspace polytope");

    AudioMetrics flowBuild = syntheticMetrics();
    flowBuild.style = AudioStyle::Wide;
    flowBuild.spectralFlux = 0.56f;
    flowBuild.stereoWidth = 0.74f;
    flowBuild.section = ArrangementSection::Build;
    flowBuild.sectionConfidence = 0.82f;
    flowBuild.sectionProgress = 0.64f;
    flowBuild.keyConfidence = 0.25f;
    flowBuild.harmonicEnergy = 0.38f;
    SceneDirector flowDirector;
    (void)flowDirector.resolve(base, flowBuild, 0.1);
    directed = flowDirector.resolve(base, flowBuild, 0.9);
    require(directed.mode == VisualMode::PhaseWeave,
            "wide high-flux build metrics should direct toward phase weave");

    AudioMetrics harmonicBuild = syntheticMetrics();
    harmonicBuild.style = AudioStyle::Wide;
    harmonicBuild.spectralFlux = 0.43f;
    harmonicBuild.stereoWidth = 0.36f;
    harmonicBuild.section = ArrangementSection::Build;
    harmonicBuild.sectionConfidence = 0.88f;
    harmonicBuild.sectionProgress = 0.56f;
    harmonicBuild.keyIndex = 2;
    harmonicBuild.keyMode = MusicalMode::Minor;
    harmonicBuild.keyConfidence = 0.82f;
    harmonicBuild.harmonicEnergy = 0.78f;
    harmonicBuild.bandOnsets = {0.12f, 0.18f, 0.52f, 0.46f, 0.2f};
    SceneDirector tessellationDirector;
    (void)tessellationDirector.resolve(base, harmonicBuild, 0.1);
    directed = tessellationDirector.resolve(base, harmonicBuild, 0.9);
    require(directed.mode == VisualMode::ResonanceTessellation,
            "harmonic build metrics should direct toward resonance tessellation");

    AudioMetrics neuralGroove = syntheticMetrics();
    neuralGroove.style = AudioStyle::Wide;
    neuralGroove.section = ArrangementSection::Groove;
    neuralGroove.sectionConfidence = 0.82f;
    neuralGroove.stereoWidth = 0.58f;
    neuralGroove.spectralFlux = 0.32f;
    neuralGroove.phraseIntensity = 0.38f;
    neuralGroove.barConfidence = 0.72f;
    neuralGroove.downbeat = true;
    neuralGroove.downbeatConfidence = 0.86f;
    neuralGroove.keyIndex = 9;
    neuralGroove.keyMode = MusicalMode::Minor;
    neuralGroove.keyConfidence = 0.64f;
    neuralGroove.harmonicEnergy = 0.48f;
    SceneDirector neuralDirector;
    (void)neuralDirector.resolve(base, neuralGroove, 0.1);
    directed = neuralDirector.resolve(base, neuralGroove, 0.9);
    require(directed.mode == VisualMode::NeuralConstellation,
            "bar-locked harmonic groove metrics should direct toward neural constellation");

    AudioMetrics phraseBuild = syntheticMetrics();
    phraseBuild.style = AudioStyle::Techno;
    phraseBuild.styleConfidence = 0.82f;
    phraseBuild.section = ArrangementSection::Build;
    phraseBuild.sectionConfidence = 0.72f;
    phraseBuild.sectionProgress = 0.48f;
    phraseBuild.phrasePhase = 0.86f;
    phraseBuild.phraseConfidence = 0.78f;
    phraseBuild.buildTension = 0.88f;
    phraseBuild.dropIntensity = 0.18f;
    phraseBuild.spectralFlux = 0.36f;
    phraseBuild.stereoWidth = 0.28f;
    SceneDirector phraseDirector;
    (void)phraseDirector.resolve(base, phraseBuild, 0.1);
    directed = phraseDirector.resolve(base, phraseBuild, 0.9);
    require(directed.intensity > base.intensity * 1.1f,
            "phrase build tension should lift auto-scene intensity before the drop");
    require(directed.speed > base.speed,
            "phrase build tension should accelerate auto-scene motion before the drop");

    AudioMetrics cymaticBuild = syntheticMetrics();
    cymaticBuild.style = AudioStyle::Wide;
    cymaticBuild.section = ArrangementSection::Build;
    cymaticBuild.sectionConfidence = 0.84f;
    cymaticBuild.sectionProgress = 0.62f;
    cymaticBuild.spectralFlux = 0.38f;
    cymaticBuild.stereoWidth = 0.42f;
    cymaticBuild.phraseConfidence = 0.78f;
    cymaticBuild.phraseIntensity = 0.44f;
    cymaticBuild.buildTension = 0.74f;
    cymaticBuild.dropIntensity = 0.14f;
    cymaticBuild.keyIndex = 2;
    cymaticBuild.keyMode = MusicalMode::Minor;
    cymaticBuild.keyConfidence = 0.86f;
    cymaticBuild.harmonicEnergy = 0.82f;
    SceneDirector cymaticDirector;
    (void)cymaticDirector.resolve(base, cymaticBuild, 0.1);
    directed = cymaticDirector.resolve(base, cymaticBuild, 0.9);
    require(directed.mode == VisualMode::CymaticInterference,
            "harmonic high-tension builds should direct toward cymatic interference");

    AudioMetrics harmonic = syntheticMetrics();
    harmonic.style = AudioStyle::Wide;
    harmonic.keyIndex = 7;
    harmonic.keyMode = MusicalMode::Minor;
    harmonic.keyConfidence = 0.92f;
    harmonic.harmonicEnergy = 0.88f;
    harmonic.stereoWidth = 0.58f;
    harmonic.phraseIntensity = 0.42f;
    SceneDirector harmonicDirector;
    (void)harmonicDirector.resolve(base, harmonic, 0.1);
    directed = harmonicDirector.resolve(base, harmonic, 0.9);
    require(directed.mode == VisualMode::ChromaKaleidoscope,
            "confident harmonic metrics should direct toward chroma kaleidoscope");

    AudioMetrics ambient = syntheticMetrics();
    ambient.style = AudioStyle::Ambient;
    ambient.rms = 0.18f;
    ambient.beatConfidence = 0.0f;
    ambient.phraseIntensity = 0.72f;
    ambient.stereoWidth = 0.52f;
    directed = director.resolve(base, ambient, 2.2);
    require(directed.mode == VisualMode::FractalCathedral,
            "ambient phrase-heavy metrics should direct toward fractal cathedral");
    require(directed.palette == Palette::OceanicPulse,
            "ambient metrics should direct toward oceanic palette");

    AudioMetrics bright = syntheticMetrics();
    bright.style = AudioStyle::Bright;
    bright.treble = 0.82f;
    bright.spectralFlux = 0.52f;
    directed = director.resolve(base, bright, 4.0);
    require(directed.mode == VisualMode::SpectralOrigami,
            "bright high-flux metrics should direct toward spectral origami");
}

void sceneDirectorEmitsModeTransitionPulse()
{
    SceneDirector director;
    VisualSettings base;
    base.mode = VisualMode::LissajousMesh;
    base.palette = Palette::MonochromeLaser;
    base.autoScene = true;

    AudioMetrics drop = syntheticMetrics();
    drop.style = AudioStyle::BassHeavy;
    drop.styleConfidence = 0.94f;
    drop.bass = 0.95f;
    drop.dropIntensity = 0.84f;
    drop.beatConfidence = 0.92f;
    drop.beatPhase = 0.04f;

    VisualSettings directed = director.resolve(base, drop, 0.0);
    require(directed.mode == VisualMode::QuantumTunnel,
            "strong bass drops should switch Auto Scene into quantum tunnel");
    require(directed.sceneTransition > 0.65f,
            "mode switches should emit a strong transition pulse");
    require(directed.sceneTransitionProgress < 0.02f,
            "fresh transition pulse should start near zero progress");

    VisualSettings settled = director.resolve(base, drop, 1.4);
    require(settled.sceneTransition < 0.02f,
            "scene transition pulse should decay after the switch window");
    require(settled.sceneTransitionProgress > 0.99f,
            "completed transition should report full progress");

    base.autoScene = false;
    VisualSettings manual = director.resolve(base, drop, 1.5);
    require(manual.sceneTransition == 0.0f,
            "manual scene mode should suppress transition pulses");
    require(manual.sceneTransitionProgress == 1.0f,
            "manual scene mode should report completed transition progress");
}

void sceneTransitionAddsMorphGeometry()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::FrequencyBloom;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 1.0f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.beat = false;
    metrics.dropIntensity = 0.0f;
    metrics.phraseIntensity = 0.0f;
    metrics.bandOnsets = {};

    const GeometryFrame neutral = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 1.0);
    settings.sceneTransition = 0.85f;
    settings.sceneTransitionProgress = 0.25f;
    const GeometryFrame morph = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 1.0);

    require(countPrimitives(morph) > countPrimitives(neutral) + 20,
            "scene transitions should add a visible morph overlay");
    require(morph.polylines.size() > neutral.polylines.size(),
            "scene transitions should add folded linework");
    require(morph.flash > neutral.flash,
            "scene transitions should add a flash accent");
}

void hyperspacePolytopeRespondsToDimensionalEnergy()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::HyperspacePolytope;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.9f;

    AudioMetrics calm = syntheticMetrics();
    calm.beat = false;
    calm.beatConfidence = 0.0f;
    calm.dropIntensity = 0.0f;
    calm.spectralFlux = 0.0f;
    calm.stereoWidth = 0.05f;
    calm.harmonicEnergy = 0.0f;
    calm.chroma.fill(0.02f);

    AudioMetrics intense = calm;
    intense.beat = true;
    intense.beatConfidence = 0.92f;
    intense.dropIntensity = 0.78f;
    intense.spectralFlux = 0.62f;
    intense.stereoWidth = 0.7f;
    intense.harmonicEnergy = 0.82f;
    intense.chroma[0] = 1.0f;
    intense.chroma[4] = 0.7f;
    intense.chroma[7] = 0.8f;

    const GeometryFrame calmFrame = engine.buildFrame(calm, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame intenseFrame = engine.buildFrame(intense, settings, 1280.0f, 720.0f, 2.0);
    require(!calmFrame.polylines.empty(), "hyperspace polytope should generate projected edge linework");
    require(intenseFrame.particles.size() > calmFrame.particles.size(),
            "drop and harmonic energy should light up edge particles");
    require(intenseFrame.flash > calmFrame.flash,
            "intense hyperspace frames should preserve flash response");
}

void arrangementSectionsAddVisualAccents()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::FrequencyBloom;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 1.0f;

    AudioMetrics neutral = syntheticMetrics();
    neutral.beat = false;
    neutral.beatConfidence = 0.0f;
    neutral.dropIntensity = 0.0f;
    neutral.phraseIntensity = 0.0f;
    neutral.section = ArrangementSection::Silence;
    neutral.sectionConfidence = 1.0f;

    AudioMetrics build = neutral;
    build.section = ArrangementSection::Build;
    build.sectionConfidence = 0.82f;
    build.sectionProgress = 0.72f;
    build.spectralFlux = 0.58f;
    build.phraseIntensity = 0.64f;
    const GeometryFrame neutralFrame = engine.buildFrame(neutral, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame buildFrame = engine.buildFrame(build, settings, 1280.0f, 720.0f, 2.0);
    require(countPrimitives(buildFrame) > countPrimitives(neutralFrame),
            "build sections should add arrangement accent geometry");
    require(buildFrame.polylines.size() > neutralFrame.polylines.size(),
            "build sections should add rising linework");

    AudioMetrics drop = neutral;
    drop.section = ArrangementSection::Drop;
    drop.sectionConfidence = 0.9f;
    drop.dropIntensity = 0.76f;
    drop.beatConfidence = 0.86f;
    const GeometryFrame dropFrame = engine.buildFrame(drop, settings, 1280.0f, 720.0f, 2.0);
    require(dropFrame.flash > neutralFrame.flash, "drop sections should add flash accents");
    require(dropFrame.particles.size() > neutralFrame.particles.size(), "drop sections should add burst particles");
}

void recorderWritesPpmFrame()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::FrequencyBloom;
    const GeometryFrame frame = engine.buildFrame(syntheticMetrics(), settings, 160.0f, 90.0f, 2.0);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_capture_test";
    std::filesystem::remove_all(root);

    FrameRecorder recorder;
    std::string error;
    require(recorder.start(root, 160, 90, error), "recorder should start: " + error);
    require(recorder.writeFrame(frame, error), "recorder should write frame: " + error);
    require(recorder.frameCount() == 1, "frame count should advance");

    const std::filesystem::path framePath = recorder.sessionPath() / "frame_000000.ppm";
    require(std::filesystem::exists(framePath), "PPM frame should exist");
    require(std::filesystem::file_size(framePath) > 160U * 90U * 3U, "PPM should contain header and pixels");
    recorder.stop();
    std::filesystem::remove_all(root);
}

void recorderTrailsPersistPreviousFrame()
{
    GeometryFrame first;
    first.background = ColorRGBA{0.0f, 0.0f, 0.0f, 1.0f};
    first.particles.push_back(Particle{Vec2{16.0f, 16.0f}, 5.0f, ColorRGBA{1.0f, 1.0f, 1.0f, 1.0f}});

    GeometryFrame second;
    second.background = first.background;

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_trails_test";
    std::filesystem::remove_all(root);

    FrameRecorder recorder;
    std::string error;
    FrameRenderOptions renderOptions;
    renderOptions.trails = true;
    renderOptions.trailPersistence = 0.9f;
    require(recorder.startSession(root, 32, 32, error), "trail recorder should start: " + error);
    require(recorder.writeFrame(first, renderOptions, error), "first trail frame should write: " + error);
    require(recorder.writeFrame(second, renderOptions, error), "second trail frame should write: " + error);
    require(recorder.frameCount() == 2, "trail recorder should write two frames");
    require(sumPpmPixelBytes(root / "frame_000001.ppm") > 0,
            "trail frame should retain decayed pixels from the previous frame");

    recorder.stop();
    std::filesystem::remove_all(root);
}

void liveCapturePackageWritesShareMetadata()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::ChromaKaleidoscope;
    settings.palette = Palette::OceanicPulse;
    settings.hueShift = 0.25f;
    settings.depth3D = 0.81f;
    settings.colorImpact = 0.86f;
    settings.autoScene = true;
    const AudioMetrics metrics = syntheticMetrics();
    const GeometryFrame frame = engine.buildFrame(metrics, settings, 96.0f, 54.0f, 1.0);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_live_capture_package_test";
    std::filesystem::remove_all(root);

    FrameRecorder recorder;
    std::string error;
    require(recorder.startSession(root, 96, 54, error), "live capture recorder should start: " + error);
    FrameRenderOptions renderOptions;
    renderOptions.trails = true;
    require(recorder.writeFrame(frame, renderOptions, error), "live capture recorder should write frame: " + error);
    recorder.stop();

    CapturePackage package;
    package.sessionPath = root;
    package.styleProfilePath = root / "source.vizaudio";
    package.syncProfilePath = root / "source.vizsync";
    package.sourceLabel = "test loopback";
    package.lookName = "Warehouse Strobe";
    package.requestedSettings = settings;
    package.finalSettings = settings;
    package.finalSettings.complexity = 1.5f;
    package.finalSettings.depth3D = 0.93f;
    package.finalSettings.colorImpact = 0.95f;
    package.width = 96;
    package.height = 54;
    package.framesWritten = recorder.frameCount();
    package.durationSeconds = 0.5;
    package.averageFrameMs = 16.667;
    package.averageAnalysisMs = 1.125;
    package.averageGeometryMs = 2.250;
    package.averageRenderMs = 5.750;
    package.averageRecordMs = 0.875;
    package.peakRms = metrics.rms;
    package.estimatedBpm = metrics.bpm;
    package.beatsDetected = metrics.beat ? 1 : 0;
    package.downbeatsDetected = 1;
    package.phraseBoundariesDetected = 1;
    package.averagePhraseConfidence = 0.66;
    package.peakBuildTension = 0.73f;
    package.detectedKeyIndex = 0;
    package.detectedKeyMode = MusicalMode::Major;
    package.keyConfidence = 0.75f;
    package.dominantSection = ArrangementSection::Drop;
    package.sectionConfidence = 0.88f;
    package.timelinePath = root / "analysis_timeline.csv";
    package.timelineWritten = true;
    package.videoEncoded = true;
    package.videoPath = root / "visualizer-live-capture.mp4";
    package.videoBytes = 123456;
    require(writeCapturePackage(package, error), "live capture package should write: " + error);
    require(std::filesystem::exists(root / "capture_manifest.json"), "live capture manifest should exist");
    require(std::filesystem::exists(root / "index.html"), "live capture share page should exist");
    require(std::filesystem::exists(root / "preview.bmp"), "live capture preview image should exist");
    require(std::filesystem::file_size(root / "preview.bmp") > 54U,
            "live capture preview image should contain BMP pixels");

    {
        std::ifstream manifest(root / "capture_manifest.json");
        const std::string manifestText((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
        require(manifestText.find("\"type\": \"liveCapture\"") != std::string::npos,
                "live capture manifest should identify capture type");
        require(manifestText.find("\"look\": \"Warehouse Strobe\"") != std::string::npos,
                "live capture manifest should include look name");
        require(manifestText.find("\"finalMode\": \"Chroma Kaleidoscope\"") != std::string::npos,
                "live capture manifest should include final mode");
        require(manifestText.find("\"framesWritten\": 1") != std::string::npos,
                "live capture manifest should include frame count");
        require(manifestText.find("\"previewImage\": \"preview.bmp\"") != std::string::npos,
                "live capture manifest should include preview image");
        require(manifestText.find("\"previewFrames\": 1") != std::string::npos,
                "live capture manifest should include preview frame count");
        require(manifestText.find("\"downbeatsDetected\": 1") != std::string::npos,
                "live capture manifest should include downbeat count");
        require(manifestText.find("\"phraseBoundariesDetected\": 1") != std::string::npos,
                "live capture manifest should include phrase boundary count");
        require(manifestText.find("\"averagePhraseConfidence\": 0.660") != std::string::npos,
                "live capture manifest should include phrase confidence");
        require(manifestText.find("\"peakBuildTension\": 0.730") != std::string::npos,
                "live capture manifest should include build tension");
        require(manifestText.find("\"detectedKey\": \"C Major\"") != std::string::npos,
                "live capture manifest should include key metadata");
        require(manifestText.find("\"finalComplexity\": 1.500") != std::string::npos,
                "live capture manifest should include final complexity");
        require(manifestText.find("\"requestedDepth3D\": 0.810") != std::string::npos,
                "live capture manifest should include requested 3D depth");
        require(manifestText.find("\"finalDepth3D\": 0.930") != std::string::npos,
                "live capture manifest should include final 3D depth");
        require(manifestText.find("\"requestedColorImpact\": 0.860") != std::string::npos,
                "live capture manifest should include requested color impact");
        require(manifestText.find("\"finalColorImpact\": 0.950") != std::string::npos,
                "live capture manifest should include final color impact");
        require(manifestText.find("\"dominantSection\": \"Drop\"") != std::string::npos,
                "live capture manifest should include dominant section");
        require(manifestText.find("\"sectionConfidence\": 0.880") != std::string::npos,
                "live capture manifest should include section confidence");
        require(manifestText.find("\"timeline\": \"analysis_timeline.csv\"") != std::string::npos,
                "live capture manifest should include timeline path");
        require(manifestText.find("\"timelineWritten\": true") != std::string::npos,
                "live capture manifest should include timeline status");
        require(manifestText.find("\"styleProfile\":") != std::string::npos,
                "live capture manifest should include style profile metadata");
        require(manifestText.find("source.vizsync") != std::string::npos,
                "live capture manifest should include sync profile metadata");
        require(manifestText.find("\"video\": \"visualizer-live-capture.mp4\"") != std::string::npos,
                "live capture manifest should include relative video path");
        require(manifestText.find("\"videoEncoded\": true") != std::string::npos,
                "live capture manifest should include video encode status");
        require(manifestText.find("\"videoBytes\": 123456") != std::string::npos,
                "live capture manifest should include video size");
        require(manifestText.find("\"averageRenderMs\": 5.750") != std::string::npos,
                "live capture manifest should include renderer timing");
        require(manifestText.find("\"averageGeometryMs\": 2.250") != std::string::npos,
                "live capture manifest should include geometry timing");
    }

    {
        std::ifstream page(root / "index.html");
        const std::string pageText((std::istreambuf_iterator<char>(page)), std::istreambuf_iterator<char>());
        require(pageText.find("Visualizer Live Capture") != std::string::npos,
                "live capture page should include title");
        require(pageText.find("Warehouse Strobe") != std::string::npos,
                "live capture page should include look name");
        require(pageText.find("Style Profile") != std::string::npos,
                "live capture page should summarize the style profile");
        require(pageText.find("Sync Profile") != std::string::npos,
                "live capture page should summarize the sync profile");
        require(pageText.find("<video controls playsinline src=\"visualizer-live-capture.mp4\"") != std::string::npos,
                "live capture page should embed encoded MP4");
        require(pageText.find("<img class=\"preview\" src=\"preview.bmp\"") != std::string::npos,
                "live capture page should embed a BMP preview");
        require(pageText.find("Download MP4") != std::string::npos,
                "live capture page should link encoded MP4");
        require(pageText.find("ffmpeg -framerate 2 -i frame_%06d.ppm") != std::string::npos,
                "live capture page should include MP4 encoding command using capture FPS");
        require(pageText.find("capture_manifest.json") != std::string::npos,
                "live capture page should link manifest");
        require(pageText.find("analysis_timeline.csv") != std::string::npos,
                "live capture page should link analysis timeline");
        require(pageText.find("Phrase Lock") != std::string::npos,
                "live capture page should summarize phrase confidence");
        require(pageText.find("Build Tension") != std::string::npos,
                "live capture page should summarize build tension");
        require(pageText.find("Depth 3D") != std::string::npos,
                "live capture page should summarize 3D depth");
        require(pageText.find("Color Impact") != std::string::npos,
                "live capture page should summarize color impact");
        require(pageText.find("Complexity") != std::string::npos,
                "live capture page should summarize complexity");
        require(pageText.find("Section") != std::string::npos,
                "live capture page should summarize section");
        require(pageText.find("Avg Render") != std::string::npos,
                "live capture page should summarize renderer timing");
        require(pageText.find("5.75 ms") != std::string::npos,
                "live capture page should include formatted renderer timing");
    }

    std::filesystem::remove_all(root);
}

void interactionChangesGeometry()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::TechnoMandala;
    settings.interactiveField = true;
    const AudioMetrics metrics = syntheticMetrics();

    InteractionState interaction;
    interaction.enabled = true;
    interaction.active = true;
    interaction.pressed = true;
    interaction.normalizedX = 0.78f;
    interaction.normalizedY = 0.34f;
    interaction.velocity = 1.5f;
    interaction.strength = 1.0f;

    const GeometryFrame neutral = engine.buildFrame(metrics, settings, 800.0f, 600.0f, 4.0);
    const GeometryFrame interactive = engine.buildFrame(metrics, settings, interaction, 800.0f, 600.0f, 4.0);
    require(interactive.rings.size() > neutral.rings.size(), "interaction should add field rings");
    require(interactive.beams.size() > neutral.beams.size(), "interaction should add field beams");
    require(!interactive.polylines.empty(), "interactive frame should still preserve mode polylines");
    require(interactive.polylines.front().points.front().x != neutral.polylines.front().points.front().x ||
                interactive.polylines.front().points.front().y != neutral.polylines.front().points.front().y,
            "interaction should bend generated mesh points");
}

void syncMetricsAddVisualAccents()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::FrequencyBloom;
    settings.interactiveField = false;

    AudioMetrics base = syntheticMetrics();
    base.beat = false;
    base.beatConfidence = 0.0f;
    base.dropIntensity = 0.0f;
    base.phraseIntensity = 0.0f;
    base.bandOnsets = {};

    AudioMetrics accented = base;
    accented.bandOnsets = {0.7f, 0.32f, 0.18f, 0.56f, 0.44f};
    accented.dropIntensity = 0.68f;
    accented.phraseIntensity = 0.5f;
    accented.phraseBoundary = true;
    accented.phrasePhase = 0.0f;
    accented.phraseConfidence = 0.82f;
    accented.buildTension = 0.74f;
    accented.spectralFlux = 0.4f;
    accented.beatPhase = 0.35f;
    accented.barPhase = 0.0f;
    accented.barConfidence = 0.84f;
    accented.downbeat = true;
    accented.downbeatConfidence = 0.92f;

    const GeometryFrame neutral = engine.buildFrame(base, settings, 960.0f, 540.0f, 3.0);
    const GeometryFrame syncFrame = engine.buildFrame(accented, settings, 960.0f, 540.0f, 3.0);

    const std::size_t neutralCount = neutral.rings.size() + neutral.beams.size() +
                                     neutral.particles.size() + neutral.polylines.size();
    const std::size_t syncCount = syncFrame.rings.size() + syncFrame.beams.size() +
                                  syncFrame.particles.size() + syncFrame.polylines.size();
    require(syncCount > neutralCount, "advanced sync metrics should add visual accents");
    require(syncFrame.beams.size() > neutral.beams.size(), "downbeat metrics should add shared sync beams");

    AudioMetrics phraseBuild = base;
    phraseBuild.phrasePhase = 0.88f;
    phraseBuild.phraseConfidence = 0.86f;
    phraseBuild.buildTension = 0.78f;
    phraseBuild.spectralFlux = 0.24f;
    const GeometryFrame phraseFrame = engine.buildFrame(phraseBuild, settings, 960.0f, 540.0f, 3.0);
    require(countPrimitives(phraseFrame) > countPrimitives(neutral),
            "phrase build tension should add global sync geometry before a drop");
    require(phraseFrame.beams.size() > neutral.beams.size(),
            "phrase build tension should add tension spokes");
    require(syncFrame.flash > neutral.flash, "drop intensity should contribute to visual flash");
}

void environmentStateAddsVisualContext()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::LissajousMesh;
    settings.interactiveField = false;
    settings.environmentReactive = true;

    EnvironmentState environment;
    environment.enabled = true;
    environment.timeOfDay = 0.25f;
    environment.motion = 0.8f;
    environment.ambient = 0.9f;

    const AudioMetrics metrics = syntheticMetrics();
    const GeometryFrame neutral = engine.buildFrame(metrics, settings, InteractionState{}, EnvironmentState{}, 960.0f, 540.0f, 3.0);
    const GeometryFrame contextual = engine.buildFrame(metrics, settings, InteractionState{}, environment, 960.0f, 540.0f, 3.0);
    require(countPrimitives(contextual) > countPrimitives(neutral),
            "environment state should add visual context geometry");
    require(contextual.background.r != neutral.background.r ||
                contextual.background.g != neutral.background.g ||
                contextual.background.b != neutral.background.b,
            "environment state should influence background tone");

    settings.environmentReactive = false;
    const GeometryFrame disabled = engine.buildFrame(metrics, settings, InteractionState{}, environment, 960.0f, 540.0f, 3.0);
    require(countPrimitives(disabled) == countPrimitives(neutral),
            "environment geometry should be disabled by settings");
}

void hueShiftChangesRenderedPalette()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::FrequencyBloom;
    settings.palette = Palette::NeonVoltage;
    settings.interactiveField = false;
    settings.environmentReactive = false;

    const AudioMetrics metrics = syntheticMetrics();
    const GeometryFrame base = engine.buildFrame(metrics, settings, 960.0f, 540.0f, 3.0);
    settings.hueShift = 1.0f / 3.0f;
    const GeometryFrame shifted = engine.buildFrame(metrics, settings, 960.0f, 540.0f, 3.0);

    require(countPrimitives(base) == countPrimitives(shifted),
            "hue shift should recolor without changing geometry density");
    require(!base.beams.empty() && !shifted.beams.empty(), "frequency bloom should generate beams for hue comparison");
    require(colorDistance(base.beams.front().color, shifted.beams.front().color) > 0.2f,
            "hue shift should visibly change generated colors");
    require(colorDistance(base.background, shifted.background) > 0.01f,
            "hue shift should influence background tint");
}

void depth3DProjectsGeometryIntoPerspectiveSpace()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::HyperspacePolytope;
    settings.palette = Palette::NeonVoltage;
    settings.colorImpact = 0.65f;
    settings.depth3D = 0.0f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.9f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.beat = true;
    metrics.beatConfidence = 0.86f;
    metrics.bass = 0.82f;
    metrics.stereoWidth = 0.72f;
    metrics.dropIntensity = 0.68f;
    metrics.buildTension = 0.64f;

    const GeometryFrame flat = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 4.0);
    settings.depth3D = 1.0f;
    const GeometryFrame deep = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 4.0);

    require(countPrimitives(deep) > countPrimitives(flat),
            "3D depth should add vanishing-space guide geometry");
    require(!flat.rings.empty() && !deep.rings.empty(), "depth comparison should have rings");
    require(std::fabs(averageRingRadius(flat) - averageRingRadius(deep)) > 0.5f,
            "3D depth should change projected ring scale");
    require(averagePolylinePointDistance(deep, Vec2{640.0f, 360.0f}) !=
                averagePolylinePointDistance(flat, Vec2{640.0f, 360.0f}),
            "3D depth should project polyline points through perspective space");
}

void colorImpactStrengthensPalettePersonality()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::FrequencyBloom;
    settings.palette = Palette::NeonVoltage;
    settings.hueShift = 0.62f;
    settings.depth3D = 0.0f;
    settings.colorImpact = 0.0f;
    settings.interactiveField = false;
    settings.environmentReactive = false;

    AudioMetrics metrics = syntheticMetrics();
    metrics.keyIndex = 9;
    metrics.keyMode = MusicalMode::Minor;
    metrics.keyConfidence = 0.84f;
    metrics.harmonicEnergy = 0.8f;
    metrics.dropIntensity = 0.58f;
    metrics.buildTension = 0.62f;
    metrics.treble = 0.72f;

    const GeometryFrame subtle = engine.buildFrame(metrics, settings, 960.0f, 540.0f, 3.0);
    settings.colorImpact = 1.0f;
    const GeometryFrame intense = engine.buildFrame(metrics, settings, 960.0f, 540.0f, 3.0);

    require(countPrimitives(subtle) == countPrimitives(intense),
            "color impact should recolor without changing geometry density when depth is fixed");
    require(!subtle.beams.empty() && !intense.beams.empty(), "frequency bloom should generate beams");
    require(colorDistance(subtle.beams.front().color, intense.beams.front().color) > 0.12f,
            "color impact should strongly alter generated colors");
    require(colorDistance(subtle.background, intense.background) > 0.02f,
            "color impact should shift the whole frame mood, including background");
}

void chromaKaleidoscopeRespondsToHarmony()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::ChromaKaleidoscope;
    settings.palette = Palette::NeonVoltage;
    settings.interactiveField = false;
    settings.environmentReactive = false;

    AudioMetrics neutral = syntheticMetrics();
    neutral.beat = false;
    neutral.keyIndex = -1;
    neutral.keyConfidence = 0.0f;
    neutral.harmonicEnergy = 0.0f;
    neutral.chroma.fill(0.02f);

    AudioMetrics harmonic = neutral;
    harmonic.keyIndex = 7;
    harmonic.keyMode = MusicalMode::Minor;
    harmonic.keyConfidence = 0.88f;
    harmonic.harmonicEnergy = 0.92f;
    harmonic.chroma.fill(0.04f);
    harmonic.chroma[7] = 1.0f;
    harmonic.chroma[10] = 0.72f;
    harmonic.chroma[2] = 0.64f;

    const GeometryFrame neutralFrame = engine.buildFrame(neutral, settings, 960.0f, 540.0f, 3.0);
    const GeometryFrame harmonicFrame = engine.buildFrame(harmonic, settings, 960.0f, 540.0f, 3.0);
    require(countPrimitives(neutralFrame) > 80, "chroma kaleidoscope should generate dense geometry");
    require(countPrimitives(harmonicFrame) >= countPrimitives(neutralFrame),
            "harmonic input should preserve or increase kaleidoscope density");
    require(!neutralFrame.polylines.empty() && !harmonicFrame.polylines.empty(),
            "chroma kaleidoscope should generate prism polylines");
    require(std::fabs(neutralFrame.polylines.front().points.front().x -
                      harmonicFrame.polylines.front().points.front().x) > 0.1f ||
                std::fabs(neutralFrame.polylines.front().points.front().y -
                          harmonicFrame.polylines.front().points.front().y) > 0.1f ||
                colorDistance(neutralFrame.polylines.front().color, harmonicFrame.polylines.front().color) > 0.05f,
            "key and chroma should alter kaleidoscope geometry or color");
}

void phaseWeaveRespondsToStereoHarmonyAndDrops()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::PhaseWeave;
    settings.palette = Palette::NeonVoltage;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.86f;
    settings.complexity = 1.25f;

    AudioMetrics calm = syntheticMetrics();
    calm.beat = false;
    calm.beatConfidence = 0.0f;
    calm.beatPhase = 0.0f;
    calm.stereoWidth = 0.05f;
    calm.spectralFlux = 0.02f;
    calm.dropIntensity = 0.0f;
    calm.phraseIntensity = 0.0f;
    calm.keyConfidence = 0.0f;
    calm.harmonicEnergy = 0.0f;
    calm.section = ArrangementSection::Groove;
    calm.sectionConfidence = 0.2f;
    calm.chroma.fill(0.01f);

    AudioMetrics woven = calm;
    woven.beat = true;
    woven.beatConfidence = 0.91f;
    woven.beatPhase = 0.37f;
    woven.stereoWidth = 0.82f;
    woven.spectralFlux = 0.64f;
    woven.dropIntensity = 0.72f;
    woven.phraseIntensity = 0.58f;
    woven.keyIndex = 9;
    woven.keyMode = MusicalMode::Minor;
    woven.keyConfidence = 0.86f;
    woven.harmonicEnergy = 0.9f;
    woven.section = ArrangementSection::Build;
    woven.sectionConfidence = 0.9f;
    woven.sectionProgress = 0.68f;
    woven.chroma.fill(0.04f);
    woven.chroma[9] = 1.0f;
    woven.chroma[0] = 0.68f;
    woven.chroma[4] = 0.52f;

    const GeometryFrame calmFrame = engine.buildFrame(calm, settings, 1280.0f, 720.0f, 4.0);
    const GeometryFrame wovenFrame = engine.buildFrame(woven, settings, 1280.0f, 720.0f, 4.0);

    require(!calmFrame.polylines.empty(), "phase weave should generate streamlines");
    require(wovenFrame.polylines.size() > calmFrame.polylines.size(),
            "build/drop metrics should add phase-weave ribbons");
    require(countPrimitives(wovenFrame) > countPrimitives(calmFrame),
            "phase weave should increase geometry under stereo harmonic drop energy");
    require(wovenFrame.flash > calmFrame.flash, "phase weave should raise flash on flux/drop energy");
    require(colorDistance(calmFrame.polylines.front().color, wovenFrame.polylines.front().color) > 0.03f ||
                std::fabs(calmFrame.polylines.front().points.back().x -
                          wovenFrame.polylines.front().points.back().x) > 0.5f ||
                std::fabs(calmFrame.polylines.front().points.back().y -
                          wovenFrame.polylines.front().points.back().y) > 0.5f,
            "stereo, beat phase, and harmonic input should alter phase-weave color or flow");
}

void resonanceTessellationRespondsToHarmonyBuildsAndDrops()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::ResonanceTessellation;
    settings.palette = Palette::AcidAurora;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.78f;
    settings.complexity = 1.12f;

    AudioMetrics calm = syntheticMetrics();
    calm.beat = false;
    calm.beatConfidence = 0.0f;
    calm.spectralFlux = 0.02f;
    calm.dropIntensity = 0.0f;
    calm.phraseIntensity = 0.0f;
    calm.harmonicEnergy = 0.0f;
    calm.keyConfidence = 0.0f;
    calm.section = ArrangementSection::Groove;
    calm.sectionConfidence = 0.2f;
    calm.bandOnsets = {};
    calm.chroma.fill(0.02f);

    AudioMetrics charged = calm;
    charged.beat = true;
    charged.beatConfidence = 0.88f;
    charged.beatPhase = 0.42f;
    charged.spectralFlux = 0.58f;
    charged.stereoWidth = 0.52f;
    charged.dropIntensity = 0.64f;
    charged.phraseIntensity = 0.56f;
    charged.section = ArrangementSection::Build;
    charged.sectionConfidence = 0.9f;
    charged.sectionProgress = 0.72f;
    charged.keyIndex = 2;
    charged.keyMode = MusicalMode::Minor;
    charged.keyConfidence = 0.86f;
    charged.harmonicEnergy = 0.84f;
    charged.bandOnsets = {0.18f, 0.34f, 0.62f, 0.54f, 0.36f};
    charged.chroma.fill(0.04f);
    charged.chroma[2] = 1.0f;
    charged.chroma[5] = 0.62f;
    charged.chroma[9] = 0.74f;

    const GeometryFrame calmFrame = engine.buildFrame(calm, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame chargedFrame = engine.buildFrame(charged, settings, 1280.0f, 720.0f, 2.0);

    require(calmFrame.polylines.size() > 60, "resonance tessellation should generate a dense triangular field");
    require(chargedFrame.polylines.size() > calmFrame.polylines.size(),
            "build/drop energy should increase tessellation density and fault lines");
    require(chargedFrame.rings.size() > calmFrame.rings.size(),
            "harmonic onsets should add tessellation glyph rings");
    require(chargedFrame.particles.size() > calmFrame.particles.size(),
            "band onsets should add tessellation particles");
    require(chargedFrame.flash > calmFrame.flash, "drop and build energy should raise tessellation flash");
    require(colorDistance(calmFrame.polylines.front().color, chargedFrame.polylines.front().color) > 0.03f ||
                std::fabs(calmFrame.polylines.front().points.front().x -
                          chargedFrame.polylines.front().points.front().x) > 0.5f ||
                std::fabs(calmFrame.polylines.front().points.front().y -
                          chargedFrame.polylines.front().points.front().y) > 0.5f,
            "key, chroma, and beat phase should alter tessellation color or geometry");
}

void neuralConstellationRespondsToBarsHarmonyAndOnsets()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::NeuralConstellation;
    settings.palette = Palette::NeonVoltage;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.82f;
    settings.complexity = 1.18f;

    AudioMetrics calm = syntheticMetrics();
    calm.beat = false;
    calm.beatConfidence = 0.0f;
    calm.barPhase = 0.0f;
    calm.barConfidence = 0.0f;
    calm.downbeat = false;
    calm.downbeatConfidence = 0.0f;
    calm.spectralFlux = 0.02f;
    calm.dropIntensity = 0.0f;
    calm.phraseIntensity = 0.0f;
    calm.stereoWidth = 0.08f;
    calm.harmonicEnergy = 0.0f;
    calm.keyConfidence = 0.0f;
    calm.bandOnsets = {};
    calm.chroma.fill(0.02f);

    AudioMetrics locked = calm;
    locked.beat = true;
    locked.beatConfidence = 0.9f;
    locked.beatPhase = 0.16f;
    locked.barPhase = 0.02f;
    locked.barConfidence = 0.78f;
    locked.downbeat = true;
    locked.downbeatConfidence = 0.92f;
    locked.spectralFlux = 0.48f;
    locked.stereoWidth = 0.64f;
    locked.dropIntensity = 0.34f;
    locked.phraseIntensity = 0.55f;
    locked.keyIndex = 9;
    locked.keyMode = MusicalMode::Minor;
    locked.keyConfidence = 0.82f;
    locked.harmonicEnergy = 0.74f;
    locked.bandOnsets = {0.42f, 0.18f, 0.54f, 0.36f, 0.28f};
    locked.chroma.fill(0.04f);
    locked.chroma[9] = 1.0f;
    locked.chroma[0] = 0.62f;
    locked.chroma[4] = 0.5f;

    const GeometryFrame calmFrame = engine.buildFrame(calm, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame lockedFrame = engine.buildFrame(locked, settings, 1280.0f, 720.0f, 2.0);

    require(calmFrame.particles.size() > 20, "neural constellation should generate node particles");
    require(calmFrame.polylines.size() > 20, "neural constellation should generate connection polylines");
    require(lockedFrame.rings.size() > calmFrame.rings.size(),
            "downbeats and harmony should add neural constellation rings");
    require(lockedFrame.beams.size() > calmFrame.beams.size(),
            "drop and spectral energy should add neural constellation beams");
    require(countPrimitives(lockedFrame) > countPrimitives(calmFrame),
            "bar-locked harmonic input should increase neural constellation density");
    require(lockedFrame.flash > calmFrame.flash,
            "downbeat and drop input should raise neural constellation flash");
}

void cymaticInterferenceRespondsToHarmonyBuildsAndDownbeats()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::CymaticInterference;
    settings.palette = Palette::AcidAurora;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.82f;
    settings.complexity = 1.22f;

    AudioMetrics calm = syntheticMetrics();
    calm.beat = false;
    calm.beatConfidence = 0.0f;
    calm.barPhase = 0.0f;
    calm.barConfidence = 0.0f;
    calm.downbeat = false;
    calm.downbeatConfidence = 0.0f;
    calm.spectralFlux = 0.04f;
    calm.stereoWidth = 0.08f;
    calm.dropIntensity = 0.0f;
    calm.phraseIntensity = 0.0f;
    calm.phraseConfidence = 0.0f;
    calm.buildTension = 0.0f;
    calm.keyConfidence = 0.0f;
    calm.harmonicEnergy = 0.0f;
    calm.bandOnsets = {};
    calm.chroma.fill(0.02f);

    AudioMetrics charged = calm;
    charged.beat = true;
    charged.beatConfidence = 0.88f;
    charged.beatPhase = 0.18f;
    charged.barPhase = 0.03f;
    charged.barConfidence = 0.78f;
    charged.downbeat = true;
    charged.downbeatConfidence = 0.9f;
    charged.spectralFlux = 0.52f;
    charged.stereoWidth = 0.58f;
    charged.dropIntensity = 0.28f;
    charged.phraseIntensity = 0.5f;
    charged.phrasePhase = 0.82f;
    charged.phraseConfidence = 0.76f;
    charged.phraseBoundary = true;
    charged.buildTension = 0.72f;
    charged.keyIndex = 2;
    charged.keyMode = MusicalMode::Minor;
    charged.keyConfidence = 0.86f;
    charged.harmonicEnergy = 0.82f;
    charged.bandOnsets = {0.24f, 0.18f, 0.52f, 0.46f, 0.32f};
    charged.chroma.fill(0.04f);
    charged.chroma[2] = 1.0f;
    charged.chroma[5] = 0.68f;
    charged.chroma[9] = 0.72f;

    const GeometryFrame calmFrame = engine.buildFrame(calm, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame chargedFrame = engine.buildFrame(charged, settings, 1280.0f, 720.0f, 2.0);

    require(calmFrame.polylines.size() >= 10, "cymatic interference should generate contour polylines");
    require(chargedFrame.rings.size() > calmFrame.rings.size(),
            "downbeat, phrase, and build accents should add cymatic rings");
    require(chargedFrame.beams.size() >= calmFrame.beams.size(),
            "harmonic energy should preserve or increase cymatic spokes");
    require(chargedFrame.particles.size() > calmFrame.particles.size(),
            "band onsets should add cymatic nodal particles");
    require(countPrimitives(chargedFrame) > countPrimitives(calmFrame),
            "harmonic build input should increase cymatic primitive density");
    require(chargedFrame.flash > calmFrame.flash,
            "downbeat and drop input should raise cymatic flash");
    require(colorDistance(calmFrame.polylines.front().color, chargedFrame.polylines.front().color) > 0.03f ||
                std::fabs(calmFrame.polylines.front().points.front().x -
                          chargedFrame.polylines.front().points.front().x) > 0.5f ||
                std::fabs(calmFrame.polylines.front().points.front().y -
                          chargedFrame.polylines.front().points.front().y) > 0.5f,
            "key, chroma, and phrase phase should alter cymatic color or contour geometry");
}

void presetRoundTripsSettings()
{
    VisualPreset preset;
    preset.name = "acid-test";
    preset.settings.mode = VisualMode::HyperspacePolytope;
    preset.settings.palette = Palette::AcidAurora;
    preset.settings.hueShift = 0.37f;
    preset.settings.depth3D = 0.88f;
    preset.settings.colorImpact = 0.91f;
    preset.settings.complexity = 1.42f;
    preset.settings.intensity = 2.35f;
    preset.settings.speed = 1.7f;
    preset.settings.qualityScale = 0.66f;
    preset.settings.adaptiveQuality = false;
    preset.settings.showHud = false;
    preset.settings.trails = false;
    preset.settings.interactiveField = false;
    preset.settings.environmentReactive = false;
    preset.settings.autoScene = true;

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_preset_test";
    const std::filesystem::path path = root / "acid.vizpreset";
    std::filesystem::remove_all(root);

    std::string error;
    require(savePreset(path, preset, error), "preset should save: " + error);
    const std::optional<VisualPreset> loaded = loadPreset(path, error);
    require(loaded.has_value(), "preset should load: " + error);
    require(loaded->name == preset.name, "preset name should round-trip");
    require(loaded->settings.mode == preset.settings.mode, "preset mode should round-trip");
    require(parseVisualMode("lattice").has_value() &&
                *parseVisualMode("lattice") == VisualMode::PolyrhythmLattice,
            "new visual modes should parse from aliases");
    require(parseVisualMode("origami").has_value() &&
                *parseVisualMode("origami") == VisualMode::SpectralOrigami,
            "spectral origami should parse from aliases");
    require(parseVisualMode("kaleid").has_value() &&
                *parseVisualMode("kaleid") == VisualMode::ChromaKaleidoscope,
            "chroma kaleidoscope should parse from aliases");
    require(parseVisualMode("tesseract").has_value() &&
                *parseVisualMode("tesseract") == VisualMode::HyperspacePolytope,
            "hyperspace polytope should parse from aliases");
    require(parseVisualMode("flow").has_value() &&
                *parseVisualMode("flow") == VisualMode::PhaseWeave,
            "phase weave should parse from aliases");
    require(parseVisualMode("tess").has_value() &&
                *parseVisualMode("tess") == VisualMode::ResonanceTessellation,
            "resonance tessellation should parse from aliases");
    require(parseVisualMode("neural").has_value() &&
                *parseVisualMode("neural") == VisualMode::NeuralConstellation,
            "neural constellation should parse from aliases");
    require(parseVisualMode("chladni").has_value() &&
                *parseVisualMode("chladni") == VisualMode::CymaticInterference,
            "cymatic interference should parse from aliases");
    require(loaded->settings.palette == preset.settings.palette, "preset palette should round-trip");
    require(loaded->settings.hueShift > 0.36f && loaded->settings.hueShift < 0.38f, "hue shift should round-trip");
    require(loaded->settings.depth3D > 0.87f && loaded->settings.depth3D < 0.89f,
            "3D depth should round-trip");
    require(loaded->settings.colorImpact > 0.90f && loaded->settings.colorImpact < 0.92f,
            "color impact should round-trip");
    require(loaded->settings.complexity > 1.41f && loaded->settings.complexity < 1.43f,
            "complexity should round-trip");
    require(loaded->settings.intensity > 2.3f && loaded->settings.intensity < 2.4f, "intensity should round-trip");
    require(loaded->settings.speed > 1.6f && loaded->settings.speed < 1.8f, "speed should round-trip");
    require(loaded->settings.qualityScale > 0.65f && loaded->settings.qualityScale < 0.67f, "quality should round-trip");
    require(!loaded->settings.adaptiveQuality, "adaptive quality flag should round-trip");
    require(!loaded->settings.showHud, "HUD flag should round-trip");
    require(!loaded->settings.trails, "trails flag should round-trip");
    require(!loaded->settings.interactiveField, "interaction flag should round-trip");
    require(!loaded->settings.environmentReactive, "environment flag should round-trip");
    require(loaded->settings.autoScene, "auto scene flag should round-trip");

    std::filesystem::remove_all(root);
}

void curatedPresetBankProvidesRenderableLooks()
{
    const std::vector<VisualPreset>& presets = curatedPresets();
    require(presets.size() >= 8, "curated preset bank should provide several live-ready looks");

    const std::optional<std::size_t> warehouseIndex = findCuratedPresetIndex("warehouse");
    require(warehouseIndex.has_value(), "curated preset aliases should resolve");
    require(presets[*warehouseIndex].name == "Warehouse Strobe", "warehouse alias should select strobe look");

    const std::optional<VisualPreset> hyperspace = findCuratedPreset("4d");
    require(hyperspace.has_value(), "4d alias should resolve");
    require(hyperspace->settings.mode == VisualMode::HyperspacePolytope, "4d alias should select hyperspace look");
    const std::optional<VisualPreset> phase = findCuratedPreset("weave");
    require(phase.has_value(), "weave alias should resolve");
    require(phase->settings.mode == VisualMode::PhaseWeave, "weave alias should select phase-weave look");
    const std::optional<VisualPreset> tessellation = findCuratedPreset("tess");
    require(tessellation.has_value(), "tess alias should resolve");
    require(tessellation->settings.mode == VisualMode::ResonanceTessellation,
            "tess alias should select resonance tessellation look");
    const std::optional<VisualPreset> neural = findCuratedPreset("network");
    require(neural.has_value(), "network alias should resolve");
    require(neural->settings.mode == VisualMode::NeuralConstellation,
            "network alias should select neural constellation look");
    const std::optional<VisualPreset> cymatic = findCuratedPreset("chladni");
    require(cymatic.has_value(), "chladni alias should resolve");
    require(cymatic->settings.mode == VisualMode::CymaticInterference,
            "chladni alias should select cymatic interference look");

    AudioMetrics metrics;
    metrics.rms = 0.42f;
    metrics.peak = 0.85f;
    metrics.bass = 0.72f;
    metrics.lowMid = 0.36f;
    metrics.mid = 0.28f;
    metrics.highMid = 0.20f;
    metrics.treble = 0.48f;
    metrics.spectralFlux = 0.55f;
    metrics.onset = 0.62f;
    metrics.beat = true;
    metrics.beatConfidence = 0.9f;
    metrics.beatPhase = 0.25f;
    metrics.bpm = 132.0f;
    metrics.dropIntensity = 0.75f;
    metrics.phraseIntensity = 0.45f;
    metrics.keyIndex = 6;
    metrics.keyMode = MusicalMode::Minor;
    metrics.keyConfidence = 0.8f;
    metrics.harmonicEnergy = 0.7f;

    bool sawAutoScene = false;
    bool sawHyperspace = false;
    bool sawKaleidoscope = false;
    bool sawPhaseWeave = false;
    bool sawTessellation = false;
    bool sawNeural = false;
    bool sawCymatic = false;
    bool sawCrispStrobe = false;
    VisualizerEngine engine;

    for (std::size_t i = 0; i < presets.size(); ++i) {
        const VisualPreset& preset = presets[i];
        require(!preset.name.empty(), "curated preset names should be nonempty");
        for (std::size_t j = i + 1; j < presets.size(); ++j) {
            require(preset.name != presets[j].name, "curated preset names should be unique");
        }

        require(preset.settings.hueShift >= 0.0f && preset.settings.hueShift <= 1.0f,
                "curated hue shift should be normalized");
        require(preset.settings.depth3D >= 0.0f && preset.settings.depth3D <= 1.0f,
                "curated depth should be normalized");
        require(preset.settings.colorImpact >= 0.0f && preset.settings.colorImpact <= 1.0f,
                "curated color impact should be normalized");
        require(preset.settings.complexity >= 0.35f && preset.settings.complexity <= 1.8f,
                "curated complexity should stay in supported range");
        require(preset.settings.intensity >= 0.15f && preset.settings.intensity <= 4.0f,
                "curated intensity should stay in supported range");
        require(preset.settings.speed >= 0.1f && preset.settings.speed <= 4.0f,
                "curated speed should stay in supported range");
        require(preset.settings.showHud, "curated presets should keep HUD available");
        require(preset.settings.interactiveField, "curated presets should keep interaction available");
        require(preset.settings.environmentReactive, "curated presets should keep environment reactivity available");
        require(preset.settings.adaptiveQuality, "curated presets should keep adaptive quality available");

        const GeometryFrame frame = engine.buildFrame(metrics,
                                                      preset.settings,
                                                      InteractionState{},
                                                      EnvironmentState{},
                                                      640.0f,
                                                      360.0f,
                                                      1.0 + static_cast<double>(i) * 0.25);
        require(countPrimitives(frame) > 40, "curated presets should render meaningful geometry");

        sawAutoScene = sawAutoScene || preset.settings.autoScene;
        sawHyperspace = sawHyperspace || preset.settings.mode == VisualMode::HyperspacePolytope;
        sawKaleidoscope = sawKaleidoscope || preset.settings.mode == VisualMode::ChromaKaleidoscope;
        sawPhaseWeave = sawPhaseWeave || preset.settings.mode == VisualMode::PhaseWeave;
        sawTessellation = sawTessellation || preset.settings.mode == VisualMode::ResonanceTessellation;
        sawNeural = sawNeural || preset.settings.mode == VisualMode::NeuralConstellation;
        sawCymatic = sawCymatic || preset.settings.mode == VisualMode::CymaticInterference;
        sawCrispStrobe = sawCrispStrobe || (preset.name == "Warehouse Strobe" && !preset.settings.trails);
    }

    require(sawAutoScene, "curated bank should include an Auto Scene look");
    require(sawHyperspace, "curated bank should include a 4D look");
    require(sawKaleidoscope, "curated bank should include a harmonic kaleidoscope look");
    require(sawPhaseWeave, "curated bank should include a phase-weave flow-field look");
    require(sawTessellation, "curated bank should include a resonance tessellation look");
    require(sawNeural, "curated bank should include a neural constellation look");
    require(sawCymatic, "curated bank should include a cymatic interference look");
    require(sawCrispStrobe, "curated bank should include a crisp strobe look");
}

void userPresetLibrarySavesScansAndLoads()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_user_preset_library_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    VisualPreset preset;
    preset.name = "Acid Geometry / Late Set!";
    preset.settings.mode = VisualMode::NeuralConstellation;
    preset.settings.palette = Palette::AcidAurora;
    preset.settings.hueShift = 0.27f;
    preset.settings.depth3D = 0.84f;
    preset.settings.colorImpact = 0.89f;
    preset.settings.complexity = 1.6f;
    preset.settings.intensity = 2.2f;
    preset.settings.speed = 1.4f;
    preset.settings.trails = false;
    preset.settings.autoScene = true;

    require(sanitizePresetFileStem(preset.name) == "acid_geometry_late_set",
            "user preset file names should be portable and deterministic");
    require(sanitizePresetFileStem(" !!! ") == "visual_preset",
            "empty sanitized preset names should receive a stable fallback");
    require(defaultUserPresetDirectory().filename() == "presets",
            "default user preset library should live under the presets profile folder");

    std::filesystem::path firstPath;
    std::filesystem::path secondPath;
    std::string error;
    require(saveUserPreset(root, preset, firstPath, error), "first user preset save should succeed: " + error);
    require(firstPath.filename() == "acid_geometry_late_set.vizpreset",
            "first user preset should use the sanitized base name");
    require(saveUserPreset(root, preset, secondPath, error), "duplicate user preset save should succeed: " + error);
    require(secondPath.filename() == "acid_geometry_late_set_2.vizpreset",
            "duplicate user presets should avoid overwriting existing files");

    {
        std::ofstream ignored(root / "notes.txt");
        ignored << "not a preset\n";
    }

    const std::vector<PresetLibraryEntry> entries = scanUserPresetLibrary(root);
    require(entries.size() == 2U, "user preset scan should discover saved .vizpreset files only");
    require(entries.front().name == preset.name, "user preset scan should expose the saved display name");
    require(entries.front().path.filename() == firstPath.filename(),
            "user preset scan should sort duplicate names by file path");

    const std::optional<PresetLibraryEntry> byName = findUserPreset(root, "Acid Geometry / Late Set!");
    require(byName.has_value(), "user preset lookup should match display names");
    require(byName->path.filename() == firstPath.filename(),
            "display-name lookup should return the first stable entry for duplicate names");

    const std::optional<PresetLibraryEntry> byFileStem = findUserPreset(root, "acid_geometry_late_set_2.vizpreset");
    require(byFileStem.has_value(), "user preset lookup should match file names with extensions");
    require(byFileStem->path.filename() == secondPath.filename(),
            "file-name lookup should resolve numbered duplicate presets");
    require(!findUserPreset(root, "missing preset").has_value(),
            "missing user preset lookup should report no match");

    const std::optional<VisualPreset> loaded = loadUserPresetEntry(entries.front(), error);
    require(loaded.has_value(), "user preset library entry should load: " + error);
    require(loaded->settings.mode == VisualMode::NeuralConstellation,
            "loaded user preset should preserve the visual mode");
    require(loaded->settings.palette == Palette::AcidAurora,
            "loaded user preset should preserve the palette");
    require(loaded->settings.depth3D > 0.83f && loaded->settings.depth3D < 0.85f,
            "loaded user preset should preserve 3D depth");
    require(loaded->settings.colorImpact > 0.88f && loaded->settings.colorImpact < 0.90f,
            "loaded user preset should preserve color impact");
    require(!loaded->settings.trails, "loaded user preset should preserve trails");
    require(loaded->settings.autoScene, "loaded user preset should preserve Auto Scene");

    std::filesystem::remove_all(root);
}

void controlPanelHitTestsButtonsAndSliders()
{
    VisualSettings settings;
    settings.mode = VisualMode::FrequencyBloom;
    settings.palette = Palette::OceanicPulse;
    settings.intensity = 2.0f;
    settings.speed = 1.2f;
    settings.depth3D = 0.77f;
    settings.colorImpact = 0.82f;

    const ControlPanelLayout layout = buildControlPanelLayout(1280.0f, 720.0f, settings, true);
    require(!layout.items.empty(), "control panel should build interactive items");

    bool foundRecord = false;
    bool foundResetProfiles = false;
    bool foundSlider = false;
    bool foundQuality = false;
    bool foundHueShift = false;
    bool foundDepth = false;
    bool foundColorImpact = false;
    bool foundComplexity = false;
    bool foundAutoScene = false;
    bool foundTrails = false;
    bool foundOrigami = false;
    bool foundEnvironment = false;
    bool foundKaleidoscope = false;
    bool foundHyperspace = false;
    bool foundPhaseWeave = false;
    bool foundTessellation = false;
    bool foundNeural = false;
    bool foundCymatic = false;
    bool foundLookPrevious = false;
    bool foundLookNext = false;
    bool foundUserPrevious = false;
    bool foundUserNext = false;
    bool foundSaveUser = false;
    for (const PanelItem& item : layout.items) {
        require(item.rect.bottom <= layout.panel.bottom, "control item should fit inside the panel");
        if (item.control == PanelControl::Record) {
            const PanelItem hit = hitTestControlPanel(layout,
                                                      (item.rect.left + item.rect.right) * 0.5f,
                                                      (item.rect.top + item.rect.bottom) * 0.5f);
            require(hit.control == PanelControl::Record, "record button should hit-test");
            require(hit.active, "record button should reflect active recording state");
            foundRecord = true;
        }
        if (item.control == PanelControl::ResetAudioProfiles) {
            require(!item.slider, "audio profile reset should be a button");
            foundResetProfiles = true;
        }
        if (item.control == PanelControl::IntensitySlider) {
            const float value = normalizedSliderValue(item, item.rect.left + (item.rect.right - item.rect.left) * 0.75f);
            require(value > 0.74f && value < 0.76f, "slider normalized value should map from x coordinate");
            foundSlider = true;
        }
        if (item.control == PanelControl::QualitySlider) {
            require(item.slider, "quality control should be a slider");
            foundQuality = true;
        }
        if (item.control == PanelControl::HueShiftSlider) {
            require(item.slider, "hue shift control should be a slider");
            require(item.value >= 0.0f && item.value <= 1.0f, "hue shift slider value should be normalized");
            foundHueShift = true;
        }
        if (item.control == PanelControl::DepthSlider) {
            require(item.slider, "3D depth control should be a slider");
            require(item.value > 0.76f && item.value < 0.78f, "3D depth slider should reflect settings");
            foundDepth = true;
        }
        if (item.control == PanelControl::ColorImpactSlider) {
            require(item.slider, "color impact control should be a slider");
            require(item.value > 0.81f && item.value < 0.83f, "color impact slider should reflect settings");
            foundColorImpact = true;
        }
        if (item.control == PanelControl::ComplexitySlider) {
            require(item.slider, "complexity control should be a slider");
            require(item.rect.bottom <= layout.panel.bottom, "complexity slider should fit inside the panel");
            foundComplexity = true;
        }
        if (item.control == PanelControl::ToggleAutoScene) {
            foundAutoScene = true;
        }
        if (item.control == PanelControl::ToggleTrails) {
            require(item.active, "trails control should reflect default active setting");
            foundTrails = true;
        }
        if (item.control == PanelControl::ToggleEnvironment) {
            require(item.active, "environment control should reflect default active setting");
            foundEnvironment = true;
        }
        if (item.control == PanelControl::ModeSpectralOrigami) {
            foundOrigami = true;
        }
        if (item.control == PanelControl::ModeChromaKaleidoscope) {
            foundKaleidoscope = true;
        }
        if (item.control == PanelControl::ModeHyperspacePolytope) {
            foundHyperspace = true;
        }
        if (item.control == PanelControl::ModePhaseWeave) {
            foundPhaseWeave = true;
        }
        if (item.control == PanelControl::ModeResonanceTessellation) {
            foundTessellation = true;
        }
        if (item.control == PanelControl::ModeNeuralConstellation) {
            foundNeural = true;
        }
        if (item.control == PanelControl::ModeCymaticInterference) {
            foundCymatic = true;
        }
        if (item.control == PanelControl::CuratedPresetPrevious) {
            foundLookPrevious = true;
        }
        if (item.control == PanelControl::CuratedPresetNext) {
            foundLookNext = true;
        }
        if (item.control == PanelControl::UserPresetPrevious) {
            foundUserPrevious = true;
        }
        if (item.control == PanelControl::UserPresetNext) {
            foundUserNext = true;
        }
        if (item.control == PanelControl::SaveUserPreset) {
            require(!item.slider, "user preset save should be a button");
            foundSaveUser = true;
        }
    }

    require(foundRecord, "record control should exist");
    require(foundResetProfiles, "audio profile reset control should exist");
    require(foundSlider, "intensity slider should exist");
    require(foundQuality, "quality slider should exist");
    require(foundHueShift, "hue shift slider should exist");
    require(foundDepth, "3D depth slider should exist");
    require(foundColorImpact, "color impact slider should exist");
    require(foundComplexity, "complexity slider should exist");
    require(foundAutoScene, "auto scene control should exist");
    require(foundTrails, "trails control should exist");
    require(foundEnvironment, "environment control should exist");
    require(foundOrigami, "spectral origami mode control should exist");
    require(foundKaleidoscope, "chroma kaleidoscope mode control should exist");
    require(foundHyperspace, "hyperspace polytope mode control should exist");
    require(foundPhaseWeave, "phase weave mode control should exist");
    require(foundTessellation, "resonance tessellation mode control should exist");
    require(foundNeural, "neural constellation mode control should exist");
    require(foundCymatic, "cymatic interference mode control should exist");
    require(foundLookPrevious, "previous curated look control should exist");
    require(foundLookNext, "next curated look control should exist");
    require(foundUserPrevious, "previous user preset control should exist");
    require(foundUserNext, "next user preset control should exist");
    require(foundSaveUser, "save user preset control should exist");
}

void runtimeInspectorFormatsSourceProfileAndCaptureState()
{
    RuntimeInspectorState live;
    live.sourceLabel = "Live loopback";
    live.sourceDetail = "Default Windows output";
    live.activeLook = "Neural Constellation";
    live.styleProfileName = "live_loopback.vizaudio";
    live.syncProfileName = "live_loopback.vizsync";
    live.presetLibraryName = "presets";
    live.userPresetCount = 3;
    live.sampleRate = 48000;
    live.channelCount = 2;

    const std::vector<std::string> liveLines = formatRuntimeInspectorLines(live);
    require(!liveLines.empty(), "runtime inspector should format live metadata lines");
    require(std::find(liveLines.begin(), liveLines.end(), "SOURCE") != liveLines.end(),
            "runtime inspector should include a source heading");
    require(std::find(liveLines.begin(), liveLines.end(), "Live loopback") != liveLines.end(),
            "runtime inspector should include live source label");
    require(std::find(liveLines.begin(), liveLines.end(), "Mode live loopback") != liveLines.end(),
            "runtime inspector should identify loopback mode");
    require(std::find(liveLines.begin(), liveLines.end(), "LOOK Neural Constellation") != liveLines.end(),
            "runtime inspector should include active look");
    require(std::find(liveLines.begin(), liveLines.end(), "User presets 3  presets") != liveLines.end(),
            "runtime inspector should include user preset library count");
    require(std::find(liveLines.begin(), liveLines.end(), "Style live_loopback.vizaudio") != liveLines.end(),
            "runtime inspector should include style profile");
    require(std::find(liveLines.begin(), liveLines.end(), "Sync live_loopback.vizsync") != liveLines.end(),
            "runtime inspector should include sync profile");

    RuntimeInspectorState file;
    file.fileSource = true;
    file.recording = true;
    file.sourceLabel = "track.wav";
    file.sourceDetail = "music";
    file.activeLook = "Warehouse Strobe";
    file.styleProfileName = "audio_track_abc.vizaudio";
    file.syncProfileName = "audio_track_abc.vizsync";
    file.playbackPositionSeconds = 65.2;
    file.playbackDurationSeconds = 130.0;
    file.captureDurationSeconds = 2.4;
    file.captureDirectory = "visualizer_20260607_012000";
    file.sampleRate = 44100;
    file.channelCount = 2;

    const std::vector<std::string> fileLines = formatRuntimeInspectorLines(file);
    require(std::find(fileLines.begin(), fileLines.end(), "track.wav") != fileLines.end(),
            "runtime inspector should include audio file name");
    require(std::find(fileLines.begin(), fileLines.end(), "Time 1:05 / 2:10  50%") != fileLines.end(),
            "runtime inspector should include file playback progress");
    require(std::find(fileLines.begin(), fileLines.end(), "Audio 44100 Hz  2 ch") != fileLines.end(),
            "runtime inspector should include decoded audio format");
    require(std::find(fileLines.begin(), fileLines.end(), "REC 0:02  visualizer_20260607_012000") != fileLines.end(),
            "runtime inspector should include capture state");
}

void performanceTrackerAdaptsQuality()
{
    FramePerformanceTracker tracker;
    PerformanceStats stats;
    for (int i = 0; i < 40; ++i) {
        stats = tracker.recordFrame(34.0, 5000, true, 1.0f);
    }
    require(stats.qualityScale < 1.0f, "slow frames should lower adaptive quality");
    require(stats.adaptiveQualityActive, "quality reduction should be reported active");

    stats = tracker.recordFrame(10.0, 1000, false, 0.62f);
    require(stats.qualityScale > 0.61f && stats.qualityScale < 0.63f, "manual quality should be preserved when adaptive is off");
}

void performanceTrackerRecordsTimingBreakdown()
{
    FramePerformanceTracker tracker;
    PerformanceStats stats = tracker.recordFrame(FrameTimingBreakdown{
                                                     20.0,
                                                     2.5,
                                                     4.5,
                                                     8.0,
                                                     1.0
                                                 },
                                                 1200,
                                                 true,
                                                 1.0f);
    require(stats.lastFrameMs == 20.0, "frame timing should retain the last frame duration");
    require(stats.lastAnalysisMs == 2.5, "timing should retain the last analysis duration");
    require(stats.lastGeometryMs == 4.5, "timing should retain the last geometry duration");
    require(stats.lastRenderMs == 8.0, "timing should retain the last renderer duration");
    require(stats.lastRecordMs == 1.0, "timing should retain the last recording duration");
    require(stats.averageCoreMs > 6.9 && stats.averageCoreMs < 7.1,
            "core timing should combine analysis and geometry work");
    require(stats.renderShare > 0.39 && stats.renderShare < 0.41,
            "render share should describe the renderer's frame-time fraction");

    stats = tracker.recordFrame(FrameTimingBreakdown{
                                    30.0,
                                    3.5,
                                    5.5,
                                    12.0,
                                    2.0
                                },
                                1800,
                                true,
                                1.0f);
    require(stats.averageFrameMs > 20.9 && stats.averageFrameMs < 21.1,
            "frame timing should be exponentially smoothed");
    require(stats.averageRenderMs > 8.3 && stats.averageRenderMs < 8.5,
            "render timing should be exponentially smoothed");
    require(stats.primitiveCount == 1800, "primitive count should remain the latest frame count");
}

void qualityScaleReducesGeometryDensity()
{
    VisualizerEngine engine;
    VisualSettings high;
    high.mode = VisualMode::TechnoMandala;
    high.qualityScale = 1.0f;

    VisualSettings low = high;
    low.qualityScale = 0.45f;

    const AudioMetrics metrics = syntheticMetrics();
    const GeometryFrame highFrame = engine.buildFrame(metrics, high, 1280.0f, 720.0f, 2.0);
    const GeometryFrame lowFrame = engine.buildFrame(metrics, low, 1280.0f, 720.0f, 2.0);
    require(countPrimitives(lowFrame) < countPrimitives(highFrame), "lower quality should reduce generated primitives");
}

void complexityControlsGeometryDensity()
{
    VisualizerEngine engine;
    VisualSettings sparse;
    sparse.mode = VisualMode::ChromaKaleidoscope;
    sparse.complexity = 0.45f;
    sparse.qualityScale = 1.0f;
    sparse.interactiveField = false;
    sparse.environmentReactive = false;

    VisualSettings dense = sparse;
    dense.complexity = 1.65f;

    const AudioMetrics metrics = syntheticMetrics();
    const GeometryFrame sparseFrame = engine.buildFrame(metrics, sparse, 1280.0f, 720.0f, 2.0);
    const GeometryFrame denseFrame = engine.buildFrame(metrics, dense, 1280.0f, 720.0f, 2.0);
    require(countPrimitives(denseFrame) > countPrimitives(sparseFrame),
            "higher complexity should increase generated geometry density");
    require(denseFrame.polylines.size() > sparseFrame.polylines.size(),
            "higher complexity should increase repeated procedural linework");
}

void offlineExporterWritesDeterministicFrames()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_export_test";
    const std::filesystem::path wavPath = root / "source.wav";
    const std::filesystem::path outPath = root / "frames";
    const std::filesystem::path syncProfilePath = root / "source.vizsync";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    writeExportTestWav(wavPath, 24000, 2400);
    {
        std::ofstream syncProfile(syncProfilePath);
        syncProfile << "beatSensitivity=1.110\n";
        syncProfile << "sectionSensitivity=1.080\n";
        syncProfile << "learnedWeight=0.250\n";
    }

    OfflineExportOptions options;
    options.inputAudio = wavPath;
    options.outputDirectory = outPath;
    options.width = 96;
    options.height = 54;
    options.frameRate = 10;
    options.maxSeconds = 0.1;
    options.settings.mode = VisualMode::QuantumTunnel;
    options.settings.palette = Palette::NeonVoltage;
    options.settings.hueShift = 0.25f;
    options.settings.depth3D = 0.83f;
    options.settings.colorImpact = 0.87f;
    options.settings.complexity = 1.33f;
    options.syncProfile = syncProfilePath;

    OfflineExportResult result;
    std::string error;
    require(exportAudioToFrames(options, result, error), "offline export should complete: " + error);
    require(result.framesWritten == 1, "0.1 seconds at 10 fps should write one frame");
    require(result.peakRms > 0.1f, "offline export should analyze audible content on the first frame");
    require(std::filesystem::exists(outPath / "frame_000000.ppm"), "offline export should write first frame");
    require(std::filesystem::exists(outPath / "export_manifest.txt"), "offline export should write manifest");
    require(std::filesystem::exists(outPath / "analysis_timeline.csv"), "offline export should write timeline");
    require(result.timelineWritten, "offline export should report timeline output");
    require(result.trackSummary.maxPrimitiveCount > 0, "offline export should summarize peak primitive count");
    require(result.trackSummary.averageBarConfidence >= 0.0f &&
                result.trackSummary.averageBarConfidence <= 1.0f,
            "offline export should summarize normalized bar confidence");
    require(result.trackSummary.averagePhraseConfidence >= 0.0f &&
                result.trackSummary.averagePhraseConfidence <= 1.0f,
            "offline export should summarize normalized phrase confidence");
    require(result.trackSummary.peakBuildTension >= 0.0f &&
                result.trackSummary.peakBuildTension <= 1.0f,
            "offline export should summarize normalized build tension");
    require(std::filesystem::file_size(outPath / "frame_000000.ppm") > 96U * 54U * 3U, "frame should include PPM pixels");
    {
        std::ifstream manifest(outPath / "export_manifest.txt");
        const std::string manifestText((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
        require(manifestText.find("hueShift=0.250") != std::string::npos,
                "offline manifest should include hue shift");
        require(manifestText.find("finalHueShift=0.250") != std::string::npos,
                "offline manifest should include final hue shift");
        require(manifestText.find("depth3D=0.830") != std::string::npos,
                "offline manifest should include 3D depth");
        require(manifestText.find("finalDepth3D=0.830") != std::string::npos,
                "offline manifest should include final 3D depth");
        require(manifestText.find("colorImpact=0.870") != std::string::npos,
                "offline manifest should include color impact");
        require(manifestText.find("finalColorImpact=0.870") != std::string::npos,
                "offline manifest should include final color impact");
        require(manifestText.find("complexity=1.330") != std::string::npos,
                "offline manifest should include complexity");
        require(manifestText.find("dominantSection=") != std::string::npos,
                "offline manifest should include dominant section");
        require(manifestText.find("timeline=") != std::string::npos,
                "offline manifest should include timeline path");
        require(manifestText.find("syncProfile=") != std::string::npos,
                "offline manifest should include sync profile path");
        require(manifestText.find("source.vizsync") != std::string::npos,
                "offline manifest should name the loaded sync profile");
        require(manifestText.find("downbeatsDetected=") != std::string::npos,
                "offline manifest should include downbeat summary");
        require(manifestText.find("phraseBoundariesDetected=") != std::string::npos,
                "offline manifest should include phrase boundary summary");
        require(manifestText.find("averageBarConfidence=") != std::string::npos,
                "offline manifest should include bar confidence summary");
        require(manifestText.find("averagePhraseConfidence=") != std::string::npos,
                "offline manifest should include phrase confidence summary");
        require(manifestText.find("peakBuildTension=") != std::string::npos,
                "offline manifest should include build tension summary");
        require(manifestText.find("dominantStyle=") != std::string::npos,
                "offline manifest should include dominant style summary");
        require(manifestText.find("maxPrimitiveCount=") != std::string::npos,
                "offline manifest should include peak primitive summary");
    }
    {
        std::ifstream timeline(outPath / "analysis_timeline.csv");
        const std::string timelineText((std::istreambuf_iterator<char>(timeline)), std::istreambuf_iterator<char>());
        require(timelineText.find("frame,timeSeconds,mode,palette") != std::string::npos,
                "timeline should include a CSV header");
        require(timelineText.find("hueShift,depth3D,colorImpact,complexity") != std::string::npos,
                "timeline should include depth and color columns");
        require(timelineText.find("\"Quantum Tunnel\"") != std::string::npos,
                "timeline should include rendered visual mode");
        require(timelineText.find("section,sectionConfidence,sectionProgress") != std::string::npos,
                "timeline should include arrangement section columns");
        require(timelineText.find("barPhase,barConfidence,downbeat,downbeatConfidence") != std::string::npos,
                "timeline should include bar and downbeat columns");
        require(timelineText.find("phraseBoundary,phrasePhase,phraseConfidence,buildTension") != std::string::npos,
                "timeline should include phrase structure columns");
        require(timelineText.find("styleAdaptation,syncAdaptation,beatSensitivity,sectionSensitivity") != std::string::npos,
                "timeline should include adaptive audio profile columns");
    }

    std::filesystem::remove_all(root);
}

void offlineExporterWritesSharePackage()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_share_package_test";
    const std::filesystem::path wavPath = root / "source.wav";
    const std::filesystem::path outPath = root / "frames";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    writeExportTestWav(wavPath, 24000, 2400);

    OfflineExportOptions options;
    options.inputAudio = wavPath;
    options.outputDirectory = outPath;
    options.width = 96;
    options.height = 54;
    options.frameRate = 10;
    options.maxSeconds = 0.1;
    options.settings.mode = VisualMode::SpectralOrigami;
    options.settings.palette = Palette::AcidAurora;
    options.settings.hueShift = 0.42f;
    options.settings.depth3D = 0.79f;
    options.settings.colorImpact = 0.86f;
    options.settings.complexity = 1.25f;
    options.environmentTimeOfDay = 0.25f;
    options.sharePackage = true;

    OfflineExportResult result;
    std::string error;
    require(exportAudioToFrames(options, result, error), "share export should complete: " + error);
    require(result.sharePackageGenerated, "share package flag should be reported");
    require(std::filesystem::exists(outPath / "share_manifest.json"), "share manifest should exist");
    require(std::filesystem::exists(outPath / "index.html"), "share page should exist");
    require(std::filesystem::exists(outPath / "analysis_timeline.csv"), "share export should write timeline");
    require(std::filesystem::exists(outPath / "preview.bmp"), "share export should write preview image");
    require(result.previewImage.filename() == "preview.bmp", "share export result should expose preview path");
    require(result.previewFramesUsed == 1, "share export result should expose preview frame count");
    require(result.previewWidth > 0 && result.previewHeight > 0, "share export result should expose preview dimensions");
    require(std::filesystem::file_size(outPath / "preview.bmp") > 54U,
            "share export preview image should contain BMP pixels");

    {
        std::ifstream manifest(outPath / "share_manifest.json");
        const std::string manifestText((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
        require(manifestText.find("\"mode\": \"Spectral Origami\"") != std::string::npos,
                "share manifest should include visual mode");
        require(manifestText.find("\"framesWritten\": 1") != std::string::npos,
                "share manifest should include frame count");
        require(manifestText.find("\"previewImage\": \"preview.bmp\"") != std::string::npos,
                "share manifest should include preview image");
        require(manifestText.find("\"previewFrames\": 1") != std::string::npos,
                "share manifest should include preview frame count");
        require(manifestText.find("\"environmentReactive\": true") != std::string::npos,
                "share manifest should include environment state");
        require(manifestText.find("\"environmentTimeOfDay\": 0.250") != std::string::npos,
                "share manifest should include environment phase");
        require(manifestText.find("\"hueShift\": 0.420") != std::string::npos,
                "share manifest should include hue shift");
        require(manifestText.find("\"finalHueShift\": 0.420") != std::string::npos,
                "share manifest should include final hue shift");
        require(manifestText.find("\"depth3D\": 0.790") != std::string::npos,
                "share manifest should include 3D depth");
        require(manifestText.find("\"finalDepth3D\": 0.790") != std::string::npos,
                "share manifest should include final 3D depth");
        require(manifestText.find("\"colorImpact\": 0.860") != std::string::npos,
                "share manifest should include color impact");
        require(manifestText.find("\"finalColorImpact\": 0.860") != std::string::npos,
                "share manifest should include final color impact");
        require(manifestText.find("\"complexity\": 1.250") != std::string::npos,
                "share manifest should include complexity");
        require(manifestText.find("\"dominantSection\":") != std::string::npos,
                "share manifest should include dominant section");
        require(manifestText.find("\"timeline\": \"analysis_timeline.csv\"") != std::string::npos,
                "share manifest should include timeline path");
        require(manifestText.find("\"timelineWritten\": true") != std::string::npos,
                "share manifest should include timeline status");
        require(manifestText.find("\"syncProfile\": \"\"") != std::string::npos,
                "share manifest should include sync profile metadata even when no profile is supplied");
        require(manifestText.find("\"trackIntelligence\":") != std::string::npos,
                "share manifest should include a track intelligence object");
        require(manifestText.find("\"averageBarConfidence\":") != std::string::npos,
                "share manifest should include bar confidence summary");
        require(manifestText.find("\"phraseBoundariesDetected\":") != std::string::npos,
                "share manifest should include phrase boundary summary");
        require(manifestText.find("\"averagePhraseConfidence\":") != std::string::npos,
                "share manifest should include phrase confidence summary");
        require(manifestText.find("\"peakBuildTension\":") != std::string::npos,
                "share manifest should include build tension summary");
        require(manifestText.find("\"dominantStyle\":") != std::string::npos,
                "share manifest should include dominant style summary");
        require(manifestText.find("\"maxPrimitiveCount\":") != std::string::npos,
                "share manifest should include primitive peak summary");
    }

    {
        std::ifstream page(outPath / "index.html");
        const std::string pageText((std::istreambuf_iterator<char>(page)), std::istreambuf_iterator<char>());
        require(pageText.find("Visualizer Export") != std::string::npos,
                "share page should include export title");
        require(pageText.find("share_manifest.json") != std::string::npos,
                "share page should link machine-readable metadata");
        require(pageText.find("analysis_timeline.csv") != std::string::npos,
                "share page should link timeline");
        require(pageText.find("<img class=\"preview\" src=\"preview.bmp\"") != std::string::npos,
                "share page should embed a BMP preview");
        require(pageText.find("Hue Shift") != std::string::npos,
                "share page should summarize hue shift");
        require(pageText.find("Final Hue Shift") != std::string::npos,
                "share page should summarize final hue shift");
        require(pageText.find("Depth 3D") != std::string::npos,
                "share page should summarize 3D depth");
        require(pageText.find("Color Impact") != std::string::npos,
                "share page should summarize color impact");
        require(pageText.find("Complexity") != std::string::npos,
                "share page should summarize complexity");
        require(pageText.find("Bar Lock") != std::string::npos,
                "share page should summarize track sync intelligence");
        require(pageText.find("Phrase Lock") != std::string::npos,
                "share page should summarize track phrase intelligence");
        require(pageText.find("Build Tension") != std::string::npos,
                "share page should summarize build tension");
        require(pageText.find("Dominant Style") != std::string::npos,
                "share page should summarize track style intelligence");
    }

    std::filesystem::remove_all(root);
}

void batchExporterWritesGalleryForAudioDirectory()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_batch_export_test";
    const std::filesystem::path input = root / "music";
    const std::filesystem::path output = root / "batch";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(input);
    writeExportTestWav(input / "Alpha Track.wav", 24000, 2400);
    writeExportTestWav(input / "beta.wav", 24000, 2400);
    {
        std::ofstream ignored(input / "notes.txt");
        ignored << "not audio\n";
    }

    BatchExportOptions options;
    options.inputDirectory = input;
    options.outputDirectory = output;
    options.width = 80;
    options.height = 45;
    options.frameRate = 10;
    options.maxSeconds = 0.1;
    options.settings.mode = VisualMode::ResonanceTessellation;
    options.settings.palette = Palette::AcidAurora;
    options.settings.complexity = 1.2f;
    options.settings.depth3D = 0.8f;
    options.settings.colorImpact = 0.84f;
    options.lookName = "Batch Tessellation";
    options.sharePackage = true;

    BatchExportResult result;
    std::string error;
    require(exportAudioBatch(options, result, error), "batch export should complete: " + error);
    require(result.filesDiscovered == 2, "batch export should discover supported audio files only");
    require(result.filesExported == 2, "batch export should render both WAV files");
    require(result.filesFailed == 0, "batch export should report no failures for generated WAV files");
    require(std::filesystem::exists(output / "batch_manifest.json"), "batch manifest should exist");
    require(std::filesystem::exists(output / "index.html"), "batch index page should exist");
    require(std::filesystem::exists(output / "001_alpha_track" / "share_manifest.json"),
            "first track should receive a share manifest");
    require(std::filesystem::exists(output / "002_beta" / "share_manifest.json"),
            "second track should receive a share manifest");
    require(std::filesystem::exists(output / "001_alpha_track" / "analysis_timeline.csv"),
            "first track should receive a timeline");
    require(std::filesystem::exists(output / "001_alpha_track" / "preview.bmp"),
            "first track should receive a preview image");
    require(std::filesystem::exists(output / "002_beta" / "preview.bmp"),
            "second track should receive a preview image");
    require(result.items.size() == 2U && !result.items.front().previewImage.empty(),
            "batch export item should expose preview image path");

    {
        std::ifstream manifest(output / "batch_manifest.json");
        const std::string manifestText((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
        require(manifestText.find("\"title\": \"Visualizer Batch Export\"") != std::string::npos,
                "batch manifest should include a stable title");
        require(manifestText.find("\"filesExported\": 2") != std::string::npos,
                "batch manifest should include exported count");
        require(manifestText.find("Alpha Track.wav") != std::string::npos,
                "batch manifest should include source file names");
        require(manifestText.find("001_alpha_track") != std::string::npos,
                "batch manifest should include deterministic output folders");
        require(manifestText.find("\"mode\": \"Resonance Tessellation\"") != std::string::npos,
                "batch manifest should include selected visual mode");
        require(manifestText.find("\"depth3D\": 0.800") != std::string::npos,
                "batch manifest should include configured 3D depth");
        require(manifestText.find("\"colorImpact\": 0.840") != std::string::npos,
                "batch manifest should include configured color impact");
        require(manifestText.find("\"trackIntelligence\":") != std::string::npos,
                "batch manifest should include per-track intelligence");
        require(manifestText.find("\"dominantStyle\":") != std::string::npos,
                "batch manifest should include per-track style summary");
        require(manifestText.find("\"averagePhraseConfidence\":") != std::string::npos,
                "batch manifest should include per-track phrase confidence");
        require(manifestText.find("\"peakBuildTension\":") != std::string::npos,
                "batch manifest should include per-track build tension");
        require(manifestText.find("\"previewImage\": \"001_alpha_track/preview.bmp\"") != std::string::npos,
                "batch manifest should include per-track preview path");
        require(manifestText.find("\"previewFrames\": 1") != std::string::npos,
                "batch manifest should include per-track preview frame count");
    }
    {
        std::ifstream index(output / "index.html");
        const std::string indexText((std::istreambuf_iterator<char>(index)), std::istreambuf_iterator<char>());
        require(indexText.find("Visualizer Batch Export") != std::string::npos,
                "batch index should include a title");
        require(indexText.find("001_alpha_track/index.html") != std::string::npos,
                "batch index should link per-track share pages");
        require(indexText.find("batch_manifest.json") != std::string::npos,
                "batch index should link machine-readable metadata");
        require(indexText.find("<th>Preview</th><th>Input</th>") != std::string::npos,
                "batch index should expose a preview column");
        require(indexText.find("<th>Style</th><th>Sync</th>") != std::string::npos,
                "batch index should expose per-track intelligence columns");
        require(indexText.find("<th>Phrase</th>") != std::string::npos,
                "batch index should expose per-track phrase columns");
        require(indexText.find("<img class=\"thumb\" src=\"001_alpha_track/preview.bmp\"") != std::string::npos,
                "batch index should embed per-track preview images");
    }
    {
        std::ifstream timeline(output / "001_alpha_track" / "analysis_timeline.csv");
        const std::string timelineText((std::istreambuf_iterator<char>(timeline)), std::istreambuf_iterator<char>());
        require(timelineText.find("\"Resonance Tessellation\"") != std::string::npos,
                "batch timeline should contain rendered mode name");
        require(timelineText.find("hueShift,depth3D,colorImpact,complexity") != std::string::npos,
                "batch timeline should contain depth and color columns");
        require(timelineText.find("phraseBoundary,phrasePhase,phraseConfidence,buildTension") != std::string::npos,
                "batch timeline should contain phrase structure columns");
    }

    std::filesystem::remove_all(root);
}

void videoEncoderBuildsShareCommand()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer video encode test";
    const std::filesystem::path frames = root / "frames with space";
    const std::filesystem::path outputMp4 = root / "share output.mp4";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(frames);
    {
        std::ofstream firstFrame(frames / "frame_000000.ppm", std::ios::binary);
        firstFrame << "P6\n1 1\n255\n";
        const unsigned char pixel[3] = {0, 0, 0};
        firstFrame.write(reinterpret_cast<const char*>(pixel), 3);
    }

    VideoEncodeOptions options;
    options.framesDirectory = frames;
    options.outputMp4 = outputMp4;
    options.frameRate = 24;
    options.crf = 20;
    options.preset = "fast";

    std::string command;
    std::string error;
    require(buildFfmpegEncodeCommand(options, command, error), "video encoder command should build: " + error);
    require(command.find("-framerate 24") != std::string::npos, "command should include requested frame rate");
    require(command.find("frame_%06d.ppm") != std::string::npos, "command should use numbered PPM input pattern");
    require(command.find("share output.mp4") != std::string::npos, "command should include quoted MP4 output path");
    require(command.find("-crf 20") != std::string::npos, "command should include requested CRF");
    require(command.find("-preset fast") != std::string::npos, "command should include requested preset");
    require(command.find("-movflags +faststart") != std::string::npos, "command should optimize MP4 for sharing");

    std::filesystem::remove_all(root);
}

} // namespace

} // namespace viz::tests

int main()
{
    const viz::tests::TestCase tests[] = {
        {"analyzerReportsSilence", viz::tests::analyzerReportsSilence},
        {"analyzerFindsBassAndStereo", viz::tests::analyzerFindsBassAndStereo},
        {"analyzerDetectsChromaAndKey", viz::tests::analyzerDetectsChromaAndKey},
        {"wavLoaderReadsGeneratedPcm", viz::tests::wavLoaderReadsGeneratedPcm},
        {"audioFileLoaderReadsGeneratedWav", viz::tests::audioFileLoaderReadsGeneratedWav},
        {"adaptiveStyleModelPredictsTechno", viz::tests::adaptiveStyleModelPredictsTechno},
        {"adaptiveStyleModelPersistsProfile", viz::tests::adaptiveStyleModelPersistsProfile},
        {"audioAnalyzerLoadsAndSavesStyleProfile", viz::tests::audioAnalyzerLoadsAndSavesStyleProfile},
        {"audioSyncProfilePersistsSensitivity", viz::tests::audioSyncProfilePersistsSensitivity},
        {"audioAnalyzerLoadsAndSavesSyncProfile", viz::tests::audioAnalyzerLoadsAndSavesSyncProfile},
        {"analyzerReportsAdvancedSyncMetrics", viz::tests::analyzerReportsAdvancedSyncMetrics},
        {"analyzerTracksBarPhaseAndDownbeats", viz::tests::analyzerTracksBarPhaseAndDownbeats},
        {"analyzerDetectsArrangementSections", viz::tests::analyzerDetectsArrangementSections},
        {"geometryModesProduceShapes", viz::tests::geometryModesProduceShapes},
        {"advancedModesReactToSyncMetrics", viz::tests::advancedModesReactToSyncMetrics},
        {"sceneDirectorAdaptsVisualSettings", viz::tests::sceneDirectorAdaptsVisualSettings},
        {"sceneDirectorEmitsModeTransitionPulse", viz::tests::sceneDirectorEmitsModeTransitionPulse},
        {"sceneTransitionAddsMorphGeometry", viz::tests::sceneTransitionAddsMorphGeometry},
        {"hyperspacePolytopeRespondsToDimensionalEnergy", viz::tests::hyperspacePolytopeRespondsToDimensionalEnergy},
        {"arrangementSectionsAddVisualAccents", viz::tests::arrangementSectionsAddVisualAccents},
        {"interactionChangesGeometry", viz::tests::interactionChangesGeometry},
        {"syncMetricsAddVisualAccents", viz::tests::syncMetricsAddVisualAccents},
        {"environmentStateAddsVisualContext", viz::tests::environmentStateAddsVisualContext},
        {"hueShiftChangesRenderedPalette", viz::tests::hueShiftChangesRenderedPalette},
        {"depth3DProjectsGeometryIntoPerspectiveSpace", viz::tests::depth3DProjectsGeometryIntoPerspectiveSpace},
        {"colorImpactStrengthensPalettePersonality", viz::tests::colorImpactStrengthensPalettePersonality},
        {"chromaKaleidoscopeRespondsToHarmony", viz::tests::chromaKaleidoscopeRespondsToHarmony},
        {"phaseWeaveRespondsToStereoHarmonyAndDrops", viz::tests::phaseWeaveRespondsToStereoHarmonyAndDrops},
        {"resonanceTessellationRespondsToHarmonyBuildsAndDrops", viz::tests::resonanceTessellationRespondsToHarmonyBuildsAndDrops},
        {"neuralConstellationRespondsToBarsHarmonyAndOnsets", viz::tests::neuralConstellationRespondsToBarsHarmonyAndOnsets},
        {"cymaticInterferenceRespondsToHarmonyBuildsAndDownbeats", viz::tests::cymaticInterferenceRespondsToHarmonyBuildsAndDownbeats},
        {"controlPanelHitTestsButtonsAndSliders", viz::tests::controlPanelHitTestsButtonsAndSliders},
        {"runtimeInspectorFormatsSourceProfileAndCaptureState", viz::tests::runtimeInspectorFormatsSourceProfileAndCaptureState},
        {"performanceTrackerAdaptsQuality", viz::tests::performanceTrackerAdaptsQuality},
        {"performanceTrackerRecordsTimingBreakdown", viz::tests::performanceTrackerRecordsTimingBreakdown},
        {"qualityScaleReducesGeometryDensity", viz::tests::qualityScaleReducesGeometryDensity},
        {"complexityControlsGeometryDensity", viz::tests::complexityControlsGeometryDensity},
        {"presetRoundTripsSettings", viz::tests::presetRoundTripsSettings},
        {"curatedPresetBankProvidesRenderableLooks", viz::tests::curatedPresetBankProvidesRenderableLooks},
        {"userPresetLibrarySavesScansAndLoads", viz::tests::userPresetLibrarySavesScansAndLoads},
        {"offlineExporterWritesDeterministicFrames", viz::tests::offlineExporterWritesDeterministicFrames},
        {"offlineExporterWritesSharePackage", viz::tests::offlineExporterWritesSharePackage},
        {"batchExporterWritesGalleryForAudioDirectory", viz::tests::batchExporterWritesGalleryForAudioDirectory},
        {"videoEncoderBuildsShareCommand", viz::tests::videoEncoderBuildsShareCommand},
        {"recorderWritesPpmFrame", viz::tests::recorderWritesPpmFrame},
        {"recorderTrailsPersistPreviousFrame", viz::tests::recorderTrailsPersistPreviousFrame},
        {"supportBundleWritesDiagnosticsWithoutCopyingLargeMedia", viz::tests::supportBundleWritesDiagnosticsWithoutCopyingLargeMedia},
        {"liveCapturePackageWritesShareMetadata", viz::tests::liveCapturePackageWritesShareMetadata}
    };

    return viz::tests::runTests(tests, static_cast<int>(sizeof(tests) / sizeof(tests[0])));
}
