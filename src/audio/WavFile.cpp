#include "Visualizer/Audio/WavFile.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace viz {
namespace {

constexpr std::uint16_t kWaveFormatPcm = 0x0001;
constexpr std::uint16_t kWaveFormatIeeeFloat = 0x0003;
constexpr std::uint16_t kWaveFormatExtensible = 0xFFFE;

std::uint16_t readU16(const unsigned char* bytes)
{
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

std::uint32_t readU32(const unsigned char* bytes)
{
    return static_cast<std::uint32_t>(bytes[0] |
                                      (bytes[1] << 8) |
                                      (bytes[2] << 16) |
                                      (bytes[3] << 24));
}

bool readExact(std::ifstream& input, unsigned char* destination, std::size_t size)
{
    input.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(input.gcount()) == size;
}

float clampSample(float sample)
{
    return std::clamp(sample, -1.0f, 1.0f);
}

float readPcmSample(const unsigned char* data, int bitsPerSample)
{
    switch (bitsPerSample) {
    case 8:
        return (static_cast<int>(data[0]) - 128) / 128.0f;
    case 16: {
        const auto value = static_cast<std::int16_t>(readU16(data));
        return static_cast<float>(value) / 32768.0f;
    }
    case 24: {
        std::int32_t value = static_cast<std::int32_t>(data[0]) |
                             (static_cast<std::int32_t>(data[1]) << 8) |
                             (static_cast<std::int32_t>(data[2]) << 16);
        if ((value & 0x00800000) != 0) {
            value |= static_cast<std::int32_t>(0xFF000000);
        }
        return static_cast<float>(value) / 8388608.0f;
    }
    case 32: {
        const auto value = static_cast<std::int32_t>(readU32(data));
        return static_cast<float>(value) / 2147483648.0f;
    }
    default:
        return 0.0f;
    }
}

float readFloatSample(const unsigned char* data, int bitsPerSample)
{
    if (bitsPerSample != 32) {
        return 0.0f;
    }
    float value = 0.0f;
    std::memcpy(&value, data, sizeof(float));
    return clampSample(value);
}

} // namespace

std::optional<WavAudio> loadWavFile(const std::filesystem::path& path, std::string& error)
{
    error.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Unable to open WAV file.";
        return std::nullopt;
    }

    std::array<unsigned char, 12> header{};
    if (!readExact(input, header.data(), header.size()) ||
        std::memcmp(header.data(), "RIFF", 4) != 0 ||
        std::memcmp(header.data() + 8, "WAVE", 4) != 0) {
        error = "File is not a RIFF/WAVE file.";
        return std::nullopt;
    }

    std::uint16_t formatTag = 0;
    std::uint16_t effectiveFormatTag = 0;
    std::uint16_t channelCount = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t blockAlign = 0;
    std::uint16_t bitsPerSample = 0;
    std::vector<unsigned char> dataBytes;
    bool sawFormat = false;

    while (input) {
        std::array<unsigned char, 8> chunkHeader{};
        if (!readExact(input, chunkHeader.data(), chunkHeader.size())) {
            break;
        }

        const std::uint32_t chunkSize = readU32(chunkHeader.data() + 4);
        const bool isFormat = std::memcmp(chunkHeader.data(), "fmt ", 4) == 0;
        const bool isData = std::memcmp(chunkHeader.data(), "data", 4) == 0;

        if (isFormat) {
            if (chunkSize < 16) {
                error = "Invalid WAV fmt chunk.";
                return std::nullopt;
            }

            std::vector<unsigned char> fmt(chunkSize);
            if (!readExact(input, fmt.data(), fmt.size())) {
                error = "Unable to read WAV fmt chunk.";
                return std::nullopt;
            }

            formatTag = readU16(fmt.data());
            effectiveFormatTag = formatTag;
            channelCount = readU16(fmt.data() + 2);
            sampleRate = readU32(fmt.data() + 4);
            blockAlign = readU16(fmt.data() + 12);
            bitsPerSample = readU16(fmt.data() + 14);
            if (formatTag == kWaveFormatExtensible && fmt.size() >= 40) {
                effectiveFormatTag = readU16(fmt.data() + 24);
            }
            sawFormat = true;
        } else if (isData) {
            dataBytes.resize(chunkSize);
            if (!readExact(input, dataBytes.data(), dataBytes.size())) {
                error = "Unable to read WAV data chunk.";
                return std::nullopt;
            }
        } else {
            input.seekg(chunkSize, std::ios::cur);
            if (!input) {
                error = "Invalid WAV chunk size.";
                return std::nullopt;
            }
        }

        if ((chunkSize % 2U) == 1U) {
            input.seekg(1, std::ios::cur);
        }
    }

    if (!sawFormat) {
        error = "WAV file is missing fmt chunk.";
        return std::nullopt;
    }
    if (dataBytes.empty()) {
        error = "WAV file is missing audio data.";
        return std::nullopt;
    }
    if (channelCount == 0 || sampleRate == 0 || blockAlign == 0) {
        error = "WAV file has invalid format fields.";
        return std::nullopt;
    }
    if (effectiveFormatTag != kWaveFormatPcm && effectiveFormatTag != kWaveFormatIeeeFloat) {
        error = "Unsupported WAV encoding. Use PCM or IEEE-float WAV.";
        return std::nullopt;
    }

    const int bytesPerSample = static_cast<int>(bitsPerSample / 8);
    if (bytesPerSample <= 0 || (bitsPerSample % 8) != 0) {
        error = "Unsupported WAV bit depth.";
        return std::nullopt;
    }

    const std::size_t totalFrames = dataBytes.size() / blockAlign;
    if (totalFrames == 0) {
        error = "WAV file has no complete audio frames.";
        return std::nullopt;
    }

    WavAudio result{};
    result.sampleRate = static_cast<int>(sampleRate);
    result.channelCount = static_cast<int>(channelCount);
    result.bitsPerSample = static_cast<int>(bitsPerSample);
    result.durationSeconds = static_cast<double>(totalFrames) / static_cast<double>(sampleRate);
    result.sourcePath = path;
    result.samples.reserve(totalFrames * channelCount);

    for (std::size_t frame = 0; frame < totalFrames; ++frame) {
        const unsigned char* frameData = dataBytes.data() + (frame * blockAlign);
        for (std::uint16_t channel = 0; channel < channelCount; ++channel) {
            const unsigned char* sampleData = frameData + (static_cast<std::size_t>(channel) *
                                                           static_cast<std::size_t>(bytesPerSample));
            const float sample = effectiveFormatTag == kWaveFormatIeeeFloat
                                     ? readFloatSample(sampleData, bitsPerSample)
                                     : readPcmSample(sampleData, bitsPerSample);
            result.samples.push_back(clampSample(sample));
        }
    }

    return result;
}

} // namespace viz
