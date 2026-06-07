#include "Visualizer/Visualization/PresetStore.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace viz {
namespace {

std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string normalize(std::string_view value)
{
    std::string output;
    output.reserve(value.size());
    for (char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
            output.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return output;
}

std::string pathSortKey(const std::filesystem::path& path)
{
    std::string output = path.generic_string();
    for (char& ch : output) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return output;
}

bool parseBool(std::string_view value, bool fallback)
{
    const std::string normalized = normalize(value);
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
        return false;
    }
    return fallback;
}

float parseFloat(std::string_view value, float fallback)
{
    try {
        return std::stof(std::string(value));
    } catch (...) {
        return fallback;
    }
}

VisualPreset makeCuratedPreset(std::string name,
                               VisualMode mode,
                               Palette palette,
                               float hueShift,
                               float complexity,
                               float intensity,
                               float speed,
                               bool trails = true,
                               bool autoScene = false,
                               float depth3D = 0.65f,
                               float colorImpact = 0.75f,
                               float objectDensity3D = 0.65f,
                               float interactionDepth = 0.65f,
                               float lightingGlow = 0.62f,
                               float scenePersonality = 0.5f,
                               float response3D = 1.0f)
{
    VisualPreset preset;
    preset.name = std::move(name);
    preset.settings.mode = mode;
    preset.settings.palette = palette;
    preset.settings.hueShift = hueShift;
    preset.settings.depth3D = depth3D;
    preset.settings.colorImpact = colorImpact;
    preset.settings.objectDensity3D = objectDensity3D;
    preset.settings.interactionDepth = interactionDepth;
    preset.settings.lightingGlow = lightingGlow;
    preset.settings.scenePersonality = scenePersonality;
    preset.settings.response3D = response3D;
    preset.settings.complexity = complexity;
    preset.settings.intensity = intensity;
    preset.settings.speed = speed;
    preset.settings.qualityScale = 1.0f;
    preset.settings.trails = trails;
    preset.settings.showHud = true;
    preset.settings.interactiveField = true;
    preset.settings.environmentReactive = true;
    preset.settings.adaptiveQuality = true;
    preset.settings.autoScene = autoScene;
    return preset;
}

} // namespace

std::optional<VisualMode> parseVisualMode(std::string_view value)
{
    const std::string normalized = normalize(value);
    if (normalized == "quantumtunnel" || normalized == "tunnel" || normalized == "1") {
        return VisualMode::QuantumTunnel;
    }
    if (normalized == "technomandala" || normalized == "mandala" || normalized == "2") {
        return VisualMode::TechnoMandala;
    }
    if (normalized == "lissajousmesh" || normalized == "mesh" || normalized == "3") {
        return VisualMode::LissajousMesh;
    }
    if (normalized == "frequencybloom" || normalized == "bloom" || normalized == "4") {
        return VisualMode::FrequencyBloom;
    }
    if (normalized == "fractalcathedral" || normalized == "cathedral" || normalized == "5") {
        return VisualMode::FractalCathedral;
    }
    if (normalized == "polyrhythmlattice" || normalized == "lattice" || normalized == "poly" || normalized == "6") {
        return VisualMode::PolyrhythmLattice;
    }
    if (normalized == "spectralorigami" || normalized == "origami" || normalized == "fold" || normalized == "7") {
        return VisualMode::SpectralOrigami;
    }
    if (normalized == "chromakaleidoscope" || normalized == "kaleidoscope" || normalized == "kaleid" ||
        normalized == "chroma" || normalized == "prism" || normalized == "8") {
        return VisualMode::ChromaKaleidoscope;
    }
    if (normalized == "hyperspacepolytope" || normalized == "hyperspace" || normalized == "polytope" ||
        normalized == "tesseract" || normalized == "4d" || normalized == "9") {
        return VisualMode::HyperspacePolytope;
    }
    if (normalized == "phaseweave" || normalized == "weave" || normalized == "phase" ||
        normalized == "flowfield" || normalized == "flow" || normalized == "field" || normalized == "10" ||
        normalized == "0") {
        return VisualMode::PhaseWeave;
    }
    if (normalized == "resonancetessellation" || normalized == "resonance" || normalized == "tessellation" ||
        normalized == "tessellate" || normalized == "tess" || normalized == "mosaic" || normalized == "triangulate" ||
        normalized == "triangulation" || normalized == "11") {
        return VisualMode::ResonanceTessellation;
    }
    if (normalized == "neuralconstellation" || normalized == "neural" || normalized == "constellation" ||
        normalized == "network" || normalized == "nodes" || normalized == "synapse" || normalized == "synaptic" ||
        normalized == "12") {
        return VisualMode::NeuralConstellation;
    }
    if (normalized == "cymaticinterference" || normalized == "cymatic" || normalized == "interference" ||
        normalized == "chladni" || normalized == "nodal" || normalized == "plate" || normalized == "13") {
        return VisualMode::CymaticInterference;
    }
    return std::nullopt;
}

std::optional<Palette> parsePalette(std::string_view value)
{
    const std::string normalized = normalize(value);
    if (normalized == "neonvoltage" || normalized == "neon" || normalized == "1") {
        return Palette::NeonVoltage;
    }
    if (normalized == "infraredchrome" || normalized == "infrared" || normalized == "2") {
        return Palette::InfraredChrome;
    }
    if (normalized == "acidaurora" || normalized == "acid" || normalized == "3") {
        return Palette::AcidAurora;
    }
    if (normalized == "monochromelaser" || normalized == "mono" || normalized == "4") {
        return Palette::MonochromeLaser;
    }
    if (normalized == "oceanicpulse" || normalized == "oceanic" || normalized == "5") {
        return Palette::OceanicPulse;
    }
    return std::nullopt;
}

const std::vector<VisualPreset>& curatedPresets()
{
    static const std::vector<VisualPreset> presets = {
        makeCuratedPreset("Warehouse Strobe",
                          VisualMode::PolyrhythmLattice,
                          Palette::InfraredChrome,
                          0.97f,
                          1.48f,
                          2.45f,
                          1.80f,
                          false,
                          false,
                          0.78f,
                          0.88f),
        makeCuratedPreset("Acid Geometry",
                          VisualMode::TechnoMandala,
                          Palette::AcidAurora,
                          0.15f,
                          1.66f,
                          2.10f,
                          1.35f),
        makeCuratedPreset("4D Hyperspace",
                          VisualMode::HyperspacePolytope,
                          Palette::NeonVoltage,
                          0.62f,
                          1.72f,
                          1.85f,
                          1.20f,
                          true,
                          false,
                          0.92f,
                          0.82f),
        makeCuratedPreset("Harmonic Glass",
                          VisualMode::ChromaKaleidoscope,
                          Palette::OceanicPulse,
                          0.52f,
                          1.28f,
                          1.30f,
                          0.90f,
                          true,
                          false,
                          0.70f,
                          0.78f),
        makeCuratedPreset("Fractal Cathedral",
                          VisualMode::FractalCathedral,
                          Palette::MonochromeLaser,
                          0.00f,
                          1.42f,
                          1.65f,
                          0.78f,
                          true,
                          false,
                          0.82f,
                          0.56f),
        makeCuratedPreset("Breakbeat Origami",
                          VisualMode::SpectralOrigami,
                          Palette::AcidAurora,
                          0.31f,
                          1.54f,
                          2.00f,
                          1.70f),
        makeCuratedPreset("Deep Bloom",
                          VisualMode::FrequencyBloom,
                          Palette::OceanicPulse,
                          0.58f,
                          1.04f,
                          1.12f,
                          0.72f),
        makeCuratedPreset("Phase Weave",
                          VisualMode::PhaseWeave,
                          Palette::NeonVoltage,
                          0.74f,
                          1.58f,
                          1.74f,
                          1.24f,
                          true,
                          false,
                          0.84f,
                          0.80f),
        makeCuratedPreset("Stereo Loom",
                          VisualMode::PhaseWeave,
                          Palette::OceanicPulse,
                          0.41f,
                          1.34f,
                          1.42f,
                          0.96f,
                          true,
                          false,
                          0.88f,
                          0.66f),
        makeCuratedPreset("Resonance Tessellation",
                          VisualMode::ResonanceTessellation,
                          Palette::AcidAurora,
                          0.22f,
                          1.50f,
                          1.68f,
                          1.12f,
                          true,
                          false,
                          0.76f,
                          0.84f),
        makeCuratedPreset("Neural Constellation",
                          VisualMode::NeuralConstellation,
                          Palette::NeonVoltage,
                          0.68f,
                          1.44f,
                          1.58f,
                          0.98f,
                          true,
                          false,
                          0.80f,
                          0.76f),
        makeCuratedPreset("Cymatic Interference",
                          VisualMode::CymaticInterference,
                          Palette::AcidAurora,
                          0.36f,
                          1.62f,
                          1.82f,
                          1.08f,
                          true,
                          false,
                          0.74f,
                          0.90f),
        makeCuratedPreset("Auto DJ Director",
                          VisualMode::QuantumTunnel,
                          Palette::NeonVoltage,
                          0.05f,
                          1.30f,
                          1.60f,
                          1.00f,
                          true,
                          true,
                          0.76f,
                          0.82f)
    };
    return presets;
}

std::optional<std::size_t> findCuratedPresetIndex(std::string_view value)
{
    const std::string wanted = normalize(value);
    if (wanted.empty()) {
        return std::nullopt;
    }

    const std::vector<VisualPreset>& presets = curatedPresets();
    for (std::size_t i = 0; i < presets.size(); ++i) {
        if (normalize(presets[i].name) == wanted) {
            return i;
        }
    }

    static constexpr std::array aliases = {
        std::pair<std::string_view, std::string_view>{"warehouse", "Warehouse Strobe"},
        std::pair<std::string_view, std::string_view>{"strobe", "Warehouse Strobe"},
        std::pair<std::string_view, std::string_view>{"acid", "Acid Geometry"},
        std::pair<std::string_view, std::string_view>{"geometry", "Acid Geometry"},
        std::pair<std::string_view, std::string_view>{"4d", "4D Hyperspace"},
        std::pair<std::string_view, std::string_view>{"hyperspace", "4D Hyperspace"},
        std::pair<std::string_view, std::string_view>{"glass", "Harmonic Glass"},
        std::pair<std::string_view, std::string_view>{"harmonic", "Harmonic Glass"},
        std::pair<std::string_view, std::string_view>{"cathedral", "Fractal Cathedral"},
        std::pair<std::string_view, std::string_view>{"breakbeat", "Breakbeat Origami"},
        std::pair<std::string_view, std::string_view>{"origami", "Breakbeat Origami"},
        std::pair<std::string_view, std::string_view>{"bloom", "Deep Bloom"},
        std::pair<std::string_view, std::string_view>{"deep", "Deep Bloom"},
        std::pair<std::string_view, std::string_view>{"phase", "Phase Weave"},
        std::pair<std::string_view, std::string_view>{"weave", "Phase Weave"},
        std::pair<std::string_view, std::string_view>{"flow", "Phase Weave"},
        std::pair<std::string_view, std::string_view>{"field", "Phase Weave"},
        std::pair<std::string_view, std::string_view>{"loom", "Stereo Loom"},
        std::pair<std::string_view, std::string_view>{"stereo", "Stereo Loom"},
        std::pair<std::string_view, std::string_view>{"resonance", "Resonance Tessellation"},
        std::pair<std::string_view, std::string_view>{"tessellation", "Resonance Tessellation"},
        std::pair<std::string_view, std::string_view>{"tess", "Resonance Tessellation"},
        std::pair<std::string_view, std::string_view>{"mosaic", "Resonance Tessellation"},
        std::pair<std::string_view, std::string_view>{"neural", "Neural Constellation"},
        std::pair<std::string_view, std::string_view>{"constellation", "Neural Constellation"},
        std::pair<std::string_view, std::string_view>{"network", "Neural Constellation"},
        std::pair<std::string_view, std::string_view>{"nodes", "Neural Constellation"},
        std::pair<std::string_view, std::string_view>{"cymatic", "Cymatic Interference"},
        std::pair<std::string_view, std::string_view>{"interference", "Cymatic Interference"},
        std::pair<std::string_view, std::string_view>{"chladni", "Cymatic Interference"},
        std::pair<std::string_view, std::string_view>{"nodal", "Cymatic Interference"},
        std::pair<std::string_view, std::string_view>{"auto", "Auto DJ Director"},
        std::pair<std::string_view, std::string_view>{"dj", "Auto DJ Director"},
        std::pair<std::string_view, std::string_view>{"director", "Auto DJ Director"}
    };
    for (const auto& [alias, presetName] : aliases) {
        if (wanted == normalize(alias)) {
            for (std::size_t i = 0; i < presets.size(); ++i) {
                if (presets[i].name == presetName) {
                    return i;
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<VisualPreset> findCuratedPreset(std::string_view value)
{
    const std::optional<std::size_t> index = findCuratedPresetIndex(value);
    if (!index) {
        return std::nullopt;
    }
    const std::vector<VisualPreset>& presets = curatedPresets();
    if (*index >= presets.size()) {
        return std::nullopt;
    }
    return presets[*index];
}

std::string sanitizePresetFileStem(std::string_view name)
{
    const std::string trimmed = trim(std::string(name));
    std::string output;
    output.reserve(trimmed.size());

    for (char ch : trimmed) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte >= 'a' && byte <= 'z') {
            output.push_back(static_cast<char>(byte));
        } else if (byte >= 'A' && byte <= 'Z') {
            output.push_back(static_cast<char>(byte - 'A' + 'a'));
        } else if (byte >= '0' && byte <= '9') {
            output.push_back(static_cast<char>(byte));
        } else if (!output.empty() && output.back() != '_') {
            output.push_back('_');
        }

        if (output.size() >= 64U) {
            break;
        }
    }

    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }

    return output.empty() ? "visual_preset" : output;
}

std::filesystem::path defaultUserPresetDirectory()
{
    return std::filesystem::current_path() / "profiles" / "presets";
}

std::vector<PresetLibraryEntry> scanUserPresetLibrary(const std::filesystem::path& directory)
{
    std::vector<PresetLibraryEntry> entries;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec) || !std::filesystem::is_directory(directory, ec)) {
        return entries;
    }

    std::filesystem::directory_iterator it(directory,
                                           std::filesystem::directory_options::skip_permission_denied,
                                           ec);
    const std::filesystem::directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
        std::error_code itemError;
        if (!it->is_regular_file(itemError) || itemError) {
            continue;
        }
        const std::filesystem::path path = it->path();
        if (normalize(path.extension().string()) != "vizpreset") {
            continue;
        }

        std::string error;
        std::optional<VisualPreset> preset = loadPreset(path, error);
        if (!preset) {
            continue;
        }
        PresetLibraryEntry entry;
        entry.name = preset->name.empty() ? path.stem().string() : preset->name;
        entry.path = path;
        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const PresetLibraryEntry& left, const PresetLibraryEntry& right) {
        const std::string leftName = normalize(left.name);
        const std::string rightName = normalize(right.name);
        if (leftName != rightName) {
            return leftName < rightName;
        }
        return pathSortKey(left.path) < pathSortKey(right.path);
    });
    return entries;
}

std::optional<PresetLibraryEntry> findUserPreset(const std::filesystem::path& directory, std::string_view value)
{
    const std::string wanted = normalize(value);
    const std::string wantedStem = normalize(std::filesystem::path(std::string(value)).stem().string());
    if (wanted.empty() && wantedStem.empty()) {
        return std::nullopt;
    }

    for (const PresetLibraryEntry& entry : scanUserPresetLibrary(directory)) {
        const std::string entryName = normalize(entry.name);
        const std::string entryStem = normalize(entry.path.stem().string());
        if ((!wanted.empty() && (entryName == wanted || entryStem == wanted)) ||
            (!wantedStem.empty() && entryStem == wantedStem)) {
            return entry;
        }
    }
    return std::nullopt;
}

bool savePreset(const std::filesystem::path& path,
                const VisualPreset& preset,
                std::string& error)
{
    error.clear();
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = "Unable to create preset directory: " + ec.message();
            return false;
        }
    }

    std::ofstream output(path);
    if (!output) {
        error = "Unable to open preset for writing.";
        return false;
    }

    output << "# Visualizer preset v1\n";
    output << "name=" << preset.name << "\n";
    output << "mode=" << toString(preset.settings.mode) << "\n";
    output << "palette=" << toString(preset.settings.palette) << "\n";
    output << "hueShift=" << preset.settings.hueShift << "\n";
    output << "depth3D=" << preset.settings.depth3D << "\n";
    output << "colorImpact=" << preset.settings.colorImpact << "\n";
    output << "objectDensity3D=" << preset.settings.objectDensity3D << "\n";
    output << "interactionDepth=" << preset.settings.interactionDepth << "\n";
    output << "lightingGlow=" << preset.settings.lightingGlow << "\n";
    output << "scenePersonality=" << preset.settings.scenePersonality << "\n";
    output << "response3D=" << preset.settings.response3D << "\n";
    output << "complexity=" << preset.settings.complexity << "\n";
    output << "intensity=" << preset.settings.intensity << "\n";
    output << "speed=" << preset.settings.speed << "\n";
    output << "qualityScale=" << preset.settings.qualityScale << "\n";
    output << "trails=" << (preset.settings.trails ? "true" : "false") << "\n";
    output << "showHud=" << (preset.settings.showHud ? "true" : "false") << "\n";
    output << "interactiveField=" << (preset.settings.interactiveField ? "true" : "false") << "\n";
    output << "environmentReactive=" << (preset.settings.environmentReactive ? "true" : "false") << "\n";
    output << "adaptiveQuality=" << (preset.settings.adaptiveQuality ? "true" : "false") << "\n";
    output << "autoScene=" << (preset.settings.autoScene ? "true" : "false") << "\n";
    if (!output) {
        error = "Failed while writing preset.";
        return false;
    }
    return true;
}

std::optional<VisualPreset> loadPreset(const std::filesystem::path& path, std::string& error)
{
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "Unable to open preset.";
        return std::nullopt;
    }

    std::unordered_map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        values[normalize(trimmed.substr(0, equals))] = trim(trimmed.substr(equals + 1));
    }

    VisualPreset preset;
    if (const auto it = values.find("name"); it != values.end()) {
        preset.name = it->second;
    }
    if (const auto it = values.find("mode"); it != values.end()) {
        if (const std::optional<VisualMode> mode = parseVisualMode(it->second)) {
            preset.settings.mode = *mode;
        }
    }
    if (const auto it = values.find("palette"); it != values.end()) {
        if (const std::optional<Palette> palette = parsePalette(it->second)) {
            preset.settings.palette = *palette;
        }
    }
    if (const auto it = values.find("hueshift"); it != values.end()) {
        preset.settings.hueShift = std::clamp(parseFloat(it->second, preset.settings.hueShift), 0.0f, 1.0f);
    }
    if (const auto it = values.find("depth3d"); it != values.end()) {
        preset.settings.depth3D = std::clamp(parseFloat(it->second, preset.settings.depth3D), 0.0f, 1.0f);
    }
    if (const auto it = values.find("colorimpact"); it != values.end()) {
        preset.settings.colorImpact = std::clamp(parseFloat(it->second, preset.settings.colorImpact), 0.0f, 1.0f);
    }
    if (const auto it = values.find("objectdensity3d"); it != values.end()) {
        preset.settings.objectDensity3D =
            std::clamp(parseFloat(it->second, preset.settings.objectDensity3D), 0.0f, 1.0f);
    }
    if (const auto it = values.find("interactiondepth"); it != values.end()) {
        preset.settings.interactionDepth =
            std::clamp(parseFloat(it->second, preset.settings.interactionDepth), 0.0f, 1.0f);
    }
    if (const auto it = values.find("lightingglow"); it != values.end()) {
        preset.settings.lightingGlow =
            std::clamp(parseFloat(it->second, preset.settings.lightingGlow), 0.0f, 1.0f);
    }
    if (const auto it = values.find("scenepersonality"); it != values.end()) {
        preset.settings.scenePersonality =
            std::clamp(parseFloat(it->second, preset.settings.scenePersonality), 0.0f, 1.0f);
    }
    if (const auto it = values.find("response3d"); it != values.end()) {
        preset.settings.response3D = std::clamp(parseFloat(it->second, preset.settings.response3D), 0.0f, 1.0f);
    }
    if (const auto it = values.find("complexity"); it != values.end()) {
        preset.settings.complexity = std::clamp(parseFloat(it->second, preset.settings.complexity), 0.35f, 1.8f);
    }
    if (const auto it = values.find("intensity"); it != values.end()) {
        preset.settings.intensity = std::clamp(parseFloat(it->second, preset.settings.intensity), 0.15f, 4.0f);
    }
    if (const auto it = values.find("speed"); it != values.end()) {
        preset.settings.speed = std::clamp(parseFloat(it->second, preset.settings.speed), 0.1f, 4.0f);
    }
    if (const auto it = values.find("qualityscale"); it != values.end()) {
        preset.settings.qualityScale = std::clamp(parseFloat(it->second, preset.settings.qualityScale), 0.45f, 1.0f);
    }
    if (const auto it = values.find("trails"); it != values.end()) {
        preset.settings.trails = parseBool(it->second, preset.settings.trails);
    }
    if (const auto it = values.find("showhud"); it != values.end()) {
        preset.settings.showHud = parseBool(it->second, preset.settings.showHud);
    }
    if (const auto it = values.find("interactivefield"); it != values.end()) {
        preset.settings.interactiveField = parseBool(it->second, preset.settings.interactiveField);
    }
    if (const auto it = values.find("environmentreactive"); it != values.end()) {
        preset.settings.environmentReactive = parseBool(it->second, preset.settings.environmentReactive);
    }
    if (const auto it = values.find("adaptivequality"); it != values.end()) {
        preset.settings.adaptiveQuality = parseBool(it->second, preset.settings.adaptiveQuality);
    }
    if (const auto it = values.find("autoscene"); it != values.end()) {
        preset.settings.autoScene = parseBool(it->second, preset.settings.autoScene);
    }

    return preset;
}

bool saveUserPreset(const std::filesystem::path& directory,
                    const VisualPreset& preset,
                    std::filesystem::path& savedPath,
                    std::string& error)
{
    error.clear();
    savedPath.clear();

    const std::filesystem::path targetDirectory = directory.empty() ? defaultUserPresetDirectory() : directory;
    const std::string stem = sanitizePresetFileStem(preset.name);
    for (int suffix = 0; suffix < 10000; ++suffix) {
        std::string fileStem = stem;
        if (suffix > 0) {
            fileStem += "_" + std::to_string(suffix + 1);
        }

        const std::filesystem::path candidate = targetDirectory / (fileStem + ".vizpreset");
        std::error_code existsError;
        if (std::filesystem::exists(candidate, existsError)) {
            if (existsError) {
                error = "Unable to inspect preset path: " + existsError.message();
                return false;
            }
            continue;
        }
        if (existsError) {
            error = "Unable to inspect preset path: " + existsError.message();
            return false;
        }

        VisualPreset savedPreset = preset;
        if (savedPreset.name.empty()) {
            savedPreset.name = "Untitled";
        }
        if (!savePreset(candidate, savedPreset, error)) {
            return false;
        }
        savedPath = candidate;
        return true;
    }

    error = "Unable to find an available preset file name.";
    return false;
}

std::optional<VisualPreset> loadUserPresetEntry(const PresetLibraryEntry& entry, std::string& error)
{
    if (entry.path.empty()) {
        error = "Preset library entry has no path.";
        return std::nullopt;
    }
    return loadPreset(entry.path, error);
}

} // namespace viz
