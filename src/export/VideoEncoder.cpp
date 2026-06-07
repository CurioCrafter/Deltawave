#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "Visualizer/Export/VideoEncoder.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <vector>

namespace viz {
namespace {

bool containsUnsafeShellCharacter(const std::string& value)
{
    for (const char ch : value) {
        if (ch == '"' || ch == '\n' || ch == '\r') {
            return true;
        }
    }
    return false;
}

std::string quoteArgument(const std::string& value)
{
    return "\"" + value + "\"";
}

bool appendQuotedPath(std::ostringstream& command,
                      const std::filesystem::path& path,
                      const char* label,
                      std::string& error)
{
    const std::string value = path.string();
    if (value.empty()) {
        error = std::string(label) + " path is required.";
        return false;
    }
    if (containsUnsafeShellCharacter(value)) {
        error = std::string(label) + " path contains unsupported quote or newline characters.";
        return false;
    }
    command << quoteArgument(value);
    return true;
}

bool isValidPreset(std::string_view preset)
{
    constexpr std::array<std::string_view, 9> kAllowed = {
        "ultrafast",
        "superfast",
        "veryfast",
        "faster",
        "fast",
        "medium",
        "slow",
        "slower",
        "veryslow"
    };

    for (const std::string_view allowed : kAllowed) {
        if (preset == allowed) {
            return true;
        }
    }
    return false;
}

bool validateOptions(const VideoEncodeOptions& options, std::string& error)
{
    error.clear();
    if (options.framesDirectory.empty()) {
        error = "Frame directory is required.";
        return false;
    }
    if (options.outputMp4.empty()) {
        error = "Output MP4 path is required.";
        return false;
    }
    if (options.frameRate <= 0 || options.frameRate > 240) {
        error = "Video frame rate must be between 1 and 240.";
        return false;
    }
    if (options.crf < 0 || options.crf > 51) {
        error = "Video CRF must be between 0 and 51.";
        return false;
    }
    if (!isValidPreset(options.preset)) {
        error = "Unsupported FFmpeg preset. Use ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, or veryslow.";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(options.framesDirectory, ec) ||
        !std::filesystem::is_directory(options.framesDirectory, ec)) {
        error = "Frame directory does not exist.";
        return false;
    }

    const std::filesystem::path firstFrame = options.framesDirectory / "frame_000000.ppm";
    if (!std::filesystem::exists(firstFrame, ec)) {
        error = "Frame directory is missing frame_000000.ppm.";
        return false;
    }
    return true;
}

bool ensureParentDirectory(const std::filesystem::path& outputMp4, std::string& error)
{
    const std::filesystem::path parent = outputMp4.parent_path();
    if (parent.empty()) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        error = "Unable to create MP4 output directory: " + ec.message();
        return false;
    }
    return true;
}

#if defined(_WIN32)
std::string formatLastWindowsError(DWORD errorCode)
{
    std::ostringstream stream;
    stream << "Windows error " << errorCode;
    return stream.str();
}
#endif

int runCommand(const std::string& command, std::string& error)
{
#if defined(_WIN32)
    std::wstring wideCommand(command.begin(), command.end());
    std::vector<wchar_t> mutableCommand(wideCommand.begin(), wideCommand.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    if (CreateProcessW(nullptr,
                       mutableCommand.data(),
                       nullptr,
                       nullptr,
                       FALSE,
                       0,
                       nullptr,
                       nullptr,
                       &startupInfo,
                       &processInfo) != TRUE) {
        error = "Unable to start FFmpeg: " + formatLastWindowsError(GetLastError()) + ".";
        return -1;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return static_cast<int>(exitCode);
#else
    (void)error;
    return std::system(command.c_str());
#endif
}

} // namespace

bool buildFfmpegEncodeCommand(const VideoEncodeOptions& options,
                              std::string& command,
                              std::string& error)
{
    command.clear();
    if (!validateOptions(options, error)) {
        return false;
    }

    const std::filesystem::path pattern = options.framesDirectory / "frame_%06d.ppm";
    std::ostringstream stream;
    if (!appendQuotedPath(stream, options.ffmpegExecutable, "FFmpeg executable", error)) {
        return false;
    }
    stream << " -hide_banner -loglevel error"
           << (options.overwrite ? " -y" : " -n")
           << " -framerate " << options.frameRate
           << " -i ";
    if (!appendQuotedPath(stream, pattern, "Frame pattern", error)) {
        return false;
    }
    stream << " -c:v libx264"
           << " -pix_fmt yuv420p"
           << " -crf " << options.crf
           << " -preset " << options.preset;
    if (options.fastStart) {
        stream << " -movflags +faststart";
    }
    stream << " ";
    if (!appendQuotedPath(stream, options.outputMp4, "Output MP4", error)) {
        return false;
    }

    command = stream.str();
    return true;
}

bool encodeFrameSequenceToMp4(const VideoEncodeOptions& options,
                              VideoEncodeResult& result,
                              std::string& error)
{
    result = {};
    if (!ensureParentDirectory(options.outputMp4, error)) {
        return false;
    }

    std::string command;
    if (!buildFfmpegEncodeCommand(options, command, error)) {
        return false;
    }

    const int exitCode = runCommand(command, error);
    if (exitCode != 0) {
        std::ostringstream message;
        message << "FFmpeg MP4 encode failed with exit code " << exitCode << ". Command: " << command;
        if (!error.empty()) {
            message << " " << error;
        }
        error = message.str();
        return false;
    }

    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(options.outputMp4, ec);
    if (ec || size == 0U) {
        error = "FFmpeg finished but the MP4 output is missing or empty.";
        return false;
    }

    result.outputMp4 = options.outputMp4;
    result.bytesWritten = size;
    result.command = command;
    return true;
}

} // namespace viz
