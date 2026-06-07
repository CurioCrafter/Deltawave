#include "Visualizer/Export/BatchExporter.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace viz {
namespace {

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

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

std::string slashPath(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::string formatFloat(float value, int precision = 3)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

std::filesystem::path absolutePath(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
    return ec ? path : absolute.lexically_normal();
}

std::filesystem::path relativeTo(const std::filesystem::path& path,
                                 const std::filesystem::path& root)
{
    std::error_code ec;
    const std::filesystem::path relative = std::filesystem::relative(path, root, ec);
    if (!ec && !relative.empty()) {
        return relative.lexically_normal();
    }
    return path.filename();
}

bool isAudioCandidate(const std::filesystem::path& path)
{
    const std::string extension = lower(path.extension().string());
    return extension == ".wav" ||
           extension == ".wave" ||
           extension == ".mp3" ||
           extension == ".m4a" ||
           extension == ".aac" ||
           extension == ".wma" ||
           extension == ".flac" ||
           extension == ".ogg";
}

std::string sanitizedStem(const std::filesystem::path& path)
{
    std::string stem = path.stem().string();
    std::string output;
    output.reserve(stem.size());
    for (char ch : stem) {
        const auto byte = static_cast<unsigned char>(ch);
        if (std::isalnum(byte) != 0) {
            output.push_back(static_cast<char>(std::tolower(byte)));
        } else if (ch == '-' || ch == '_') {
            output.push_back(ch);
        } else if (!output.empty() && output.back() != '_') {
            output.push_back('_');
        }
    }
    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }
    return output.empty() ? "track" : output;
}

std::string numberedSlug(int index, const std::filesystem::path& path)
{
    std::ostringstream output;
    output << std::setw(3) << std::setfill('0') << index << "_" << sanitizedStem(path);
    return output.str();
}

std::vector<std::filesystem::path> collectAudioInputs(const BatchExportOptions& options)
{
    std::vector<std::filesystem::path> inputs;
    std::error_code ec;
    if (!std::filesystem::exists(options.inputDirectory, ec)) {
        return inputs;
    }

    if (options.recursive) {
        for (std::filesystem::recursive_directory_iterator it(options.inputDirectory,
                                                              std::filesystem::directory_options::skip_permission_denied,
                                                              ec),
             end;
             it != end;
             it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (it->is_regular_file(ec) && !ec && isAudioCandidate(it->path())) {
                inputs.push_back(absolutePath(it->path()));
            }
            ec.clear();
        }
    } else {
        for (std::filesystem::directory_iterator it(options.inputDirectory,
                                                    std::filesystem::directory_options::skip_permission_denied,
                                                    ec),
             end;
             it != end;
             it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (it->is_regular_file(ec) && !ec && isAudioCandidate(it->path())) {
                inputs.push_back(absolutePath(it->path()));
            }
            ec.clear();
        }
    }

    std::sort(inputs.begin(), inputs.end(), [](const std::filesystem::path& left,
                                               const std::filesystem::path& right) {
        return left.generic_string() < right.generic_string();
    });

    if (options.maxFiles > 0 && static_cast<int>(inputs.size()) > options.maxFiles) {
        inputs.resize(static_cast<std::size_t>(options.maxFiles));
    }
    return inputs;
}

OfflineExportOptions itemOptions(const BatchExportOptions& options,
                                 const std::filesystem::path& input,
                                 const std::filesystem::path& outputDirectory,
                                 const std::string& slug)
{
    OfflineExportOptions item;
    item.inputAudio = input;
    item.outputDirectory = outputDirectory;
    item.settings = options.settings;
    item.width = options.width;
    item.height = options.height;
    item.frameRate = options.frameRate;
    item.maxSeconds = options.maxSeconds;
    item.styleProfile = options.styleProfile;
    item.syncProfile = options.syncProfile;
    item.ffmpegExecutable = options.ffmpegExecutable;
    item.lookName = options.lookName;
    item.videoCrf = options.videoCrf;
    item.videoPreset = options.videoPreset;
    item.autoScene = options.autoScene;
    item.environmentTimeOfDay = options.environmentTimeOfDay;
    item.sharePackage = options.sharePackage;
    if (options.encodeMp4) {
        item.outputVideo = outputDirectory / (slug + ".mp4");
    }
    return item;
}

bool writeManifest(const BatchExportOptions& options,
                   const BatchExportResult& result,
                   const std::filesystem::path& inputRoot,
                   std::string& error)
{
    std::ofstream output(result.manifestPath);
    if (!output) {
        error = "Unable to write batch manifest.";
        return false;
    }

    output << "{\n";
    output << "  \"title\": \"Visualizer Batch Export\",\n";
    output << "  \"inputDirectory\": \"" << escapeJson(slashPath(inputRoot)) << "\",\n";
    output << "  \"outputDirectory\": \"" << escapeJson(slashPath(result.outputDirectory)) << "\",\n";
    output << "  \"look\": \"" << escapeJson(options.lookName.empty() ? "Custom" : options.lookName) << "\",\n";
    output << "  \"mode\": \"" << escapeJson(std::string(toString(options.settings.mode))) << "\",\n";
    output << "  \"palette\": \"" << escapeJson(std::string(toString(options.settings.palette))) << "\",\n";
    output << "  \"depth3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.depth3D, 0.0f, 1.0f) << ",\n";
    output << "  \"colorImpact\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.colorImpact, 0.0f, 1.0f) << ",\n";
    output << "  \"objectDensity3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.objectDensity3D, 0.0f, 1.0f) << ",\n";
    output << "  \"interactionDepth\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.interactionDepth, 0.0f, 1.0f) << ",\n";
    output << "  \"lightingGlow\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.lightingGlow, 0.0f, 1.0f) << ",\n";
    output << "  \"scenePersonality\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.scenePersonality, 0.0f, 1.0f) << ",\n";
    output << "  \"response3D\": " << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.response3D, 0.0f, 1.0f) << ",\n";
    output << "  \"width\": " << options.width << ",\n";
    output << "  \"height\": " << options.height << ",\n";
    output << "  \"frameRate\": " << options.frameRate << ",\n";
    output << "  \"maxSeconds\": " << std::fixed << std::setprecision(3) << options.maxSeconds << ",\n";
    output << "  \"autoScene\": " << ((options.autoScene || options.settings.autoScene) ? "true" : "false") << ",\n";
    output << "  \"sharePackages\": " << (options.sharePackage ? "true" : "false") << ",\n";
    output << "  \"mp4Encoded\": " << (options.encodeMp4 ? "true" : "false") << ",\n";
    output << "  \"filesDiscovered\": " << result.filesDiscovered << ",\n";
    output << "  \"filesExported\": " << result.filesExported << ",\n";
    output << "  \"filesFailed\": " << result.filesFailed << ",\n";
    output << "  \"items\": [\n";
    for (std::size_t i = 0; i < result.items.size(); ++i) {
        const BatchExportItemResult& item = result.items[i];
        output << "    {\n";
        output << "      \"inputAudio\": \"" << escapeJson(slashPath(relativeTo(item.inputAudio, inputRoot))) << "\",\n";
        output << "      \"outputDirectory\": \"" << escapeJson(slashPath(relativeTo(item.outputDirectory, result.outputDirectory))) << "\",\n";
        output << "      \"success\": " << (item.success ? "true" : "false") << ",\n";
        output << "      \"framesWritten\": " << item.framesWritten << ",\n";
        output << "      \"durationSeconds\": " << std::fixed << std::setprecision(3) << item.durationSeconds << ",\n";
        output << "      \"peakRms\": " << std::setprecision(4) << item.peakRms << ",\n";
        output << "      \"estimatedBpm\": " << std::setprecision(2) << item.estimatedBpm << ",\n";
        output << "      \"beatsDetected\": " << item.beatsDetected << ",\n";
        output << "      \"trackIntelligence\": {\n";
        output << "        \"downbeatsDetected\": " << item.trackSummary.downbeatsDetected << ",\n";
        output << "        \"phraseBoundariesDetected\": " << item.trackSummary.phraseBoundariesDetected << ",\n";
        output << "        \"averageBarConfidence\": " << std::fixed << std::setprecision(3)
               << item.trackSummary.averageBarConfidence << ",\n";
        output << "        \"averagePhraseConfidence\": " << std::fixed << std::setprecision(3)
               << item.trackSummary.averagePhraseConfidence << ",\n";
        output << "        \"peakDownbeatConfidence\": " << std::fixed << std::setprecision(3)
               << item.trackSummary.peakDownbeatConfidence << ",\n";
        output << "        \"peakDropIntensity\": " << std::fixed << std::setprecision(3)
               << item.trackSummary.peakDropIntensity << ",\n";
        output << "        \"peakPhraseIntensity\": " << std::fixed << std::setprecision(3)
               << item.trackSummary.peakPhraseIntensity << ",\n";
        output << "        \"peakBuildTension\": " << std::fixed << std::setprecision(3)
               << item.trackSummary.peakBuildTension << ",\n";
        output << "        \"averageHarmonicEnergy\": " << std::fixed << std::setprecision(3)
               << item.trackSummary.averageHarmonicEnergy << ",\n";
        output << "        \"dominantStyle\": \"" << escapeJson(std::string(toString(item.trackSummary.dominantStyle))) << "\",\n";
        output << "        \"dominantStyleConfidence\": " << std::fixed << std::setprecision(3)
               << item.trackSummary.dominantStyleConfidence << ",\n";
        output << "        \"maxPrimitiveCount\": " << item.trackSummary.maxPrimitiveCount << "\n";
        output << "      },\n";
        output << "      \"previewImage\": \""
               << escapeJson(item.previewImage.empty() ? std::string{} : slashPath(relativeTo(item.previewImage, result.outputDirectory)))
               << "\",\n";
        output << "      \"previewFrames\": " << item.previewFramesUsed << ",\n";
        output << "      \"previewWidth\": " << item.previewWidth << ",\n";
        output << "      \"previewHeight\": " << item.previewHeight << ",\n";
        output << "      \"videoEncoded\": " << (item.videoEncoded ? "true" : "false") << ",\n";
        output << "      \"sharePackageGenerated\": " << (item.sharePackageGenerated ? "true" : "false");
        if (!item.outputVideo.empty()) {
            output << ",\n";
            output << "      \"outputVideo\": \"" << escapeJson(slashPath(relativeTo(item.outputVideo, result.outputDirectory))) << "\"";
        }
        if (!item.sharePage.empty()) {
            output << ",\n";
            output << "      \"sharePage\": \"" << escapeJson(slashPath(relativeTo(item.sharePage, result.outputDirectory))) << "\"";
        }
        if (!item.shareManifest.empty()) {
            output << ",\n";
            output << "      \"shareManifest\": \"" << escapeJson(slashPath(relativeTo(item.shareManifest, result.outputDirectory))) << "\"";
        }
        if (!item.error.empty()) {
            output << ",\n";
            output << "      \"error\": \"" << escapeJson(item.error) << "\"";
        }
        output << "\n";
        output << "    }" << (i + 1U == result.items.size() ? "\n" : ",\n");
    }
    output << "  ]\n";
    output << "}\n";

    if (!output) {
        error = "Failed while writing batch manifest.";
        return false;
    }
    return true;
}

bool writeIndex(const BatchExportOptions& options,
                const BatchExportResult& result,
                const std::filesystem::path& inputRoot,
                std::string& error)
{
    std::ofstream output(result.indexPath);
    if (!output) {
        error = "Unable to write batch index page.";
        return false;
    }

    output << "<!doctype html>\n";
    output << "<html lang=\"en\">\n<head>\n";
    output << "<meta charset=\"utf-8\">\n";
    output << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    output << "<title>Visualizer Batch Export</title>\n";
    output << "<style>\n";
    output << "body{margin:0;background:#07090f;color:#eef3ff;font-family:Segoe UI,Arial,sans-serif;}\n";
    output << "main{max-width:1120px;margin:0 auto;padding:28px;}\n";
    output << "table{width:100%;border-collapse:collapse;margin-top:18px;}\n";
    output << "th,td{border-bottom:1px solid #263044;padding:9px;text-align:left;vertical-align:top;}\n";
    output << "th{color:#8fa3c8;font-size:12px;text-transform:uppercase;letter-spacing:.08em;}\n";
    output << ".thumb{width:180px;max-width:24vw;border:1px solid #263044;background:#000;display:block;}\n";
    output << ".ok{color:#6dffc8}.fail{color:#ff8d9a}a{color:#5ee7ff;}code{color:#c8d7f5;}\n";
    output << "</style>\n</head>\n<body>\n<main>\n";
    output << "<h1>Visualizer Batch Export</h1>\n";
    output << "<p>Rendered " << result.filesExported << " of " << result.filesDiscovered
           << " discovered audio files from <code>" << escapeHtml(slashPath(inputRoot)) << "</code>.</p>\n";
    output << "<p>Look: " << escapeHtml(options.lookName.empty() ? "Custom" : options.lookName)
           << " | Mode: " << escapeHtml(std::string(toString(options.settings.mode)))
           << " | Depth: " << escapeHtml(formatFloat(options.settings.depth3D, 2))
           << " | Objects: " << escapeHtml(formatFloat(options.settings.objectDensity3D, 2))
           << " | Mouse 3D: " << escapeHtml(formatFloat(options.settings.interactionDepth, 2))
           << " | Glow: " << escapeHtml(formatFloat(options.settings.lightingGlow, 2))
           << " | Color: " << escapeHtml(formatFloat(options.settings.colorImpact, 2))
           << " | Persona: " << escapeHtml(formatFloat(options.settings.scenePersonality, 2))
           << " | React: " << escapeHtml(formatFloat(options.settings.response3D, 2))
           << " | Resolution: " << options.width << " x " << options.height
           << " | FPS: " << options.frameRate << "</p>\n";
    output << "<p><a href=\"batch_manifest.json\">batch_manifest.json</a> contains machine-readable export metadata.</p>\n";
    output << "<table>\n<thead><tr><th>Status</th><th>Preview</th><th>Input</th><th>Frames</th><th>Peak RMS</th><th>BPM</th><th>Style</th><th>Sync</th><th>Phrase</th><th>Links</th></tr></thead>\n<tbody>\n";
    for (const BatchExportItemResult& item : result.items) {
        output << "<tr>";
        output << "<td class=\"" << (item.success ? "ok" : "fail") << "\">"
               << (item.success ? "Exported" : "Failed") << "</td>";
        output << "<td>";
        if (!item.previewImage.empty()) {
            output << "<img class=\"thumb\" src=\""
                   << escapeHtml(slashPath(relativeTo(item.previewImage, result.outputDirectory)))
                   << "\" alt=\"Preview for "
                   << escapeHtml(slashPath(relativeTo(item.inputAudio, inputRoot))) << "\">";
        }
        output << "</td>";
        output << "<td>" << escapeHtml(slashPath(relativeTo(item.inputAudio, inputRoot))) << "</td>";
        output << "<td>" << item.framesWritten << "</td>";
        output << "<td>" << std::fixed << std::setprecision(3) << item.peakRms << "</td>";
        output << "<td>" << std::setprecision(2) << item.estimatedBpm << "</td>";
        output << "<td>" << escapeHtml(std::string(toString(item.trackSummary.dominantStyle)))
               << " " << escapeHtml(formatFloat(item.trackSummary.dominantStyleConfidence, 2)) << "</td>";
        output << "<td>bar " << escapeHtml(formatFloat(item.trackSummary.averageBarConfidence, 2))
               << "<br>downbeats " << item.trackSummary.downbeatsDetected
               << "<br>drop " << escapeHtml(formatFloat(item.trackSummary.peakDropIntensity, 2)) << "</td>";
        output << "<td>lock " << escapeHtml(formatFloat(item.trackSummary.averagePhraseConfidence, 2))
               << "<br>boundaries " << item.trackSummary.phraseBoundariesDetected
               << "<br>tension " << escapeHtml(formatFloat(item.trackSummary.peakBuildTension, 2)) << "</td>";
        output << "<td>";
        if (item.sharePackageGenerated && !item.sharePage.empty()) {
            output << "<a href=\"" << escapeHtml(slashPath(relativeTo(item.sharePage, result.outputDirectory))) << "\">share page</a> ";
        }
        if (!item.shareManifest.empty()) {
            output << "<a href=\"" << escapeHtml(slashPath(relativeTo(item.shareManifest, result.outputDirectory))) << "\">manifest</a> ";
        }
        if (!item.outputVideo.empty()) {
            output << "<a href=\"" << escapeHtml(slashPath(relativeTo(item.outputVideo, result.outputDirectory))) << "\">mp4</a> ";
        }
        if (!item.error.empty()) {
            output << escapeHtml(item.error);
        }
        output << "</td>";
        output << "</tr>\n";
    }
    output << "</tbody>\n</table>\n";
    output << "</main>\n</body>\n</html>\n";

    if (!output) {
        error = "Failed while writing batch index page.";
        return false;
    }
    return true;
}

} // namespace

bool exportAudioBatch(const BatchExportOptions& options,
                      BatchExportResult& result,
                      std::string& error)
{
    error.clear();
    result = {};

    if (options.inputDirectory.empty()) {
        error = "Input directory is required.";
        return false;
    }
    if (options.outputDirectory.empty()) {
        error = "Output directory is required.";
        return false;
    }
    if (options.width <= 0 || options.height <= 0) {
        error = "Export dimensions must be positive.";
        return false;
    }
    if (options.frameRate <= 0 || options.frameRate > 240) {
        error = "Frame rate must be between 1 and 240.";
        return false;
    }
    if (options.videoCrf < 0 || options.videoCrf > 51) {
        error = "Video CRF must be between 0 and 51.";
        return false;
    }

    const std::filesystem::path inputRoot = absolutePath(options.inputDirectory);
    std::error_code ec;
    if (!std::filesystem::is_directory(inputRoot, ec) || ec) {
        error = "Input directory does not exist: " + slashPath(inputRoot);
        return false;
    }

    result.outputDirectory = absolutePath(options.outputDirectory);
    result.manifestPath = result.outputDirectory / "batch_manifest.json";
    result.indexPath = result.outputDirectory / "index.html";

    std::filesystem::create_directories(result.outputDirectory, ec);
    if (ec) {
        error = "Unable to create batch output directory: " + ec.message();
        return false;
    }

    const std::vector<std::filesystem::path> inputs = collectAudioInputs(options);
    result.filesDiscovered = static_cast<int>(inputs.size());
    if (inputs.empty()) {
        error = "No supported audio files found in input directory.";
        return false;
    }

    for (std::size_t i = 0; i < inputs.size(); ++i) {
        const std::string slug = numberedSlug(static_cast<int>(i + 1U), inputs[i]);
        const std::filesystem::path itemDirectory = result.outputDirectory / slug;
        BatchExportItemResult item;
        item.inputAudio = inputs[i];
        item.outputDirectory = itemDirectory;

        OfflineExportResult exportResult;
        std::string exportError;
        const OfflineExportOptions exportOptions = itemOptions(options, inputs[i], itemDirectory, slug);
        if (exportAudioToFrames(exportOptions, exportResult, exportError)) {
            item.outputDirectory = exportResult.outputDirectory;
            item.outputVideo = exportResult.outputVideo;
            item.shareManifest = exportResult.shareManifest;
            item.sharePage = exportResult.sharePage;
            item.previewImage = exportResult.previewImage;
            item.previewFramesUsed = exportResult.previewFramesUsed;
            item.previewWidth = exportResult.previewWidth;
            item.previewHeight = exportResult.previewHeight;
            item.framesWritten = exportResult.framesWritten;
            item.durationSeconds = exportResult.durationSeconds;
            item.peakRms = exportResult.peakRms;
            item.estimatedBpm = exportResult.estimatedBpm;
            item.beatsDetected = exportResult.beatsDetected;
            item.trackSummary = exportResult.trackSummary;
            item.videoEncoded = exportResult.videoEncoded;
            item.sharePackageGenerated = exportResult.sharePackageGenerated;
            item.success = true;
            ++result.filesExported;
        } else {
            item.error = exportError.empty() ? "Export failed." : exportError;
            ++result.filesFailed;
        }
        result.items.push_back(std::move(item));
    }

    if (!writeManifest(options, result, inputRoot, error)) {
        return false;
    }
    if (!writeIndex(options, result, inputRoot, error)) {
        return false;
    }

    return result.filesExported > 0;
}

} // namespace viz
