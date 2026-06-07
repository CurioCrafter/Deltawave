#include "Visualizer/Support/SupportBundle.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string_view>
#include <vector>

namespace viz {
namespace {

constexpr std::string_view kBundleVersion = "1";

struct FileRecord {
    std::filesystem::path absolutePath;
    std::filesystem::path relativePath;
    std::filesystem::path copiedRelativePath;
    std::uintmax_t size = 0;
    std::filesystem::file_time_type modified{};
    bool copied = false;
    std::string skipReason;
};

struct DiagnosticDirectory {
    std::filesystem::path absolutePath;
    std::filesystem::path relativePath;
    std::filesystem::file_time_type latestModified{};
    std::vector<FileRecord> metadataFiles;
    int renderPayloadFiles = 0;
    std::uintmax_t renderPayloadBytes = 0;
};

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

std::string slashPath(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::tm localTime(std::time_t value)
{
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    return local;
}

std::string timestampForDirectory()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::tm local = localTime(now);
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d_%H%M%S");
    return output.str();
}

std::string timestampForManifest()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::tm local = localTime(now);
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

std::string formatFileTime(const std::filesystem::file_time_type& value)
{
    if (value == std::filesystem::file_time_type{}) {
        return {};
    }

    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        value - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    const std::time_t asTime = std::chrono::system_clock::to_time_t(systemTime);
    const std::tm local = localTime(asTime);
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
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

bool isInsidePath(const std::filesystem::path& path, const std::filesystem::path& parent)
{
    if (parent.empty()) {
        return false;
    }

    std::error_code ec;
    const std::filesystem::path absoluteChild = std::filesystem::weakly_canonical(path, ec);
    const std::filesystem::path child = ec ? absolutePath(path) : absoluteChild;
    ec.clear();
    const std::filesystem::path absoluteParent = std::filesystem::weakly_canonical(parent, ec);
    const std::filesystem::path root = ec ? absolutePath(parent) : absoluteParent;

    auto childIt = child.begin();
    auto rootIt = root.begin();
    for (; rootIt != root.end(); ++rootIt, ++childIt) {
        if (childIt == child.end()) {
            return false;
        }
#if defined(_WIN32)
        if (lower(childIt->string()) != lower(rootIt->string())) {
            return false;
        }
#else
        if (*childIt != *rootIt) {
            return false;
        }
#endif
    }
    return true;
}

std::uintmax_t fileSize(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    return ec ? 0U : size;
}

std::filesystem::file_time_type lastWrite(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::file_time_type modified = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : modified;
}

bool hasProfileExtension(const std::filesystem::path& path)
{
    const std::string extension = lower(path.extension().string());
    return extension == ".vizaudio" || extension == ".vizsync" || extension == ".vizpreset";
}

bool isMetadataFile(const std::filesystem::path& path)
{
    const std::string filename = lower(path.filename().string());
    return filename == "capture_manifest.json" ||
           filename == "share_manifest.json" ||
           filename == "batch_manifest.json" ||
           filename == "export_manifest.txt" ||
           filename == "analysis_timeline.csv";
}

bool isRenderPayloadFile(const std::filesystem::path& path)
{
    const std::string extension = lower(path.extension().string());
    return extension == ".ppm" ||
           extension == ".mp4" ||
           extension == ".wav" ||
           extension == ".wave" ||
           extension == ".mp3" ||
           extension == ".aac" ||
           extension == ".m4a" ||
           extension == ".wma" ||
           extension == ".flac" ||
           extension == ".ogg" ||
           extension == ".mov" ||
           extension == ".avi";
}

bool isSupportBundleDirectory(const std::filesystem::path& path)
{
    const std::string directoryName = lower(path.filename().string());
    return directoryName.rfind("support_bundle", 0) == 0;
}

std::vector<FileRecord> collectProfileFiles(const std::filesystem::path& workspaceRoot)
{
    std::vector<FileRecord> profiles;
    const std::filesystem::path profileRoot = workspaceRoot / "profiles";
    std::error_code ec;
    if (!std::filesystem::exists(profileRoot, ec)) {
        return profiles;
    }

    for (std::filesystem::recursive_directory_iterator it(profileRoot,
                                                          std::filesystem::directory_options::skip_permission_denied,
                                                          ec),
         end;
         it != end;
         it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec) || ec || !hasProfileExtension(it->path())) {
            ec.clear();
            continue;
        }

        FileRecord record;
        record.absolutePath = absolutePath(it->path());
        record.relativePath = relativeTo(record.absolutePath, workspaceRoot);
        record.size = fileSize(record.absolutePath);
        record.modified = lastWrite(record.absolutePath);
        profiles.push_back(record);
    }

    std::sort(profiles.begin(), profiles.end(), [](const FileRecord& left, const FileRecord& right) {
        return left.relativePath.generic_string() < right.relativePath.generic_string();
    });
    return profiles;
}

void addMetadataFile(std::map<std::string, DiagnosticDirectory>& entries,
                     const std::filesystem::path& workspaceRoot,
                     const std::filesystem::path& path)
{
    const std::filesystem::path absolute = absolutePath(path);
    const std::filesystem::path directory = absolute.parent_path();
    const std::string key = slashPath(directory);
    DiagnosticDirectory& entry = entries[key];
    entry.absolutePath = directory;
    entry.relativePath = relativeTo(directory, workspaceRoot);

    FileRecord record;
    record.absolutePath = absolute;
    record.relativePath = relativeTo(absolute, workspaceRoot);
    record.size = fileSize(absolute);
    record.modified = lastWrite(absolute);
    entry.latestModified = std::max(entry.latestModified, record.modified);
    entry.metadataFiles.push_back(record);
}

void countRenderPayload(DiagnosticDirectory& entry)
{
    std::error_code ec;
    if (!std::filesystem::exists(entry.absolutePath, ec)) {
        return;
    }

    for (std::filesystem::recursive_directory_iterator it(entry.absolutePath,
                                                          std::filesystem::directory_options::skip_permission_denied,
                                                          ec),
         end;
         it != end;
         it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec) || ec || !isRenderPayloadFile(it->path())) {
            ec.clear();
            continue;
        }
        ++entry.renderPayloadFiles;
        entry.renderPayloadBytes += fileSize(it->path());
    }
}

std::vector<DiagnosticDirectory> collectDiagnosticDirectories(const std::filesystem::path& workspaceRoot,
                                                              const std::filesystem::path& outputDirectory,
                                                              std::string_view subdirectory,
                                                              int maximum)
{
    std::map<std::string, DiagnosticDirectory> entries;
    const std::filesystem::path root = workspaceRoot / subdirectory;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return {};
    }

    for (std::filesystem::recursive_directory_iterator it(root,
                                                          std::filesystem::directory_options::skip_permission_denied,
                                                          ec),
         end;
         it != end;
         it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (isInsidePath(it->path(), outputDirectory)) {
            if (it->is_directory(ec)) {
                it.disable_recursion_pending();
            }
            ec.clear();
            continue;
        }
        if (it->is_directory(ec) && !ec && isSupportBundleDirectory(it->path())) {
            it.disable_recursion_pending();
            continue;
        }
        ec.clear();
        if (!it->is_regular_file(ec) || ec || !isMetadataFile(it->path())) {
            ec.clear();
            continue;
        }
        addMetadataFile(entries, workspaceRoot, it->path());
    }

    std::vector<DiagnosticDirectory> directories;
    directories.reserve(entries.size());
    for (auto& [unused, entry] : entries) {
        (void)unused;
        std::sort(entry.metadataFiles.begin(), entry.metadataFiles.end(), [](const FileRecord& left, const FileRecord& right) {
            return left.relativePath.generic_string() < right.relativePath.generic_string();
        });
        countRenderPayload(entry);
        directories.push_back(std::move(entry));
    }

    std::sort(directories.begin(), directories.end(), [](const DiagnosticDirectory& left, const DiagnosticDirectory& right) {
        if (left.latestModified == right.latestModified) {
            return left.relativePath.generic_string() < right.relativePath.generic_string();
        }
        return left.latestModified > right.latestModified;
    });

    if (maximum >= 0 && static_cast<int>(directories.size()) > maximum) {
        directories.resize(static_cast<std::size_t>(maximum));
    }
    return directories;
}

bool copySmallFile(FileRecord& record,
                   const std::filesystem::path& outputDirectory,
                   const std::filesystem::path& category,
                   std::uintmax_t maximumBytes,
                   SupportBundleResult& result,
                   std::string& error)
{
    if (record.size > maximumBytes) {
        record.skipReason = "larger than maxCopiedFileBytes";
        ++result.filesSkippedForSize;
        return true;
    }

    const std::filesystem::path destination = outputDirectory / category / record.relativePath;
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        error = "Unable to create support bundle directory: " + ec.message();
        return false;
    }

    std::filesystem::copy_file(record.absolutePath,
                               destination,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
        error = "Unable to copy " + slashPath(record.relativePath) + ": " + ec.message();
        return false;
    }

    record.copied = true;
    record.copiedRelativePath = relativeTo(destination, outputDirectory);
    result.copiedBytes += record.size;
    return true;
}

std::vector<FileRecord> collectWorkspaceFiles(const std::filesystem::path& workspaceRoot)
{
    const std::filesystem::path candidates[] = {
        "README.md",
        "CMakeLists.txt",
        "scripts/build.ps1",
        "docs/ARCHITECTURE.md",
        "docs/CONTROLS.md",
        "docs/EXPORT.md",
        "docs/PERFORMANCE.md",
        "docs/SUPPORT.md"
    };

    std::vector<FileRecord> files;
    for (const std::filesystem::path& candidate : candidates) {
        const std::filesystem::path absolute = workspaceRoot / candidate;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(absolute, ec) || ec) {
            continue;
        }

        FileRecord record;
        record.absolutePath = absolutePath(absolute);
        record.relativePath = relativeTo(record.absolutePath, workspaceRoot);
        record.size = fileSize(record.absolutePath);
        record.modified = lastWrite(record.absolutePath);
        files.push_back(record);
    }
    return files;
}

std::vector<FileRecord> collectBinaryFiles(const std::filesystem::path& workspaceRoot)
{
    const std::filesystem::path candidates[] = {
        "build/vs2022/Release/Visualizer.exe",
        "build/vs2022/Release/VisualizerExport.exe",
        "build/vs2022/Release/VisualizerBenchmark.exe",
        "build/vs2022/Release/VisualizerSupport.exe",
        "build/vs2022/Release/visualizer_tests.exe"
    };

    std::vector<FileRecord> files;
    for (const std::filesystem::path& candidate : candidates) {
        const std::filesystem::path absolute = workspaceRoot / candidate;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(absolute, ec) || ec) {
            continue;
        }

        FileRecord record;
        record.absolutePath = absolutePath(absolute);
        record.relativePath = relativeTo(record.absolutePath, workspaceRoot);
        record.size = fileSize(record.absolutePath);
        record.modified = lastWrite(record.absolutePath);
        files.push_back(record);
    }
    return files;
}

void writeFileArray(std::ofstream& output,
                    std::string_view name,
                    const std::vector<FileRecord>& files,
                    bool trailingComma)
{
    output << "  \"" << name << "\": [\n";
    for (std::size_t i = 0; i < files.size(); ++i) {
        const FileRecord& file = files[i];
        output << "    {\n";
        output << "      \"path\": \"" << escapeJson(slashPath(file.relativePath)) << "\",\n";
        output << "      \"sizeBytes\": " << file.size << ",\n";
        output << "      \"lastModified\": \"" << escapeJson(formatFileTime(file.modified)) << "\"";
        if (file.copied || !file.skipReason.empty()) {
            output << ",\n";
            output << "      \"copied\": " << (file.copied ? "true" : "false");
            if (file.copied) {
                output << ",\n";
                output << "      \"bundlePath\": \"" << escapeJson(slashPath(file.copiedRelativePath)) << "\"";
            } else {
                output << ",\n";
                output << "      \"skipReason\": \"" << escapeJson(file.skipReason) << "\"";
            }
        }
        output << "\n";
        output << "    }" << (i + 1U == files.size() ? "\n" : ",\n");
    }
    output << "  ]" << (trailingComma ? "," : "") << "\n";
}

void writeDirectoryArray(std::ofstream& output,
                         std::string_view name,
                         const std::vector<DiagnosticDirectory>& directories,
                         bool trailingComma)
{
    output << "  \"" << name << "\": [\n";
    for (std::size_t i = 0; i < directories.size(); ++i) {
        const DiagnosticDirectory& directory = directories[i];
        output << "    {\n";
        output << "      \"path\": \"" << escapeJson(slashPath(directory.relativePath)) << "\",\n";
        output << "      \"lastModified\": \"" << escapeJson(formatFileTime(directory.latestModified)) << "\",\n";
        output << "      \"renderPayloadFiles\": " << directory.renderPayloadFiles << ",\n";
        output << "      \"renderPayloadBytes\": " << directory.renderPayloadBytes << ",\n";
        output << "      \"metadataFiles\": [\n";
        for (std::size_t j = 0; j < directory.metadataFiles.size(); ++j) {
            const FileRecord& file = directory.metadataFiles[j];
            output << "        {\n";
            output << "          \"path\": \"" << escapeJson(slashPath(file.relativePath)) << "\",\n";
            output << "          \"sizeBytes\": " << file.size << ",\n";
            output << "          \"lastModified\": \"" << escapeJson(formatFileTime(file.modified)) << "\",\n";
            output << "          \"copied\": " << (file.copied ? "true" : "false");
            if (file.copied) {
                output << ",\n";
                output << "          \"bundlePath\": \"" << escapeJson(slashPath(file.copiedRelativePath)) << "\"\n";
            } else if (!file.skipReason.empty()) {
                output << ",\n";
                output << "          \"skipReason\": \"" << escapeJson(file.skipReason) << "\"\n";
            } else {
                output << "\n";
            }
            output << "        }" << (j + 1U == directory.metadataFiles.size() ? "\n" : ",\n");
        }
        output << "      ]\n";
        output << "    }" << (i + 1U == directories.size() ? "\n" : ",\n");
    }
    output << "  ]" << (trailingComma ? "," : "") << "\n";
}

bool writeManifest(const std::filesystem::path& manifestPath,
                   const std::filesystem::path& workspaceRoot,
                   const SupportBundleOptions& options,
                   const SupportBundleResult& result,
                   const std::vector<FileRecord>& profiles,
                   const std::vector<DiagnosticDirectory>& captures,
                   const std::vector<DiagnosticDirectory>& artifacts,
                   const std::vector<FileRecord>& workspaceFiles,
                   const std::vector<FileRecord>& binaries,
                   const std::string& generatedAt,
                   std::string& error)
{
    std::ofstream output(manifestPath);
    if (!output) {
        error = "Unable to write support manifest.";
        return false;
    }

    output << "{\n";
    output << "  \"title\": \"Visualizer Support Bundle\",\n";
    output << "  \"version\": \"" << kBundleVersion << "\",\n";
    output << "  \"generatedAt\": \"" << escapeJson(generatedAt) << "\",\n";
    output << "  \"workspaceRoot\": \"" << escapeJson(slashPath(workspaceRoot)) << "\",\n";
    output << "  \"bundleDirectory\": \"" << escapeJson(slashPath(result.bundleDirectory)) << "\",\n";
    output << "  \"copyPolicy\": {\n";
    output << "    \"copySmallFiles\": " << (options.copySmallFiles ? "true" : "false") << ",\n";
    output << "    \"maxCopiedFileBytes\": " << options.maxCopiedFileBytes << ",\n";
    output << "    \"largeMediaCopied\": false\n";
    output << "  },\n";
    output << "  \"summary\": {\n";
    output << "    \"profilesFound\": " << result.profilesFound << ",\n";
    output << "    \"profilesCopied\": " << result.profilesCopied << ",\n";
    output << "    \"captureDirectoriesFound\": " << result.captureDirectoriesFound << ",\n";
    output << "    \"artifactDirectoriesFound\": " << result.artifactDirectoriesFound << ",\n";
    output << "    \"metadataFilesCopied\": " << result.metadataFilesCopied << ",\n";
    output << "    \"filesSkippedForSize\": " << result.filesSkippedForSize << ",\n";
    output << "    \"copiedBytes\": " << result.copiedBytes << "\n";
    output << "  },\n";
    writeFileArray(output, "profiles", profiles, true);
    writeDirectoryArray(output, "recentCaptures", captures, true);
    writeDirectoryArray(output, "recentArtifacts", artifacts, true);
    writeFileArray(output, "workspaceFiles", workspaceFiles, true);
    writeFileArray(output, "binaries", binaries, false);
    output << "}\n";

    if (!output) {
        error = "Failed while writing support manifest.";
        return false;
    }
    return true;
}

bool writeSummary(const std::filesystem::path& summaryPath,
                  const std::filesystem::path& workspaceRoot,
                  const SupportBundleOptions& options,
                  const SupportBundleResult& result,
                  const std::vector<DiagnosticDirectory>& captures,
                  const std::vector<DiagnosticDirectory>& artifacts,
                  const std::string& generatedAt,
                  std::string& error)
{
    std::ofstream output(summaryPath);
    if (!output) {
        error = "Unable to write support summary.";
        return false;
    }

    output << "Visualizer Support Bundle\n";
    output << "Generated: " << generatedAt << "\n";
    output << "Workspace: " << slashPath(workspaceRoot) << "\n";
    output << "Bundle: " << slashPath(result.bundleDirectory) << "\n\n";
    output << "Copy policy\n";
    output << "- Small metadata/profile copy: " << (options.copySmallFiles ? "enabled" : "disabled") << "\n";
    output << "- Max copied file bytes: " << options.maxCopiedFileBytes << "\n";
    output << "- Large PPM, MP4, and audio payloads are summarized, not copied.\n\n";
    output << "Summary\n";
    output << "- Profiles: " << result.profilesFound << " found, " << result.profilesCopied << " copied\n";
    output << "- Recent live capture directories: " << result.captureDirectoriesFound << "\n";
    output << "- Recent export/artifact directories: " << result.artifactDirectoriesFound << "\n";
    output << "- Metadata files copied: " << result.metadataFilesCopied << "\n";
    output << "- Files skipped for size: " << result.filesSkippedForSize << "\n";
    output << "- Copied bytes: " << result.copiedBytes << "\n\n";

    output << "Recent live captures\n";
    if (captures.empty()) {
        output << "- None found\n";
    } else {
        for (const DiagnosticDirectory& capture : captures) {
            output << "- " << slashPath(capture.relativePath)
                   << " | metadata " << capture.metadataFiles.size()
                   << " | render payload files " << capture.renderPayloadFiles
                   << " | render payload bytes " << capture.renderPayloadBytes << "\n";
        }
    }

    output << "\nRecent export artifacts\n";
    if (artifacts.empty()) {
        output << "- None found\n";
    } else {
        for (const DiagnosticDirectory& artifact : artifacts) {
            output << "- " << slashPath(artifact.relativePath)
                   << " | metadata " << artifact.metadataFiles.size()
                   << " | render payload files " << artifact.renderPayloadFiles
                   << " | render payload bytes " << artifact.renderPayloadBytes << "\n";
        }
    }

    output << "\nSee support_manifest.json for machine-readable details.\n";
    if (!output) {
        error = "Failed while writing support summary.";
        return false;
    }
    return true;
}

std::filesystem::path resolveWorkspaceRoot(const SupportBundleOptions& options)
{
    const std::filesystem::path requested = options.workspaceRoot.empty()
        ? std::filesystem::current_path()
        : options.workspaceRoot;
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(requested, ec);
    return ec ? absolutePath(requested) : canonical;
}

std::filesystem::path resolveOutputDirectory(const SupportBundleOptions& options,
                                             const std::filesystem::path& workspaceRoot)
{
    if (options.outputDirectory.empty()) {
        return workspaceRoot / "artifacts" / ("support_bundle_" + timestampForDirectory());
    }

    if (options.outputDirectory.is_absolute()) {
        return absolutePath(options.outputDirectory);
    }
    return absolutePath(workspaceRoot / options.outputDirectory);
}

} // namespace

bool writeSupportBundle(const SupportBundleOptions& options,
                        SupportBundleResult& result,
                        std::string& error)
{
    error.clear();
    result = SupportBundleResult{};

    const std::filesystem::path workspaceRoot = resolveWorkspaceRoot(options);
    const std::filesystem::path outputDirectory = resolveOutputDirectory(options, workspaceRoot);
    const int maxCaptures = std::max(0, options.maxCaptures);
    const int maxArtifacts = std::max(0, options.maxArtifacts);

    std::error_code ec;
    if (!std::filesystem::exists(workspaceRoot, ec) || !std::filesystem::is_directory(workspaceRoot, ec)) {
        error = "Workspace root does not exist or is not a directory: " + slashPath(workspaceRoot);
        return false;
    }

    std::vector<FileRecord> profiles = collectProfileFiles(workspaceRoot);
    std::vector<DiagnosticDirectory> captures = collectDiagnosticDirectories(workspaceRoot,
                                                                             outputDirectory,
                                                                             "captures",
                                                                             maxCaptures);
    std::vector<DiagnosticDirectory> artifacts = collectDiagnosticDirectories(workspaceRoot,
                                                                              outputDirectory,
                                                                              "artifacts",
                                                                              maxArtifacts);
    const std::vector<FileRecord> workspaceFiles = collectWorkspaceFiles(workspaceRoot);
    const std::vector<FileRecord> binaries = collectBinaryFiles(workspaceRoot);

    std::filesystem::create_directories(outputDirectory, ec);
    if (ec) {
        error = "Unable to create support bundle directory: " + ec.message();
        return false;
    }

    result.bundleDirectory = outputDirectory;
    result.manifestPath = outputDirectory / "support_manifest.json";
    result.summaryPath = outputDirectory / "support_summary.txt";
    result.profilesFound = static_cast<int>(profiles.size());
    result.captureDirectoriesFound = static_cast<int>(captures.size());
    result.artifactDirectoriesFound = static_cast<int>(artifacts.size());

    if (options.copySmallFiles) {
        for (FileRecord& profile : profiles) {
            if (!copySmallFile(profile,
                               outputDirectory,
                               "profile_files",
                               options.maxCopiedFileBytes,
                               result,
                               error)) {
                return false;
            }
            if (profile.copied) {
                ++result.profilesCopied;
            }
        }

        const auto copyMetadata = [&](std::vector<DiagnosticDirectory>& directories) -> bool {
            for (DiagnosticDirectory& directory : directories) {
                for (FileRecord& metadata : directory.metadataFiles) {
                    if (!copySmallFile(metadata,
                                       outputDirectory,
                                       "metadata_files",
                                       options.maxCopiedFileBytes,
                                       result,
                                       error)) {
                        return false;
                    }
                    if (metadata.copied) {
                        ++result.metadataFilesCopied;
                    }
                }
            }
            return true;
        };

        if (!copyMetadata(captures) || !copyMetadata(artifacts)) {
            return false;
        }
    }

    const std::string generatedAt = timestampForManifest();
    if (!writeManifest(result.manifestPath,
                       workspaceRoot,
                       options,
                       result,
                       profiles,
                       captures,
                       artifacts,
                       workspaceFiles,
                       binaries,
                       generatedAt,
                       error)) {
        return false;
    }
    return writeSummary(result.summaryPath,
                        workspaceRoot,
                        options,
                        result,
                        captures,
                        artifacts,
                        generatedAt,
                        error);
}

} // namespace viz
