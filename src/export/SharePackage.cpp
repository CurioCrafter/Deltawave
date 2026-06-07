#include "Visualizer/Export/SharePackage.hpp"

#include "Visualizer/Export/PreviewImage.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace viz {
namespace {

std::string escapeJson(std::string_view value)
{
    std::ostringstream output;
    for (char ch : value) {
        switch (ch) {
        case '\\':
            output << "\\\\";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            output << ch;
            break;
        }
    }
    return output.str();
}

std::string escapeHtml(std::string_view value)
{
    std::ostringstream output;
    for (char ch : value) {
        switch (ch) {
        case '&':
            output << "&amp;";
            break;
        case '<':
            output << "&lt;";
            break;
        case '>':
            output << "&gt;";
            break;
        case '"':
            output << "&quot;";
            break;
        default:
            output << ch;
            break;
        }
    }
    return output.str();
}

std::string slashPath(std::filesystem::path path)
{
    return path.generic_string();
}

std::string relativeAssetPath(const std::filesystem::path& fromDirectory,
                              const std::filesystem::path& assetPath)
{
    if (assetPath.empty()) {
        return {};
    }

    const std::filesystem::path relative = assetPath.lexically_relative(fromDirectory);
    if (!relative.empty()) {
        return slashPath(relative);
    }
    return slashPath(assetPath.filename());
}

std::string detectedKeyLabel(const OfflineExportResult& result)
{
    if (result.detectedKeyIndex < 0) {
        return "Unknown";
    }
    std::ostringstream label;
    label << keyName(result.detectedKeyIndex) << " " << toString(result.detectedKeyMode);
    return label.str();
}

std::string formatFloat(float value, int precision = 3)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

bool writeShareManifest(const OfflineExportOptions& options,
                        const OfflineExportResult& result,
                        const PreviewImageResult& preview,
                        const std::filesystem::path& path,
                        std::string& error)
{
    std::ofstream output(path);
    if (!output) {
        error = "Unable to write share manifest.";
        return false;
    }

    output << "{\n";
    output << "  \"title\": \"Visualizer Export\",\n";
    output << "  \"inputAudio\": \"" << escapeJson(slashPath(options.inputAudio)) << "\",\n";
    output << "  \"framesDirectory\": \"" << escapeJson(slashPath(result.outputDirectory)) << "\",\n";
    output << "  \"video\": \"" << escapeJson(result.videoEncoded ? slashPath(result.outputVideo) : std::string{}) << "\",\n";
    output << "  \"previewImage\": \"" << escapeJson(preview.written ? slashPath(preview.outputPath.filename()) : std::string{}) << "\",\n";
    output << "  \"previewFrames\": " << preview.previewFramesUsed << ",\n";
    output << "  \"previewWidth\": " << preview.width << ",\n";
    output << "  \"previewHeight\": " << preview.height << ",\n";
    output << "  \"timeline\": \"" << escapeJson(result.timelineWritten ? "analysis_timeline.csv" : "") << "\",\n";
    output << "  \"styleProfile\": \"" << escapeJson(slashPath(options.styleProfile)) << "\",\n";
    output << "  \"syncProfile\": \"" << escapeJson(slashPath(options.syncProfile)) << "\",\n";
    output << "  \"look\": \"" << escapeJson(options.lookName.empty() ? "Custom" : options.lookName) << "\",\n";
    output << "  \"mode\": \"" << escapeJson(std::string(toString(options.settings.mode))) << "\",\n";
    output << "  \"palette\": \"" << escapeJson(std::string(toString(options.settings.palette))) << "\",\n";
    output << "  \"hueShift\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.hueShift, 0.0f, 1.0f) << ",\n";
    output << "  \"finalHueShift\": " << std::fixed << std::setprecision(3)
           << std::clamp(result.finalHueShift, 0.0f, 1.0f) << ",\n";
    output << "  \"depth3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.depth3D, 0.0f, 1.0f) << ",\n";
    output << "  \"finalDepth3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(result.finalDepth3D, 0.0f, 1.0f) << ",\n";
    output << "  \"colorImpact\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.colorImpact, 0.0f, 1.0f) << ",\n";
    output << "  \"finalColorImpact\": " << std::fixed << std::setprecision(3)
           << std::clamp(result.finalColorImpact, 0.0f, 1.0f) << ",\n";
    output << "  \"objectDensity3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.objectDensity3D, 0.0f, 1.0f) << ",\n";
    output << "  \"finalObjectDensity3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(result.finalObjectDensity3D, 0.0f, 1.0f) << ",\n";
    output << "  \"interactionDepth\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.interactionDepth, 0.0f, 1.0f) << ",\n";
    output << "  \"finalInteractionDepth\": " << std::fixed << std::setprecision(3)
           << std::clamp(result.finalInteractionDepth, 0.0f, 1.0f) << ",\n";
    output << "  \"lightingGlow\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.lightingGlow, 0.0f, 1.0f) << ",\n";
    output << "  \"finalLightingGlow\": " << std::fixed << std::setprecision(3)
           << std::clamp(result.finalLightingGlow, 0.0f, 1.0f) << ",\n";
    output << "  \"scenePersonality\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.scenePersonality, 0.0f, 1.0f) << ",\n";
    output << "  \"finalScenePersonality\": " << std::fixed << std::setprecision(3)
           << std::clamp(result.finalScenePersonality, 0.0f, 1.0f) << ",\n";
    output << "  \"minimumHueShift\": " << std::fixed << std::setprecision(3)
           << std::clamp(result.minimumHueShift, 0.0f, 1.0f) << ",\n";
    output << "  \"maximumHueShift\": " << std::fixed << std::setprecision(3)
           << std::clamp(result.maximumHueShift, 0.0f, 1.0f) << ",\n";
    output << "  \"complexity\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.complexity, 0.35f, 1.8f) << ",\n";
    output << "  \"autoScene\": " << ((options.autoScene || options.settings.autoScene) ? "true" : "false") << ",\n";
    output << "  \"environmentReactive\": " << (options.settings.environmentReactive ? "true" : "false") << ",\n";
    output << "  \"environmentTimeOfDay\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.environmentTimeOfDay, 0.0f, 1.0f) << ",\n";
    output << "  \"trails\": " << (options.settings.trails ? "true" : "false") << ",\n";
    output << "  \"width\": " << options.width << ",\n";
    output << "  \"height\": " << options.height << ",\n";
    output << "  \"frameRate\": " << options.frameRate << ",\n";
    output << "  \"framesWritten\": " << result.framesWritten << ",\n";
    output << "  \"durationSeconds\": " << std::fixed << std::setprecision(3) << result.durationSeconds << ",\n";
    output << "  \"peakRms\": " << std::setprecision(4) << result.peakRms << ",\n";
    output << "  \"estimatedBpm\": " << std::setprecision(2) << result.estimatedBpm << ",\n";
    output << "  \"beatsDetected\": " << result.beatsDetected << ",\n";
    output << "  \"trackIntelligence\": {\n";
    output << "    \"downbeatsDetected\": " << result.trackSummary.downbeatsDetected << ",\n";
    output << "    \"phraseBoundariesDetected\": " << result.trackSummary.phraseBoundariesDetected << ",\n";
    output << "    \"averageBarConfidence\": " << std::fixed << std::setprecision(3)
           << result.trackSummary.averageBarConfidence << ",\n";
    output << "    \"averagePhraseConfidence\": " << std::fixed << std::setprecision(3)
           << result.trackSummary.averagePhraseConfidence << ",\n";
    output << "    \"peakDownbeatConfidence\": " << std::fixed << std::setprecision(3)
           << result.trackSummary.peakDownbeatConfidence << ",\n";
    output << "    \"peakDropIntensity\": " << std::fixed << std::setprecision(3)
           << result.trackSummary.peakDropIntensity << ",\n";
    output << "    \"peakPhraseIntensity\": " << std::fixed << std::setprecision(3)
           << result.trackSummary.peakPhraseIntensity << ",\n";
    output << "    \"peakBuildTension\": " << std::fixed << std::setprecision(3)
           << result.trackSummary.peakBuildTension << ",\n";
    output << "    \"averageHarmonicEnergy\": " << std::fixed << std::setprecision(3)
           << result.trackSummary.averageHarmonicEnergy << ",\n";
    output << "    \"dominantStyle\": \"" << escapeJson(std::string(toString(result.trackSummary.dominantStyle))) << "\",\n";
    output << "    \"dominantStyleConfidence\": " << std::fixed << std::setprecision(3)
           << result.trackSummary.dominantStyleConfidence << ",\n";
    output << "    \"maxPrimitiveCount\": " << result.trackSummary.maxPrimitiveCount << "\n";
    output << "  },\n";
    output << "  \"dominantSection\": \"" << escapeJson(std::string(toString(result.dominantSection))) << "\",\n";
    output << "  \"sectionConfidence\": " << std::setprecision(3) << result.sectionConfidence << ",\n";
    output << "  \"detectedKey\": \"" << escapeJson(detectedKeyLabel(result)) << "\",\n";
    output << "  \"keyConfidence\": " << std::setprecision(3) << result.keyConfidence << ",\n";
    output << "  \"timelineWritten\": " << (result.timelineWritten ? "true" : "false") << ",\n";
    output << "  \"videoEncoded\": " << (result.videoEncoded ? "true" : "false") << ",\n";
    output << "  \"videoBytes\": " << result.videoBytes << "\n";
    output << "}\n";

    if (!output) {
        error = "Failed while writing share manifest.";
        return false;
    }
    return true;
}

bool writeSharePage(const OfflineExportOptions& options,
                    const OfflineExportResult& result,
                    const PreviewImageResult& preview,
                    const std::filesystem::path& path,
                    std::string& error)
{
    std::ofstream output(path);
    if (!output) {
        error = "Unable to write share page.";
        return false;
    }

    const std::string videoAsset = relativeAssetPath(result.outputDirectory, result.outputVideo);
    const std::string previewAsset = preview.written ? slashPath(preview.outputPath.filename()) : std::string{};
    output << "<!doctype html>\n";
    output << "<html lang=\"en\">\n<head>\n";
    output << "<meta charset=\"utf-8\">\n";
    output << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    output << "<title>Visualizer Export</title>\n";
    output << "<style>\n";
    output << "body{margin:0;background:#07090f;color:#eef3ff;font-family:Segoe UI,Arial,sans-serif;}\n";
    output << "main{max-width:1040px;margin:0 auto;padding:28px;}\n";
    output << "video,.preview{width:100%;background:#000;border:1px solid #263044;}\n";
    output << ".preview{display:block;margin-top:14px;}\n";
    output << ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px;margin-top:18px;}\n";
    output << ".stat{border:1px solid #263044;background:#101522;padding:12px;}\n";
    output << ".label{color:#8fa3c8;font-size:12px;text-transform:uppercase;letter-spacing:.08em;}\n";
    output << ".value{font-size:18px;margin-top:4px;}\n";
    output << "a{color:#5ee7ff;}\n";
    output << "</style>\n</head>\n<body>\n<main>\n";
    output << "<h1>Visualizer Export</h1>\n";
    if (result.videoEncoded && !videoAsset.empty()) {
        output << "<video controls playsinline src=\"" << escapeHtml(videoAsset) << "\"></video>\n";
    }
    if (!previewAsset.empty()) {
        output << "<img class=\"preview\" src=\"" << escapeHtml(previewAsset)
               << "\" alt=\"Visualization preview contact sheet\">\n";
    } else if (!result.videoEncoded) {
        output << "<p>MP4 was not encoded for this package. The frame sequence is available in this folder as PPM files.</p>\n";
    }

    output << "<div class=\"grid\">\n";
    const auto stat = [&](std::string_view label, std::string_view value) {
        output << "<div class=\"stat\"><div class=\"label\">" << escapeHtml(label)
               << "</div><div class=\"value\">" << escapeHtml(value) << "</div></div>\n";
    };
    stat("Look", options.lookName.empty() ? "Custom" : options.lookName);
    stat("Mode", std::string(toString(options.settings.mode)));
    stat("Palette", std::string(toString(options.settings.palette)));
    stat("Hue Shift", std::to_string(options.settings.hueShift));
    stat("Final Hue Shift", std::to_string(result.finalHueShift));
    stat("Depth 3D", std::to_string(options.settings.depth3D));
    stat("Final Depth 3D", std::to_string(result.finalDepth3D));
    stat("3D Objects", std::to_string(options.settings.objectDensity3D));
    stat("Final 3D Objects", std::to_string(result.finalObjectDensity3D));
    stat("Mouse 3D", std::to_string(options.settings.interactionDepth));
    stat("Final Mouse 3D", std::to_string(result.finalInteractionDepth));
    stat("3D Glow", std::to_string(options.settings.lightingGlow));
    stat("Final 3D Glow", std::to_string(result.finalLightingGlow));
    stat("Color Impact", std::to_string(options.settings.colorImpact));
    stat("Final Color Impact", std::to_string(result.finalColorImpact));
    stat("Scene Personality", std::to_string(options.settings.scenePersonality));
    stat("Final Scene Personality", std::to_string(result.finalScenePersonality));
    stat("Complexity", std::to_string(options.settings.complexity));
    stat("Duration", std::to_string(result.durationSeconds) + " sec");
    stat("Frames", std::to_string(result.framesWritten));
    stat("Resolution", std::to_string(options.width) + " x " + std::to_string(options.height));
    stat("FPS", std::to_string(options.frameRate));
    stat("Peak RMS", std::to_string(result.peakRms));
    stat("BPM", std::to_string(result.estimatedBpm));
    stat("Beats", std::to_string(result.beatsDetected));
    stat("Preview Frames", std::to_string(preview.previewFramesUsed));
    stat("Downbeats", std::to_string(result.trackSummary.downbeatsDetected));
    stat("Bar Lock", formatFloat(result.trackSummary.averageBarConfidence));
    stat("Phrase Boundaries", std::to_string(result.trackSummary.phraseBoundariesDetected));
    stat("Phrase Lock", formatFloat(result.trackSummary.averagePhraseConfidence));
    stat("Downbeat Peak", formatFloat(result.trackSummary.peakDownbeatConfidence));
    stat("Drop Peak", formatFloat(result.trackSummary.peakDropIntensity));
    stat("Phrase Peak", formatFloat(result.trackSummary.peakPhraseIntensity));
    stat("Build Tension", formatFloat(result.trackSummary.peakBuildTension));
    stat("Harmonic Energy", formatFloat(result.trackSummary.averageHarmonicEnergy));
    stat("Dominant Style", std::string(toString(result.trackSummary.dominantStyle)));
    stat("Style Confidence", formatFloat(result.trackSummary.dominantStyleConfidence));
    stat("Peak Primitives", std::to_string(result.trackSummary.maxPrimitiveCount));
    stat("Section", std::string(toString(result.dominantSection)));
    stat("Key", detectedKeyLabel(result));
    stat("Timeline", result.timelineWritten ? "analysis_timeline.csv" : "Not written");
    stat("Environment", options.settings.environmentReactive ? "On" : "Off");
    stat("Time Of Day", std::to_string(options.environmentTimeOfDay));
    stat("Trails", options.settings.trails ? "On" : "Off");
    stat("Auto Scene", (options.autoScene || options.settings.autoScene) ? "On" : "Off");
    output << "</div>\n";
    output << "<p><a href=\"share_manifest.json\">share_manifest.json</a> contains machine-readable export metadata.</p>\n";
    if (result.timelineWritten) {
        output << "<p><a href=\"analysis_timeline.csv\">analysis_timeline.csv</a> contains one row of sync metrics per rendered frame.</p>\n";
    }
    output << "</main>\n</body>\n</html>\n";

    if (!output) {
        error = "Failed while writing share page.";
        return false;
    }
    return true;
}

} // namespace

bool writeSharePackage(const OfflineExportOptions& options,
                       OfflineExportResult& result,
                       std::string& error)
{
    if (result.outputDirectory.empty()) {
        error = "Share package requires an export output directory.";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(result.outputDirectory, ec);
    if (ec) {
        error = "Unable to create share package directory: " + ec.message();
        return false;
    }

    const std::filesystem::path manifestPath = result.outputDirectory / "share_manifest.json";
    const std::filesystem::path pagePath = result.outputDirectory / "index.html";
    PreviewImageResult preview;
    std::string previewError;
    if (writeFramePreviewContactSheet(result.outputDirectory,
                                      result.outputDirectory / "preview.bmp",
                                      6,
                                      220,
                                      preview,
                                      previewError)) {
        result.previewImage = preview.outputPath;
        result.previewFramesUsed = preview.previewFramesUsed;
        result.previewWidth = preview.width;
        result.previewHeight = preview.height;
    }

    return writeShareManifest(options, result, preview, manifestPath, error) &&
           writeSharePage(options, result, preview, pagePath, error);
}

} // namespace viz
