#include "Visualizer/Export/AnalysisTimeline.hpp"

#include <algorithm>
#include <iomanip>

namespace viz {
namespace {

std::string keyLabel(const AudioMetrics& metrics)
{
    if (metrics.keyIndex < 0) {
        return "Unknown";
    }
    std::string label(keyName(metrics.keyIndex));
    label += " ";
    label += std::string(toString(metrics.keyMode));
    return label;
}

} // namespace

bool AnalysisTimelineWriter::open(const std::filesystem::path& path, std::string& error)
{
    close();
    error.clear();

    if (path.empty()) {
        error = "Timeline path is required.";
        return false;
    }

    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = "Unable to create timeline directory: " + ec.message();
            return false;
        }
    }

    output_.open(path);
    if (!output_) {
        error = "Unable to write analysis timeline.";
        return false;
    }

    path_ = path;
    output_ << "frame,timeSeconds,mode,palette,hueShift,depth3D,colorImpact,complexity,intensity,speed,qualityScale,"
            << "rms,peak,bass,lowMid,mid,highMid,treble,spectralFlux,onset,"
            << "beat,beatConfidence,beatPhase,barPhase,barConfidence,downbeat,downbeatConfidence,bpm,dropIntensity,phraseIntensity,"
            << "phraseBoundary,phrasePhase,phraseConfidence,buildTension,"
            << "section,sectionConfidence,sectionProgress,style,styleConfidence,"
            << "styleAdaptation,syncAdaptation,beatSensitivity,sectionSensitivity,"
            << "key,keyConfidence,harmonicEnergy,primitiveCount\n";
    if (!output_) {
        error = "Failed while writing timeline header.";
        close();
        return false;
    }
    return true;
}

bool AnalysisTimelineWriter::write(const AnalysisTimelineEntry& entry, std::string& error)
{
    error.clear();
    if (!output_) {
        error = "Analysis timeline is not open.";
        return false;
    }

    const AudioMetrics& metrics = entry.metrics;
    const VisualSettings& settings = entry.settings;
    output_ << entry.frameIndex << ","
            << std::fixed << std::setprecision(4) << entry.timeSeconds << ","
            << '"' << toString(settings.mode) << '"' << ","
            << '"' << toString(settings.palette) << '"' << ","
            << std::setprecision(3) << std::clamp(settings.hueShift, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.depth3D, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.colorImpact, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.complexity, 0.35f, 1.8f) << ","
            << std::setprecision(3) << std::clamp(settings.intensity, 0.15f, 4.0f) << ","
            << std::setprecision(3) << std::clamp(settings.speed, 0.1f, 4.0f) << ","
            << std::setprecision(3) << std::clamp(settings.qualityScale, 0.45f, 1.0f) << ","
            << std::setprecision(5) << metrics.rms << ","
            << std::setprecision(5) << metrics.peak << ","
            << std::setprecision(5) << metrics.bass << ","
            << std::setprecision(5) << metrics.lowMid << ","
            << std::setprecision(5) << metrics.mid << ","
            << std::setprecision(5) << metrics.highMid << ","
            << std::setprecision(5) << metrics.treble << ","
            << std::setprecision(5) << metrics.spectralFlux << ","
            << std::setprecision(5) << metrics.onset << ","
            << (metrics.beat ? 1 : 0) << ","
            << std::setprecision(5) << metrics.beatConfidence << ","
            << std::setprecision(5) << metrics.beatPhase << ","
            << std::setprecision(5) << metrics.barPhase << ","
            << std::setprecision(5) << metrics.barConfidence << ","
            << (metrics.downbeat ? 1 : 0) << ","
            << std::setprecision(5) << metrics.downbeatConfidence << ","
            << std::setprecision(3) << metrics.bpm << ","
            << std::setprecision(5) << metrics.dropIntensity << ","
            << std::setprecision(5) << metrics.phraseIntensity << ","
            << (metrics.phraseBoundary ? 1 : 0) << ","
            << std::setprecision(5) << metrics.phrasePhase << ","
            << std::setprecision(5) << metrics.phraseConfidence << ","
            << std::setprecision(5) << metrics.buildTension << ","
            << '"' << toString(metrics.section) << '"' << ","
            << std::setprecision(5) << metrics.sectionConfidence << ","
            << std::setprecision(5) << metrics.sectionProgress << ","
            << '"' << toString(metrics.style) << '"' << ","
            << std::setprecision(5) << metrics.styleConfidence << ","
            << std::setprecision(5) << metrics.styleAdaptation << ","
            << std::setprecision(5) << metrics.syncAdaptation << ","
            << std::setprecision(5) << metrics.beatSensitivity << ","
            << std::setprecision(5) << metrics.sectionSensitivity << ","
            << '"' << keyLabel(metrics) << '"' << ","
            << std::setprecision(5) << metrics.keyConfidence << ","
            << std::setprecision(5) << metrics.harmonicEnergy << ","
            << entry.primitiveCount << "\n";

    if (!output_) {
        error = "Failed while writing timeline row.";
        return false;
    }
    return true;
}

void AnalysisTimelineWriter::close()
{
    if (output_.is_open()) {
        output_.close();
    }
}

} // namespace viz
