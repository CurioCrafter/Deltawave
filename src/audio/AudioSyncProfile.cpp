#include "Visualizer/Audio/AudioSyncProfile.hpp"

#include "Visualizer/Audio/AudioAnalyzer.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace viz {
namespace {

float clampSensitivity(float value)
{
    return std::clamp(value, 0.65f, 1.45f);
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
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

std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::optional<float> parseFloat(std::string_view value)
{
    try {
        std::size_t consumed = 0;
        const float parsed = std::stof(std::string(value), &consumed);
        if (consumed == 0) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

float smooth(float current, float target, float rate)
{
    return current * (1.0f - rate) + target * rate;
}

} // namespace

float AudioSyncProfile::beatThresholdScale() const noexcept
{
    return 1.0f / clampSensitivity(beatSensitivity_);
}

float AudioSyncProfile::beatConfidenceGain() const noexcept
{
    return std::clamp(0.82f + (beatSensitivity_ * 0.18f), 0.85f, 1.12f);
}

float AudioSyncProfile::sectionThresholdScale() const noexcept
{
    return 1.0f / clampSensitivity(sectionSensitivity_);
}

float AudioSyncProfile::sectionConfidenceGain() const noexcept
{
    return std::clamp(0.84f + (sectionSensitivity_ * 0.16f), 0.88f, 1.15f);
}

void AudioSyncProfile::reset()
{
    beatSensitivity_ = 1.0f;
    sectionSensitivity_ = 1.0f;
    learnedWeight_ = 0.0f;
}

void AudioSyncProfile::learnBeat(const AudioMetrics& metrics, float lowEnergy, float threshold)
{
    if (metrics.rms < 0.008f || metrics.peak < 0.02f) {
        return;
    }

    float target = 1.0f;
    const float ratio = threshold > 0.001f ? lowEnergy / threshold : 1.0f;
    if (metrics.beat && metrics.beatConfidence > 0.35f) {
        target = std::clamp(1.0f + (1.32f - ratio) * 0.16f, 0.82f, 1.22f);
    } else if (!metrics.beat && metrics.onset > 0.035f && lowEnergy > threshold * 0.82f) {
        target = 1.12f;
    } else if (metrics.beatConfidence < 0.03f && metrics.onset < 0.008f) {
        target = 0.98f;
    }

    const float rate = 0.004f + metrics.beatConfidence * 0.008f + metrics.onset * 0.018f;
    beatSensitivity_ = clampSensitivity(smooth(beatSensitivity_, target, std::clamp(rate, 0.003f, 0.025f)));
    learnedWeight_ = clamp01(learnedWeight_ + std::clamp(rate * 0.4f, 0.001f, 0.008f));
}

void AudioSyncProfile::learnSection(const AudioMetrics& metrics)
{
    if (metrics.rms < 0.008f || metrics.peak < 0.02f) {
        return;
    }

    float target = 1.0f;
    if ((metrics.dropIntensity > 0.28f || metrics.phraseIntensity > 0.32f) && metrics.sectionConfidence < 0.42f) {
        target = 1.12f;
    } else if ((metrics.section == ArrangementSection::Drop || metrics.section == ArrangementSection::Build) &&
               metrics.sectionConfidence > 0.62f) {
        target = 1.02f;
    } else if (metrics.section == ArrangementSection::Groove && metrics.sectionConfidence > 0.65f) {
        target = 0.98f;
    }

    const float activity = std::max(metrics.dropIntensity, metrics.phraseIntensity);
    const float rate = std::clamp(0.003f + activity * 0.012f + metrics.sectionConfidence * 0.004f, 0.003f, 0.022f);
    sectionSensitivity_ = clampSensitivity(smooth(sectionSensitivity_, target, rate));
    learnedWeight_ = clamp01(learnedWeight_ + std::clamp(rate * 0.32f, 0.001f, 0.007f));
}

bool AudioSyncProfile::saveProfile(const std::filesystem::path& path, std::string& error) const
{
    error.clear();
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = "Unable to create sync profile directory: " + ec.message();
            return false;
        }
    }

    std::ofstream output(path);
    if (!output) {
        error = "Unable to open sync profile for writing.";
        return false;
    }

    output << "# Visualizer adaptive sync profile v1\n";
    output << std::fixed << std::setprecision(6);
    output << "beatSensitivity=" << clampSensitivity(beatSensitivity_) << "\n";
    output << "sectionSensitivity=" << clampSensitivity(sectionSensitivity_) << "\n";
    output << "learnedWeight=" << clamp01(learnedWeight_) << "\n";

    if (!output) {
        error = "Failed while writing sync profile.";
        return false;
    }
    return true;
}

bool AudioSyncProfile::loadProfile(const std::filesystem::path& path, std::string& error)
{
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "Unable to open sync profile.";
        return false;
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

    const auto beatIt = values.find("beatsensitivity");
    const auto sectionIt = values.find("sectionsensitivity");
    const auto weightIt = values.find("learnedweight");
    if (beatIt == values.end() || sectionIt == values.end() || weightIt == values.end()) {
        error = "Sync profile is missing beatSensitivity, sectionSensitivity, or learnedWeight.";
        return false;
    }

    const std::optional<float> beat = parseFloat(beatIt->second);
    const std::optional<float> section = parseFloat(sectionIt->second);
    const std::optional<float> weight = parseFloat(weightIt->second);
    if (!beat || !section || !weight) {
        error = "Sync profile contains invalid numeric values.";
        return false;
    }

    AudioSyncProfile candidate;
    candidate.beatSensitivity_ = clampSensitivity(*beat);
    candidate.sectionSensitivity_ = clampSensitivity(*section);
    candidate.learnedWeight_ = clamp01(*weight);
    *this = candidate;
    return true;
}

} // namespace viz
