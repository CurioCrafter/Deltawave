#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace viz {

struct VideoEncodeOptions {
    std::filesystem::path framesDirectory;
    std::filesystem::path outputMp4;
    std::filesystem::path ffmpegExecutable = "ffmpeg";
    int frameRate = 60;
    int crf = 18;
    std::string preset = "medium";
    bool overwrite = true;
    bool fastStart = true;
};

struct VideoEncodeResult {
    std::filesystem::path outputMp4;
    std::uintmax_t bytesWritten = 0;
    std::string command;
};

[[nodiscard]] bool buildFfmpegEncodeCommand(const VideoEncodeOptions& options,
                                            std::string& command,
                                            std::string& error);

bool encodeFrameSequenceToMp4(const VideoEncodeOptions& options,
                              VideoEncodeResult& result,
                              std::string& error);

} // namespace viz
