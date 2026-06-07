#include "Visualizer/Audio/AudioAnalyzer.hpp"
#include "Visualizer/Performance/FramePerformanceTracker.hpp"
#include "Visualizer/Visualization/PresetStore.hpp"
#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;

int parseIntArg(int argc, char** argv, const std::string& name, int fallback)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            try {
                return std::stoi(argv[i + 1]);
            } catch (...) {
                return fallback;
            }
        }
    }
    return fallback;
}

std::string parseStringArg(int argc, char** argv, const std::string& name, std::string fallback)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return fallback;
}

bool hasFlag(int argc, char** argv, const std::string& name)
{
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == name) {
            return true;
        }
    }
    return false;
}

void printUsage()
{
    std::cout
        << "VisualizerBenchmark [--frames N] [--width N] [--height N] [--mode NAME] [--all-modes]\n"
        << "Runs analyzer + geometry generation with synthetic techno-like audio.\n";
}

struct BenchmarkResult {
    viz::VisualMode mode = viz::VisualMode::TechnoMandala;
    double averageMs = 0.0;
    double estimatedFps = 0.0;
    viz::PerformanceStats stats{};
    int maxPrimitives = 0;
};

BenchmarkResult runBenchmark(viz::VisualMode mode, int frameCount, int width, int height)
{
    constexpr int sampleRate = 48000;
    constexpr int channels = 2;
    constexpr int analysisFrames = 2048;

    viz::AudioAnalyzer analyzer(sampleRate, channels);
    viz::VisualizerEngine engine;
    viz::VisualSettings settings;
    settings.mode = mode;
    settings.palette = viz::Palette::AcidAurora;
    settings.intensity = 1.35f;
    settings.speed = 1.1f;
    settings.adaptiveQuality = true;

    viz::FramePerformanceTracker tracker;
    viz::PerformanceStats stats;
    std::vector<float> samples(static_cast<std::size_t>(analysisFrames) * channels, 0.0f);
    double totalMs = 0.0;
    int maxPrimitives = 0;

    for (int frame = 0; frame < frameCount; ++frame) {
        const double timeSeconds = static_cast<double>(frame) / 60.0;
        const float envelope = (frame % 64) < 8 ? 0.95f : 0.42f;
        for (int i = 0; i < analysisFrames; ++i) {
            const float t = static_cast<float>(timeSeconds) + static_cast<float>(i) / static_cast<float>(sampleRate);
            const float kick = std::sin(2.0f * kPi * 62.0f * t) * envelope;
            const float bass = std::sin(2.0f * kPi * 126.0f * t) * 0.42f;
            const float hat = std::sin(2.0f * kPi * 7600.0f * t) * (((frame + i / 256) % 4) == 0 ? 0.24f : 0.06f);
            samples[static_cast<std::size_t>(i) * 2U] = kick + bass + hat;
            samples[(static_cast<std::size_t>(i) * 2U) + 1U] = kick - bass + hat * 0.7f;
        }

        settings.qualityScale = stats.qualityScale > 0.0f ? stats.qualityScale : settings.qualityScale;
        const auto started = std::chrono::steady_clock::now();
        const viz::AudioMetrics metrics = analyzer.analyzeInterleaved(samples.data(), analysisFrames, timeSeconds);
        const auto analysisDone = std::chrono::steady_clock::now();
        const viz::GeometryFrame geometry = engine.buildFrame(metrics,
                                                              settings,
                                                              static_cast<float>(width),
                                                              static_cast<float>(height),
                                                              timeSeconds);
        const auto geometryDone = std::chrono::steady_clock::now();
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(geometryDone - started).count();
        totalMs += elapsedMs;
        const int primitives = viz::countPrimitives(geometry);
        maxPrimitives = std::max(maxPrimitives, primitives);
        stats = tracker.recordFrame(viz::FrameTimingBreakdown{
                                        elapsedMs,
                                        std::chrono::duration<double, std::milli>(analysisDone - started).count(),
                                        std::chrono::duration<double, std::milli>(geometryDone - analysisDone).count(),
                                        0.0,
                                        0.0
                                    },
                                    primitives,
                                    settings.adaptiveQuality,
                                    settings.qualityScale);
    }

    const double averageMs = totalMs / static_cast<double>(frameCount);
    return BenchmarkResult{
        mode,
        averageMs,
        averageMs > 0.0 ? 1000.0 / averageMs : 0.0,
        stats,
        maxPrimitives
    };
}

void printResult(const BenchmarkResult& result)
{
    std::cout << "mode=" << viz::toString(result.mode) << "\n";
    std::cout << "averageAnalyzerAndGeometryMs=" << result.averageMs << "\n";
    std::cout << "estimatedFps=" << result.estimatedFps << "\n";
    std::cout << "trackerAverageMs=" << result.stats.averageFrameMs << "\n";
    std::cout << "trackerAverageAnalysisMs=" << result.stats.averageAnalysisMs << "\n";
    std::cout << "trackerAverageGeometryMs=" << result.stats.averageGeometryMs << "\n";
    std::cout << "trackerAverageRenderMs=" << result.stats.averageRenderMs << "\n";
    std::cout << "trackerFps=" << result.stats.fps << "\n";
    std::cout << "finalQualityScale=" << result.stats.qualityScale << "\n";
    std::cout << "maxPrimitiveCount=" << result.maxPrimitives << "\n";
}

} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }

    const int frameCount = std::max(1, parseIntArg(argc, argv, "--frames", 240));
    const int width = std::max(64, parseIntArg(argc, argv, "--width", 1920));
    const int height = std::max(64, parseIntArg(argc, argv, "--height", 1080));
    std::cout << "frames=" << frameCount << "\n";
    std::cout << "resolution=" << width << "x" << height << "\n";

    if (hasFlag(argc, argv, "--all-modes")) {
        const viz::VisualMode modes[] = {
            viz::VisualMode::QuantumTunnel,
            viz::VisualMode::TechnoMandala,
            viz::VisualMode::LissajousMesh,
            viz::VisualMode::FrequencyBloom,
            viz::VisualMode::FractalCathedral,
            viz::VisualMode::PolyrhythmLattice,
            viz::VisualMode::SpectralOrigami,
            viz::VisualMode::ChromaKaleidoscope,
            viz::VisualMode::HyperspacePolytope,
            viz::VisualMode::PhaseWeave,
            viz::VisualMode::ResonanceTessellation,
            viz::VisualMode::NeuralConstellation,
            viz::VisualMode::CymaticInterference
        };
        for (viz::VisualMode mode : modes) {
            printResult(runBenchmark(mode, frameCount, width, height));
        }
        return 0;
    }

    const std::optional<viz::VisualMode> mode = viz::parseVisualMode(
        parseStringArg(argc, argv, "--mode", "TechnoMandala"));
    if (!mode) {
        std::cerr << "Unknown mode.\n";
        printUsage();
        return 2;
    }

    printResult(runBenchmark(*mode, frameCount, width, height));
    return 0;
}
