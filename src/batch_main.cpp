#include "Visualizer/Export/BatchExporter.hpp"
#include "Visualizer/Visualization/PresetStore.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

void printUsage()
{
    std::cout
        << "VisualizerBatch --input-dir music --output exports [options]\n\n"
        << "Options:\n"
        << "  --input-dir DIR    Directory containing WAV or Windows-supported audio files\n"
        << "  --output DIR       Output directory for batch index and per-track exports\n"
        << "  --recursive        Scan input directory recursively\n"
        << "  --max-files N      Export at most N discovered audio files\n"
        << "  --width N          Output width, default 1280\n"
        << "  --height N         Output height, default 720\n"
        << "  --fps N            Frames per second, default 60\n"
        << "  --seconds N        Export only the first N seconds of each track\n"
        << "  --mp4              Encode one H.264 MP4 per track with FFmpeg\n"
        << "  --share            Write per-track index.html and share_manifest.json, default\n"
        << "  --no-share         Skip per-track share pages while keeping batch_manifest.json\n"
        << "  --style-profile FILE  Load an adaptive style profile before each analysis\n"
        << "  --sync-profile FILE   Load an adaptive beat/section sync profile before each analysis\n"
        << "  --ffmpeg FILE      FFmpeg executable, default ffmpeg\n"
        << "  --crf N            H.264 quality, 0-51, lower is better, default 18\n"
        << "  --video-preset P   FFmpeg preset, default medium\n"
        << "  --auto-scene       Let audio metrics adapt mode, palette, speed, and intensity\n"
        << "  --environment      Enable deterministic environmental visual influence\n"
        << "  --no-environment   Disable environmental visual influence\n"
        << "  --time-of-day N    Environment phase from 0.0 midnight to 1.0 next midnight, default 0.5\n"
        << "  --trails           Keep decaying geometry trails in frame output\n"
        << "  --no-trails        Render crisp frames with a full background clear each frame\n"
        << "  --hue-shift N      Shift palette hue from 0.0 to 1.0, default 0.0\n"
        << "  --depth-3d N       True 3D camera depth from 0.0 flat to 1.0 deep\n"
        << "  --object-density-3d N  3D object density from 0.0 sparse to 1.0 packed\n"
        << "  --interaction-depth N  Mouse/depth interaction strength from 0.0 off to 1.0 strong\n"
        << "  --lighting-glow N  3D lighting and glow strength from 0.0 matte to 1.0 luminous\n"
        << "  --color-impact N   Palette personality strength from 0.0 subtle to 1.0 intense\n"
        << "  --scene-personality N  Scene personality/motion bias from 0.0 restrained to 1.0 extreme\n"
        << "  --response-3d N    Music-to-3D response gain from 0.0 restrained to 1.0 intense\n"
        << "  --motion-stability N  Smooth camera/object jitter from 0.0 wild to 1.0 stable\n"
        << "  --pattern-clarity N  Preserve readable geometry from 0.0 chaotic to 1.0 crisp\n"
        << "  --complexity N     Geometry density from 0.35 sparse to 1.8 dense, default 1.0\n"
        << "  --look NAME        Apply a built-in curated look; use VisualizerExport --list-looks\n"
        << "  --user-preset NAME Apply a user preset from profiles/presets or --preset-library\n"
        << "  --preset-library DIR  User preset library directory, default profiles/presets\n"
        << "  --list-user-presets   Print user preset library entries and exit\n"
        << "  --mode NAME        QuantumTunnel, TechnoMandala, LissajousMesh, FrequencyBloom, FractalCathedral, PolyrhythmLattice, SpectralOrigami, ChromaKaleidoscope, HyperspacePolytope, PhaseWeave, ResonanceTessellation, NeuralConstellation, CymaticInterference\n"
        << "  --palette NAME     NeonVoltage, InfraredChrome, AcidAurora, MonochromeLaser, OceanicPulse\n"
        << "  --preset FILE      Load settings from .vizpreset\n"
        << "  --help, -h         Show this help\n";
}

void printUserPresets(const std::filesystem::path& directory)
{
    const std::vector<viz::PresetLibraryEntry> entries = viz::scanUserPresetLibrary(directory);
    std::cout << "User presets in " << directory.string() << ":\n";
    if (entries.empty()) {
        std::cout << "  (none)\n";
        return;
    }
    for (const viz::PresetLibraryEntry& entry : entries) {
        std::cout << "  " << entry.name
                  << " | " << entry.path.filename().string()
                  << "\n";
    }
}

bool nextValue(int& index, int argc, char** argv, std::string& value)
{
    if (index + 1 >= argc) {
        return false;
    }
    value = argv[++index];
    return true;
}

int parseInt(const std::string& value, int fallback)
{
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

double parseDouble(const std::string& value, double fallback)
{
    try {
        return std::stod(value);
    } catch (...) {
        return fallback;
    }
}

} // namespace

int main(int argc, char** argv)
{
    viz::BatchExportOptions options;
    std::filesystem::path userPresetLibrary = viz::defaultUserPresetDirectory();
    bool listUserPresets = false;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--preset-library" && i + 1 < argc) {
            userPresetLibrary = argv[i + 1];
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        std::string value;
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
        if (arg == "--list-user-presets") {
            listUserPresets = true;
            continue;
        }
        if (arg == "--input-dir" && nextValue(i, argc, argv, value)) {
            options.inputDirectory = value;
        } else if (arg == "--output" && nextValue(i, argc, argv, value)) {
            options.outputDirectory = value;
        } else if (arg == "--recursive") {
            options.recursive = true;
        } else if (arg == "--max-files" && nextValue(i, argc, argv, value)) {
            options.maxFiles = parseInt(value, options.maxFiles);
        } else if (arg == "--width" && nextValue(i, argc, argv, value)) {
            options.width = parseInt(value, options.width);
        } else if (arg == "--height" && nextValue(i, argc, argv, value)) {
            options.height = parseInt(value, options.height);
        } else if (arg == "--fps" && nextValue(i, argc, argv, value)) {
            options.frameRate = parseInt(value, options.frameRate);
        } else if (arg == "--seconds" && nextValue(i, argc, argv, value)) {
            options.maxSeconds = parseDouble(value, options.maxSeconds);
        } else if (arg == "--mp4") {
            options.encodeMp4 = true;
        } else if (arg == "--share") {
            options.sharePackage = true;
        } else if (arg == "--no-share") {
            options.sharePackage = false;
        } else if (arg == "--style-profile" && nextValue(i, argc, argv, value)) {
            options.styleProfile = value;
        } else if (arg == "--sync-profile" && nextValue(i, argc, argv, value)) {
            options.syncProfile = value;
        } else if (arg == "--ffmpeg" && nextValue(i, argc, argv, value)) {
            options.ffmpegExecutable = value;
        } else if (arg == "--crf" && nextValue(i, argc, argv, value)) {
            options.videoCrf = parseInt(value, options.videoCrf);
        } else if (arg == "--video-preset" && nextValue(i, argc, argv, value)) {
            options.videoPreset = value;
        } else if (arg == "--auto-scene") {
            options.autoScene = true;
            options.settings.autoScene = true;
        } else if (arg == "--environment") {
            options.settings.environmentReactive = true;
        } else if (arg == "--no-environment") {
            options.settings.environmentReactive = false;
        } else if (arg == "--time-of-day" && nextValue(i, argc, argv, value)) {
            options.environmentTimeOfDay = std::clamp(static_cast<float>(parseDouble(value, options.environmentTimeOfDay)),
                                                      0.0f,
                                                      1.0f);
        } else if (arg == "--trails") {
            options.settings.trails = true;
        } else if (arg == "--no-trails") {
            options.settings.trails = false;
        } else if (arg == "--hue-shift" && nextValue(i, argc, argv, value)) {
            options.settings.hueShift = std::clamp(static_cast<float>(parseDouble(value, options.settings.hueShift)),
                                                   0.0f,
                                                   1.0f);
        } else if ((arg == "--depth-3d" || arg == "--depth") && nextValue(i, argc, argv, value)) {
            options.settings.depth3D = std::clamp(static_cast<float>(parseDouble(value, options.settings.depth3D)),
                                                  0.0f,
                                                  1.0f);
        } else if ((arg == "--object-density-3d" || arg == "--objects") && nextValue(i, argc, argv, value)) {
            options.settings.objectDensity3D =
                std::clamp(static_cast<float>(parseDouble(value, options.settings.objectDensity3D)), 0.0f, 1.0f);
        } else if ((arg == "--interaction-depth" || arg == "--mouse-depth") && nextValue(i, argc, argv, value)) {
            options.settings.interactionDepth =
                std::clamp(static_cast<float>(parseDouble(value, options.settings.interactionDepth)), 0.0f, 1.0f);
        } else if ((arg == "--lighting-glow" || arg == "--glow") && nextValue(i, argc, argv, value)) {
            options.settings.lightingGlow =
                std::clamp(static_cast<float>(parseDouble(value, options.settings.lightingGlow)), 0.0f, 1.0f);
        } else if ((arg == "--color-impact" || arg == "--color") && nextValue(i, argc, argv, value)) {
            options.settings.colorImpact = std::clamp(static_cast<float>(parseDouble(value, options.settings.colorImpact)),
                                                      0.0f,
                                                      1.0f);
        } else if ((arg == "--scene-personality" || arg == "--personality") && nextValue(i, argc, argv, value)) {
            options.settings.scenePersonality =
                std::clamp(static_cast<float>(parseDouble(value, options.settings.scenePersonality)), 0.0f, 1.0f);
        } else if ((arg == "--response-3d" || arg == "--response" || arg == "--reactivity") &&
                   nextValue(i, argc, argv, value)) {
            options.settings.response3D =
                std::clamp(static_cast<float>(parseDouble(value, options.settings.response3D)), 0.0f, 1.0f);
        } else if ((arg == "--motion-stability" || arg == "--stability") && nextValue(i, argc, argv, value)) {
            options.settings.motionStability =
                std::clamp(static_cast<float>(parseDouble(value, options.settings.motionStability)), 0.0f, 1.0f);
        } else if ((arg == "--pattern-clarity" || arg == "--clarity") && nextValue(i, argc, argv, value)) {
            options.settings.patternClarity =
                std::clamp(static_cast<float>(parseDouble(value, options.settings.patternClarity)), 0.0f, 1.0f);
        } else if (arg == "--complexity" && nextValue(i, argc, argv, value)) {
            options.settings.complexity = std::clamp(static_cast<float>(parseDouble(value, options.settings.complexity)),
                                                     0.35f,
                                                     1.8f);
        } else if (arg == "--look" && nextValue(i, argc, argv, value)) {
            const std::optional<viz::VisualPreset> preset = viz::findCuratedPreset(value);
            if (!preset) {
                std::cerr << "Unknown look: " << value << "\n";
                return 2;
            }
            options.settings = preset->settings;
            options.autoScene = preset->settings.autoScene;
            options.lookName = preset->name;
        } else if (arg == "--preset-library" && nextValue(i, argc, argv, value)) {
            userPresetLibrary = value;
        } else if (arg == "--user-preset" && nextValue(i, argc, argv, value)) {
            const std::optional<viz::PresetLibraryEntry> entry = viz::findUserPreset(userPresetLibrary, value);
            if (!entry) {
                std::cerr << "Unknown user preset: " << value << "\n";
                printUserPresets(userPresetLibrary);
                return 2;
            }
            std::string error;
            const std::optional<viz::VisualPreset> preset = viz::loadUserPresetEntry(*entry, error);
            if (!preset) {
                std::cerr << "Unable to load user preset: " << error << "\n";
                return 2;
            }
            options.settings = preset->settings;
            options.autoScene = preset->settings.autoScene;
            options.lookName = preset->name;
        } else if (arg == "--mode" && nextValue(i, argc, argv, value)) {
            if (const std::optional<viz::VisualMode> mode = viz::parseVisualMode(value)) {
                options.settings.mode = *mode;
            } else {
                std::cerr << "Unknown mode: " << value << "\n";
                return 2;
            }
        } else if (arg == "--palette" && nextValue(i, argc, argv, value)) {
            if (const std::optional<viz::Palette> palette = viz::parsePalette(value)) {
                options.settings.palette = *palette;
            } else {
                std::cerr << "Unknown palette: " << value << "\n";
                return 2;
            }
        } else if (arg == "--preset" && nextValue(i, argc, argv, value)) {
            std::string error;
            const std::optional<viz::VisualPreset> preset = viz::loadPreset(value, error);
            if (!preset) {
                std::cerr << "Unable to load preset: " << error << "\n";
                return 2;
            }
            options.settings = preset->settings;
            options.autoScene = preset->settings.autoScene;
            options.lookName.clear();
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            printUsage();
            return 2;
        }
    }

    if (listUserPresets) {
        printUserPresets(userPresetLibrary);
        return 0;
    }

    if (options.inputDirectory.empty() || options.outputDirectory.empty()) {
        printUsage();
        return 2;
    }

    viz::BatchExportResult result;
    std::string error;
    if (!viz::exportAudioBatch(options, result, error)) {
        std::cerr << "Batch export failed: " << error << "\n";
        return 1;
    }

    std::cout << "Batch export " << result.filesExported << "/" << result.filesDiscovered
              << " files to " << result.outputDirectory.string() << "\n";
    std::cout << "Index " << result.indexPath.string() << "\n";
    std::cout << "Manifest " << result.manifestPath.string() << "\n";
    if (result.filesFailed > 0) {
        std::cout << "Failures " << result.filesFailed << "\n";
        return 1;
    }
    return 0;
}
