#include "Visualizer/Audio/AudioAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace viz {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::array<float, kChromaBinCount> kMajorProfile{
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};
constexpr std::array<float, kChromaBinCount> kMinorProfile{
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float mean(const std::deque<float>& values)
{
    if (values.empty()) {
        return 0.0f;
    }
    return std::accumulate(values.begin(), values.end(), 0.0f) /
           static_cast<float>(values.size());
}

float standardDeviation(const std::deque<float>& values, float average)
{
    if (values.size() < 2) {
        return 0.0f;
    }

    float variance = 0.0f;
    for (float value : values) {
        const float delta = value - average;
        variance += delta * delta;
    }
    return std::sqrt(variance / static_cast<float>(values.size() - 1));
}

float bandAverage(const std::array<float, kSpectrumBinCount>& spectrum,
                  const std::array<float, kSpectrumBinCount>& frequencies,
                  float low,
                  float high)
{
    float sum = 0.0f;
    float count = 0.0f;
    for (std::size_t i = 0; i < spectrum.size(); ++i) {
        if (frequencies[i] >= low && frequencies[i] < high) {
            sum += spectrum[i];
            count += 1.0f;
        }
    }
    return count > 0.0f ? sum / count : 0.0f;
}

float pitchClassCorrelation(const std::array<float, kChromaBinCount>& chroma,
                            const std::array<float, kChromaBinCount>& profile,
                            int root)
{
    float score = 0.0f;
    for (std::size_t i = 0; i < kChromaBinCount; ++i) {
        const std::size_t rotated = (i + static_cast<std::size_t>(root)) % kChromaBinCount;
        score += chroma[rotated] * profile[i];
    }
    return score;
}

struct SectionCandidate {
    ArrangementSection section = ArrangementSection::Silence;
    float confidence = 0.0f;
};

} // namespace

std::string_view toString(AudioStyle style)
{
    switch (style) {
    case AudioStyle::Silence:
        return "Silence";
    case AudioStyle::Ambient:
        return "Ambient";
    case AudioStyle::Techno:
        return "Techno";
    case AudioStyle::BassHeavy:
        return "Bass Heavy";
    case AudioStyle::Bright:
        return "Bright";
    case AudioStyle::Wide:
        return "Wide";
    }
    return "Unknown";
}

std::string_view toString(MusicalMode mode)
{
    switch (mode) {
    case MusicalMode::Unknown:
        return "Unknown";
    case MusicalMode::Major:
        return "Major";
    case MusicalMode::Minor:
        return "Minor";
    }
    return "Unknown";
}

std::string_view toString(ArrangementSection section)
{
    switch (section) {
    case ArrangementSection::Silence:
        return "Silence";
    case ArrangementSection::Breakdown:
        return "Breakdown";
    case ArrangementSection::Build:
        return "Build";
    case ArrangementSection::Drop:
        return "Drop";
    case ArrangementSection::Groove:
        return "Groove";
    }
    return "Unknown";
}

std::string_view keyName(int keyIndex)
{
    if (keyIndex < 0) {
        return "-";
    }
    switch ((keyIndex % 12 + 12) % 12) {
    case 0:
        return "C";
    case 1:
        return "C#";
    case 2:
        return "D";
    case 3:
        return "D#";
    case 4:
        return "E";
    case 5:
        return "F";
    case 6:
        return "F#";
    case 7:
        return "G";
    case 8:
        return "G#";
    case 9:
        return "A";
    case 10:
        return "A#";
    case 11:
        return "B";
    default:
        return "-";
    }
}

AudioAnalyzer::AudioAnalyzer(int sampleRate, int channelCount)
{
    configure(sampleRate, channelCount);
}

void AudioAnalyzer::configure(int sampleRate, int channelCount)
{
    sampleRate_ = std::max(8000, sampleRate);
    channelCount_ = std::max(1, channelCount);
    reset();
}

void AudioAnalyzer::reset()
{
    last_ = {};
    lowEnergyHistory_.clear();
    shortEnergyHistory_.clear();
    longEnergyHistory_.clear();
    beatTimes_.clear();
    lastBeatTime_ = -10.0;
    beatIntervalSeconds_ = 0.0;
    sectionStartSeconds_ = 0.0;
    lastSectionSwitchSeconds_ = -10.0;
    lastDownbeatTime_ = -10.0;
    lastPhraseBoundaryTime_ = -10.0;
    previousLowEnergy_ = 0.0f;
    beatCountInBar_ = 0;
    barsSincePhraseBoundary_ = 0;
    previousBandEnergies_ = {};
    currentSection_ = ArrangementSection::Silence;
}

void AudioAnalyzer::resetStyleProfile()
{
    styleModel_.resetAdaptation();
}

void AudioAnalyzer::resetSyncProfile()
{
    syncProfile_.reset();
}

bool AudioAnalyzer::saveStyleProfile(const std::filesystem::path& path, std::string& error) const
{
    return styleModel_.saveProfile(path, error);
}

bool AudioAnalyzer::loadStyleProfile(const std::filesystem::path& path, std::string& error)
{
    return styleModel_.loadProfile(path, error);
}

bool AudioAnalyzer::saveSyncProfile(const std::filesystem::path& path, std::string& error) const
{
    return syncProfile_.saveProfile(path, error);
}

bool AudioAnalyzer::loadSyncProfile(const std::filesystem::path& path, std::string& error)
{
    return syncProfile_.loadProfile(path, error);
}

float AudioAnalyzer::learnedStyleWeight(AudioStyle style) const
{
    return styleModel_.learnedWeight(style);
}

float AudioAnalyzer::syncProfileLearnedWeight() const
{
    return syncProfile_.learnedWeight();
}

float AudioAnalyzer::beatSensitivity() const
{
    return syncProfile_.beatSensitivity();
}

float AudioAnalyzer::sectionSensitivity() const
{
    return syncProfile_.sectionSensitivity();
}

AudioMetrics AudioAnalyzer::analyzeInterleaved(const float* interleavedSamples,
                                               std::size_t frameCount,
                                               double timeSeconds)
{
    AudioMetrics metrics{};
    metrics.timeSeconds = timeSeconds;

    if (interleavedSamples == nullptr || frameCount == 0) {
        last_ = metrics;
        return last_;
    }

    double squareSum = 0.0;
    float peak = 0.0f;
    double stereoDelta = 0.0;
    double stereoReference = 0.0;

    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        for (int channel = 0; channel < channelCount_; ++channel) {
            const float sample = interleavedSamples[(frame * static_cast<std::size_t>(channelCount_)) +
                                                    static_cast<std::size_t>(channel)];
            peak = std::max(peak, std::abs(sample));
            squareSum += static_cast<double>(sample) * static_cast<double>(sample);
        }

        if (channelCount_ >= 2) {
            const float left = interleavedSamples[frame * static_cast<std::size_t>(channelCount_)];
            const float right = interleavedSamples[(frame * static_cast<std::size_t>(channelCount_)) + 1U];
            stereoDelta += std::abs(left - right);
            stereoReference += std::abs(left) + std::abs(right);
        }
    }

    metrics.rms = static_cast<float>(
        std::sqrt(squareSum / static_cast<double>(frameCount * static_cast<std::size_t>(channelCount_))));
    metrics.peak = clamp01(peak);
    metrics.stereoWidth = stereoReference > 0.00001
                              ? clamp01(static_cast<float>(stereoDelta / stereoReference))
                              : 0.0f;

    constexpr std::size_t windowSize = 1024;
    const std::size_t analysisFrames = std::min(frameCount, windowSize);
    const std::size_t startFrame = frameCount - analysisFrames;
    monoScratch_.assign(windowSize, 0.0f);

    for (std::size_t i = 0; i < analysisFrames; ++i) {
        float mono = 0.0f;
        for (int channel = 0; channel < channelCount_; ++channel) {
            mono += interleavedSamples[((startFrame + i) * static_cast<std::size_t>(channelCount_)) +
                                       static_cast<std::size_t>(channel)];
        }
        monoScratch_[i] = mono / static_cast<float>(channelCount_);
    }

    std::array<float, kSpectrumBinCount> frequencies{};
    const float lowFrequency = 30.0f;
    const float highFrequency = std::min(18000.0f, static_cast<float>(sampleRate_) * 0.48f);
    const float logLow = std::log(lowFrequency);
    const float logHigh = std::log(highFrequency);
    double weightedFrequency = 0.0;
    double weightedMagnitude = 0.0;

    for (std::size_t bin = 0; bin < kSpectrumBinCount; ++bin) {
        const float t = static_cast<float>(bin) / static_cast<float>(kSpectrumBinCount - 1);
        const float frequency = std::exp(logLow + (logHigh - logLow) * t);
        frequencies[bin] = frequency;

        const float omega = 2.0f * kPi * frequency / static_cast<float>(sampleRate_);
        double real = 0.0;
        double imaginary = 0.0;
        for (std::size_t i = 0; i < windowSize; ++i) {
            const float window = 0.5f - 0.5f * std::cos((2.0f * kPi * static_cast<float>(i)) /
                                                        static_cast<float>(windowSize - 1));
            const float sample = monoScratch_[i] * window;
            const float phase = omega * static_cast<float>(i);
            real += sample * std::cos(phase);
            imaginary -= sample * std::sin(phase);
        }

        const float magnitude = static_cast<float>(
            std::sqrt((real * real) + (imaginary * imaginary)) / static_cast<double>(windowSize));
        const float normalized = clamp01(std::sqrt(magnitude) * 4.5f);
        metrics.spectrum[bin] = (last_.spectrum[bin] * 0.58f) + (normalized * 0.42f);
        metrics.spectralFlux += std::max(0.0f, metrics.spectrum[bin] - last_.spectrum[bin]);
        weightedFrequency += static_cast<double>(frequency) * normalized;
        weightedMagnitude += normalized;
    }
    metrics.spectralFlux = clamp01((metrics.spectralFlux / static_cast<float>(kSpectrumBinCount)) * 4.0f);

    metrics.bass = bandAverage(metrics.spectrum, frequencies, 30.0f, 160.0f);
    metrics.lowMid = bandAverage(metrics.spectrum, frequencies, 160.0f, 500.0f);
    metrics.mid = bandAverage(metrics.spectrum, frequencies, 500.0f, 2000.0f);
    metrics.highMid = bandAverage(metrics.spectrum, frequencies, 2000.0f, 6000.0f);
    metrics.treble = bandAverage(metrics.spectrum, frequencies, 6000.0f, highFrequency + 1.0f);
    metrics.spectralCentroid = weightedMagnitude > 0.00001
                                   ? clamp01(static_cast<float>((weightedFrequency / weightedMagnitude) /
                                                                static_cast<double>(highFrequency)))
                                   : 0.0f;
    updateChromaMetrics(metrics, frequencies);

    const float lowEnergy = clamp01((metrics.bass * 0.72f) + (metrics.lowMid * 0.28f) + (metrics.rms * 0.35f));
    metrics.onset = std::max(0.0f, lowEnergy - previousLowEnergy_);
    previousLowEnergy_ = (previousLowEnergy_ * 0.82f) + (lowEnergy * 0.18f);

    updateBeatState(metrics, lowEnergy, timeSeconds);
    updateAdvancedSyncMetrics(metrics);
    StylePrediction prediction = styleModel_.predict(metrics);
    const AudioStyle heuristic = heuristicStyle(metrics);
    if (prediction.confidence < 0.34f && heuristic != AudioStyle::Ambient) {
        prediction.style = heuristic;
        prediction.confidence = std::max(prediction.confidence, 0.42f);
    }
    metrics.style = prediction.style;
    metrics.styleConfidence = prediction.confidence;
    styleModel_.learn(metrics, metrics.style, metrics.styleConfidence);
    metrics.styleAdaptation = styleModel_.learnedWeight(metrics.style);
    metrics.syncAdaptation = syncProfile_.learnedWeight();
    metrics.beatSensitivity = syncProfile_.beatSensitivity();
    metrics.sectionSensitivity = syncProfile_.sectionSensitivity();
    last_ = metrics;
    return last_;
}

void AudioAnalyzer::updateBeatState(AudioMetrics& metrics, float lowEnergy, double timeSeconds)
{
    const float historyMean = mean(lowEnergyHistory_);
    const float historyStdDev = standardDeviation(lowEnergyHistory_, historyMean);
    const float thresholdScale = syncProfile_.beatThresholdScale();
    const float threshold = std::max(0.055f * thresholdScale,
                                     historyMean + (historyStdDev * 1.15f * thresholdScale) +
                                         (0.025f * thresholdScale));
    const double secondsSinceBeat = timeSeconds - lastBeatTime_;

    metrics.beatConfidence = clamp01((lowEnergy - threshold) * 5.0f * syncProfile_.beatConfidenceGain());
    metrics.beat = lowEnergy > threshold &&
                   metrics.onset > 0.015f * thresholdScale &&
                   metrics.rms > 0.008f &&
                   secondsSinceBeat > 0.18;

    if (metrics.beat) {
        updateBpm(metrics, timeSeconds);
        lastBeatTime_ = timeSeconds;
    } else if (!beatTimes_.empty() && timeSeconds - beatTimes_.back() < 4.0) {
        metrics.bpm = last_.bpm;
    }

    lowEnergyHistory_.push_back(lowEnergy);
    while (lowEnergyHistory_.size() > 90) {
        lowEnergyHistory_.pop_front();
    }

    if (metrics.beat) {
        metrics.beatPhase = 0.0f;
    } else if (beatIntervalSeconds_ > 0.001 && lastBeatTime_ > 0.0) {
        metrics.beatPhase = clamp01(static_cast<float>((timeSeconds - lastBeatTime_) / beatIntervalSeconds_));
    }
    syncProfile_.learnBeat(metrics, lowEnergy, threshold);
}

void AudioAnalyzer::updateBpm(AudioMetrics& metrics, double timeSeconds)
{
    if (!beatTimes_.empty()) {
        const double interval = timeSeconds - beatTimes_.back();
        if (interval >= 0.25 && interval <= 1.5) {
            beatTimes_.push_back(timeSeconds);
        } else if (interval > 1.5) {
            beatTimes_.clear();
            beatTimes_.push_back(timeSeconds);
        }
    } else {
        beatTimes_.push_back(timeSeconds);
    }

    while (beatTimes_.size() > 10) {
        beatTimes_.pop_front();
    }

    if (beatTimes_.size() < 3) {
        metrics.bpm = last_.bpm;
        return;
    }

    std::vector<double> intervals;
    intervals.reserve(beatTimes_.size() - 1);
    for (std::size_t i = 1; i < beatTimes_.size(); ++i) {
        intervals.push_back(beatTimes_[i] - beatTimes_[i - 1]);
    }

    std::sort(intervals.begin(), intervals.end());
    const double medianInterval = intervals[intervals.size() / 2];
    const float bpm = static_cast<float>(60.0 / medianInterval);
    beatIntervalSeconds_ = medianInterval;
    metrics.bpm = (bpm >= 40.0f && bpm <= 240.0f) ? bpm : last_.bpm;
}

void AudioAnalyzer::updateChromaMetrics(AudioMetrics& metrics,
                                        const std::array<float, kSpectrumBinCount>& frequencies)
{
    float tonalSum = 0.0f;
    float chromaPeak = 0.0f;
    for (std::size_t bin = 0; bin < kSpectrumBinCount; ++bin) {
        const float frequency = frequencies[bin];
        if (frequency < 55.0f || frequency > 5200.0f) {
            continue;
        }

        const float magnitude = metrics.spectrum[bin];
        if (magnitude <= 0.0001f) {
            continue;
        }

        const float midi = 69.0f + 12.0f * (std::log(frequency / 440.0f) / std::log(2.0f));
        const int pitchClass = ((static_cast<int>(std::lround(midi)) % 12) + 12) % 12;
        const float tuningOffset = std::abs(midi - std::round(midi));
        const float tuningWeight = std::clamp(1.0f - tuningOffset * 0.45f, 0.55f, 1.0f);
        const float weight = std::sqrt(magnitude) * tuningWeight;
        metrics.chroma[static_cast<std::size_t>(pitchClass)] += weight;
        tonalSum += weight;
    }

    if (tonalSum <= 0.0001f) {
        metrics.keyIndex = -1;
        metrics.keyMode = MusicalMode::Unknown;
        metrics.keyConfidence = 0.0f;
        metrics.harmonicEnergy = 0.0f;
        return;
    }

    for (std::size_t i = 0; i < metrics.chroma.size(); ++i) {
        metrics.chroma[i] = (last_.chroma[i] * 0.5f) + ((metrics.chroma[i] / tonalSum) * 0.5f);
        chromaPeak = std::max(chromaPeak, metrics.chroma[i]);
    }
    metrics.harmonicEnergy = clamp01(chromaPeak * 4.0f);

    float bestScore = -1.0f;
    float secondScore = -1.0f;
    int bestRoot = -1;
    MusicalMode bestMode = MusicalMode::Unknown;
    for (int root = 0; root < 12; ++root) {
        const std::size_t tonic = static_cast<std::size_t>(root);
        const float majorTriad = metrics.chroma[tonic] * 3.2f +
                                 metrics.chroma[(tonic + 4U) % kChromaBinCount] * 1.8f +
                                 metrics.chroma[(tonic + 7U) % kChromaBinCount] * 2.1f;
        const float minorTriad = metrics.chroma[tonic] * 3.2f +
                                 metrics.chroma[(tonic + 3U) % kChromaBinCount] * 1.8f +
                                 metrics.chroma[(tonic + 7U) % kChromaBinCount] * 2.1f;
        const float majorScore = pitchClassCorrelation(metrics.chroma, kMajorProfile, root) + majorTriad;
        const float minorScore = pitchClassCorrelation(metrics.chroma, kMinorProfile, root) + minorTriad;
        if (majorScore > bestScore) {
            secondScore = bestScore;
            bestScore = majorScore;
            bestRoot = root;
            bestMode = MusicalMode::Major;
        } else {
            secondScore = std::max(secondScore, majorScore);
        }
        if (minorScore > bestScore) {
            secondScore = bestScore;
            bestScore = minorScore;
            bestRoot = root;
            bestMode = MusicalMode::Minor;
        } else {
            secondScore = std::max(secondScore, minorScore);
        }
    }

    const float separation = bestScore > 0.0001f ? (bestScore - std::max(0.0f, secondScore)) / bestScore : 0.0f;
    metrics.keyIndex = bestRoot;
    metrics.keyMode = bestMode;
    metrics.keyConfidence = clamp01((separation * 5.5f + metrics.harmonicEnergy * 0.45f) *
                                    std::clamp(metrics.rms * 3.0f, 0.35f, 1.0f));
    if (metrics.keyConfidence < 0.08f) {
        metrics.keyIndex = -1;
        metrics.keyMode = MusicalMode::Unknown;
    }
}

void AudioAnalyzer::updateAdvancedSyncMetrics(AudioMetrics& metrics)
{
    const std::array<float, kAudioBandCount> bands{
        metrics.bass,
        metrics.lowMid,
        metrics.mid,
        metrics.highMid,
        metrics.treble
    };

    for (std::size_t i = 0; i < bands.size(); ++i) {
        const float delta = std::max(0.0f, bands[i] - previousBandEnergies_[i]);
        metrics.bandOnsets[i] = clamp01(delta * 3.4f + metrics.spectralFlux * 0.18f);
        previousBandEnergies_[i] = (previousBandEnergies_[i] * 0.72f) + (bands[i] * 0.28f);
    }

    const float longAverage = mean(longEnergyHistory_);
    const float shortAverage = mean(shortEnergyHistory_);
    const float lowBandHit = clamp01(metrics.bandOnsets[0] * 0.6f + metrics.bandOnsets[1] * 0.4f);
    const bool enoughHistory = longEnergyHistory_.size() >= 24;
    if (enoughHistory && longAverage > 0.004f) {
        const float lift = std::max(0.0f, metrics.rms - longAverage);
        const float shortLift = std::max(0.0f, shortAverage - longAverage);
        metrics.dropIntensity = clamp01(lift * 4.4f + lowBandHit * 0.48f + metrics.beatConfidence * 0.24f);
        metrics.phraseIntensity = clamp01(shortLift * 3.8f + metrics.spectralFlux * 0.62f + metrics.beatConfidence * 0.16f);
    } else {
        metrics.dropIntensity = clamp01(lowBandHit * 0.35f + metrics.beatConfidence * 0.18f);
        metrics.phraseIntensity = clamp01(metrics.spectralFlux * 0.45f);
    }

    updateArrangementSection(metrics, longAverage, shortAverage, lowBandHit);
    updateBarState(metrics);

    shortEnergyHistory_.push_back(metrics.rms);
    longEnergyHistory_.push_back(metrics.rms);
    while (shortEnergyHistory_.size() > 18) {
        shortEnergyHistory_.pop_front();
    }
    while (longEnergyHistory_.size() > 220) {
        longEnergyHistory_.pop_front();
    }
}

void AudioAnalyzer::updateArrangementSection(AudioMetrics& metrics,
                                             float longAverage,
                                             float shortAverage,
                                             float lowBandHit)
{
    const bool silent = metrics.rms < 0.006f && metrics.peak < 0.02f;
    SectionCandidate candidate;

    if (silent) {
        candidate = SectionCandidate{ArrangementSection::Silence, 1.0f};
    } else {
        const bool enoughHistory = longEnergyHistory_.size() >= 24;
        const float sectionThresholdScale = syncProfile_.sectionThresholdScale();
        const float sectionConfidenceGain = syncProfile_.sectionConfidenceGain();
        const float relativeEnergy = enoughHistory && longAverage > 0.004f
                                         ? metrics.rms / std::max(0.004f, longAverage)
                                         : 1.0f;
        const float shortRelative = enoughHistory && longAverage > 0.004f
                                        ? shortAverage / std::max(0.004f, longAverage)
                                        : 1.0f;
        const float buildScore = clamp01((metrics.phraseIntensity * 0.55f +
                                          metrics.spectralFlux * 0.65f +
                                          metrics.beatConfidence * 0.16f +
                                          std::max(0.0f, shortRelative - 1.0f) * 0.35f) *
                                         sectionConfidenceGain);
        const float dropScore = clamp01((metrics.dropIntensity * 0.72f +
                                         lowBandHit * 0.3f +
                                         metrics.beatConfidence * 0.22f) *
                                        sectionConfidenceGain);
        const float breakdownScore = enoughHistory
                                         ? clamp01((1.0f - relativeEnergy) * 1.35f +
                                                   (0.28f - metrics.bass) * 0.65f +
                                                   metrics.stereoWidth * 0.12f)
                                         : 0.0f;
        const float grooveScore = clamp01(0.36f +
                                          metrics.beatConfidence * 0.32f +
                                          metrics.rms * 1.35f +
                                          metrics.bass * 0.16f -
                                          metrics.dropIntensity * 0.18f -
                                          metrics.phraseIntensity * 0.14f);

        candidate = SectionCandidate{ArrangementSection::Groove, grooveScore};
        if (breakdownScore > candidate.confidence && metrics.rms > 0.006f &&
            metrics.dropIntensity < 0.38f * sectionThresholdScale) {
            candidate = SectionCandidate{ArrangementSection::Breakdown, breakdownScore};
        }
        if (buildScore > candidate.confidence && metrics.dropIntensity < 0.62f * sectionThresholdScale) {
            candidate = SectionCandidate{ArrangementSection::Build, buildScore};
        }
        if (dropScore > candidate.confidence && metrics.dropIntensity > 0.38f * sectionThresholdScale) {
            candidate = SectionCandidate{ArrangementSection::Drop, dropScore};
        }
    }

    const double secondsSinceSwitch = metrics.timeSeconds - lastSectionSwitchSeconds_;
    const bool sameSection = candidate.section == currentSection_;
    const bool forceSilence = candidate.section == ArrangementSection::Silence;
    const bool forceDrop = candidate.section == ArrangementSection::Drop && candidate.confidence > 0.62f;
    const bool confidentSwitch = candidate.confidence > metrics.sectionConfidence + 0.12f;
    const bool switchAllowed = sameSection ||
                               forceSilence ||
                               forceDrop ||
                               secondsSinceSwitch > 0.75 ||
                               confidentSwitch;

    if (!sameSection && switchAllowed) {
        currentSection_ = candidate.section;
        sectionStartSeconds_ = metrics.timeSeconds;
        lastSectionSwitchSeconds_ = metrics.timeSeconds;
    }

    metrics.section = currentSection_;
    metrics.sectionConfidence = currentSection_ == candidate.section
                                    ? candidate.confidence
                                    : std::max(0.2f, candidate.confidence * 0.72f);
    const float beatPhraseSeconds = metrics.bpm > 1.0f
                                        ? std::clamp((60.0f / metrics.bpm) * 16.0f, 3.0f, 12.0f)
                                        : 8.0f;
    metrics.sectionProgress = clamp01(static_cast<float>((metrics.timeSeconds - sectionStartSeconds_) /
                                                         static_cast<double>(beatPhraseSeconds)));
    syncProfile_.learnSection(metrics);
}

void AudioAnalyzer::updateBarState(AudioMetrics& metrics)
{
    const bool hasTempo = beatIntervalSeconds_ > 0.001 || metrics.bpm > 1.0f;
    const double beatInterval = beatIntervalSeconds_ > 0.001
                                    ? beatIntervalSeconds_
                                    : (metrics.bpm > 1.0f ? 60.0 / static_cast<double>(metrics.bpm) : 0.0);
    const double barInterval = beatInterval > 0.001 ? beatInterval * 4.0 : 0.0;
    const bool sectionBoundary = std::abs(metrics.timeSeconds - lastSectionSwitchSeconds_) < 0.03 &&
                                 metrics.sectionConfidence > 0.48f &&
                                 metrics.section != ArrangementSection::Silence;
    const bool dropBoundary = metrics.section == ArrangementSection::Drop &&
                              metrics.sectionConfidence > 0.54f &&
                              metrics.dropIntensity > 0.35f;
    const bool firstDownbeat = lastDownbeatTime_ < -1.0;
    const bool periodicDownbeat = metrics.beat && beatCountInBar_ >= 3;
    const bool firstPhrase = lastPhraseBoundaryTime_ < -1.0;

    if (metrics.beat) {
        const bool likelyDownbeat = firstDownbeat || sectionBoundary || dropBoundary || periodicDownbeat;
        if (likelyDownbeat) {
            metrics.downbeat = true;
            beatCountInBar_ = 0;
            lastDownbeatTime_ = metrics.timeSeconds;
            const bool phraseReset = firstPhrase ||
                                     sectionBoundary ||
                                     dropBoundary ||
                                     barsSincePhraseBoundary_ >= 3;
            if (phraseReset) {
                metrics.phraseBoundary = true;
                barsSincePhraseBoundary_ = 0;
                lastPhraseBoundaryTime_ = metrics.timeSeconds;
            } else {
                ++barsSincePhraseBoundary_;
            }
        } else {
            ++beatCountInBar_;
        }
    } else if (sectionBoundary && firstDownbeat) {
        lastDownbeatTime_ = metrics.timeSeconds;
        beatCountInBar_ = 0;
        metrics.phraseBoundary = true;
        barsSincePhraseBoundary_ = 0;
        lastPhraseBoundaryTime_ = metrics.timeSeconds;
    }

    const float tempoConfidence = hasTempo
                                      ? std::clamp(static_cast<float>(beatTimes_.size()) / 6.0f, 0.2f, 1.0f)
                                      : 0.0f;
    metrics.barConfidence = clamp01(tempoConfidence * 0.44f +
                                    metrics.sectionConfidence * 0.22f +
                                    metrics.beatConfidence * 0.18f +
                                    metrics.dropIntensity * 0.1f +
                                    syncProfile_.learnedWeight() * 0.06f);

    if (barInterval > 0.001 && lastDownbeatTime_ > -1.0) {
        double elapsed = metrics.timeSeconds - lastDownbeatTime_;
        if (elapsed < 0.0) {
            elapsed = 0.0;
        }
        metrics.barPhase = clamp01(static_cast<float>(std::fmod(elapsed, barInterval) / barInterval));
    } else if (hasTempo) {
        const float beatUnit = static_cast<float>(std::max(0, beatCountInBar_)) * 0.25f;
        metrics.barPhase = clamp01(beatUnit + metrics.beatPhase * 0.25f);
    } else {
        metrics.barPhase = 0.0f;
    }

    const float phaseProximity = 1.0f - std::min(1.0f, std::abs(metrics.barPhase) * 8.0f);
    metrics.downbeatConfidence = metrics.downbeat
                                     ? clamp01(0.42f +
                                               metrics.barConfidence * 0.42f +
                                               metrics.beatConfidence * 0.18f +
                                               metrics.dropIntensity * 0.16f)
                                     : clamp01(phaseProximity * metrics.barConfidence * 0.5f);

    const float phrasePhaseRaw = (static_cast<float>(std::max(0, barsSincePhraseBoundary_)) +
                                  metrics.barPhase) /
                                 4.0f;
    metrics.phrasePhase = clamp01(phrasePhaseRaw);
    metrics.phraseConfidence = clamp01(metrics.barConfidence * 0.48f +
                                       metrics.downbeatConfidence * 0.18f +
                                       metrics.sectionConfidence * 0.16f +
                                       tempoConfidence * 0.12f +
                                       syncProfile_.learnedWeight() * 0.06f);
    if (metrics.phraseBoundary) {
        metrics.phraseConfidence = std::max(metrics.phraseConfidence,
                                            clamp01(0.42f +
                                                    metrics.downbeatConfidence * 0.24f +
                                                    metrics.sectionConfidence * 0.18f +
                                                    metrics.dropIntensity * 0.16f));
    }

    const float phraseRamp = metrics.phrasePhase * metrics.phrasePhase * (3.0f - 2.0f * metrics.phrasePhase);
    float tension = phraseRamp * 0.42f +
                    metrics.phraseIntensity * 0.28f +
                    metrics.spectralFlux * 0.18f +
                    metrics.beatConfidence * 0.08f;
    if (metrics.section == ArrangementSection::Build) {
        tension += metrics.sectionProgress * metrics.sectionConfidence * 0.42f;
    } else if (metrics.section == ArrangementSection::Breakdown) {
        tension += std::max(0.0f, metrics.phrasePhase - 0.48f) * metrics.sectionConfidence * 0.2f;
    } else if (metrics.section == ArrangementSection::Drop) {
        tension = std::max(tension * 0.35f, metrics.dropIntensity * 0.36f);
    }
    if (metrics.phraseBoundary && metrics.section == ArrangementSection::Drop) {
        tension = std::max(tension, metrics.dropIntensity);
    }
    metrics.buildTension = clamp01(tension * (0.45f + metrics.phraseConfidence * 0.8f));
}

AudioStyle AudioAnalyzer::heuristicStyle(const AudioMetrics& metrics) const
{
    if (metrics.rms < 0.006f && metrics.peak < 0.02f) {
        return AudioStyle::Silence;
    }
    if (metrics.stereoWidth > 0.52f && metrics.rms > 0.02f) {
        return AudioStyle::Wide;
    }
    if (metrics.bpm >= 112.0f && metrics.bpm <= 156.0f && metrics.bass > metrics.mid * 0.82f) {
        return AudioStyle::Techno;
    }
    if (metrics.bass > 0.38f && metrics.bass > metrics.treble * 1.35f) {
        return AudioStyle::BassHeavy;
    }
    if ((metrics.treble + metrics.highMid) > (metrics.bass + metrics.lowMid) * 1.15f) {
        return AudioStyle::Bright;
    }
    return AudioStyle::Ambient;
}

} // namespace viz
