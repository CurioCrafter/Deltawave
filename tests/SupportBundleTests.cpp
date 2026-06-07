#include "TestHarness.hpp"
#include "Visualizer/Support/SupportBundle.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace viz::tests {
namespace {

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool bundleContainsFileNamed(const std::filesystem::path& root, const std::string& filename)
{
    std::error_code ec;
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
        if (it->path().filename() == filename) {
            return true;
        }
    }
    return false;
}

} // namespace

void supportBundleWritesDiagnosticsWithoutCopyingLargeMedia()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_support_bundle_test";
    const std::filesystem::path output = root / "bundle";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "profiles" / "sources");
    std::filesystem::create_directories(root / "captures" / "visualizer_20260607_010203");
    std::filesystem::create_directories(root / "artifacts" / "export_smoke" / "frames");
    std::filesystem::create_directories(root / "artifacts" / "support_bundle_previous" / "metadata_files" / "captures" / "old");
    std::filesystem::create_directories(root / "docs");
    std::filesystem::create_directories(root / "scripts");

    {
        std::ofstream profile(root / "profiles" / "sources" / "live_loopback.vizaudio");
        profile << "style=Techno\nlearnedWeight=0.5\n";
    }
    {
        std::ofstream profile(root / "profiles" / "sources" / "live_loopback.vizsync");
        profile << "beatSensitivity=1.2\nsectionSensitivity=1.1\nlearnedWeight=0.4\n";
    }
    {
        std::ofstream manifest(root / "captures" / "visualizer_20260607_010203" / "capture_manifest.json");
        manifest << "{\"title\":\"Visualizer Live Capture\",\"framesWritten\":12}\n";
    }
    {
        std::ofstream timeline(root / "captures" / "visualizer_20260607_010203" / "analysis_timeline.csv");
        timeline << "frame,timeSeconds,mode,palette\n0,0.0,Phase Weave,Neon Voltage\n";
    }
    {
        std::ofstream frame(root / "captures" / "visualizer_20260607_010203" / "frame_000000.ppm",
                            std::ios::binary);
        frame << "P6\n1 1\n255\n";
        const unsigned char pixel[3] = {0, 0, 0};
        frame.write(reinterpret_cast<const char*>(pixel), 3);
    }
    {
        std::ofstream manifest(root / "artifacts" / "export_smoke" / "frames" / "share_manifest.json");
        manifest << "{\"title\":\"Visualizer Export\",\"look\":\"Phase Weave\"}\n";
    }
    {
        std::ofstream manifest(root / "artifacts" / "export_smoke" / "frames" / "export_manifest.txt");
        manifest << "frames=3\nsyncProfile=profiles/sources/live_loopback.vizsync\n";
    }
    {
        std::ofstream manifest(root / "artifacts" / "export_smoke" / "frames" / "batch_manifest.json");
        manifest << "{\"title\":\"Visualizer Batch Export\",\"filesExported\":1}\n";
    }
    {
        std::ofstream manifest(root / "artifacts" / "support_bundle_previous" / "support_manifest.json");
        manifest << "{\"title\":\"Older Support Bundle\"}\n";
    }
    {
        std::ofstream copiedManifest(root / "artifacts" / "support_bundle_previous" / "metadata_files" / "captures" / "old" / "capture_manifest.json");
        copiedManifest << "{\"title\":\"Copied Old Capture\"}\n";
    }
    {
        std::ofstream readme(root / "README.md");
        readme << "# Visualizer\n";
    }
    {
        std::ofstream support(root / "docs" / "SUPPORT.md");
        support << "# Support\n";
    }
    {
        std::ofstream script(root / "scripts" / "build.ps1");
        script << "Write-Host build\n";
    }

    SupportBundleOptions options;
    options.workspaceRoot = root;
    options.outputDirectory = output;
    options.maxCaptures = 2;
    options.maxArtifacts = 2;
    options.maxCopiedFileBytes = 1024;

    SupportBundleResult result;
    std::string error;
    require(writeSupportBundle(options, result, error), "support bundle should write: " + error);
    require(std::filesystem::exists(output / "support_manifest.json"), "support manifest should exist");
    require(std::filesystem::exists(output / "support_summary.txt"), "support summary should exist");
    require(result.profilesFound == 2, "support bundle should count adaptive audio profiles");
    require(result.profilesCopied == 2, "support bundle should copy small profile files");
    require(result.captureDirectoriesFound == 1, "support bundle should summarize recent live captures");
    require(result.artifactDirectoriesFound == 1, "support bundle should summarize recent export artifacts");
    require(result.metadataFilesCopied >= 5, "support bundle should copy capture/export/batch metadata");

    const std::string manifest = readText(output / "support_manifest.json");
    require(manifest.find("\"title\": \"Visualizer Support Bundle\"") != std::string::npos,
            "support manifest should have a stable title");
    require(manifest.find("live_loopback.vizsync") != std::string::npos,
            "support manifest should include sync profile metadata");
    require(manifest.find("capture_manifest.json") != std::string::npos,
            "support manifest should include live capture metadata");
    require(manifest.find("share_manifest.json") != std::string::npos,
            "support manifest should include export share metadata");
    require(manifest.find("batch_manifest.json") != std::string::npos,
            "support manifest should include batch export metadata");
    require(manifest.find("support_bundle_previous") == std::string::npos,
            "support bundle scan should ignore earlier support bundles");
    require(manifest.find("\"largeMediaCopied\": false") != std::string::npos,
            "support manifest should document that media payloads are not copied");
    require(!bundleContainsFileNamed(output, "frame_000000.ppm"),
            "support bundle should not copy large render frame payloads");

    std::filesystem::remove_all(root);
}

} // namespace viz::tests
