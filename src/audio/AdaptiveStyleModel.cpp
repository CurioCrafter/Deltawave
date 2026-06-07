#include "Visualizer/Audio/AdaptiveStyleModel.hpp"

#include "Visualizer/Audio/AudioAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace viz {
namespace {

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

AudioFeatureVector vectorOf(float rms,
                            float bass,
                            float lowMid,
                            float mid,
                            float highMid,
                            float treble,
                            float centroid,
                            float width,
                            float bpm,
                            float onset)
{
    return AudioFeatureVector{{rms, bass, lowMid, mid, highMid, treble, centroid, width, bpm, onset}};
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

std::optional<AudioStyle> parseAudioStyle(std::string_view value)
{
    const std::string normalized = normalize(value);
    if (normalized == "silence") {
        return AudioStyle::Silence;
    }
    if (normalized == "ambient") {
        return AudioStyle::Ambient;
    }
    if (normalized == "techno") {
        return AudioStyle::Techno;
    }
    if (normalized == "bassheavy" || normalized == "bass") {
        return AudioStyle::BassHeavy;
    }
    if (normalized == "bright") {
        return AudioStyle::Bright;
    }
    if (normalized == "wide") {
        return AudioStyle::Wide;
    }
    return std::nullopt;
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

std::optional<AudioFeatureVector> parseFeatureVector(std::string_view value)
{
    AudioFeatureVector features;
    std::stringstream stream{std::string(value)};
    std::string token;
    std::size_t index = 0;
    while (std::getline(stream, token, ',')) {
        if (index >= features.values.size()) {
            return std::nullopt;
        }
        const std::optional<float> parsed = parseFloat(trim(token));
        if (!parsed) {
            return std::nullopt;
        }
        features.values[index++] = clamp01(*parsed);
    }
    if (index != features.values.size()) {
        return std::nullopt;
    }
    return features;
}

} // namespace

AdaptiveStyleModel::AdaptiveStyleModel()
{
    centroids_ = {{
        Centroid{AudioStyle::Silence, vectorOf(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), 0.0f},
        Centroid{AudioStyle::Ambient, vectorOf(0.13f, 0.16f, 0.24f, 0.32f, 0.24f, 0.22f, 0.28f, 0.32f, 0.0f, 0.05f), 0.0f},
        Centroid{AudioStyle::Techno, vectorOf(0.46f, 0.72f, 0.58f, 0.38f, 0.44f, 0.42f, 0.31f, 0.28f, 0.58f, 0.45f), 0.0f},
        Centroid{AudioStyle::BassHeavy, vectorOf(0.52f, 0.86f, 0.55f, 0.26f, 0.18f, 0.14f, 0.16f, 0.24f, 0.44f, 0.35f), 0.0f},
        Centroid{AudioStyle::Bright, vectorOf(0.34f, 0.22f, 0.28f, 0.42f, 0.72f, 0.84f, 0.68f, 0.22f, 0.36f, 0.28f), 0.0f},
        Centroid{AudioStyle::Wide, vectorOf(0.36f, 0.34f, 0.36f, 0.42f, 0.45f, 0.48f, 0.42f, 0.82f, 0.34f, 0.24f), 0.0f}
    }};
}

StylePrediction AdaptiveStyleModel::predict(const AudioMetrics& metrics) const
{
    if (metrics.rms < 0.006f && metrics.peak < 0.02f) {
        return StylePrediction{AudioStyle::Silence, 1.0f};
    }

    const AudioFeatureVector features = featuresFrom(metrics);
    float bestDistance = std::numeric_limits<float>::max();
    float secondDistance = std::numeric_limits<float>::max();
    AudioStyle bestStyle = AudioStyle::Ambient;

    for (const Centroid& centroid : centroids_) {
        const float d = distance(features, centroid.features);
        if (d < bestDistance) {
            secondDistance = bestDistance;
            bestDistance = d;
            bestStyle = centroid.style;
        } else if (d < secondDistance) {
            secondDistance = d;
        }
    }

    const float margin = secondDistance < std::numeric_limits<float>::max()
                             ? secondDistance - bestDistance
                             : 1.0f;
    const float confidence = clamp01(0.2f + margin * 2.4f + (1.0f - bestDistance) * 0.18f);
    return StylePrediction{bestStyle, confidence};
}

void AdaptiveStyleModel::learn(const AudioMetrics& metrics, AudioStyle style, float confidence)
{
    if (style == AudioStyle::Silence || confidence < 0.28f || metrics.rms < 0.01f) {
        return;
    }

    const AudioFeatureVector features = featuresFrom(metrics);
    for (Centroid& centroid : centroids_) {
        if (centroid.style != style) {
            continue;
        }

        const float learningRate = std::clamp(0.015f + confidence * 0.025f, 0.015f, 0.055f);
        for (std::size_t i = 0; i < centroid.features.values.size(); ++i) {
            centroid.features.values[i] =
                centroid.features.values[i] * (1.0f - learningRate) +
                features.values[i] * learningRate;
        }
        centroid.learnedWeight = std::min(1.0f, centroid.learnedWeight + learningRate);
        return;
    }
}

void AdaptiveStyleModel::resetAdaptation()
{
    *this = AdaptiveStyleModel{};
}

float AdaptiveStyleModel::learnedWeight(AudioStyle style) const
{
    for (const Centroid& centroid : centroids_) {
        if (centroid.style == style) {
            return centroid.learnedWeight;
        }
    }
    return 0.0f;
}

bool AdaptiveStyleModel::saveProfile(const std::filesystem::path& path, std::string& error) const
{
    error.clear();
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = "Unable to create style profile directory: " + ec.message();
            return false;
        }
    }

    std::ofstream output(path);
    if (!output) {
        error = "Unable to open style profile for writing.";
        return false;
    }

    output << "# Visualizer adaptive style profile v1\n";
    output << std::fixed << std::setprecision(6);
    for (const Centroid& centroid : centroids_) {
        output << "style=" << toString(centroid.style) << "\n";
        output << "learnedWeight=" << std::clamp(centroid.learnedWeight, 0.0f, 1.0f) << "\n";
        output << "features=";
        for (std::size_t i = 0; i < centroid.features.values.size(); ++i) {
            if (i > 0) {
                output << ",";
            }
            output << std::clamp(centroid.features.values[i], 0.0f, 1.0f);
        }
        output << "\n";
    }

    if (!output) {
        error = "Failed while writing style profile.";
        return false;
    }
    return true;
}

bool AdaptiveStyleModel::loadProfile(const std::filesystem::path& path, std::string& error)
{
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "Unable to open style profile.";
        return false;
    }

    AdaptiveStyleModel candidate;
    std::unordered_map<std::string, std::string> values;
    int loaded = 0;
    const auto commitBlock = [&]() -> bool {
        if (values.empty()) {
            return true;
        }

        const auto styleIt = values.find("style");
        const auto weightIt = values.find("learnedweight");
        const auto featuresIt = values.find("features");
        if (styleIt == values.end() || weightIt == values.end() || featuresIt == values.end()) {
            error = "Style profile block is missing style, learnedWeight, or features.";
            return false;
        }

        const std::optional<AudioStyle> style = parseAudioStyle(styleIt->second);
        const std::optional<float> weight = parseFloat(weightIt->second);
        const std::optional<AudioFeatureVector> features = parseFeatureVector(featuresIt->second);
        if (!style || !weight || !features) {
            error = "Style profile contains invalid style, weight, or feature values.";
            return false;
        }

        bool matched = false;
        for (Centroid& centroid : candidate.centroids_) {
            if (centroid.style != *style) {
                continue;
            }
            centroid.features = *features;
            centroid.learnedWeight = std::clamp(*weight, 0.0f, 1.0f);
            matched = true;
            ++loaded;
            break;
        }
        if (!matched) {
            error = "Style profile contains an unsupported style.";
            return false;
        }

        values.clear();
        return true;
    };

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
        const std::string key = normalize(trimmed.substr(0, equals));
        const std::string value = trim(trimmed.substr(equals + 1));
        if (key == "style" && !values.empty()) {
            if (!commitBlock()) {
                return false;
            }
        }
        values[key] = value;
    }

    if (!commitBlock()) {
        return false;
    }
    if (loaded == 0) {
        error = "Style profile did not contain any centroids.";
        return false;
    }

    *this = candidate;
    return true;
}

AudioFeatureVector AdaptiveStyleModel::featuresFrom(const AudioMetrics& metrics)
{
    return vectorOf(
        clamp01(metrics.rms * 1.7f),
        clamp01(metrics.bass),
        clamp01(metrics.lowMid),
        clamp01(metrics.mid),
        clamp01(metrics.highMid),
        clamp01(metrics.treble),
        clamp01(metrics.spectralCentroid),
        clamp01(metrics.stereoWidth),
        clamp01((metrics.bpm - 70.0f) / 140.0f),
        clamp01(metrics.onset * 4.0f + metrics.beatConfidence * 0.35f));
}

float AdaptiveStyleModel::distance(const AudioFeatureVector& a, const AudioFeatureVector& b)
{
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.values.size(); ++i) {
        const float delta = a.values[i] - b.values[i];
        sum += delta * delta;
    }
    return std::sqrt(sum / static_cast<float>(a.values.size()));
}

} // namespace viz
