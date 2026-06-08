#include "Visualizer/Export/CapturePackage.hpp"

#include "Visualizer/Export/PreviewImage.hpp"

#include <algorithm>
#include <cmath>
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

std::string detectedKeyLabel(const CapturePackage& package)
{
    if (package.detectedKeyIndex < 0) {
        return "Unknown";
    }
    std::ostringstream label;
    label << keyName(package.detectedKeyIndex) << " " << toString(package.detectedKeyMode);
    return label.str();
}

double estimatedFrameRate(const CapturePackage& package)
{
    if (package.durationSeconds <= 0.0001) {
        return 0.0;
    }
    return static_cast<double>(package.framesWritten) / package.durationSeconds;
}

std::string videoReference(const CapturePackage& package)
{
    if (package.videoPath.empty()) {
        return {};
    }
    std::error_code ec;
    const std::filesystem::path relative = std::filesystem::relative(package.videoPath, package.sessionPath, ec);
    return ec ? slashPath(package.videoPath) : slashPath(relative);
}

std::string previewReference(const PreviewImageResult& preview)
{
    return preview.written ? slashPath(preview.outputPath.filename()) : std::string{};
}

std::string onOff(bool value)
{
    return value ? "On" : "Off";
}

std::string formatMilliseconds(double value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << std::max(0.0, value) << " ms";
    return output.str();
}

bool writeManifest(const CapturePackage& package,
                   const PreviewImageResult& preview,
                   const std::filesystem::path& path,
                   std::string& error)
{
    std::ofstream output(path);
    if (!output) {
        error = "Unable to write capture manifest.";
        return false;
    }

    output << "{\n";
    output << "  \"title\": \"Visualizer Live Capture\",\n";
    output << "  \"type\": \"liveCapture\",\n";
    output << "  \"source\": \"" << escapeJson(package.sourceLabel) << "\",\n";
    output << "  \"look\": \"" << escapeJson(package.lookName.empty() ? "Custom" : package.lookName) << "\",\n";
    output << "  \"framesDirectory\": \"" << escapeJson(slashPath(package.sessionPath)) << "\",\n";
    output << "  \"framePattern\": \"frame_%06d.ppm\",\n";
    output << "  \"previewImage\": \"" << escapeJson(previewReference(preview)) << "\",\n";
    output << "  \"previewFrames\": " << preview.previewFramesUsed << ",\n";
    output << "  \"previewWidth\": " << preview.width << ",\n";
    output << "  \"previewHeight\": " << preview.height << ",\n";
    if (!package.videoPath.empty()) {
        output << "  \"video\": \"" << escapeJson(videoReference(package)) << "\",\n";
    }
    if (!package.timelinePath.empty()) {
        output << "  \"timeline\": \"analysis_timeline.csv\",\n";
    }
    output << "  \"styleProfile\": \"" << escapeJson(slashPath(package.styleProfilePath)) << "\",\n";
    output << "  \"syncProfile\": \"" << escapeJson(slashPath(package.syncProfilePath)) << "\",\n";
    output << "  \"requestedMode\": \"" << escapeJson(std::string(toString(package.requestedSettings.mode))) << "\",\n";
    output << "  \"finalMode\": \"" << escapeJson(std::string(toString(package.finalSettings.mode))) << "\",\n";
    output << "  \"requestedPalette\": \"" << escapeJson(std::string(toString(package.requestedSettings.palette))) << "\",\n";
    output << "  \"finalPalette\": \"" << escapeJson(std::string(toString(package.finalSettings.palette))) << "\",\n";
    output << "  \"requestedHueShift\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.hueShift, 0.0f, 1.0f) << ",\n";
    output << "  \"finalHueShift\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.hueShift, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedDepth3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.depth3D, 0.0f, 1.0f) << ",\n";
    output << "  \"finalDepth3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.depth3D, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedColorImpact\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.colorImpact, 0.0f, 1.0f) << ",\n";
    output << "  \"finalColorImpact\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.colorImpact, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedObjectDensity3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.objectDensity3D, 0.0f, 1.0f) << ",\n";
    output << "  \"finalObjectDensity3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.objectDensity3D, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedInteractionDepth\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.interactionDepth, 0.0f, 1.0f) << ",\n";
    output << "  \"finalInteractionDepth\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.interactionDepth, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedLightingGlow\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.lightingGlow, 0.0f, 1.0f) << ",\n";
    output << "  \"finalLightingGlow\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.lightingGlow, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedScenePersonality\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.scenePersonality, 0.0f, 1.0f) << ",\n";
    output << "  \"finalScenePersonality\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.scenePersonality, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedResponse3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.response3D, 0.0f, 1.0f) << ",\n";
    output << "  \"finalResponse3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.response3D, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedMotionStability\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.motionStability, 0.0f, 1.0f) << ",\n";
    output << "  \"finalMotionStability\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.motionStability, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedPatternClarity\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.patternClarity, 0.0f, 1.0f) << ",\n";
    output << "  \"finalPatternClarity\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.patternClarity, 0.0f, 1.0f) << ",\n";
    output << "  \"requestedComplexity\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.requestedSettings.complexity, 0.35f, 1.8f) << ",\n";
    output << "  \"finalComplexity\": " << std::fixed << std::setprecision(3)
           << std::clamp(package.finalSettings.complexity, 0.35f, 1.8f) << ",\n";
    output << "  \"autoScene\": " << (package.requestedSettings.autoScene ? "true" : "false") << ",\n";
    output << "  \"environmentReactive\": " << (package.requestedSettings.environmentReactive ? "true" : "false") << ",\n";
    output << "  \"interactiveField\": " << (package.requestedSettings.interactiveField ? "true" : "false") << ",\n";
    output << "  \"trails\": " << (package.requestedSettings.trails ? "true" : "false") << ",\n";
    output << "  \"width\": " << package.width << ",\n";
    output << "  \"height\": " << package.height << ",\n";
    output << "  \"framesWritten\": " << package.framesWritten << ",\n";
    output << "  \"durationSeconds\": " << std::fixed << std::setprecision(3) << package.durationSeconds << ",\n";
    output << "  \"estimatedFrameRate\": " << std::setprecision(2) << estimatedFrameRate(package) << ",\n";
    output << "  \"averageFrameMs\": " << std::setprecision(3) << std::max(0.0, package.averageFrameMs) << ",\n";
    output << "  \"averageAnalysisMs\": " << std::setprecision(3) << std::max(0.0, package.averageAnalysisMs) << ",\n";
    output << "  \"averageGeometryMs\": " << std::setprecision(3) << std::max(0.0, package.averageGeometryMs) << ",\n";
    output << "  \"averageRenderMs\": " << std::setprecision(3) << std::max(0.0, package.averageRenderMs) << ",\n";
    output << "  \"averageRecordMs\": " << std::setprecision(3) << std::max(0.0, package.averageRecordMs) << ",\n";
    output << "  \"peakRms\": " << std::setprecision(4) << package.peakRms << ",\n";
    output << "  \"estimatedBpm\": " << std::setprecision(2) << package.estimatedBpm << ",\n";
    output << "  \"beatsDetected\": " << package.beatsDetected << ",\n";
    output << "  \"downbeatsDetected\": " << package.downbeatsDetected << ",\n";
    output << "  \"phraseBoundariesDetected\": " << package.phraseBoundariesDetected << ",\n";
    output << "  \"averagePhraseConfidence\": " << std::fixed << std::setprecision(3)
           << std::max(0.0, package.averagePhraseConfidence) << ",\n";
    output << "  \"peakBuildTension\": " << std::fixed << std::setprecision(3)
           << std::max(0.0f, package.peakBuildTension) << ",\n";
    output << "  \"dominantSection\": \"" << escapeJson(std::string(toString(package.dominantSection))) << "\",\n";
    output << "  \"sectionConfidence\": " << std::setprecision(3) << package.sectionConfidence << ",\n";
    output << "  \"detectedKey\": \"" << escapeJson(detectedKeyLabel(package)) << "\",\n";
    output << "  \"timelineWritten\": " << (package.timelineWritten ? "true" : "false") << ",\n";
    output << "  \"videoEncoded\": " << (package.videoEncoded ? "true" : "false") << ",\n";
    output << "  \"videoBytes\": " << package.videoBytes << ",\n";
    if (!package.timelineWriteError.empty()) {
        output << "  \"timelineWriteError\": \"" << escapeJson(package.timelineWriteError) << "\",\n";
    }
    if (!package.videoEncodeError.empty()) {
        output << "  \"videoEncodeError\": \"" << escapeJson(package.videoEncodeError) << "\",\n";
    }
    output << "  \"keyConfidence\": " << std::setprecision(3) << package.keyConfidence << "\n";
    output << "}\n";

    if (!output) {
        error = "Failed while writing capture manifest.";
        return false;
    }
    return true;
}

bool writePage(const CapturePackage& package,
               const PreviewImageResult& preview,
               const std::filesystem::path& path,
               std::string& error)
{
    std::ofstream output(path);
    if (!output) {
        error = "Unable to write capture share page.";
        return false;
    }

    output << "<!doctype html>\n";
    output << "<html lang=\"en\">\n<head>\n";
    output << "<meta charset=\"utf-8\">\n";
    output << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    output << "<title>Visualizer Live Capture</title>\n";
    output << "<style>\n";
    output << "body{margin:0;background:#07090f;color:#eef3ff;font-family:Segoe UI,Arial,sans-serif;}\n";
    output << "main{max-width:1040px;margin:0 auto;padding:28px;}\n";
    output << "video,.preview{width:100%;background:#000;border:1px solid #263044;margin:16px 0;}\n";
    output << "video{max-height:68vh;}\n";
    output << ".preview{display:block;}\n";
    output << ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px;margin-top:18px;}\n";
    output << ".stat{border:1px solid #263044;background:#101522;padding:12px;}\n";
    output << ".label{color:#8fa3c8;font-size:12px;text-transform:uppercase;letter-spacing:.08em;}\n";
    output << ".value{font-size:18px;margin-top:4px;overflow-wrap:anywhere;}\n";
    output << "code{display:block;white-space:pre-wrap;border:1px solid #263044;background:#101522;padding:12px;margin-top:12px;}\n";
    output << "a{color:#5ee7ff;}\n";
    output << "</style>\n</head>\n<body>\n<main>\n";
    output << "<h1>Visualizer Live Capture</h1>\n";
    output << "<p>This folder contains a lossless PPM frame sequence captured from the live app.</p>\n";
    const std::string previewAsset = previewReference(preview);
    if (!previewAsset.empty()) {
        output << "<img class=\"preview\" src=\"" << escapeHtml(previewAsset)
               << "\" alt=\"Live capture preview contact sheet\">\n";
    }
    if (package.videoEncoded && !package.videoPath.empty()) {
        output << "<video controls playsinline src=\"" << escapeHtml(videoReference(package)) << "\"></video>\n";
        output << "<p><a href=\"" << escapeHtml(videoReference(package)) << "\">Download MP4</a></p>\n";
    }

    output << "<div class=\"grid\">\n";
    const auto stat = [&](std::string_view label, std::string_view value) {
        output << "<div class=\"stat\"><div class=\"label\">" << escapeHtml(label)
               << "</div><div class=\"value\">" << escapeHtml(value) << "</div></div>\n";
    };
    stat("Source", package.sourceLabel);
    stat("Look", package.lookName.empty() ? "Custom" : package.lookName);
    stat("Style Profile", package.styleProfilePath.empty() ? "None" : package.styleProfilePath.filename().string());
    stat("Sync Profile", package.syncProfilePath.empty() ? "None" : package.syncProfilePath.filename().string());
    stat("Final Mode", std::string(toString(package.finalSettings.mode)));
    stat("Final Palette", std::string(toString(package.finalSettings.palette)));
    stat("Depth 3D", std::to_string(package.finalSettings.depth3D));
    stat("3D Objects", std::to_string(package.finalSettings.objectDensity3D));
    stat("Mouse 3D", std::to_string(package.finalSettings.interactionDepth));
    stat("3D Glow", std::to_string(package.finalSettings.lightingGlow));
    stat("Color Impact", std::to_string(package.finalSettings.colorImpact));
    stat("Scene Personality", std::to_string(package.finalSettings.scenePersonality));
    stat("3D Response", std::to_string(package.finalSettings.response3D));
    stat("Motion Stability", std::to_string(package.finalSettings.motionStability));
    stat("Pattern Clarity", std::to_string(package.finalSettings.patternClarity));
    stat("Complexity", std::to_string(package.finalSettings.complexity));
    stat("Frames", std::to_string(package.framesWritten));
    stat("Preview Frames", std::to_string(preview.previewFramesUsed));
    stat("Duration", std::to_string(package.durationSeconds) + " sec");
    stat("Estimated FPS", std::to_string(estimatedFrameRate(package)));
    stat("Avg Frame", formatMilliseconds(package.averageFrameMs));
    stat("Avg Core", formatMilliseconds(package.averageAnalysisMs + package.averageGeometryMs));
    stat("Avg Render", formatMilliseconds(package.averageRenderMs));
    stat("Avg Record", formatMilliseconds(package.averageRecordMs));
    stat("Resolution", std::to_string(package.width) + " x " + std::to_string(package.height));
    stat("MP4", package.videoEncoded ? (std::to_string(package.videoBytes) + " bytes") : "Not encoded");
    stat("Peak RMS", std::to_string(package.peakRms));
    stat("BPM", std::to_string(package.estimatedBpm));
    stat("Beats", std::to_string(package.beatsDetected));
    stat("Downbeats", std::to_string(package.downbeatsDetected));
    stat("Phrase Boundaries", std::to_string(package.phraseBoundariesDetected));
    stat("Phrase Lock", std::to_string(package.averagePhraseConfidence));
    stat("Build Tension", std::to_string(package.peakBuildTension));
    stat("Section", std::string(toString(package.dominantSection)));
    stat("Key", detectedKeyLabel(package));
    stat("Timeline", package.timelineWritten ? "analysis_timeline.csv" : "Not written");
    stat("Trails", onOff(package.requestedSettings.trails));
    stat("Auto Scene", onOff(package.requestedSettings.autoScene));
    stat("Environment", onOff(package.requestedSettings.environmentReactive));
    output << "</div>\n";

    output << "<h2>Encode MP4</h2>\n";
    if (!package.videoEncodeError.empty()) {
        output << "<p>Automatic MP4 encoding was skipped: " << escapeHtml(package.videoEncodeError) << "</p>\n";
    }
    output << "<code>ffmpeg -framerate "
           << std::max(1, static_cast<int>(std::round(estimatedFrameRate(package))))
           << " -i frame_%06d.ppm -pix_fmt yuv420p visualizer-live-capture.mp4</code>\n";
    output << "<p><a href=\"capture_manifest.json\">capture_manifest.json</a> contains machine-readable live capture metadata.</p>\n";
    if (package.timelineWritten) {
        output << "<p><a href=\"analysis_timeline.csv\">analysis_timeline.csv</a> contains one row of sync metrics per recorded frame.</p>\n";
    } else if (!package.timelineWriteError.empty()) {
        output << "<p>Timeline was not written: " << escapeHtml(package.timelineWriteError) << "</p>\n";
    }
    output << "</main>\n</body>\n</html>\n";

    if (!output) {
        error = "Failed while writing capture share page.";
        return false;
    }
    return true;
}

} // namespace

bool writeCapturePackage(const CapturePackage& package, std::string& error)
{
    error.clear();
    if (package.sessionPath.empty()) {
        error = "Capture package requires a session directory.";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(package.sessionPath, ec);
    if (ec) {
        error = "Unable to create capture package directory: " + ec.message();
        return false;
    }

    PreviewImageResult preview;
    std::string previewError;
    writeFramePreviewContactSheet(package.sessionPath,
                                  package.sessionPath / "preview.bmp",
                                  6,
                                  220,
                                  preview,
                                  previewError);

    return writeManifest(package, preview, package.sessionPath / "capture_manifest.json", error) &&
           writePage(package, preview, package.sessionPath / "index.html", error);
}

} // namespace viz
