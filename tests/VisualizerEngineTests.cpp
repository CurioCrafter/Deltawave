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
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
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
void analyzerClassifiesMusicalStyleCues();
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

int filledPolylineCount(const GeometryFrame& frame)
{
    return static_cast<int>(std::count_if(frame.polylines.begin(), frame.polylines.end(), [](const Polyline& line) {
        return line.filled;
    }));
}

bool containsObjectKind(const GeometryFrame& frame, Object3DKind kind)
{
    return std::any_of(frame.objects3D.begin(), frame.objects3D.end(), [kind](const Object3D& object) {
        return object.kind == kind;
    });
}

bool containsAnyObjectKind(const GeometryFrame& frame, std::initializer_list<Object3DKind> kinds)
{
    return std::any_of(kinds.begin(), kinds.end(), [&frame](Object3DKind kind) {
        return containsObjectKind(frame, kind);
    });
}

bool intentIs(SceneIntent actual, std::initializer_list<SceneIntent> expected)
{
    return std::find(expected.begin(), expected.end(), actual) != expected.end();
}

int objectKindIndex(Object3DKind kind)
{
    switch (kind) {
    case Object3DKind::Polyhedron:
        return 0;
    case Object3DKind::Shard:
        return 1;
    case Object3DKind::Ribbon:
        return 2;
    case Object3DKind::Node:
        return 3;
    case Object3DKind::Link:
        return 4;
    case Object3DKind::Plate:
        return 5;
    case Object3DKind::TunnelRib:
        return 6;
    case Object3DKind::Particle:
        return 7;
    case Object3DKind::DepthPlane:
        return 8;
    case Object3DKind::Column:
        return 9;
    case Object3DKind::Cage:
        return 10;
    case Object3DKind::WaveSurface:
        return 11;
    case Object3DKind::Orbiter:
        return 12;
    case Object3DKind::Anchor:
        return 13;
    }
    return 0;
}

std::array<int, 14> objectKindSignature(const GeometryFrame& frame)
{
    std::array<int, 14> signature{};
    for (const Object3D& object : frame.objects3D) {
        ++signature[static_cast<std::size_t>(objectKindIndex(object.kind))];
    }
    return signature;
}

int objectKindCount(const GeometryFrame& frame, Object3DKind kind)
{
    return objectKindSignature(frame)[static_cast<std::size_t>(objectKindIndex(kind))];
}

int objectFamilyCount(const GeometryFrame& frame, std::initializer_list<Object3DKind> kinds)
{
    int count = 0;
    const std::array<int, 14> signature = objectKindSignature(frame);
    for (Object3DKind kind : kinds) {
        count += signature[static_cast<std::size_t>(objectKindIndex(kind))];
    }
    return count;
}

std::array<int, 5> objectSpatialSignature(const GeometryFrame& frame)
{
    if (frame.objects3D.empty()) {
        return {};
    }

    float minX = frame.objects3D.front().position.x;
    float maxX = minX;
    float minY = frame.objects3D.front().position.y;
    float maxY = minY;
    float minZ = frame.objects3D.front().position.z;
    float maxZ = minZ;
    float sumX = 0.0f;
    float sumY = 0.0f;
    for (const Object3D& object : frame.objects3D) {
        minX = std::min(minX, object.position.x);
        maxX = std::max(maxX, object.position.x);
        minY = std::min(minY, object.position.y);
        maxY = std::max(maxY, object.position.y);
        minZ = std::min(minZ, object.position.z);
        maxZ = std::max(maxZ, object.position.z);
        sumX += object.position.x;
        sumY += object.position.y;
    }

    const float count = static_cast<float>(frame.objects3D.size());
    const float xRange = maxX - minX;
    const float yRange = maxY - minY;
    const float zRange = maxZ - minZ;
    return {
        static_cast<int>(std::round(xRange / 34.0f)),
        static_cast<int>(std::round(yRange / 34.0f)),
        static_cast<int>(std::round(zRange / 44.0f)),
        static_cast<int>(std::round((sumX / count) / 28.0f)),
        static_cast<int>(std::round((sumY / count) / 28.0f))
    };
}

float averageObjectZ(const GeometryFrame& frame)
{
    if (frame.objects3D.empty()) {
        return 0.0f;
    }
    float total = 0.0f;
    for (const Object3D& object : frame.objects3D) {
        total += object.position.z;
    }
    return total / static_cast<float>(frame.objects3D.size());
}

float averageObjectScale(const GeometryFrame& frame)
{
    if (frame.objects3D.empty()) {
        return 0.0f;
    }
    float total = 0.0f;
    for (const Object3D& object : frame.objects3D) {
        total += (object.scale.x + object.scale.y + object.scale.z) / 3.0f;
    }
    return total / static_cast<float>(frame.objects3D.size());
}

float averageObjectGlow(const GeometryFrame& frame)
{
    if (frame.objects3D.empty()) {
        return 0.0f;
    }
    float total = 0.0f;
    for (const Object3D& object : frame.objects3D) {
        total += object.glow;
    }
    return total / static_cast<float>(frame.objects3D.size());
}

float visualEnergyScore(const GeometryFrame& frame)
{
    return frame.flash * 80.0f +
           averageObjectGlow(frame) * 18.0f +
           averageObjectScale(frame) * 0.015f +
           frame.objectDepthRange * 0.006f +
           frame.cameraDepth * 0.002f +
           static_cast<float>(viz::countPrimitives(frame)) * 0.08f;
}

float objectMotionSignature(const GeometryFrame& frame)
{
    float kindMix = 0.0f;
    const std::array<int, 14> signature = objectKindSignature(frame);
    for (std::size_t i = 0; i < signature.size(); ++i) {
        kindMix += static_cast<float>(signature[i]) * static_cast<float>(i + 1U) * 0.11f;
    }
    float orbitMix = 0.0f;
    for (const Object3D& object : frame.objects3D) {
        orbitMix += std::fabs(object.position.x) * 0.0025f +
                    std::fabs(object.position.y) * 0.0035f +
                    std::fabs(object.position.z) * 0.0045f +
                    std::fabs(object.rotation.x) * 1.7f +
                    std::fabs(object.rotation.y) * 1.3f +
                    std::fabs(object.rotation.z) * 1.1f;
    }
    if (!frame.objects3D.empty()) {
        orbitMix /= static_cast<float>(frame.objects3D.size());
    }
    return kindMix +
           orbitMix +
           averageObjectGlow(frame) * 18.0f +
           averageObjectScale(frame) * 0.012f +
           std::fabs(averageObjectZ(frame)) * 0.012f +
           frame.objectDepthRange * 0.010f +
           frame.cameraDepth * 0.002f;
}

float cameraPoseSignature(const GeometryFrame& frame)
{
    return frame.cameraDepth * 0.006f +
           std::fabs(frame.cameraYaw) * 140.0f +
           std::fabs(frame.cameraPitch) * 130.0f +
           std::fabs(frame.cameraRoll) * 120.0f +
           std::fabs(frame.cameraCenterOffset.x) * 0.045f +
           std::fabs(frame.cameraCenterOffset.y) * 0.050f;
}

bool objectDepthsAreSorted(const GeometryFrame& frame)
{
    for (std::size_t i = 1; i < frame.objects3D.size(); ++i) {
        if (frame.objects3D[i - 1U].depth < frame.objects3D[i].depth) {
            return false;
        }
    }
    return true;
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
    require(directed.mode == VisualMode::PolyrhythmLattice || directed.mode == VisualMode::TechnoMandala,
            "techno metrics should direct toward a mechanical techno scene");
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
    require(dropFrame.projected3DPrimitiveCount > neutralFrame.projected3DPrimitiveCount ||
                visualEnergyScore(dropFrame) > visualEnergyScore(neutralFrame) + 1.0f,
            "drop sections should add visible 3D burst energy");
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

void recorderFillsMaterialPolygons()
{
    GeometryFrame frame;
    frame.background = ColorRGBA{0.0f, 0.0f, 0.0f, 1.0f};
    frame.polylines.push_back(Polyline{
        {Vec2{8.0f, 8.0f}, Vec2{56.0f, 8.0f}, Vec2{56.0f, 56.0f}, Vec2{8.0f, 56.0f}},
        1.0f,
        ColorRGBA{0.1f, 0.8f, 1.0f, 0.70f},
        true,
        true
    });

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_fill_polygon_test";
    std::filesystem::remove_all(root);

    FrameRecorder recorder;
    std::string error;
    require(recorder.startSession(root, 64, 64, error), "fill recorder should start: " + error);
    require(recorder.writeFrame(frame, error), "fill recorder should write: " + error);
    recorder.stop();

    const std::uint64_t filledSum = sumPpmPixelBytes(root / "frame_000000.ppm");
    std::filesystem::remove_all(root);
    require(filledSum > 280000U, "filled material polygon should affect the interior pixels, not just the outline");
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
    settings.motionStyle = MotionStyle::Liquid;
    settings.hueShift = 0.25f;
    settings.depth3D = 0.81f;
    settings.colorImpact = 0.86f;
    settings.objectDensity3D = 0.72f;
    settings.interactionDepth = 0.69f;
    settings.lightingGlow = 0.83f;
    settings.scenePersonality = 0.61f;
    settings.response3D = 0.74f;
    settings.motionStability = 0.73f;
    settings.patternClarity = 0.81f;
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
    package.finalSettings.objectDensity3D = 0.88f;
    package.finalSettings.interactionDepth = 0.77f;
    package.finalSettings.lightingGlow = 0.91f;
    package.finalSettings.scenePersonality = 0.79f;
    package.finalSettings.motionStyle = MotionStyle::Hyperspace;
    package.finalSettings.response3D = 0.96f;
    package.finalSettings.motionStability = 0.88f;
    package.finalSettings.patternClarity = 0.93f;
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
        require(manifestText.find("\"requestedObjectDensity3D\": 0.720") != std::string::npos,
                "live capture manifest should include requested 3D object density");
        require(manifestText.find("\"finalObjectDensity3D\": 0.880") != std::string::npos,
                "live capture manifest should include final 3D object density");
        require(manifestText.find("\"requestedInteractionDepth\": 0.690") != std::string::npos,
                "live capture manifest should include requested mouse depth");
        require(manifestText.find("\"finalInteractionDepth\": 0.770") != std::string::npos,
                "live capture manifest should include final mouse depth");
        require(manifestText.find("\"requestedLightingGlow\": 0.830") != std::string::npos,
                "live capture manifest should include requested 3D glow");
        require(manifestText.find("\"finalLightingGlow\": 0.910") != std::string::npos,
                "live capture manifest should include final 3D glow");
        require(manifestText.find("\"requestedScenePersonality\": 0.610") != std::string::npos,
                "live capture manifest should include requested scene personality");
        require(manifestText.find("\"finalScenePersonality\": 0.790") != std::string::npos,
                "live capture manifest should include final scene personality");
        require(manifestText.find("\"requestedMotionStyle\": \"Liquid\"") != std::string::npos,
                "live capture manifest should include requested motion style");
        require(manifestText.find("\"finalMotionStyle\": \"Hyperspace\"") != std::string::npos,
                "live capture manifest should include final motion style");
        require(manifestText.find("\"requestedResponse3D\": 0.740") != std::string::npos,
                "live capture manifest should include requested 3D response");
        require(manifestText.find("\"finalResponse3D\": 0.960") != std::string::npos,
                "live capture manifest should include final 3D response");
        require(manifestText.find("\"requestedMotionStability\": 0.730") != std::string::npos,
                "live capture manifest should include requested motion stability");
        require(manifestText.find("\"finalMotionStability\": 0.880") != std::string::npos,
                "live capture manifest should include final motion stability");
        require(manifestText.find("\"requestedPatternClarity\": 0.810") != std::string::npos,
                "live capture manifest should include requested pattern clarity");
        require(manifestText.find("\"finalPatternClarity\": 0.930") != std::string::npos,
                "live capture manifest should include final pattern clarity");
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
        require(pageText.find("3D Objects") != std::string::npos,
                "live capture page should summarize 3D object density");
        require(pageText.find("Mouse 3D") != std::string::npos,
                "live capture page should summarize mouse depth");
        require(pageText.find("3D Glow") != std::string::npos,
                "live capture page should summarize 3D glow");
        require(pageText.find("Color Impact") != std::string::npos,
                "live capture page should summarize color impact");
        require(pageText.find("Scene Personality") != std::string::npos,
                "live capture page should summarize scene personality");
        require(pageText.find("Motion Style") != std::string::npos,
                "live capture page should summarize motion style");
        require(pageText.find("3D Response") != std::string::npos,
                "live capture page should summarize 3D response");
        require(pageText.find("Motion Stability") != std::string::npos,
                "live capture page should summarize motion stability");
        require(pageText.find("Pattern Clarity") != std::string::npos,
                "live capture page should summarize pattern clarity");
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
    require(std::fabs(averageObjectZ(interactive) - averageObjectZ(neutral)) > 0.1f ||
                std::fabs(interactive.projected3DVisualWeight - neutral.projected3DVisualWeight) > 0.1f,
            "interaction should change depth-aware 3D geometry");
    require(interactive.threeDDominance > 0.5f, "interaction should keep 3D as the visible layer");
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
    require(visualEnergyScore(phraseFrame) > visualEnergyScore(neutral) + 0.8f ||
                phraseFrame.projected3DPrimitiveCount > neutral.projected3DPrimitiveCount,
            "phrase build tension should add visible 3D tension energy");
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
    require(deep.cameraDepth > 500.0f, "3D depth should create a real perspective camera");
    require(deep.objectDepthRange > 100.0f, "3D depth should create a visible object depth range");
    require(!deep.objects3D.empty(), "3D depth should author 3D objects instead of relying on flat rings");
    require(deep.projected3DPrimitiveCount > deep.retained2DPrimitiveCount,
            "3D depth should make projected 3D primitives dominate retained 2D guides");
    require(deep.retained2DPrimitiveRatio < 0.36f,
            "high-depth projection should suppress most legacy 2D guide primitives");
    require(averagePolylinePointDistance(deep, Vec2{640.0f, 360.0f}) !=
                averagePolylinePointDistance(flat, Vec2{640.0f, 360.0f}),
            "3D depth should project polyline points through perspective space");
}

void object3DModesProduceDistinctSignatures()
{
    struct ExpectedMode {
        VisualMode mode;
        std::string_view name;
        std::initializer_list<Object3DKind> requiredKinds;
    };

    const ExpectedMode modes[] = {
        {VisualMode::QuantumTunnel, "Quantum Tunnel Volume", {Object3DKind::DepthPlane, Object3DKind::Orbiter}},
        {VisualMode::TechnoMandala, "Techno Mandala Machine", {Object3DKind::Column, Object3DKind::Cage}},
        {VisualMode::LissajousMesh, "Lissajous Ribbon Mesh", {Object3DKind::Ribbon}},
        {VisualMode::FrequencyBloom, "Frequency Bloom Sculpture", {Object3DKind::WaveSurface}},
        {VisualMode::FractalCathedral, "Fractal Cathedral Vault", {Object3DKind::Column, Object3DKind::Cage}},
        {VisualMode::PolyrhythmLattice, "Polyrhythm Lattice Rig", {Object3DKind::Column}},
        {VisualMode::SpectralOrigami, "Spectral Origami Storm", {Object3DKind::DepthPlane, Object3DKind::Shard}},
        {VisualMode::ChromaKaleidoscope, "Chroma Kaleidoscope Prism", {Object3DKind::Cage}},
        {VisualMode::HyperspacePolytope, "Hyperspace Polytope Cage", {Object3DKind::Cage}},
        {VisualMode::PhaseWeave, "Phase Weave Current", {Object3DKind::Ribbon}},
        {VisualMode::ResonanceTessellation, "Resonance Tessellation Field", {Object3DKind::DepthPlane}},
        {VisualMode::NeuralConstellation, "Neural Constellation Depth", {Object3DKind::Anchor}},
        {VisualMode::CymaticInterference, "Cymatic Interference Sculpture", {Object3DKind::WaveSurface}}
    };

    VisualizerEngine engine;
    VisualSettings settings;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.9f;
    settings.lightingGlow = 0.86f;
    settings.scenePersonality = 0.82f;
    settings.response3D = 1.0f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.rms = 0.66f;
    metrics.beat = true;
    metrics.beatConfidence = 0.94f;
    metrics.barConfidence = 0.86f;
    metrics.downbeat = true;
    metrics.downbeatConfidence = 0.9f;
    metrics.dropIntensity = 0.82f;
    metrics.phraseIntensity = 0.74f;
    metrics.phraseBoundary = true;
    metrics.buildTension = 0.8f;
    metrics.harmonicEnergy = 0.82f;
    metrics.keyConfidence = 0.78f;
    metrics.treble = 0.78f;
    metrics.stereoWidth = 0.72f;
    metrics.spectralFlux = 0.55f;

    std::vector<std::string_view> names;
    std::vector<std::array<int, 14>> signatures;
    for (std::size_t i = 0; i < std::size(modes); ++i) {
        settings.mode = modes[i].mode;
        const GeometryFrame frame = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 2.0 + static_cast<double>(i));
        require(frame.scene3DName == modes[i].name, "each visual mode should resolve to its own 3D scene identity");
        require(std::find(names.begin(), names.end(), frame.scene3DName) == names.end(),
                "all visual modes should expose distinct 3D scene names");
        names.push_back(frame.scene3DName);
        require(frame.objects3D.size() >= 10U, "each mode should emit meaningful 3D object content");
        require(containsAnyObjectKind(frame, modes[i].requiredKinds),
                "each mode should include its expected mode-specific 3D object kind");
        require(!frame.polylines.empty() || !frame.particles.empty(),
                "3D mode objects should project into visible geometry primitives");
        require(frame.cameraDepth > 0.0f, "each 3D mode should report camera depth");
        require(frame.objectDepthRange > 1.0f, "each 3D mode should occupy a measurable z range");

        const std::array<int, 14> signature = objectKindSignature(frame);
        require(std::find(signatures.begin(), signatures.end(), signature) == signatures.end(),
                "mode-specific 3D signatures should not collapse to identical object-kind mixes");
        signatures.push_back(signature);
    }
}

void object3DRespondsStronglyToMusicAcrossModes()
{
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

    VisualizerEngine engine;
    VisualSettings settings;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.78f;
    settings.lightingGlow = 0.82f;
    settings.scenePersonality = 0.74f;
    settings.response3D = 1.0f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.9f;

    AudioMetrics calm = syntheticMetrics();
    calm.rms = 0.04f;
    calm.peak = 0.08f;
    calm.bass = 0.03f;
    calm.lowMid = 0.02f;
    calm.mid = 0.02f;
    calm.highMid = 0.02f;
    calm.treble = 0.02f;
    calm.stereoWidth = 0.04f;
    calm.spectralFlux = 0.01f;
    calm.onset = 0.0f;
    calm.beat = false;
    calm.beatConfidence = 0.02f;
    calm.downbeat = false;
    calm.downbeatConfidence = 0.0f;
    calm.dropIntensity = 0.0f;
    calm.phraseIntensity = 0.0f;
    calm.phraseBoundary = false;
    calm.phraseConfidence = 0.0f;
    calm.buildTension = 0.0f;
    calm.harmonicEnergy = 0.02f;
    calm.keyConfidence = 0.0f;
    calm.bandOnsets = {};

    AudioMetrics intense = syntheticMetrics();
    intense.rms = 0.82f;
    intense.peak = 1.0f;
    intense.bass = 0.92f;
    intense.lowMid = 0.7f;
    intense.mid = 0.58f;
    intense.highMid = 0.74f;
    intense.treble = 0.86f;
    intense.stereoWidth = 0.88f;
    intense.spectralFlux = 0.74f;
    intense.onset = 0.82f;
    intense.beat = true;
    intense.beatConfidence = 0.96f;
    intense.beatPhase = 0.12f;
    intense.downbeat = true;
    intense.downbeatConfidence = 0.92f;
    intense.dropIntensity = 0.9f;
    intense.phraseIntensity = 0.86f;
    intense.phraseBoundary = true;
    intense.phrasePhase = 0.9f;
    intense.phraseConfidence = 0.84f;
    intense.buildTension = 0.88f;
    intense.harmonicEnergy = 0.84f;
    intense.keyConfidence = 0.82f;
    intense.bandOnsets = {0.85f, 0.65f, 0.42f, 0.7f, 0.58f};

    for (std::size_t i = 0; i < std::size(modes); ++i) {
        settings.mode = modes[i];
        const double time = 3.5 + static_cast<double>(i) * 0.17;
        const GeometryFrame calmFrame = engine.buildFrame(calm, settings, 1280.0f, 720.0f, time);
        const GeometryFrame intenseFrame = engine.buildFrame(intense, settings, 1280.0f, 720.0f, time);

        require(!calmFrame.objects3D.empty(), "calm music should keep a sparse 3D scaffold");
        require(!intenseFrame.objects3D.empty(), "intense music should emit 3D objects");
        require(intenseFrame.objects3D.size() >= calmFrame.objects3D.size(),
                "intense music should preserve or increase 3D object count");

        const bool countMoved = intenseFrame.objects3D.size() > calmFrame.objects3D.size();
        const bool glowMoved = averageObjectGlow(intenseFrame) > averageObjectGlow(calmFrame) + 0.04f;
        const bool scaleMoved = std::fabs(averageObjectScale(intenseFrame) - averageObjectScale(calmFrame)) > 0.35f;
        const bool zMoved = std::fabs(averageObjectZ(intenseFrame) - averageObjectZ(calmFrame)) > 8.0f;
        const bool depthMoved = std::fabs(intenseFrame.objectDepthRange - calmFrame.objectDepthRange) > 8.0f;
        const bool cameraMoved = std::fabs(intenseFrame.cameraDepth - calmFrame.cameraDepth) > 8.0f;
        const bool primitiveMoved = countPrimitives(intenseFrame) > countPrimitives(calmFrame) + 4;

        require(countMoved || glowMoved || scaleMoved || zMoved || depthMoved || cameraMoved || primitiveMoved,
                "every mode's 3D layer should materially react to intense music metrics");
    }
}

void allModesStay3DFirstInStillFrames()
{
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

    VisualizerEngine engine;
    VisualSettings settings;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.92f;
    settings.lightingGlow = 0.9f;
    settings.scenePersonality = 0.86f;
    settings.response3D = 1.0f;
    settings.motionStability = 0.86f;
    settings.patternClarity = 0.9f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.rms = 0.62f;
    metrics.peak = 0.94f;
    metrics.bass = 0.72f;
    metrics.lowMid = 0.56f;
    metrics.mid = 0.52f;
    metrics.highMid = 0.68f;
    metrics.treble = 0.74f;
    metrics.stereoWidth = 0.70f;
    metrics.spectralFlux = 0.58f;
    metrics.onset = 0.54f;
    metrics.beat = true;
    metrics.beatConfidence = 0.9f;
    metrics.downbeat = true;
    metrics.downbeatConfidence = 0.78f;
    metrics.dropIntensity = 0.52f;
    metrics.phraseIntensity = 0.70f;
    metrics.phraseConfidence = 0.82f;
    metrics.buildTension = 0.68f;
    metrics.keyIndex = 5;
    metrics.keyMode = MusicalMode::Minor;
    metrics.keyConfidence = 0.78f;
    metrics.harmonicEnergy = 0.76f;
    metrics.bandOnsets = {0.72f, 0.58f, 0.42f, 0.66f, 0.60f};

    std::vector<std::array<int, 5>> spatialSignatures;
    for (std::size_t i = 0; i < std::size(modes); ++i) {
        settings.mode = modes[i];
        const GeometryFrame frame = engine.buildFrame(metrics,
                                                      settings,
                                                      1280.0f,
                                                      720.0f,
                                                      5.0 + static_cast<double>(i) * 0.23);
        require(frame.projected3DPrimitiveCount > 0, "each visual mode should project real 3D geometry");
        require(frame.objects3D.size() >= 8U, "each visual mode should author a meaningful 3D object set");
        require(frame.retained2DPrimitiveCount < frame.authored2DPrimitiveCount,
                "3D-first composition should suppress legacy 2D primitives in every mode");
        require(frame.retained2DPrimitiveRatio < 0.36f,
                "each mode should retain only a small legacy 2D guide layer");
        require(frame.retained2DVisualRatio < 0.20f,
                "each mode should heavily fade legacy 2D visual weight");
        require(frame.projected3DVisualWeight > frame.retained2DVisualWeight * 1.4f,
                "projected 3D visual weight should dominate retained 2D guides in every mode");
        require(frame.threeDDominance > 1.25f, "each mode should report clear 3D dominance");
        require(frame.objectDepthRange > 80.0f, "each mode should occupy visible depth in still captures");
        spatialSignatures.push_back(objectSpatialSignature(frame));
    }

    std::sort(spatialSignatures.begin(), spatialSignatures.end());
    const auto uniqueSpatialEnd = std::unique(spatialSignatures.begin(), spatialSignatures.end());
    require(std::distance(spatialSignatures.begin(), uniqueSpatialEnd) >= 9,
            "manual modes should not collapse into the same 3D spatial silhouette");
}

void songProfilesScaleMusicallyWithoutChaos()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::PhaseWeave;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.82f;
    settings.lightingGlow = 0.82f;
    settings.scenePersonality = 0.76f;
    settings.response3D = 0.88f;
    settings.motionStability = 0.78f;
    settings.patternClarity = 0.84f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.9f;

    AudioMetrics silence{};
    silence.style = AudioStyle::Silence;
    silence.section = ArrangementSection::Silence;
    silence.beatPhase = 0.42f;
    silence.barPhase = 0.2f;
    silence.phrasePhase = 0.3f;

    AudioMetrics low = syntheticMetrics();
    low.rms = 0.055f;
    low.peak = 0.10f;
    low.bass = 0.04f;
    low.lowMid = 0.035f;
    low.mid = 0.04f;
    low.highMid = 0.04f;
    low.treble = 0.04f;
    low.stereoWidth = 0.08f;
    low.spectralFlux = 0.02f;
    low.beat = false;
    low.beatConfidence = 0.06f;
    low.dropIntensity = 0.0f;
    low.phraseIntensity = 0.06f;
    low.buildTension = 0.02f;
    low.style = AudioStyle::Ambient;
    low.styleConfidence = 0.55f;

    AudioMetrics steady = syntheticMetrics();
    steady.rms = 0.34f;
    steady.peak = 0.58f;
    steady.bass = 0.48f;
    steady.lowMid = 0.36f;
    steady.mid = 0.28f;
    steady.highMid = 0.26f;
    steady.treble = 0.24f;
    steady.stereoWidth = 0.32f;
    steady.spectralFlux = 0.16f;
    steady.beat = true;
    steady.beatConfidence = 0.86f;
    steady.beatPhase = 0.08f;
    steady.dropIntensity = 0.08f;
    steady.phraseIntensity = 0.28f;
    steady.buildTension = 0.12f;
    steady.style = AudioStyle::Techno;
    steady.styleConfidence = 0.86f;
    steady.section = ArrangementSection::Groove;

    AudioMetrics ambient = low;
    ambient.rms = 0.22f;
    ambient.peak = 0.34f;
    ambient.bass = 0.12f;
    ambient.lowMid = 0.20f;
    ambient.mid = 0.28f;
    ambient.highMid = 0.34f;
    ambient.treble = 0.36f;
    ambient.stereoWidth = 0.74f;
    ambient.phraseIntensity = 0.62f;
    ambient.phraseConfidence = 0.78f;
    ambient.harmonicEnergy = 0.68f;
    ambient.keyConfidence = 0.74f;
    ambient.style = AudioStyle::Ambient;
    ambient.styleConfidence = 0.9f;
    ambient.section = ArrangementSection::Breakdown;

    AudioMetrics breakbeat = steady;
    breakbeat.rms = 0.48f;
    breakbeat.peak = 0.78f;
    breakbeat.bass = 0.58f;
    breakbeat.treble = 0.56f;
    breakbeat.spectralFlux = 0.52f;
    breakbeat.onset = 0.66f;
    breakbeat.beatConfidence = 0.72f;
    breakbeat.beatPhase = 0.28f;
    breakbeat.dropIntensity = 0.22f;
    breakbeat.style = AudioStyle::Bright;
    breakbeat.styleConfidence = 0.68f;
    breakbeat.bandOnsets = {0.64f, 0.50f, 0.42f, 0.56f, 0.62f};

    AudioMetrics drop = steady;
    drop.rms = 0.82f;
    drop.peak = 1.0f;
    drop.bass = 0.94f;
    drop.lowMid = 0.74f;
    drop.mid = 0.58f;
    drop.highMid = 0.70f;
    drop.treble = 0.82f;
    drop.stereoWidth = 0.78f;
    drop.spectralFlux = 0.70f;
    drop.onset = 0.86f;
    drop.beatConfidence = 0.96f;
    drop.beatPhase = 0.04f;
    drop.downbeat = true;
    drop.downbeatConfidence = 0.92f;
    drop.dropIntensity = 0.92f;
    drop.phraseIntensity = 0.82f;
    drop.phraseBoundary = true;
    drop.buildTension = 0.82f;
    drop.harmonicEnergy = 0.80f;
    drop.keyConfidence = 0.72f;
    drop.style = AudioStyle::BassHeavy;
    drop.styleConfidence = 0.9f;
    drop.section = ArrangementSection::Drop;
    drop.bandOnsets = {0.90f, 0.74f, 0.52f, 0.62f, 0.70f};

    const GeometryFrame silenceFrame = engine.buildFrame(silence, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame lowFrame = engine.buildFrame(low, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame steadyFrame = engine.buildFrame(steady, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame ambientFrame = engine.buildFrame(ambient, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame breakbeatFrame = engine.buildFrame(breakbeat, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame dropFrame = engine.buildFrame(drop, settings, 1280.0f, 720.0f, 2.0);

    require(silenceFrame.flash == 0.0f, "silence should not trigger beat/drop flash");
    require(!silenceFrame.objects3D.empty(), "silence should keep a readable base 3D scaffold");
    require(visualEnergyScore(lowFrame) > visualEnergyScore(silenceFrame) + 0.5f,
            "low-volume audio should move more than silence");
    require(visualEnergyScore(steadyFrame) > visualEnergyScore(lowFrame) + 1.0f,
            "steady techno should feel more active than low-volume audio");
    require(visualEnergyScore(ambientFrame) > visualEnergyScore(silenceFrame) + 1.0f,
            "ambient material should still create musical phrase motion");
    require(visualEnergyScore(breakbeatFrame) > visualEnergyScore(lowFrame) + 2.0f,
            "breakbeat transients should add visible accents");
    require(visualEnergyScore(dropFrame) > visualEnergyScore(steadyFrame) + 3.0f,
            "bass-heavy drops should push the scene harder than a steady groove");
    require(dropFrame.flash < 0.7f, "drop flash should be intense but not a full-screen panic strobe");
    require(dropFrame.objectDepthRange < 3200.0f, "drop response should stay within a readable depth range");
}

void motionStylesCreateDistinct3DChoreography()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::HyperspacePolytope;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.86f;
    settings.lightingGlow = 0.88f;
    settings.scenePersonality = 0.8f;
    settings.response3D = 0.94f;
    settings.motionStability = 0.68f;
    settings.patternClarity = 0.78f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.rms = 0.62f;
    metrics.peak = 0.96f;
    metrics.bass = 0.82f;
    metrics.treble = 0.72f;
    metrics.stereoWidth = 0.74f;
    metrics.spectralFlux = 0.68f;
    metrics.onset = 0.66f;
    metrics.beat = true;
    metrics.beatConfidence = 0.94f;
    metrics.beatPhase = 0.08f;
    metrics.dropIntensity = 0.72f;
    metrics.phraseIntensity = 0.64f;
    metrics.phrasePhase = 0.38f;
    metrics.buildTension = 0.58f;
    metrics.keyIndex = 7;
    metrics.keyConfidence = 0.82f;
    metrics.harmonicEnergy = 0.74f;
    metrics.bandOnsets = {0.78f, 0.54f, 0.44f, 0.62f, 0.76f};

    const MotionStyle styles[] = {
        MotionStyle::Smooth,
        MotionStyle::Mechanical,
        MotionStyle::Liquid,
        MotionStyle::Hyperspace,
        MotionStyle::HeavyBass,
        MotionStyle::AmbientDrift,
        MotionStyle::Breakbeat
    };
    std::vector<int> signatureBuckets;
    for (MotionStyle style : styles) {
        settings.motionStyle = style;
        const GeometryFrame frame = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 4.0);
        require(!frame.objects3D.empty(), "each motion style should render 3D choreography objects");
        require(frame.objectDepthRange > 120.0f, "each motion style should preserve visible depth");
        signatureBuckets.push_back(static_cast<int>(std::round(objectMotionSignature(frame) * 4.0f)));
    }
    std::sort(signatureBuckets.begin(), signatureBuckets.end());
    const auto uniqueEnd = std::unique(signatureBuckets.begin(), signatureBuckets.end());
    const int uniqueMotionBuckets = static_cast<int>(std::distance(signatureBuckets.begin(), uniqueEnd));
    std::string bucketMessage = "motion styles should produce several distinct 3D choreography signatures; buckets:";
    for (int bucket : signatureBuckets) {
        bucketMessage += " " + std::to_string(bucket);
    }
    require(uniqueMotionBuckets >= 5, bucketMessage);
}

void musicProfilesDriveDifferent3DChoreography()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::QuantumTunnel;
    settings.motionStyle = MotionStyle::HeavyBass;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.82f;
    settings.lightingGlow = 0.86f;
    settings.scenePersonality = 0.76f;
    settings.response3D = 0.96f;
    settings.motionStability = 0.72f;
    settings.patternClarity = 0.82f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics ambient = syntheticMetrics();
    ambient.rms = 0.20f;
    ambient.peak = 0.34f;
    ambient.bass = 0.14f;
    ambient.lowMid = 0.22f;
    ambient.mid = 0.28f;
    ambient.highMid = 0.20f;
    ambient.treble = 0.16f;
    ambient.stereoWidth = 0.72f;
    ambient.spectralFlux = 0.08f;
    ambient.onset = 0.04f;
    ambient.beat = false;
    ambient.beatConfidence = 0.10f;
    ambient.dropIntensity = 0.0f;
    ambient.phraseIntensity = 0.58f;
    ambient.phraseConfidence = 0.78f;
    ambient.harmonicEnergy = 0.68f;
    ambient.style = AudioStyle::Ambient;
    ambient.styleConfidence = 0.9f;

    AudioMetrics bassDrop = syntheticMetrics();
    bassDrop.rms = 0.78f;
    bassDrop.peak = 1.0f;
    bassDrop.bass = 0.98f;
    bassDrop.lowMid = 0.76f;
    bassDrop.treble = 0.30f;
    bassDrop.stereoWidth = 0.42f;
    bassDrop.spectralFlux = 0.44f;
    bassDrop.onset = 0.78f;
    bassDrop.beat = true;
    bassDrop.beatConfidence = 0.98f;
    bassDrop.beatPhase = 0.04f;
    bassDrop.dropIntensity = 0.96f;
    bassDrop.phraseIntensity = 0.70f;
    bassDrop.buildTension = 0.74f;
    bassDrop.style = AudioStyle::BassHeavy;
    bassDrop.styleConfidence = 0.92f;
    bassDrop.section = ArrangementSection::Drop;
    bassDrop.bandOnsets = {0.94f, 0.72f, 0.44f, 0.28f, 0.18f};

    AudioMetrics brightBreaks = syntheticMetrics();
    brightBreaks.rms = 0.54f;
    brightBreaks.peak = 0.92f;
    brightBreaks.bass = 0.30f;
    brightBreaks.lowMid = 0.34f;
    brightBreaks.highMid = 0.82f;
    brightBreaks.treble = 0.94f;
    brightBreaks.stereoWidth = 0.62f;
    brightBreaks.spectralFlux = 0.86f;
    brightBreaks.onset = 0.82f;
    brightBreaks.beat = true;
    brightBreaks.beatConfidence = 0.72f;
    brightBreaks.dropIntensity = 0.20f;
    brightBreaks.phraseIntensity = 0.42f;
    brightBreaks.keyIndex = 2;
    brightBreaks.keyConfidence = 0.78f;
    brightBreaks.harmonicEnergy = 0.58f;
    brightBreaks.style = AudioStyle::Bright;
    brightBreaks.styleConfidence = 0.88f;
    brightBreaks.bandOnsets = {0.26f, 0.34f, 0.62f, 0.82f, 0.94f};

    const GeometryFrame ambientFrame = engine.buildFrame(ambient, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame bassFrame = engine.buildFrame(bassDrop, settings, 1280.0f, 720.0f, 2.0);
    settings.mode = VisualMode::SpectralOrigami;
    settings.motionStyle = MotionStyle::Breakbeat;
    const GeometryFrame brightFrame = engine.buildFrame(brightBreaks, settings, 1280.0f, 720.0f, 2.0);

    require(visualEnergyScore(ambientFrame) > visualEnergyScore(GeometryFrame{}) + 1.0f,
            "ambient music should still create visible 3D motion");
    require(bassFrame.objectDepthRange > ambientFrame.objectDepthRange + 120.0f ||
                std::fabs(averageObjectZ(bassFrame) - averageObjectZ(ambientFrame)) > 18.0f,
            "bass drops should create stronger depth pressure than ambient music");
    require(visualEnergyScore(bassFrame) > visualEnergyScore(ambientFrame) + 2.0f,
            "bass-heavy drops should feel more physically intense than ambient profiles");
    require(averageObjectGlow(brightFrame) > averageObjectGlow(ambientFrame),
            "bright transient material should create more shimmer/glow than ambient profiles");
    require(std::fabs(objectMotionSignature(bassFrame) - objectMotionSignature(brightFrame)) > 4.0f,
            "bass and bright breakbeat profiles should not collapse into the same 3D choreography");
}

void threeDFirstCompositionSuppressesLegacy2D()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::CymaticInterference;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.9f;
    settings.lightingGlow = 0.9f;
    settings.scenePersonality = 0.82f;
    settings.response3D = 1.0f;
    settings.motionStability = 0.84f;
    settings.patternClarity = 0.9f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.rms = 0.68f;
    metrics.peak = 0.96f;
    metrics.bass = 0.72f;
    metrics.lowMid = 0.58f;
    metrics.highMid = 0.76f;
    metrics.treble = 0.82f;
    metrics.stereoWidth = 0.76f;
    metrics.spectralFlux = 0.64f;
    metrics.onset = 0.56f;
    metrics.beat = true;
    metrics.beatConfidence = 0.92f;
    metrics.dropIntensity = 0.58f;
    metrics.phraseIntensity = 0.82f;
    metrics.phraseConfidence = 0.8f;
    metrics.buildTension = 0.72f;
    metrics.keyIndex = 4;
    metrics.keyMode = MusicalMode::Major;
    metrics.keyConfidence = 0.84f;
    metrics.harmonicEnergy = 0.86f;
    metrics.bandOnsets = {0.66f, 0.52f, 0.46f, 0.58f, 0.72f};

    const GeometryFrame frame = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 4.0);

    require(frame.authored2DPrimitiveCount > frame.retained2DPrimitiveCount * 2,
            "3D-first composition should substantially thin legacy screen-space primitive counts");
    require(frame.authored2DVisualWeight > frame.retained2DVisualWeight * 4.0f,
            "3D-first composition should strongly fade legacy 2D visual weight");
    require(frame.retained2DPrimitiveRatio < 0.22f,
            "high-depth 3D-first composition should keep only a small fraction of legacy 2D primitives");
    require(frame.retained2DVisualRatio < 0.08f,
            "high-depth 3D-first composition should leave very little legacy 2D visual weight");
    require(frame.projected3DPrimitiveCount > frame.retained2DPrimitiveCount,
            "projected 3D primitives should outnumber retained 2D composition guides");
    require(frame.projected3DVisualWeight > frame.retained2DVisualWeight * 1.5f,
            "projected 3D visual weight should dominate the retained 2D layer");
    require(frame.threeDDominance > 1.5f, "frame should report visible 3D dominance");
    require(!frame.objects3D.empty(), "3D-first frame should still be authored as 3D objects");
}

void sceneIntentProfilesProduceDistinct3DInterpretations()
{
    struct Profile {
        const char* name;
        AudioMetrics metrics;
        std::initializer_list<SceneIntent> expected;
    };

    AudioMetrics silence{};
    silence.style = AudioStyle::Silence;
    silence.section = ArrangementSection::Silence;
    silence.beatPhase = 0.4f;
    silence.barPhase = 0.2f;
    silence.phrasePhase = 0.3f;

    AudioMetrics low = syntheticMetrics();
    low.rms = 0.05f;
    low.peak = 0.09f;
    low.bass = 0.04f;
    low.lowMid = 0.03f;
    low.mid = 0.03f;
    low.highMid = 0.02f;
    low.treble = 0.02f;
    low.stereoWidth = 0.08f;
    low.spectralFlux = 0.02f;
    low.beat = false;
    low.beatConfidence = 0.04f;
    low.style = AudioStyle::Silence;

    AudioMetrics techno = syntheticMetrics();
    techno.rms = 0.48f;
    techno.bass = 0.62f;
    techno.lowMid = 0.54f;
    techno.treble = 0.28f;
    techno.stereoWidth = 0.34f;
    techno.spectralFlux = 0.22f;
    techno.beat = true;
    techno.beatConfidence = 0.95f;
    techno.barConfidence = 0.82f;
    techno.downbeatConfidence = 0.76f;
    techno.style = AudioStyle::Techno;
    techno.styleConfidence = 0.92f;
    techno.section = ArrangementSection::Groove;
    techno.sectionConfidence = 0.82f;

    AudioMetrics bassDrop = syntheticMetrics();
    bassDrop.rms = 0.82f;
    bassDrop.peak = 1.0f;
    bassDrop.bass = 0.98f;
    bassDrop.lowMid = 0.82f;
    bassDrop.treble = 0.22f;
    bassDrop.stereoWidth = 0.42f;
    bassDrop.spectralFlux = 0.40f;
    bassDrop.onset = 0.86f;
    bassDrop.beat = true;
    bassDrop.beatConfidence = 0.96f;
    bassDrop.dropIntensity = 0.98f;
    bassDrop.style = AudioStyle::BassHeavy;
    bassDrop.styleConfidence = 0.94f;
    bassDrop.section = ArrangementSection::Drop;
    bassDrop.sectionConfidence = 0.92f;

    AudioMetrics ambient = syntheticMetrics();
    ambient.rms = 0.18f;
    ambient.peak = 0.28f;
    ambient.bass = 0.10f;
    ambient.lowMid = 0.18f;
    ambient.mid = 0.24f;
    ambient.treble = 0.16f;
    ambient.stereoWidth = 0.88f;
    ambient.spectralFlux = 0.06f;
    ambient.beat = false;
    ambient.beatConfidence = 0.06f;
    ambient.phraseIntensity = 0.54f;
    ambient.phraseConfidence = 0.76f;
    ambient.harmonicEnergy = 0.58f;
    ambient.style = AudioStyle::Ambient;
    ambient.styleConfidence = 0.9f;

    AudioMetrics melodic = syntheticMetrics();
    melodic.rms = 0.38f;
    melodic.bass = 0.22f;
    melodic.mid = 0.58f;
    melodic.highMid = 0.60f;
    melodic.treble = 0.52f;
    melodic.stereoWidth = 0.54f;
    melodic.spectralFlux = 0.22f;
    melodic.keyIndex = 7;
    melodic.keyMode = MusicalMode::Major;
    melodic.keyConfidence = 0.94f;
    melodic.harmonicEnergy = 0.92f;
    melodic.phraseConfidence = 0.84f;

    AudioMetrics breakbeat = syntheticMetrics();
    breakbeat.rms = 0.56f;
    breakbeat.bass = 0.34f;
    breakbeat.highMid = 0.82f;
    breakbeat.treble = 0.88f;
    breakbeat.stereoWidth = 0.62f;
    breakbeat.spectralFlux = 0.92f;
    breakbeat.onset = 0.88f;
    breakbeat.beat = true;
    breakbeat.beatConfidence = 0.56f;
    breakbeat.style = AudioStyle::Bright;
    breakbeat.styleConfidence = 0.86f;
    breakbeat.bandOnsets = {0.28f, 0.42f, 0.72f, 0.92f, 0.86f};

    AudioMetrics darkMinimal = syntheticMetrics();
    darkMinimal.rms = 0.30f;
    darkMinimal.bass = 0.56f;
    darkMinimal.lowMid = 0.50f;
    darkMinimal.mid = 0.20f;
    darkMinimal.highMid = 0.08f;
    darkMinimal.treble = 0.04f;
    darkMinimal.stereoWidth = 0.18f;
    darkMinimal.spectralFlux = 0.05f;
    darkMinimal.beat = true;
    darkMinimal.beatConfidence = 0.62f;
    darkMinimal.keyIndex = 10;
    darkMinimal.keyMode = MusicalMode::Minor;
    darkMinimal.keyConfidence = 0.72f;
    darkMinimal.style = AudioStyle::Techno;
    darkMinimal.styleConfidence = 0.64f;
    darkMinimal.section = ArrangementSection::Breakdown;
    darkMinimal.sectionConfidence = 0.76f;

    const Profile profiles[] = {
        {"silence", silence, {SceneIntent::Calm}},
        {"low-volume", low, {SceneIntent::Calm, SceneIntent::Minimal}},
        {"techno", techno, {SceneIntent::Groove, SceneIntent::Industrial}},
        {"bass drop", bassDrop, {SceneIntent::Drop, SceneIntent::Heavy}},
        {"ambient", ambient, {SceneIntent::Spacious, SceneIntent::Calm, SceneIntent::Melodic}},
        {"melodic", melodic, {SceneIntent::Melodic, SceneIntent::Bright}},
        {"breakbeat", breakbeat, {SceneIntent::Chaotic, SceneIntent::Bright}},
        {"dark minimal", darkMinimal, {SceneIntent::Dark, SceneIntent::Minimal, SceneIntent::Industrial}}
    };

    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::PhaseWeave;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.86f;
    settings.lightingGlow = 0.86f;
    settings.scenePersonality = 0.8f;
    settings.response3D = 0.96f;
    settings.motionStability = 0.82f;
    settings.patternClarity = 0.88f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.9f;

    std::vector<SceneIntent> intents;
    std::vector<std::array<int, 14>> signatures;
    for (std::size_t i = 0; i < std::size(profiles); ++i) {
        const GeometryFrame frame = engine.buildFrame(profiles[i].metrics,
                                                      settings,
                                                      1280.0f,
                                                      720.0f,
                                                      1.0 + static_cast<double>(i) * 0.4);
        require(intentIs(frame.sceneIntent, profiles[i].expected),
                std::string(profiles[i].name) + " profile resolved to " + std::string(toString(frame.sceneIntent)) +
                    " instead of an appropriate musical scene intent");
        require(frame.projected3DPrimitiveCount > 0, "profile should project visible 3D geometry");
        require(frame.threeDDominance > 0.75f, "profile should keep projected 3D visually dominant");
        intents.push_back(frame.sceneIntent);
        signatures.push_back(objectKindSignature(frame));
    }

    std::sort(intents.begin(), intents.end());
    const auto uniqueIntentEnd = std::unique(intents.begin(), intents.end());
    require(std::distance(intents.begin(), uniqueIntentEnd) >= 6,
            "music profiles should not collapse into a small set of scene intents");

    std::sort(signatures.begin(), signatures.end());
    const auto uniqueSignatureEnd = std::unique(signatures.begin(), signatures.end());
    require(std::distance(signatures.begin(), uniqueSignatureEnd) >= 6,
            "music profiles should create meaningfully different 3D object signatures");
}

void autoSceneProfilesProduceDistinct3DSignatures()
{
    struct Profile {
        const char* name;
        AudioMetrics metrics;
    };

    AudioMetrics silence{};
    silence.style = AudioStyle::Silence;
    silence.styleConfidence = 1.0f;
    silence.section = ArrangementSection::Silence;

    AudioMetrics lowVolume = syntheticMetrics();
    lowVolume.rms = 0.03f;
    lowVolume.peak = 0.06f;
    lowVolume.bass = 0.012f;
    lowVolume.lowMid = 0.08f;
    lowVolume.mid = 0.02f;
    lowVolume.highMid = 0.01f;
    lowVolume.treble = 0.01f;
    lowVolume.stereoWidth = 0.74f;
    lowVolume.beatConfidence = 0.0f;
    lowVolume.dropIntensity = 0.02f;
    lowVolume.style = AudioStyle::Wide;
    lowVolume.styleConfidence = 0.64f;

    AudioMetrics ambient = syntheticMetrics();
    ambient.rms = 0.18f;
    ambient.peak = 0.30f;
    ambient.bass = 0.10f;
    ambient.lowMid = 0.22f;
    ambient.mid = 0.28f;
    ambient.highMid = 0.04f;
    ambient.treble = 0.05f;
    ambient.stereoWidth = 0.86f;
    ambient.spectralFlux = 0.08f;
    ambient.beat = false;
    ambient.beatConfidence = 0.08f;
    ambient.phraseIntensity = 0.48f;
    ambient.harmonicEnergy = 0.56f;
    ambient.style = AudioStyle::Ambient;
    ambient.styleConfidence = 0.88f;

    AudioMetrics techno = syntheticMetrics();
    techno.rms = 0.46f;
    techno.peak = 0.70f;
    techno.bass = 0.52f;
    techno.lowMid = 0.42f;
    techno.mid = 0.10f;
    techno.highMid = 0.06f;
    techno.treble = 0.08f;
    techno.spectralFlux = 0.18f;
    techno.beat = true;
    techno.beatConfidence = 0.92f;
    techno.bpm = 128.0f;
    techno.barConfidence = 0.82f;
    techno.downbeat = true;
    techno.downbeatConfidence = 0.76f;
    techno.style = AudioStyle::Techno;
    techno.styleConfidence = 0.90f;
    techno.section = ArrangementSection::Groove;
    techno.sectionConfidence = 0.84f;

    AudioMetrics bassDrop = syntheticMetrics();
    bassDrop.rms = 0.82f;
    bassDrop.peak = 1.0f;
    bassDrop.bass = 0.96f;
    bassDrop.lowMid = 0.68f;
    bassDrop.highMid = 0.04f;
    bassDrop.treble = 0.04f;
    bassDrop.spectralFlux = 0.36f;
    bassDrop.onset = 0.82f;
    bassDrop.beat = true;
    bassDrop.beatConfidence = 0.94f;
    bassDrop.dropIntensity = 0.92f;
    bassDrop.style = AudioStyle::BassHeavy;
    bassDrop.styleConfidence = 0.92f;
    bassDrop.section = ArrangementSection::Drop;
    bassDrop.sectionConfidence = 0.9f;
    bassDrop.bandOnsets = {0.88f, 0.70f, 0.34f, 0.18f, 0.12f};

    AudioMetrics melodic = syntheticMetrics();
    melodic.rms = 0.22f;
    melodic.peak = 0.34f;
    melodic.bass = 0.12f;
    melodic.lowMid = 0.20f;
    melodic.mid = 0.34f;
    melodic.highMid = 0.28f;
    melodic.treble = 0.22f;
    melodic.spectralFlux = 0.14f;
    melodic.stereoWidth = 0.44f;
    melodic.keyIndex = 7;
    melodic.keyMode = MusicalMode::Major;
    melodic.keyConfidence = 0.72f;
    melodic.harmonicEnergy = 0.72f;
    melodic.phraseIntensity = 0.34f;
    melodic.style = AudioStyle::Ambient;
    melodic.styleConfidence = 0.62f;

    AudioMetrics breakbeat = syntheticMetrics();
    breakbeat.rms = 0.10f;
    breakbeat.peak = 0.40f;
    breakbeat.bass = 0.30f;
    breakbeat.lowMid = 0.12f;
    breakbeat.mid = 0.08f;
    breakbeat.highMid = 0.08f;
    breakbeat.treble = 0.07f;
    breakbeat.spectralFlux = 0.48f;
    breakbeat.onset = 0.34f;
    breakbeat.beat = true;
    breakbeat.beatConfidence = 0.88f;
    breakbeat.dropIntensity = 0.42f;
    breakbeat.style = AudioStyle::Wide;
    breakbeat.styleConfidence = 0.54f;
    breakbeat.section = ArrangementSection::Drop;
    breakbeat.sectionConfidence = 0.74f;
    breakbeat.bandOnsets = {0.28f, 0.18f, 0.36f, 0.46f, 0.52f};

    AudioMetrics darkMinimal = syntheticMetrics();
    darkMinimal.rms = 0.24f;
    darkMinimal.peak = 0.46f;
    darkMinimal.bass = 0.48f;
    darkMinimal.lowMid = 0.32f;
    darkMinimal.mid = 0.06f;
    darkMinimal.highMid = 0.01f;
    darkMinimal.treble = 0.008f;
    darkMinimal.spectralFlux = 0.16f;
    darkMinimal.beat = true;
    darkMinimal.beatConfidence = 0.46f;
    darkMinimal.dropIntensity = 0.18f;
    darkMinimal.keyIndex = 10;
    darkMinimal.keyMode = MusicalMode::Minor;
    darkMinimal.keyConfidence = 0.58f;
    darkMinimal.harmonicEnergy = 0.42f;
    darkMinimal.style = AudioStyle::Techno;
    darkMinimal.styleConfidence = 0.58f;
    darkMinimal.section = ArrangementSection::Groove;
    darkMinimal.sectionConfidence = 0.72f;

    const Profile profiles[] = {
        {"silence", silence},
        {"low-volume", lowVolume},
        {"ambient", ambient},
        {"techno", techno},
        {"bass drop", bassDrop},
        {"melodic", melodic},
        {"breakbeat", breakbeat},
        {"dark minimal", darkMinimal}
    };

    VisualizerEngine engine;
    VisualSettings base;
    base.autoScene = true;
    base.mode = VisualMode::QuantumTunnel;
    base.depth3D = 1.0f;
    base.objectDensity3D = 0.86f;
    base.lightingGlow = 0.88f;
    base.colorImpact = 0.92f;
    base.scenePersonality = 0.86f;
    base.response3D = 1.0f;
    base.motionStability = 0.86f;
    base.patternClarity = 0.9f;
    base.interactiveField = false;
    base.environmentReactive = false;
    base.qualityScale = 0.9f;

    std::vector<VisualMode> modes;
    std::vector<std::array<int, 14>> objectSignatures;
    std::vector<std::array<int, 5>> spatialSignatures;
    std::vector<float> motionScores;

    for (std::size_t i = 0; i < std::size(profiles); ++i) {
        SceneDirector director;
        const VisualSettings directed = director.resolve(base, profiles[i].metrics, 2.0 + static_cast<double>(i) * 1.6);
        const GeometryFrame frame = engine.buildFrame(profiles[i].metrics,
                                                      directed,
                                                      1280.0f,
                                                      720.0f,
                                                      3.0 + static_cast<double>(i) * 0.45);
        require(frame.projected3DPrimitiveCount > frame.retained2DPrimitiveCount,
                std::string(profiles[i].name) + " Auto Scene profile should remain 3D-dominant");
        require(frame.objectDepthRange > 80.0f,
                std::string(profiles[i].name) + " Auto Scene profile should occupy real depth");
        modes.push_back(directed.mode);
        objectSignatures.push_back(objectKindSignature(frame));
        spatialSignatures.push_back(objectSpatialSignature(frame));
        motionScores.push_back(objectMotionSignature(frame));
    }

    std::sort(modes.begin(), modes.end());
    const auto uniqueModeEnd = std::unique(modes.begin(), modes.end());
    require(std::distance(modes.begin(), uniqueModeEnd) >= 6,
            "Auto Scene music profiles should not collapse into a few visual modes");

    std::sort(objectSignatures.begin(), objectSignatures.end());
    const auto uniqueObjectEnd = std::unique(objectSignatures.begin(), objectSignatures.end());
    require(std::distance(objectSignatures.begin(), uniqueObjectEnd) >= 6,
            "Auto Scene music profiles should create distinct 3D object-kind signatures");

    std::sort(spatialSignatures.begin(), spatialSignatures.end());
    const auto uniqueSpatialEnd = std::unique(spatialSignatures.begin(), spatialSignatures.end());
    require(std::distance(spatialSignatures.begin(), uniqueSpatialEnd) >= 6,
            "Auto Scene music profiles should create distinct 3D spatial compositions");

    std::sort(motionScores.begin(), motionScores.end());
    int distinctScores = 1;
    for (std::size_t i = 1; i < motionScores.size(); ++i) {
        if (std::fabs(motionScores[i] - motionScores[i - 1U]) > 5.5f) {
            ++distinctScores;
        }
    }
    std::string motionMessage = "Auto Scene music profiles should produce meaningfully different 3D motion signatures; scores:";
    for (float score : motionScores) {
        motionMessage += " " + std::to_string(score);
    }
    require(distinctScores >= 6, motionMessage);
}

void sameModeSongIdentitiesAuthorDistinct3DSetPieces()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::PhaseWeave;
    settings.palette = Palette::NeonVoltage;
    settings.motionStyle = MotionStyle::Liquid;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.90f;
    settings.lightingGlow = 0.90f;
    settings.colorImpact = 0.92f;
    settings.scenePersonality = 0.88f;
    settings.response3D = 1.0f;
    settings.motionStability = 0.86f;
    settings.patternClarity = 0.90f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics ambient = syntheticMetrics();
    ambient.rms = 0.18f;
    ambient.peak = 0.28f;
    ambient.bass = 0.08f;
    ambient.lowMid = 0.18f;
    ambient.mid = 0.24f;
    ambient.highMid = 0.04f;
    ambient.treble = 0.05f;
    ambient.stereoWidth = 0.90f;
    ambient.spectralFlux = 0.06f;
    ambient.beat = false;
    ambient.beatConfidence = 0.04f;
    ambient.phraseIntensity = 0.56f;
    ambient.phraseConfidence = 0.80f;
    ambient.harmonicEnergy = 0.58f;
    ambient.style = AudioStyle::Ambient;
    ambient.styleConfidence = 0.92f;

    AudioMetrics techno = syntheticMetrics();
    techno.rms = 0.48f;
    techno.peak = 0.72f;
    techno.bass = 0.56f;
    techno.lowMid = 0.44f;
    techno.mid = 0.14f;
    techno.highMid = 0.06f;
    techno.treble = 0.08f;
    techno.stereoWidth = 0.34f;
    techno.spectralFlux = 0.18f;
    techno.beat = true;
    techno.beatConfidence = 0.94f;
    techno.barConfidence = 0.84f;
    techno.downbeatConfidence = 0.72f;
    techno.bpm = 128.0f;
    techno.style = AudioStyle::Techno;
    techno.styleConfidence = 0.92f;
    techno.section = ArrangementSection::Groove;
    techno.sectionConfidence = 0.86f;

    AudioMetrics bass = syntheticMetrics();
    bass.rms = 0.84f;
    bass.peak = 1.0f;
    bass.bass = 0.98f;
    bass.lowMid = 0.78f;
    bass.mid = 0.18f;
    bass.highMid = 0.04f;
    bass.treble = 0.04f;
    bass.stereoWidth = 0.42f;
    bass.spectralFlux = 0.34f;
    bass.onset = 0.82f;
    bass.beat = true;
    bass.beatConfidence = 0.94f;
    bass.dropIntensity = 0.94f;
    bass.style = AudioStyle::BassHeavy;
    bass.styleConfidence = 0.94f;
    bass.section = ArrangementSection::Drop;
    bass.sectionConfidence = 0.92f;
    bass.bandOnsets = {0.88f, 0.70f, 0.34f, 0.18f, 0.12f};

    AudioMetrics melodic = syntheticMetrics();
    melodic.rms = 0.34f;
    melodic.peak = 0.52f;
    melodic.bass = 0.16f;
    melodic.lowMid = 0.22f;
    melodic.mid = 0.54f;
    melodic.highMid = 0.58f;
    melodic.treble = 0.50f;
    melodic.stereoWidth = 0.58f;
    melodic.spectralFlux = 0.20f;
    melodic.keyIndex = 7;
    melodic.keyMode = MusicalMode::Major;
    melodic.keyConfidence = 0.94f;
    melodic.harmonicEnergy = 0.90f;
    melodic.phraseIntensity = 0.48f;
    melodic.phraseConfidence = 0.82f;
    melodic.style = AudioStyle::Bright;
    melodic.styleConfidence = 0.72f;

    AudioMetrics breakbeat = syntheticMetrics();
    breakbeat.rms = 0.56f;
    breakbeat.peak = 0.76f;
    breakbeat.bass = 0.32f;
    breakbeat.lowMid = 0.22f;
    breakbeat.mid = 0.34f;
    breakbeat.highMid = 0.82f;
    breakbeat.treble = 0.86f;
    breakbeat.stereoWidth = 0.62f;
    breakbeat.spectralFlux = 0.88f;
    breakbeat.onset = 0.84f;
    breakbeat.beat = true;
    breakbeat.beatConfidence = 0.54f;
    breakbeat.style = AudioStyle::Bright;
    breakbeat.styleConfidence = 0.86f;
    breakbeat.bandOnsets = {0.22f, 0.38f, 0.70f, 0.92f, 0.86f};

    AudioMetrics dark = syntheticMetrics();
    dark.rms = 0.30f;
    dark.peak = 0.48f;
    dark.bass = 0.58f;
    dark.lowMid = 0.50f;
    dark.mid = 0.14f;
    dark.highMid = 0.02f;
    dark.treble = 0.01f;
    dark.stereoWidth = 0.16f;
    dark.spectralFlux = 0.05f;
    dark.beat = true;
    dark.beatConfidence = 0.56f;
    dark.keyIndex = 10;
    dark.keyMode = MusicalMode::Minor;
    dark.keyConfidence = 0.72f;
    dark.harmonicEnergy = 0.44f;
    dark.style = AudioStyle::Techno;
    dark.styleConfidence = 0.64f;
    dark.section = ArrangementSection::Breakdown;
    dark.sectionConfidence = 0.76f;

    const GeometryFrame ambientFrame = engine.buildFrame(ambient, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame technoFrame = engine.buildFrame(techno, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame bassFrame = engine.buildFrame(bass, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame melodicFrame = engine.buildFrame(melodic, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame breakFrame = engine.buildFrame(breakbeat, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame darkFrame = engine.buildFrame(dark, settings, 1280.0f, 720.0f, 2.0);

    const GeometryFrame* frames[] = {
        &ambientFrame,
        &technoFrame,
        &bassFrame,
        &melodicFrame,
        &breakFrame,
        &darkFrame
    };
    for (const GeometryFrame* frame : frames) {
        require(frame->projected3DPrimitiveCount > frame->retained2DPrimitiveCount,
                "same-mode song identity frame should remain 3D-dominant");
        require(frame->objectDepthRange > 90.0f,
                "same-mode song identity frame should use real depth");
        require(frame->sceneRoleSeparation3D > 0.58f,
                "same-mode song identity frame should keep musical roles separated instead of meshing all parts together");
    }

    require(bassFrame.sceneBassRole3D > 0.70f &&
                bassFrame.sceneBassRole3D > bassFrame.sceneMelodyRole3D + 0.24f &&
                bassFrame.sceneConvergence3D > ambientFrame.sceneConvergence3D + 0.22f,
            "bass/drop music should read as deep mass and convergence, not generic motion");
    require(technoFrame.sceneDrumRole3D > 0.58f &&
                technoFrame.sceneDrumRole3D > ambientFrame.sceneDrumRole3D + 0.24f,
            "techno should expose a strong drum/sequencer role distinct from ambient space");
    require(ambientFrame.sceneSpaceRole3D > 0.54f &&
                ambientFrame.sceneSpaceRole3D > ambientFrame.sceneBassRole3D + 0.24f,
            "ambient should emphasize spatial depth fields instead of bass mass");
    require(melodicFrame.sceneMelodyRole3D > 0.58f &&
                melodicFrame.sceneHarmonyRole3D > 0.48f &&
                melodicFrame.sceneMelodyRole3D > melodicFrame.sceneBassRole3D + 0.22f,
            "melodic music should drive elevated melody/harmony roles");
    require(breakFrame.sceneFractureRole3D > 0.56f &&
                breakFrame.sceneFractureRole3D > ambientFrame.sceneFractureRole3D + 0.26f,
            "breakbeat should create a fractured cut-plane role instead of smooth ambient orbit; break fracture=" +
                std::to_string(breakFrame.sceneFractureRole3D) +
                " ambient fracture=" + std::to_string(ambientFrame.sceneFractureRole3D));
    require(darkFrame.sceneShadowRole3D > 0.40f &&
                darkFrame.sceneShadowRole3D > ambientFrame.sceneShadowRole3D + 0.16f,
            "dark minimal should expose sparse shadow/monolith structure");

    require(objectFamilyCount(bassFrame, {Object3DKind::TunnelRib, Object3DKind::Column}) >
                objectFamilyCount(melodicFrame, {Object3DKind::TunnelRib, Object3DKind::Column}) + 4,
            "bass identity should author extra pressure ribs and massive columns");
    require(objectFamilyCount(technoFrame, {Object3DKind::Column, Object3DKind::Link, Object3DKind::Cage}) >
                objectFamilyCount(ambientFrame, {Object3DKind::Column, Object3DKind::Link, Object3DKind::Cage}) + 4,
            "techno identity should author sequencer architecture instead of ambient space");
    require(objectFamilyCount(ambientFrame, {Object3DKind::Orbiter, Object3DKind::WaveSurface, Object3DKind::DepthPlane}) >
                objectFamilyCount(technoFrame, {Object3DKind::Orbiter, Object3DKind::WaveSurface, Object3DKind::DepthPlane}) + 3,
            "ambient identity should author orbital bodies and soft depth fields");
    require(objectFamilyCount(melodicFrame, {Object3DKind::Shard, Object3DKind::Cage, Object3DKind::Link}) >
                objectFamilyCount(ambientFrame, {Object3DKind::Shard, Object3DKind::Cage, Object3DKind::Link}) + 5,
            "melodic identity should author crystalline harmonic constellations");
    const int breakFractureObjects = objectFamilyCount(breakFrame, {Object3DKind::Shard, Object3DKind::Plate});
    const int ambientFractureObjects = objectFamilyCount(ambientFrame, {Object3DKind::Shard, Object3DKind::Plate});
    const std::array<int, 14> breakSignature = objectKindSignature(breakFrame);
    std::string breakSignatureMessage = "breakbeat identity should author fractured staggered geometry; break=" +
                                        std::to_string(breakFractureObjects) +
                                        " ambient=" + std::to_string(ambientFractureObjects) +
                                        " signature:";
    for (int bucket : breakSignature) {
        breakSignatureMessage += " " + std::to_string(bucket);
    }
    require(breakFractureObjects > ambientFractureObjects + 8,
            breakSignatureMessage);
    require(objectKindCount(darkFrame, Object3DKind::Column) >= 3 &&
                objectKindCount(darkFrame, Object3DKind::Anchor) > objectKindCount(ambientFrame, Object3DKind::Anchor),
            "dark minimal identity should author sparse monoliths and a depth anchor");

    std::vector<std::array<int, 14>> signatures;
    for (const GeometryFrame* frame : frames) {
        signatures.push_back(objectKindSignature(*frame));
    }
    std::sort(signatures.begin(), signatures.end());
    const auto uniqueEnd = std::unique(signatures.begin(), signatures.end());
    require(std::distance(signatures.begin(), uniqueEnd) >= 6,
            "same visual mode should still produce distinct 3D object grammars for different song identities");
}

void songIdentitiesDriveDistinctCameraLanguage()
{
    struct Profile {
        const char* name;
        VisualMode mode;
        AudioMetrics metrics;
    };

    VisualizerEngine engine;
    VisualSettings settings;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.88f;
    settings.lightingGlow = 0.88f;
    settings.scenePersonality = 0.88f;
    settings.response3D = 1.0f;
    settings.motionStability = 0.86f;
    settings.patternClarity = 0.90f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics ambient = syntheticMetrics();
    ambient.rms = 0.18f;
    ambient.peak = 0.28f;
    ambient.bass = 0.08f;
    ambient.lowMid = 0.18f;
    ambient.mid = 0.24f;
    ambient.highMid = 0.04f;
    ambient.treble = 0.05f;
    ambient.stereoWidth = 0.90f;
    ambient.spectralFlux = 0.06f;
    ambient.beat = false;
    ambient.beatConfidence = 0.04f;
    ambient.phraseIntensity = 0.56f;
    ambient.phraseConfidence = 0.80f;
    ambient.harmonicEnergy = 0.58f;
    ambient.style = AudioStyle::Ambient;
    ambient.styleConfidence = 0.92f;
    ambient.section = ArrangementSection::Breakdown;
    ambient.sectionConfidence = 0.78f;

    AudioMetrics techno = syntheticMetrics();
    techno.rms = 0.48f;
    techno.peak = 0.72f;
    techno.bass = 0.56f;
    techno.lowMid = 0.44f;
    techno.mid = 0.14f;
    techno.highMid = 0.06f;
    techno.treble = 0.08f;
    techno.stereoWidth = 0.34f;
    techno.spectralFlux = 0.18f;
    techno.beat = true;
    techno.beatConfidence = 0.94f;
    techno.barConfidence = 0.84f;
    techno.downbeatConfidence = 0.72f;
    techno.style = AudioStyle::Techno;
    techno.styleConfidence = 0.92f;
    techno.section = ArrangementSection::Groove;
    techno.sectionConfidence = 0.86f;

    AudioMetrics bass = syntheticMetrics();
    bass.rms = 0.84f;
    bass.peak = 1.0f;
    bass.bass = 0.98f;
    bass.lowMid = 0.78f;
    bass.highMid = 0.04f;
    bass.treble = 0.04f;
    bass.stereoWidth = 0.42f;
    bass.spectralFlux = 0.34f;
    bass.onset = 0.82f;
    bass.beat = true;
    bass.beatConfidence = 0.94f;
    bass.dropIntensity = 0.94f;
    bass.style = AudioStyle::BassHeavy;
    bass.styleConfidence = 0.94f;
    bass.section = ArrangementSection::Drop;
    bass.sectionConfidence = 0.92f;

    AudioMetrics melodic = syntheticMetrics();
    melodic.rms = 0.34f;
    melodic.peak = 0.52f;
    melodic.bass = 0.16f;
    melodic.mid = 0.54f;
    melodic.highMid = 0.58f;
    melodic.treble = 0.50f;
    melodic.stereoWidth = 0.58f;
    melodic.spectralFlux = 0.20f;
    melodic.keyIndex = 7;
    melodic.keyMode = MusicalMode::Major;
    melodic.keyConfidence = 0.94f;
    melodic.harmonicEnergy = 0.90f;
    melodic.phraseIntensity = 0.48f;
    melodic.phraseConfidence = 0.82f;
    melodic.style = AudioStyle::Bright;
    melodic.styleConfidence = 0.72f;

    AudioMetrics breakbeat = syntheticMetrics();
    breakbeat.rms = 0.56f;
    breakbeat.peak = 0.76f;
    breakbeat.bass = 0.32f;
    breakbeat.lowMid = 0.22f;
    breakbeat.mid = 0.34f;
    breakbeat.highMid = 0.82f;
    breakbeat.treble = 0.86f;
    breakbeat.stereoWidth = 0.62f;
    breakbeat.spectralFlux = 0.88f;
    breakbeat.onset = 0.84f;
    breakbeat.beat = true;
    breakbeat.beatConfidence = 0.54f;
    breakbeat.beatPhase = 0.37f;
    breakbeat.style = AudioStyle::Bright;
    breakbeat.styleConfidence = 0.86f;

    AudioMetrics dark = syntheticMetrics();
    dark.rms = 0.30f;
    dark.peak = 0.48f;
    dark.bass = 0.58f;
    dark.lowMid = 0.50f;
    dark.mid = 0.14f;
    dark.highMid = 0.01f;
    dark.treble = 0.01f;
    dark.stereoWidth = 0.16f;
    dark.spectralFlux = 0.05f;
    dark.beatConfidence = 0.56f;
    dark.dropIntensity = 0.10f;
    dark.keyIndex = 10;
    dark.keyMode = MusicalMode::Minor;
    dark.keyConfidence = 0.72f;
    dark.harmonicEnergy = 0.44f;
    dark.style = AudioStyle::BassHeavy;
    dark.styleConfidence = 0.64f;
    dark.section = ArrangementSection::Breakdown;
    dark.sectionConfidence = 0.76f;

    const Profile profiles[] = {
        {"ambient", VisualMode::PhaseWeave, ambient},
        {"techno", VisualMode::TechnoMandala, techno},
        {"bass", VisualMode::QuantumTunnel, bass},
        {"melodic", VisualMode::ChromaKaleidoscope, melodic},
        {"breakbeat", VisualMode::SpectralOrigami, breakbeat},
        {"dark", VisualMode::ResonanceTessellation, dark}
    };

    std::vector<int> signatureBuckets;
    std::vector<GeometryFrame> frames;
    for (const Profile& profile : profiles) {
        settings.mode = profile.mode;
        GeometryFrame frame = engine.buildFrame(profile.metrics, settings, 1280.0f, 720.0f, 3.0);
        require(frame.cameraDepth > 500.0f, std::string(profile.name) + " should use a real camera distance");
        require(std::fabs(frame.cameraYaw) > 0.001f ||
                    std::fabs(frame.cameraPitch) > 0.001f ||
                    std::fabs(frame.cameraRoll) > 0.001f ||
                    std::fabs(frame.cameraCenterOffset.x) > 0.001f ||
                    std::fabs(frame.cameraCenterOffset.y) > 0.001f,
                std::string(profile.name) + " should report non-static camera language");
        signatureBuckets.push_back(static_cast<int>(std::round(cameraPoseSignature(frame) * 2.0f)));
        frames.push_back(frame);
    }

    std::sort(signatureBuckets.begin(), signatureBuckets.end());
    const auto uniqueEnd = std::unique(signatureBuckets.begin(), signatureBuckets.end());
    require(std::distance(signatureBuckets.begin(), uniqueEnd) >= 5,
            "song identities should produce distinct camera pose signatures");

    const GeometryFrame& ambientFrame = frames[0];
    const GeometryFrame& technoFrame = frames[1];
    const GeometryFrame& bassFrame = frames[2];
    const GeometryFrame& melodicFrame = frames[3];
    const GeometryFrame& breakFrame = frames[4];
    const GeometryFrame& darkFrame = frames[5];
    require(bassFrame.cameraDepth < ambientFrame.cameraDepth,
            "bass pressure should dolly closer than ambient space");
    require(std::fabs(technoFrame.cameraRoll) < std::fabs(breakFrame.cameraRoll),
            "techno architecture should keep a more locked roll than breakbeat cuts");
    require(melodicFrame.cameraPitch > technoFrame.cameraPitch,
            "melodic crystal scenes should get elevated harmonic framing");
    require(std::fabs(darkFrame.cameraRoll) < std::fabs(breakFrame.cameraRoll),
            "dark minimal scenes should keep roll restrained compared with breakbeat cuts");
}

void autoSceneContinuityResistsAmbiguousFrameFlips()
{
    VisualSettings base;
    base.autoScene = true;
    base.mode = VisualMode::QuantumTunnel;
    base.depth3D = 1.0f;
    base.objectDensity3D = 0.86f;
    base.lightingGlow = 0.88f;
    base.scenePersonality = 0.86f;
    base.response3D = 1.0f;
    base.motionStability = 0.86f;
    base.patternClarity = 0.90f;

    SceneDirector technoDirector;
    AudioMetrics techno = syntheticMetrics();
    techno.style = AudioStyle::Techno;
    techno.styleConfidence = 0.90f;
    techno.rms = 0.34f;
    techno.bass = 0.58f;
    techno.highMid = 0.08f;
    techno.treble = 0.07f;
    techno.spectralFlux = 0.16f;
    techno.beat = true;
    techno.beatConfidence = 0.84f;
    techno.barConfidence = 0.72f;
    techno.downbeatConfidence = 0.64f;
    techno.section = ArrangementSection::Groove;
    techno.sectionConfidence = 0.84f;
    (void)technoDirector.resolve(base, techno, 0.0);
    (void)technoDirector.resolve(base, techno, 0.6);
    (void)technoDirector.resolve(base, techno, 1.2);

    AudioMetrics ambiguousQuiet = techno;
    ambiguousQuiet.style = AudioStyle::Ambient;
    ambiguousQuiet.styleConfidence = 0.56f;
    ambiguousQuiet.rms = 0.13f;
    ambiguousQuiet.bass = 0.56f;
    ambiguousQuiet.beat = false;
    ambiguousQuiet.beatConfidence = 0.0f;
    ambiguousQuiet.downbeatConfidence = 0.0f;
    ambiguousQuiet.spectralFlux = 0.02f;
    ambiguousQuiet.harmonicEnergy = 0.56f;
    ambiguousQuiet.keyConfidence = 0.12f;
    const VisualSettings heldTechno = technoDirector.resolve(base, ambiguousQuiet, 1.8);
    require(heldTechno.mode == VisualMode::TechnoMandala ||
                heldTechno.mode == VisualMode::PolyrhythmLattice,
            "song continuity should keep established techno in mechanical 3D architecture through ambiguous quiet frames");
    require(heldTechno.motionStyle == MotionStyle::Mechanical,
            "song continuity should preserve locked techno motion language");

    SceneDirector ambientDirector;
    AudioMetrics ambient = syntheticMetrics();
    ambient.style = AudioStyle::Wide;
    ambient.styleConfidence = 0.82f;
    ambient.rms = 0.18f;
    ambient.bass = 0.16f;
    ambient.lowMid = 0.22f;
    ambient.highMid = 0.10f;
    ambient.treble = 0.12f;
    ambient.stereoWidth = 0.78f;
    ambient.spectralFlux = 0.06f;
    ambient.beat = false;
    ambient.beatConfidence = 0.04f;
    ambient.barConfidence = 0.12f;
    ambient.downbeatConfidence = 0.0f;
    ambient.harmonicEnergy = 0.60f;
    ambient.keyConfidence = 0.18f;
    (void)ambientDirector.resolve(base, ambient, 0.0);
    (void)ambientDirector.resolve(base, ambient, 0.7);
    (void)ambientDirector.resolve(base, ambient, 1.4);

    AudioMetrics falseTechno = ambient;
    falseTechno.style = AudioStyle::Techno;
    falseTechno.styleConfidence = 0.52f;
    falseTechno.bass = 0.44f;
    falseTechno.beatConfidence = 0.24f;
    falseTechno.barConfidence = 0.46f;
    falseTechno.spectralFlux = 0.10f;
    const VisualSettings heldAmbient = ambientDirector.resolve(base, falseTechno, 2.1);
    require(heldAmbient.mode == VisualMode::PhaseWeave ||
                heldAmbient.mode == VisualMode::LissajousMesh,
            "song continuity should keep established ambient material in spacious 3D fields through false techno frames");
    require(heldAmbient.motionStyle == MotionStyle::AmbientDrift,
            "song continuity should preserve ambient drift motion language");

    AudioMetrics breakbeat = ambient;
    breakbeat.rms = 0.32f;
    breakbeat.bass = 0.28f;
    breakbeat.highMid = 0.56f;
    breakbeat.treble = 0.58f;
    breakbeat.spectralFlux = 0.62f;
    breakbeat.onset = 0.70f;
    breakbeat.beat = true;
    breakbeat.beatConfidence = 0.62f;
    breakbeat.bandOnsets = {0.18f, 0.22f, 0.34f, 0.68f, 0.72f};
    breakbeat.style = AudioStyle::Bright;
    breakbeat.styleConfidence = 0.78f;
    const VisualSettings breakOverride = ambientDirector.resolve(base, breakbeat, 2.7);
    require(breakOverride.mode == VisualMode::SpectralOrigami,
            "hard breakbeat transients should still override continuity immediately");
    require(breakOverride.motionStyle == MotionStyle::Breakbeat,
            "hard breakbeat transients should select breakbeat choreography");
}

void autoSceneSelectsMotionStyleFromMusic()
{
    SceneDirector director;
    VisualSettings base;
    base.autoScene = true;
    base.motionStyle = MotionStyle::Liquid;
    base.mode = VisualMode::PhaseWeave;

    AudioMetrics ambient = syntheticMetrics();
    ambient.style = AudioStyle::Ambient;
    ambient.styleConfidence = 0.92f;
    ambient.rms = 0.18f;
    ambient.stereoWidth = 0.74f;
    ambient.phraseIntensity = 0.62f;
    ambient.dropIntensity = 0.0f;
    const VisualSettings ambientSettings = director.resolve(base, ambient, 0.0);
    require(ambientSettings.motionStyle == MotionStyle::AmbientDrift,
            "Auto Scene should select ambient drift for ambient profiles");

    AudioMetrics drop = syntheticMetrics();
    drop.style = AudioStyle::BassHeavy;
    drop.styleConfidence = 0.94f;
    drop.rms = 0.82f;
    drop.bass = 0.98f;
    drop.dropIntensity = 0.96f;
    drop.section = ArrangementSection::Drop;
    drop.sectionConfidence = 0.9f;
    const VisualSettings dropSettings = director.resolve(base, drop, 2.0);
    require(dropSettings.motionStyle == MotionStyle::HeavyBass,
            "Auto Scene should select heavy-bass choreography for bass drops");

    AudioMetrics bright = syntheticMetrics();
    bright.style = AudioStyle::Bright;
    bright.styleConfidence = 0.9f;
    bright.treble = 0.92f;
    bright.spectralFlux = 0.84f;
    bright.onset = 0.76f;
    bright.bandOnsets = {0.18f, 0.28f, 0.46f, 0.72f, 0.92f};
    bright.section = ArrangementSection::Groove;
    const VisualSettings brightSettings = director.resolve(base, bright, 4.0);
    require(brightSettings.motionStyle == MotionStyle::Breakbeat,
            "Auto Scene should select breakbeat choreography for bright transient profiles");
}

void autoSceneDrives3DCompositionThroughSections()
{
    SceneDirector director;
    VisualSettings base;
    base.autoScene = true;
    base.mode = VisualMode::LissajousMesh;
    base.palette = Palette::MonochromeLaser;
    base.motionStyle = MotionStyle::Smooth;
    base.depth3D = 0.34f;
    base.colorImpact = 0.36f;
    base.objectDensity3D = 0.34f;
    base.lightingGlow = 0.30f;
    base.scenePersonality = 0.28f;
    base.response3D = 0.36f;
    base.motionStability = 0.52f;
    base.patternClarity = 0.54f;
    base.intensity = 0.82f;
    base.speed = 0.82f;

    AudioMetrics breakdown = syntheticMetrics();
    breakdown.style = AudioStyle::Ambient;
    breakdown.styleConfidence = 0.9f;
    breakdown.rms = 0.18f;
    breakdown.stereoWidth = 0.64f;
    breakdown.harmonicEnergy = 0.62f;
    breakdown.phraseIntensity = 0.48f;
    breakdown.section = ArrangementSection::Breakdown;
    breakdown.sectionConfidence = 0.84f;
    breakdown.sectionProgress = 0.34f;
    (void)director.resolve(base, breakdown, 0.0);
    const VisualSettings breakdownSettings = director.resolve(base, breakdown, 0.7);

    require(breakdownSettings.motionStyle == MotionStyle::AmbientDrift,
            "breakdowns should become stable ambient 3D spaces");
    require(breakdownSettings.objectDensity3D > base.objectDensity3D + 0.06f,
            "breakdowns should still raise a sparse 3D scene above the base");
    require(breakdownSettings.motionStability > base.motionStability,
            "breakdowns should increase choreographic stability");
    require(breakdownSettings.patternClarity > base.patternClarity,
            "breakdowns should increase pattern readability");

    AudioMetrics build = syntheticMetrics();
    build.style = AudioStyle::Techno;
    build.styleConfidence = 0.9f;
    build.rms = 0.48f;
    build.bass = 0.52f;
    build.treble = 0.48f;
    build.stereoWidth = 0.46f;
    build.spectralFlux = 0.44f;
    build.beat = true;
    build.beatConfidence = 0.86f;
    build.buildTension = 0.84f;
    build.phraseIntensity = 0.62f;
    build.phraseConfidence = 0.78f;
    build.section = ArrangementSection::Build;
    build.sectionConfidence = 0.86f;
    build.sectionProgress = 0.68f;
    build.bandOnsets = {0.44f, 0.38f, 0.48f, 0.52f, 0.40f};
    const VisualSettings buildSettings = director.resolve(base, build, 1.55);

    require(buildSettings.objectDensity3D > breakdownSettings.objectDensity3D + 0.06f,
            "builds should visibly thicken the 3D object field");
    require(buildSettings.lightingGlow > breakdownSettings.lightingGlow + 0.06f,
            "builds should brighten 3D lighting");
    require(buildSettings.scenePersonality > breakdownSettings.scenePersonality + 0.06f,
            "builds should push scene personality beyond the breakdown");
    require(buildSettings.response3D > breakdownSettings.response3D + 0.06f,
            "builds should make 3D response more active");

    AudioMetrics drop = syntheticMetrics();
    drop.style = AudioStyle::BassHeavy;
    drop.styleConfidence = 0.94f;
    drop.rms = 0.84f;
    drop.peak = 1.0f;
    drop.bass = 0.98f;
    drop.lowMid = 0.74f;
    drop.treble = 0.42f;
    drop.stereoWidth = 0.50f;
    drop.spectralFlux = 0.50f;
    drop.onset = 0.86f;
    drop.beat = true;
    drop.beatConfidence = 0.96f;
    drop.downbeat = true;
    drop.downbeatConfidence = 0.9f;
    drop.dropIntensity = 0.96f;
    drop.phraseIntensity = 0.74f;
    drop.section = ArrangementSection::Drop;
    drop.sectionConfidence = 0.94f;
    drop.bandOnsets = {0.88f, 0.76f, 0.48f, 0.36f, 0.28f};
    const VisualSettings dropSettings = director.resolve(base, drop, 2.35);

    require(dropSettings.motionStyle == MotionStyle::HeavyBass,
            "drops should switch into heavy-bass 3D choreography");
    require(dropSettings.objectDensity3D > buildSettings.objectDensity3D,
            "drops should increase 3D mass over builds");
    require(dropSettings.lightingGlow > buildSettings.lightingGlow,
            "drops should increase 3D lighting impact");
    require(dropSettings.response3D > buildSettings.response3D,
            "drops should make the 3D scene respond harder");
    require(dropSettings.depth3D > breakdownSettings.depth3D + 0.20f,
            "drops should push the camera and objects deeper than breakdowns");
}

void motionStabilityAndPatternClarityReduceJitter()
{
    VisualizerEngine engine;
    VisualSettings wild;
    wild.mode = VisualMode::HyperspacePolytope;
    wild.depth3D = 1.0f;
    wild.objectDensity3D = 0.86f;
    wild.lightingGlow = 0.9f;
    wild.scenePersonality = 0.82f;
    wild.response3D = 1.0f;
    wild.motionStability = 0.0f;
    wild.patternClarity = 0.0f;
    wild.interactiveField = false;
    wild.environmentReactive = false;
    wild.qualityScale = 0.9f;

    VisualSettings clear = wild;
    clear.motionStability = 1.0f;
    clear.patternClarity = 1.0f;

    AudioMetrics calm = syntheticMetrics();
    calm.rms = 0.09f;
    calm.peak = 0.16f;
    calm.bass = 0.07f;
    calm.lowMid = 0.08f;
    calm.mid = 0.06f;
    calm.highMid = 0.05f;
    calm.treble = 0.05f;
    calm.stereoWidth = 0.10f;
    calm.spectralFlux = 0.02f;
    calm.beat = false;
    calm.beatConfidence = 0.06f;
    calm.dropIntensity = 0.0f;
    calm.phraseIntensity = 0.05f;
    calm.buildTension = 0.0f;
    calm.style = AudioStyle::Ambient;
    calm.styleConfidence = 0.6f;

    AudioMetrics intense = syntheticMetrics();
    intense.rms = 0.84f;
    intense.peak = 1.0f;
    intense.bass = 0.95f;
    intense.lowMid = 0.72f;
    intense.mid = 0.52f;
    intense.highMid = 0.78f;
    intense.treble = 0.88f;
    intense.stereoWidth = 0.84f;
    intense.spectralFlux = 0.78f;
    intense.onset = 0.86f;
    intense.beat = true;
    intense.beatConfidence = 0.98f;
    intense.beatPhase = 0.05f;
    intense.downbeat = true;
    intense.downbeatConfidence = 0.94f;
    intense.dropIntensity = 0.94f;
    intense.phraseIntensity = 0.86f;
    intense.phraseBoundary = true;
    intense.buildTension = 0.9f;
    intense.style = AudioStyle::BassHeavy;
    intense.styleConfidence = 0.9f;
    intense.section = ArrangementSection::Drop;
    intense.bandOnsets = {0.92f, 0.76f, 0.50f, 0.64f, 0.76f};

    const GeometryFrame wildCalm = engine.buildFrame(calm, wild, 1280.0f, 720.0f, 3.0);
    const GeometryFrame wildIntense = engine.buildFrame(intense, wild, 1280.0f, 720.0f, 3.0);
    const GeometryFrame clearCalm = engine.buildFrame(calm, clear, 1280.0f, 720.0f, 3.0);
    const GeometryFrame clearIntense = engine.buildFrame(intense, clear, 1280.0f, 720.0f, 3.0);

    const float wildness = wildIntense.objectDepthRange +
                           std::fabs(averageObjectZ(wildIntense)) * 0.20f +
                           averageObjectScale(wildIntense) * 0.08f +
                           averageObjectGlow(wildIntense) * 60.0f;
    const float clearWildness = clearIntense.objectDepthRange +
                                std::fabs(averageObjectZ(clearIntense)) * 0.20f +
                                averageObjectScale(clearIntense) * 0.08f +
                                averageObjectGlow(clearIntense) * 60.0f;

    require(clearWildness < wildness,
            "stability and clarity should cap excessive depth, scale, and glow");
    require(averageObjectGlow(clearIntense) <= averageObjectGlow(wildIntense) + 0.03f,
            "clear mode should cap glow compared with wild response");
    require(visualEnergyScore(clearIntense) > visualEnergyScore(clearCalm) + 1.5f,
            "stable and clear settings should still react to intense music");
    require(!clearIntense.objects3D.empty(), "stable and clear settings should keep 3D objects visible");
}

void silenceKeepsStableReadableScaffold()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::QuantumTunnel;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.78f;
    settings.response3D = 0.88f;
    settings.motionStability = 0.86f;
    settings.patternClarity = 0.9f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.9f;

    AudioMetrics silence{};
    silence.style = AudioStyle::Silence;
    silence.section = ArrangementSection::Silence;
    silence.beatPhase = 0.5f;
    silence.barPhase = 0.25f;
    silence.phrasePhase = 0.25f;

    const GeometryFrame first = engine.buildFrame(silence, settings, 1280.0f, 720.0f, 1.0);
    const GeometryFrame second = engine.buildFrame(silence, settings, 1280.0f, 720.0f, 1.5);

    require(first.flash == 0.0f && second.flash == 0.0f, "silence should never create flash");
    require(countPrimitives(first) == countPrimitives(second), "silence should keep a stable geometry count");
    require(std::fabs(first.cameraDepth - second.cameraDepth) < 1.0f,
            "silence should keep camera depth stable");
    require(std::fabs(first.objectDepthRange - second.objectDepthRange) < 90.0f,
            "silence should avoid random depth-range jumps");
    require(averageObjectGlow(first) < 0.75f && averageObjectGlow(second) < 0.75f,
            "silence should keep glow restrained");
}

void object3DDepthSortsAndProjects()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::NeuralConstellation;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.85f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.9f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.barConfidence = 0.82f;
    metrics.downbeatConfidence = 0.9f;
    metrics.harmonicEnergy = 0.72f;
    metrics.stereoWidth = 0.68f;

    const GeometryFrame frame = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 2.5);
    require(frame.objects3D.size() > 20U, "neural 3D scene should generate many objects");
    require(objectDepthsAreSorted(frame), "3D objects should be sorted back-to-front by projected depth");
    require(frame.cameraDepth > 500.0f, "3D projection should use a real camera distance");
    require(frame.objectDepthRange > 100.0f, "3D objects should span a visible depth volume");
    require(!frame.polylines.empty(), "3D object projection should produce visible linework");
    require(!frame.particles.empty(), "3D object projection should produce visible nodes/particles");
}

void threeDScenesRenderMaterialFacesAndDepthHaze()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::TechnoMandala;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.88f;
    settings.lightingGlow = 0.92f;
    settings.colorImpact = 0.95f;
    settings.scenePersonality = 0.86f;
    settings.response3D = 0.95f;
    settings.motionStability = 0.86f;
    settings.patternClarity = 0.90f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.rms = 0.55f;
    metrics.peak = 0.84f;
    metrics.bass = 0.62f;
    metrics.lowMid = 0.46f;
    metrics.mid = 0.24f;
    metrics.stereoWidth = 0.44f;
    metrics.beat = true;
    metrics.beatConfidence = 0.94f;
    metrics.barConfidence = 0.88f;
    metrics.downbeatConfidence = 0.76f;
    metrics.dropIntensity = 0.42f;
    metrics.style = AudioStyle::Techno;
    metrics.styleConfidence = 0.92f;
    metrics.section = ArrangementSection::Groove;
    metrics.sectionConfidence = 0.86f;

    const GeometryFrame frame = engine.buildFrame(metrics, settings, 1280.0f, 720.0f, 2.25);

    require(frame.projected3DFaceCount >= 18,
            "3D scenes should render filled material faces, not only wire outlines");
    require(filledPolylineCount(frame) >= frame.projected3DFaceCount,
            "projected material faces should be represented by filled polylines");
    require(frame.projected3DFillVisualWeight > 35.0f,
            "filled 3D faces should contribute visible material weight");
    require(frame.projected3DMaterialContrast > 0.05f,
            "3D material shading should create contrast against the background");
    require(frame.depthFogStrength > 0.08f,
            "deep 3D scenes should report depth haze/fog strength");
    require(frame.projected3DVisualWeight > frame.retained2DVisualWeight * 1.8f,
            "filled material pass should keep the frame strongly 3D dominant");
}

void sectionNarrativeAuthorsDistinct3DStructures()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::TechnoMandala;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.86f;
    settings.lightingGlow = 0.90f;
    settings.colorImpact = 0.94f;
    settings.scenePersonality = 0.88f;
    settings.response3D = 0.96f;
    settings.motionStability = 0.88f;
    settings.patternClarity = 0.90f;
    settings.interactiveField = false;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics neutral = syntheticMetrics();
    neutral.rms = 0.44f;
    neutral.peak = 0.64f;
    neutral.bass = 0.42f;
    neutral.lowMid = 0.32f;
    neutral.mid = 0.28f;
    neutral.stereoWidth = 0.46f;
    neutral.beat = true;
    neutral.beatConfidence = 0.74f;
    neutral.barConfidence = 0.62f;
    neutral.downbeatConfidence = 0.46f;
    neutral.style = AudioStyle::Techno;
    neutral.styleConfidence = 0.80f;
    neutral.sectionConfidence = 0.0f;

    AudioMetrics build = neutral;
    build.section = ArrangementSection::Build;
    build.sectionConfidence = 0.86f;
    build.sectionProgress = 0.72f;
    build.buildTension = 0.78f;
    build.phraseIntensity = 0.62f;
    build.spectralFlux = 0.28f;

    AudioMetrics drop = neutral;
    drop.section = ArrangementSection::Drop;
    drop.sectionConfidence = 0.92f;
    drop.sectionProgress = 0.18f;
    drop.dropIntensity = 0.88f;
    drop.bass = 0.84f;
    drop.bandOnsets[0] = 0.74f;

    AudioMetrics groove = neutral;
    groove.section = ArrangementSection::Groove;
    groove.sectionConfidence = 0.88f;
    groove.sectionProgress = 0.44f;
    groove.beatConfidence = 0.94f;
    groove.barConfidence = 0.88f;
    groove.downbeatConfidence = 0.78f;

    AudioMetrics breakdown = neutral;
    breakdown.section = ArrangementSection::Breakdown;
    breakdown.sectionConfidence = 0.82f;
    breakdown.sectionProgress = 0.38f;
    breakdown.rms = 0.22f;
    breakdown.bass = 0.12f;
    breakdown.stereoWidth = 0.88f;
    breakdown.harmonicEnergy = 0.70f;
    breakdown.beatConfidence = 0.08f;
    breakdown.style = AudioStyle::Ambient;
    breakdown.styleConfidence = 0.86f;

    const GeometryFrame neutralFrame = engine.buildFrame(neutral, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame buildFrame = engine.buildFrame(build, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame dropFrame = engine.buildFrame(drop, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame grooveFrame = engine.buildFrame(groove, settings, 1280.0f, 720.0f, 2.0);
    const GeometryFrame breakdownFrame = engine.buildFrame(breakdown, settings, 1280.0f, 720.0f, 2.0);

    require(neutralFrame.sectionNarrative3D < 0.10f,
            "neutral frames should not claim active 3D section narrative");
    require(buildFrame.sectionBuild3D > 0.55f && buildFrame.sectionNarrative3D > neutralFrame.sectionNarrative3D + 0.45f,
            "build sections should report a strong rising 3D narrative");
    require(dropFrame.sectionDrop3D > 0.70f,
            "drop sections should report a strong pressure 3D narrative");
    require(grooveFrame.sectionGroove3D > 0.55f,
            "groove sections should report a strong locked 3D narrative");
    require(breakdownFrame.sectionBreakdown3D > 0.55f,
            "breakdown sections should report a strong spacious 3D narrative");

    require(buildFrame.objects3D.size() > neutralFrame.objects3D.size() + 10U,
            "build narrative should add rising 3D structure objects");
    require(dropFrame.objects3D.size() > neutralFrame.objects3D.size() + 8U,
            "drop narrative should add pressure 3D structure objects");
    require(grooveFrame.objects3D.size() > neutralFrame.objects3D.size() + 16U,
            "groove narrative should add sequencer 3D structure objects");
    require(objectFamilyCount(breakdownFrame, {Object3DKind::DepthPlane, Object3DKind::Orbiter}) >
                objectFamilyCount(neutralFrame, {Object3DKind::DepthPlane, Object3DKind::Orbiter}) + 12,
            "breakdown narrative should add spacious depth-field objects without requiring total clutter");

    require(objectFamilyCount(dropFrame, {Object3DKind::TunnelRib, Object3DKind::Plate}) >
                objectFamilyCount(neutralFrame, {Object3DKind::TunnelRib, Object3DKind::Plate}) + 5,
            "drop narrative should add pressure ribs and shock plates");
    require(objectFamilyCount(grooveFrame, {Object3DKind::Column, Object3DKind::Link}) >
                objectFamilyCount(neutralFrame, {Object3DKind::Column, Object3DKind::Link}) + 12,
            "groove narrative should add locked sequencer columns and rails");
    require(buildFrame.projected3DVisualWeight > buildFrame.retained2DVisualWeight * 1.6f &&
                dropFrame.projected3DVisualWeight > dropFrame.retained2DVisualWeight * 1.6f &&
                grooveFrame.projected3DVisualWeight > grooveFrame.retained2DVisualWeight * 1.6f &&
                breakdownFrame.projected3DVisualWeight > breakdownFrame.retained2DVisualWeight * 1.6f,
            "section narrative structures should remain 3D-dominant");
}

void mouseDepthInteractionMoves3DObjects()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::TechnoMandala;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.9f;
    settings.interactionDepth = 1.0f;
    settings.interactiveField = true;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.beat = true;
    metrics.beatConfidence = 0.9f;
    metrics.dropIntensity = 0.55f;
    metrics.stereoWidth = 0.58f;

    InteractionState interaction;
    interaction.enabled = true;
    interaction.active = true;
    interaction.pressed = true;
    interaction.normalizedX = 0.5f;
    interaction.normalizedY = 0.5f;
    interaction.velocity = 1.0f;
    interaction.strength = 1.0f;

    const GeometryFrame neutral = engine.buildFrame(metrics, settings, InteractionState{}, 1280.0f, 720.0f, 3.25);
    const GeometryFrame interactive = engine.buildFrame(metrics, settings, interaction, 1280.0f, 720.0f, 3.25);
    require(neutral.objects3D.size() == interactive.objects3D.size(),
            "mouse depth interaction should move the same 3D object set");
    require(averageObjectZ(interactive) < averageObjectZ(neutral) - 0.1f,
            "mouse interaction should pull nearby 3D objects through z depth");
    require(std::any_of(interactive.objects3D.begin(), interactive.objects3D.end(), [](const Object3D& object) {
                return object.velocity.z < -0.01f;
            }),
            "mouse click should leave a depth-aware z velocity impulse on affected objects");
}

void mouseDepthInteractionAddsCameraParallax()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::PhaseWeave;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.86f;
    settings.interactionDepth = 1.0f;
    settings.interactiveField = true;
    settings.environmentReactive = false;
    settings.qualityScale = 0.92f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.stereoWidth = 0.74f;
    metrics.phraseIntensity = 0.55f;
    metrics.style = AudioStyle::Ambient;
    metrics.styleConfidence = 0.86f;

    InteractionState interaction;
    interaction.enabled = true;
    interaction.active = true;
    interaction.pressed = true;
    interaction.normalizedX = 0.86f;
    interaction.normalizedY = 0.22f;
    interaction.velocity = 0.62f;
    interaction.strength = 1.0f;

    const GeometryFrame neutral = engine.buildFrame(metrics, settings, InteractionState{}, 1280.0f, 720.0f, 2.75);
    const GeometryFrame interactive = engine.buildFrame(metrics, settings, interaction, 1280.0f, 720.0f, 2.75);

    require(std::fabs(interactive.cameraCenterOffset.x - neutral.cameraCenterOffset.x) > 8.0f,
            "mouse depth should pan the 3D camera center for parallax inspection");
    require(std::fabs(interactive.cameraYaw - neutral.cameraYaw) > 0.015f,
            "mouse depth should orbit the 3D camera yaw");
    require(std::fabs(interactive.cameraPitch - neutral.cameraPitch) > 0.010f,
            "mouse depth should tilt the 3D camera pitch");
    require(interactive.cameraDepth < neutral.cameraDepth,
            "pressed mouse depth should dolly slightly into the scene");
}

void interactionAndEnvironmentRemain3DFirst()
{
    VisualizerEngine engine;
    VisualSettings settings;
    settings.mode = VisualMode::PhaseWeave;
    settings.depth3D = 1.0f;
    settings.objectDensity3D = 0.90f;
    settings.interactionDepth = 1.0f;
    settings.lightingGlow = 0.92f;
    settings.scenePersonality = 0.88f;
    settings.response3D = 0.96f;
    settings.motionStability = 0.88f;
    settings.patternClarity = 0.92f;
    settings.interactiveField = true;
    settings.environmentReactive = true;
    settings.qualityScale = 0.92f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.rms = 0.36f;
    metrics.peak = 0.58f;
    metrics.bass = 0.18f;
    metrics.mid = 0.46f;
    metrics.highMid = 0.44f;
    metrics.treble = 0.38f;
    metrics.stereoWidth = 0.84f;
    metrics.harmonicEnergy = 0.74f;
    metrics.keyConfidence = 0.80f;
    metrics.style = AudioStyle::Ambient;
    metrics.styleConfidence = 0.88f;
    metrics.section = ArrangementSection::Breakdown;
    metrics.sectionConfidence = 0.78f;

    InteractionState interaction;
    interaction.enabled = true;
    interaction.active = true;
    interaction.pressed = true;
    interaction.normalizedX = 0.82f;
    interaction.normalizedY = 0.26f;
    interaction.velocity = 0.76f;
    interaction.strength = 1.0f;

    EnvironmentState environment;
    environment.enabled = true;
    environment.timeOfDay = 0.72f;
    environment.motion = 0.72f;
    environment.ambient = 0.64f;

    const GeometryFrame frame = engine.buildFrame(metrics, settings, interaction, environment, 1280.0f, 720.0f, 3.4);

    require(frame.retained2DPrimitiveRatio < 0.28f,
            "interactive/environmental high-depth frames should not retain a large flat 2D layer");
    require(frame.retained2DVisualRatio < 0.12f,
            "interactive/environmental high-depth frames should heavily fade flat 2D visual weight");
    require(frame.projected3DPrimitiveCount > frame.retained2DPrimitiveCount * 2,
            "3D projection should remain the visible backbone under mouse/environment input");
    require(frame.projected3DVisualWeight > frame.retained2DVisualWeight * 2.4f,
            "3D visual weight should dominate mouse/environment guide accents");
    require(frame.sectionBreakdown3D > 0.45f,
            "breakdown interpretation should still create 3D depth structure under live interaction");
}

void objectDensity3DControlsObjectCount()
{
    VisualizerEngine engine;
    VisualSettings lowDensity;
    lowDensity.mode = VisualMode::SpectralOrigami;
    lowDensity.depth3D = 1.0f;
    lowDensity.objectDensity3D = 0.0f;
    lowDensity.interactiveField = false;
    lowDensity.environmentReactive = false;
    lowDensity.qualityScale = 1.0f;

    VisualSettings highDensity = lowDensity;
    highDensity.objectDensity3D = 1.0f;

    AudioMetrics metrics = syntheticMetrics();
    metrics.treble = 0.82f;
    metrics.spectralFlux = 0.58f;
    metrics.harmonicEnergy = 0.74f;

    const GeometryFrame sparse = engine.buildFrame(metrics, lowDensity, 1280.0f, 720.0f, 4.0);
    const GeometryFrame dense = engine.buildFrame(metrics, highDensity, 1280.0f, 720.0f, 4.0);
    require(!sparse.objects3D.empty(), "lowest 3D density should still keep a sparse object scaffold");
    require(dense.objects3D.size() > sparse.objects3D.size() + 8U,
            "higher 3D object density should materially increase object count");
    require(countPrimitives(dense) > countPrimitives(sparse),
            "higher 3D object density should increase rendered primitive load");
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
    preset.settings.motionStyle = MotionStyle::Hyperspace;
    preset.settings.hueShift = 0.37f;
    preset.settings.depth3D = 0.88f;
    preset.settings.colorImpact = 0.91f;
    preset.settings.objectDensity3D = 0.73f;
    preset.settings.interactionDepth = 0.64f;
    preset.settings.lightingGlow = 0.82f;
    preset.settings.scenePersonality = 0.57f;
    preset.settings.response3D = 0.93f;
    preset.settings.motionStability = 0.68f;
    preset.settings.patternClarity = 0.79f;
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
    require(parseMotionStyle("heavy bass").has_value() &&
                *parseMotionStyle("heavy bass") == MotionStyle::HeavyBass,
            "motion style aliases should parse spaced names");
    require(parseMotionStyle("breaks").has_value() &&
                *parseMotionStyle("breaks") == MotionStyle::Breakbeat,
            "motion style aliases should parse genre shorthand");
    require(loaded->settings.palette == preset.settings.palette, "preset palette should round-trip");
    require(loaded->settings.motionStyle == MotionStyle::Hyperspace, "preset motion style should round-trip");
    require(loaded->settings.hueShift > 0.36f && loaded->settings.hueShift < 0.38f, "hue shift should round-trip");
    require(loaded->settings.depth3D > 0.87f && loaded->settings.depth3D < 0.89f,
            "3D depth should round-trip");
    require(loaded->settings.colorImpact > 0.90f && loaded->settings.colorImpact < 0.92f,
            "color impact should round-trip");
    require(loaded->settings.objectDensity3D > 0.72f && loaded->settings.objectDensity3D < 0.74f,
            "3D object density should round-trip");
    require(loaded->settings.interactionDepth > 0.63f && loaded->settings.interactionDepth < 0.65f,
            "mouse depth interaction should round-trip");
    require(loaded->settings.lightingGlow > 0.81f && loaded->settings.lightingGlow < 0.83f,
            "3D lighting glow should round-trip");
    require(loaded->settings.scenePersonality > 0.56f && loaded->settings.scenePersonality < 0.58f,
            "scene personality should round-trip");
    require(loaded->settings.response3D > 0.92f && loaded->settings.response3D < 0.94f,
            "3D response should round-trip");
    require(loaded->settings.motionStability > 0.67f && loaded->settings.motionStability < 0.69f,
            "motion stability should round-trip");
    require(loaded->settings.patternClarity > 0.78f && loaded->settings.patternClarity < 0.80f,
            "pattern clarity should round-trip");
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
    bool sawMechanicalStyle = false;
    bool sawHyperspaceStyle = false;
    bool sawHeavyBassStyle = false;
    bool sawAmbientStyle = false;
    bool sawBreakbeatStyle = false;
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
        require(preset.settings.objectDensity3D >= 0.0f && preset.settings.objectDensity3D <= 1.0f,
                "curated 3D object density should be normalized");
        require(preset.settings.interactionDepth >= 0.0f && preset.settings.interactionDepth <= 1.0f,
                "curated mouse depth should be normalized");
        require(preset.settings.lightingGlow >= 0.0f && preset.settings.lightingGlow <= 1.0f,
                "curated lighting glow should be normalized");
        require(preset.settings.scenePersonality >= 0.0f && preset.settings.scenePersonality <= 1.0f,
                "curated scene personality should be normalized");
        require(preset.settings.response3D >= 0.0f && preset.settings.response3D <= 1.0f,
                "curated 3D response should be normalized");
        require(preset.settings.motionStability >= 0.0f && preset.settings.motionStability <= 1.0f,
                "curated motion stability should be normalized");
        require(preset.settings.patternClarity >= 0.0f && preset.settings.patternClarity <= 1.0f,
                "curated pattern clarity should be normalized");
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
        sawMechanicalStyle = sawMechanicalStyle || preset.settings.motionStyle == MotionStyle::Mechanical;
        sawHyperspaceStyle = sawHyperspaceStyle || preset.settings.motionStyle == MotionStyle::Hyperspace;
        sawHeavyBassStyle = sawHeavyBassStyle || preset.settings.motionStyle == MotionStyle::HeavyBass;
        sawAmbientStyle = sawAmbientStyle || preset.settings.motionStyle == MotionStyle::AmbientDrift;
        sawBreakbeatStyle = sawBreakbeatStyle || preset.settings.motionStyle == MotionStyle::Breakbeat;

        const GeometryFrame frame = engine.buildFrame(metrics,
                                                      preset.settings,
                                                      InteractionState{},
                                                      EnvironmentState{},
                                                      640.0f,
                                                      360.0f,
                                                      1.0 + static_cast<double>(i) * 0.25);
        require(countPrimitives(frame) > 40, "curated presets should render meaningful geometry");
        require(!frame.objects3D.empty(), "curated presets should render true 3D object scenes");

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
    require(sawMechanicalStyle, "curated bank should include mechanical motion style");
    require(sawHyperspaceStyle, "curated bank should include hyperspace motion style");
    require(sawHeavyBassStyle, "curated bank should include heavy bass motion style");
    require(sawAmbientStyle, "curated bank should include ambient drift motion style");
    require(sawBreakbeatStyle, "curated bank should include breakbeat motion style");
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
    preset.settings.motionStyle = MotionStyle::Breakbeat;
    preset.settings.hueShift = 0.27f;
    preset.settings.depth3D = 0.84f;
    preset.settings.colorImpact = 0.89f;
    preset.settings.objectDensity3D = 0.76f;
    preset.settings.interactionDepth = 0.68f;
    preset.settings.lightingGlow = 0.81f;
    preset.settings.scenePersonality = 0.62f;
    preset.settings.response3D = 0.91f;
    preset.settings.motionStability = 0.73f;
    preset.settings.patternClarity = 0.83f;
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
    require(loaded->settings.motionStyle == MotionStyle::Breakbeat,
            "loaded user preset should preserve motion style");
    require(loaded->settings.depth3D > 0.83f && loaded->settings.depth3D < 0.85f,
            "loaded user preset should preserve 3D depth");
    require(loaded->settings.colorImpact > 0.88f && loaded->settings.colorImpact < 0.90f,
            "loaded user preset should preserve color impact");
    require(loaded->settings.objectDensity3D > 0.75f && loaded->settings.objectDensity3D < 0.77f,
            "loaded user preset should preserve 3D object density");
    require(loaded->settings.interactionDepth > 0.67f && loaded->settings.interactionDepth < 0.69f,
            "loaded user preset should preserve mouse depth interaction");
    require(loaded->settings.lightingGlow > 0.80f && loaded->settings.lightingGlow < 0.82f,
            "loaded user preset should preserve 3D lighting glow");
    require(loaded->settings.scenePersonality > 0.61f && loaded->settings.scenePersonality < 0.63f,
            "loaded user preset should preserve scene personality");
    require(loaded->settings.response3D > 0.90f && loaded->settings.response3D < 0.92f,
            "loaded user preset should preserve 3D response");
    require(loaded->settings.motionStability > 0.72f && loaded->settings.motionStability < 0.74f,
            "loaded user preset should preserve motion stability");
    require(loaded->settings.patternClarity > 0.82f && loaded->settings.patternClarity < 0.84f,
            "loaded user preset should preserve pattern clarity");
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
    settings.objectDensity3D = 0.74f;
    settings.interactionDepth = 0.66f;
    settings.lightingGlow = 0.88f;
    settings.scenePersonality = 0.58f;
    settings.response3D = 0.91f;
    settings.motionStability = 0.74f;
    settings.patternClarity = 0.83f;

    const ControlPanelLayout layout = buildControlPanelLayout(1280.0f, 720.0f, settings, true);
    require(!layout.items.empty(), "control panel should build interactive items");

    bool foundRecord = false;
    bool foundResetProfiles = false;
    bool foundSlider = false;
    bool foundQuality = false;
    bool foundHueShift = false;
    bool foundDepth = false;
    bool foundObjectDensity = false;
    bool foundInteractionDepth = false;
    bool foundLightingGlow = false;
    bool foundColorImpact = false;
    bool foundScenePersonality = false;
    bool foundResponse3D = false;
    bool foundMotionStability = false;
    bool foundPatternClarity = false;
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
    bool foundMotionPrevious = false;
    bool foundMotionNext = false;
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
        if (item.control == PanelControl::ObjectDensitySlider) {
            require(item.slider, "3D object density control should be a slider");
            require(item.value > 0.73f && item.value < 0.75f, "3D object density slider should reflect settings");
            foundObjectDensity = true;
        }
        if (item.control == PanelControl::InteractionDepthSlider) {
            require(item.slider, "mouse depth control should be a slider");
            require(item.value > 0.65f && item.value < 0.67f, "mouse depth slider should reflect settings");
            foundInteractionDepth = true;
        }
        if (item.control == PanelControl::LightingGlowSlider) {
            require(item.slider, "3D lighting glow control should be a slider");
            require(item.value > 0.87f && item.value < 0.89f, "3D lighting glow slider should reflect settings");
            foundLightingGlow = true;
        }
        if (item.control == PanelControl::ColorImpactSlider) {
            require(item.slider, "color impact control should be a slider");
            require(item.value > 0.81f && item.value < 0.83f, "color impact slider should reflect settings");
            foundColorImpact = true;
        }
        if (item.control == PanelControl::ScenePersonalitySlider) {
            require(item.slider, "scene personality control should be a slider");
            require(item.value > 0.57f && item.value < 0.59f, "scene personality slider should reflect settings");
            foundScenePersonality = true;
        }
        if (item.control == PanelControl::Response3DSlider) {
            require(item.slider, "3D response control should be a slider");
            require(item.value > 0.90f && item.value < 0.92f, "3D response slider should reflect settings");
            foundResponse3D = true;
        }
        if (item.control == PanelControl::MotionStabilitySlider) {
            require(item.slider, "motion stability control should be a slider");
            require(item.value > 0.73f && item.value < 0.75f, "motion stability slider should reflect settings");
            foundMotionStability = true;
        }
        if (item.control == PanelControl::PatternClaritySlider) {
            require(item.slider, "pattern clarity control should be a slider");
            require(item.value > 0.82f && item.value < 0.84f, "pattern clarity slider should reflect settings");
            foundPatternClarity = true;
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
        if (item.control == PanelControl::MotionStylePrevious) {
            require(!item.slider, "previous motion style control should be a button");
            foundMotionPrevious = true;
        }
        if (item.control == PanelControl::MotionStyleNext) {
            require(!item.slider, "next motion style control should be a button");
            foundMotionNext = true;
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
    require(foundObjectDensity, "3D object density slider should exist");
    require(foundInteractionDepth, "mouse depth slider should exist");
    require(foundLightingGlow, "3D lighting glow slider should exist");
    require(foundColorImpact, "color impact slider should exist");
    require(foundScenePersonality, "scene personality slider should exist");
    require(foundResponse3D, "3D response slider should exist");
    require(foundMotionStability, "motion stability slider should exist");
    require(foundPatternClarity, "pattern clarity slider should exist");
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
    require(foundMotionPrevious, "previous motion style control should exist");
    require(foundMotionNext, "next motion style control should exist");
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
    require(std::find(liveLines.begin(), liveLines.end(), "Capture idle - recording off") != liveLines.end(),
            "runtime inspector should clarify that capture idle only means recording is off");

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
    options.settings.motionStyle = MotionStyle::HeavyBass;
    options.settings.hueShift = 0.25f;
    options.settings.depth3D = 0.83f;
    options.settings.colorImpact = 0.87f;
    options.settings.objectDensity3D = 0.71f;
    options.settings.interactionDepth = 0.67f;
    options.settings.lightingGlow = 0.82f;
    options.settings.scenePersonality = 0.59f;
    options.settings.response3D = 0.94f;
    options.settings.motionStability = 0.76f;
    options.settings.patternClarity = 0.82f;
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
        require(manifestText.find("objectDensity3D=0.710") != std::string::npos,
                "offline manifest should include 3D object density");
        require(manifestText.find("finalObjectDensity3D=0.710") != std::string::npos,
                "offline manifest should include final 3D object density");
        require(manifestText.find("interactionDepth=0.670") != std::string::npos,
                "offline manifest should include mouse depth interaction");
        require(manifestText.find("finalInteractionDepth=0.670") != std::string::npos,
                "offline manifest should include final mouse depth interaction");
        require(manifestText.find("lightingGlow=0.820") != std::string::npos,
                "offline manifest should include 3D lighting glow");
        require(manifestText.find("finalLightingGlow=0.820") != std::string::npos,
                "offline manifest should include final 3D lighting glow");
        require(manifestText.find("scenePersonality=0.590") != std::string::npos,
                "offline manifest should include scene personality");
        require(manifestText.find("finalScenePersonality=0.590") != std::string::npos,
                "offline manifest should include final scene personality");
        require(manifestText.find("response3D=0.940") != std::string::npos,
                "offline manifest should include 3D response");
        require(manifestText.find("finalResponse3D=0.940") != std::string::npos,
                "offline manifest should include final 3D response");
        require(manifestText.find("motionStability=0.760") != std::string::npos,
                "offline manifest should include motion stability");
        require(manifestText.find("finalMotionStability=0.760") != std::string::npos,
                "offline manifest should include final motion stability");
        require(manifestText.find("patternClarity=0.820") != std::string::npos,
                "offline manifest should include pattern clarity");
        require(manifestText.find("finalPatternClarity=0.820") != std::string::npos,
                "offline manifest should include final pattern clarity");
        require(manifestText.find("motionStyle=Heavy Bass") != std::string::npos,
                "offline manifest should include requested motion style");
        require(manifestText.find("finalMotionStyle=Heavy Bass") != std::string::npos,
                "offline manifest should include final motion style");
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
        require(timelineText.find("frame,timeSeconds,mode,palette,motionStyle") != std::string::npos,
                "timeline should include a CSV header");
        require(timelineText.find("motionStyle,hueShift,depth3D,objectDensity3D,interactionDepth,lightingGlow,colorImpact,scenePersonality,response3D,motionStability,patternClarity,complexity") != std::string::npos,
                "timeline should include 3D depth, object, glow, color, personality, response, stability, and clarity columns");
        require(timelineText.find("\"Heavy Bass\"") != std::string::npos,
                "timeline should include rendered motion style");
        require(timelineText.find("\"Quantum Tunnel\"") != std::string::npos,
                "timeline should include rendered visual mode");
        require(timelineText.find("section,sectionConfidence,sectionProgress") != std::string::npos,
                "timeline should include arrangement section columns");
        require(timelineText.find("barPhase,barConfidence,downbeat,downbeatConfidence") != std::string::npos,
                "timeline should include bar and downbeat columns");
        require(timelineText.find("phraseBoundary,phrasePhase,phraseConfidence,buildTension") != std::string::npos,
                "timeline should include phrase structure columns");
        require(timelineText.find("scene3DName,sceneIntent,authored2DPrimitiveCount,retained2DPrimitiveCount,projected3DPrimitiveCount,threeDDominance") != std::string::npos,
                "timeline should include scene intent and 3D dominance columns");
        require(timelineText.find("bassRole3D,drumRole3D,melodyRole3D,harmonyRole3D,spaceRole3D,fractureRole3D,shadowRole3D,convergence3D,roleSeparation3D") != std::string::npos,
                "timeline should include separated musical role columns");
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
    options.settings.motionStyle = MotionStyle::Breakbeat;
    options.settings.hueShift = 0.42f;
    options.settings.depth3D = 0.79f;
    options.settings.colorImpact = 0.86f;
    options.settings.objectDensity3D = 0.74f;
    options.settings.interactionDepth = 0.62f;
    options.settings.lightingGlow = 0.84f;
    options.settings.scenePersonality = 0.69f;
    options.settings.response3D = 0.92f;
    options.settings.motionStability = 0.78f;
    options.settings.patternClarity = 0.86f;
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
        require(manifestText.find("\"objectDensity3D\": 0.740") != std::string::npos,
                "share manifest should include 3D object density");
        require(manifestText.find("\"finalObjectDensity3D\": 0.740") != std::string::npos,
                "share manifest should include final 3D object density");
        require(manifestText.find("\"interactionDepth\": 0.620") != std::string::npos,
                "share manifest should include mouse depth interaction");
        require(manifestText.find("\"finalInteractionDepth\": 0.620") != std::string::npos,
                "share manifest should include final mouse depth interaction");
        require(manifestText.find("\"lightingGlow\": 0.840") != std::string::npos,
                "share manifest should include 3D lighting glow");
        require(manifestText.find("\"finalLightingGlow\": 0.840") != std::string::npos,
                "share manifest should include final 3D lighting glow");
        require(manifestText.find("\"scenePersonality\": 0.690") != std::string::npos,
                "share manifest should include scene personality");
        require(manifestText.find("\"finalScenePersonality\": 0.690") != std::string::npos,
                "share manifest should include final scene personality");
        require(manifestText.find("\"response3D\": 0.920") != std::string::npos,
                "share manifest should include 3D response");
        require(manifestText.find("\"finalResponse3D\": 0.920") != std::string::npos,
                "share manifest should include final 3D response");
        require(manifestText.find("\"motionStability\": 0.780") != std::string::npos,
                "share manifest should include motion stability");
        require(manifestText.find("\"finalMotionStability\": 0.780") != std::string::npos,
                "share manifest should include final motion stability");
        require(manifestText.find("\"patternClarity\": 0.860") != std::string::npos,
                "share manifest should include pattern clarity");
        require(manifestText.find("\"finalPatternClarity\": 0.860") != std::string::npos,
                "share manifest should include final pattern clarity");
        require(manifestText.find("\"motionStyle\": \"Breakbeat\"") != std::string::npos,
                "share manifest should include requested motion style");
        require(manifestText.find("\"finalMotionStyle\": \"Breakbeat\"") != std::string::npos,
                "share manifest should include final motion style");
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
        require(pageText.find("3D Objects") != std::string::npos,
                "share page should summarize 3D object density");
        require(pageText.find("Mouse 3D") != std::string::npos,
                "share page should summarize mouse depth");
        require(pageText.find("3D Glow") != std::string::npos,
                "share page should summarize 3D glow");
        require(pageText.find("Color Impact") != std::string::npos,
                "share page should summarize color impact");
        require(pageText.find("Scene Personality") != std::string::npos,
                "share page should summarize scene personality");
        require(pageText.find("Motion Style") != std::string::npos,
                "share page should summarize motion style");
        require(pageText.find("3D Response") != std::string::npos,
                "share page should summarize 3D response");
        require(pageText.find("Motion Stability") != std::string::npos,
                "share page should summarize motion stability");
        require(pageText.find("Pattern Clarity") != std::string::npos,
                "share page should summarize pattern clarity");
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
    options.settings.motionStyle = MotionStyle::Mechanical;
    options.settings.complexity = 1.2f;
    options.settings.depth3D = 0.8f;
    options.settings.colorImpact = 0.84f;
    options.settings.objectDensity3D = 0.77f;
    options.settings.interactionDepth = 0.63f;
    options.settings.lightingGlow = 0.85f;
    options.settings.scenePersonality = 0.7f;
    options.settings.response3D = 0.95f;
    options.settings.motionStability = 0.79f;
    options.settings.patternClarity = 0.87f;
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
        require(manifestText.find("\"motionStyle\": \"Mechanical\"") != std::string::npos,
                "batch manifest should include configured motion style");
        require(manifestText.find("\"depth3D\": 0.800") != std::string::npos,
                "batch manifest should include configured 3D depth");
        require(manifestText.find("\"colorImpact\": 0.840") != std::string::npos,
                "batch manifest should include configured color impact");
        require(manifestText.find("\"objectDensity3D\": 0.770") != std::string::npos,
                "batch manifest should include configured 3D object density");
        require(manifestText.find("\"interactionDepth\": 0.630") != std::string::npos,
                "batch manifest should include configured mouse depth");
        require(manifestText.find("\"lightingGlow\": 0.850") != std::string::npos,
                "batch manifest should include configured 3D glow");
        require(manifestText.find("\"scenePersonality\": 0.700") != std::string::npos,
                "batch manifest should include configured scene personality");
        require(manifestText.find("\"response3D\": 0.950") != std::string::npos,
                "batch manifest should include configured 3D response");
        require(manifestText.find("\"motionStability\": 0.790") != std::string::npos,
                "batch manifest should include configured motion stability");
        require(manifestText.find("\"patternClarity\": 0.870") != std::string::npos,
                "batch manifest should include configured pattern clarity");
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
        require(indexText.find("React: 0.95") != std::string::npos,
                "batch index should summarize 3D response");
        require(indexText.find("Motion: Mechanical") != std::string::npos,
                "batch index should summarize motion style");
        require(indexText.find("Stable: 0.79") != std::string::npos,
                "batch index should summarize motion stability");
        require(indexText.find("Clear: 0.87") != std::string::npos,
                "batch index should summarize pattern clarity");
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
        require(timelineText.find("motionStyle,hueShift,depth3D,objectDensity3D,interactionDepth,lightingGlow,colorImpact,scenePersonality,response3D,motionStability,patternClarity,complexity") != std::string::npos,
                "batch timeline should contain 3D object, glow, color, personality, response, stability, and clarity columns");
        require(timelineText.find("phraseBoundary,phrasePhase,phraseConfidence,buildTension") != std::string::npos,
                "batch timeline should contain phrase structure columns");
        require(timelineText.find("scene3DName,sceneIntent,authored2DPrimitiveCount,retained2DPrimitiveCount,projected3DPrimitiveCount,threeDDominance") != std::string::npos,
                "batch timeline should contain scene intent and 3D dominance columns");
        require(timelineText.find("bassRole3D,drumRole3D,melodyRole3D,harmonyRole3D,spaceRole3D,fractureRole3D,shadowRole3D,convergence3D,roleSeparation3D") != std::string::npos,
                "batch timeline should contain separated musical role columns");
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
        {"analyzerClassifiesMusicalStyleCues", viz::tests::analyzerClassifiesMusicalStyleCues},
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
        {"object3DModesProduceDistinctSignatures", viz::tests::object3DModesProduceDistinctSignatures},
        {"object3DRespondsStronglyToMusicAcrossModes", viz::tests::object3DRespondsStronglyToMusicAcrossModes},
        {"allModesStay3DFirstInStillFrames", viz::tests::allModesStay3DFirstInStillFrames},
        {"songProfilesScaleMusicallyWithoutChaos", viz::tests::songProfilesScaleMusicallyWithoutChaos},
        {"motionStylesCreateDistinct3DChoreography", viz::tests::motionStylesCreateDistinct3DChoreography},
        {"musicProfilesDriveDifferent3DChoreography", viz::tests::musicProfilesDriveDifferent3DChoreography},
        {"threeDFirstCompositionSuppressesLegacy2D", viz::tests::threeDFirstCompositionSuppressesLegacy2D},
        {"sceneIntentProfilesProduceDistinct3DInterpretations", viz::tests::sceneIntentProfilesProduceDistinct3DInterpretations},
        {"autoSceneProfilesProduceDistinct3DSignatures", viz::tests::autoSceneProfilesProduceDistinct3DSignatures},
        {"sameModeSongIdentitiesAuthorDistinct3DSetPieces", viz::tests::sameModeSongIdentitiesAuthorDistinct3DSetPieces},
        {"songIdentitiesDriveDistinctCameraLanguage", viz::tests::songIdentitiesDriveDistinctCameraLanguage},
        {"autoSceneContinuityResistsAmbiguousFrameFlips", viz::tests::autoSceneContinuityResistsAmbiguousFrameFlips},
        {"autoSceneSelectsMotionStyleFromMusic", viz::tests::autoSceneSelectsMotionStyleFromMusic},
        {"autoSceneDrives3DCompositionThroughSections", viz::tests::autoSceneDrives3DCompositionThroughSections},
        {"motionStabilityAndPatternClarityReduceJitter", viz::tests::motionStabilityAndPatternClarityReduceJitter},
        {"silenceKeepsStableReadableScaffold", viz::tests::silenceKeepsStableReadableScaffold},
        {"object3DDepthSortsAndProjects", viz::tests::object3DDepthSortsAndProjects},
        {"threeDScenesRenderMaterialFacesAndDepthHaze", viz::tests::threeDScenesRenderMaterialFacesAndDepthHaze},
        {"sectionNarrativeAuthorsDistinct3DStructures", viz::tests::sectionNarrativeAuthorsDistinct3DStructures},
        {"mouseDepthInteractionMoves3DObjects", viz::tests::mouseDepthInteractionMoves3DObjects},
        {"mouseDepthInteractionAddsCameraParallax", viz::tests::mouseDepthInteractionAddsCameraParallax},
        {"interactionAndEnvironmentRemain3DFirst", viz::tests::interactionAndEnvironmentRemain3DFirst},
        {"objectDensity3DControlsObjectCount", viz::tests::objectDensity3DControlsObjectCount},
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
        {"recorderFillsMaterialPolygons", viz::tests::recorderFillsMaterialPolygons},
        {"recorderTrailsPersistPreviousFrame", viz::tests::recorderTrailsPersistPreviousFrame},
        {"supportBundleWritesDiagnosticsWithoutCopyingLargeMedia", viz::tests::supportBundleWritesDiagnosticsWithoutCopyingLargeMedia},
        {"liveCapturePackageWritesShareMetadata", viz::tests::liveCapturePackageWritesShareMetadata}
    };

    return viz::tests::runTests(tests, static_cast<int>(sizeof(tests) / sizeof(tests[0])));
}
