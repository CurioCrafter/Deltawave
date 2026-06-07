#include "Visualizer/Export/PreviewImage.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace viz {
namespace {

struct PpmFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

int clampToInt(std::size_t value)
{
    constexpr int maxInt = (std::numeric_limits<int>::max)();
    return value > static_cast<std::size_t>(maxInt) ? maxInt : static_cast<int>(value);
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isFramePpm(const std::filesystem::path& path)
{
    const std::string filename = lower(path.filename().string());
    const std::string extension = lower(path.extension().string());
    return filename.rfind("frame_", 0) == 0 && extension == ".ppm";
}

std::vector<std::filesystem::path> collectFramePaths(const std::filesystem::path& framesDirectory)
{
    std::vector<std::filesystem::path> frames;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(framesDirectory,
                                                std::filesystem::directory_options::skip_permission_denied,
                                                ec),
         end;
         it != end;
         it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (it->is_regular_file(ec) && !ec && isFramePpm(it->path())) {
            frames.push_back(it->path());
        }
        ec.clear();
    }
    std::sort(frames.begin(), frames.end(), [](const std::filesystem::path& left,
                                               const std::filesystem::path& right) {
        return left.filename().generic_string() < right.filename().generic_string();
    });
    return frames;
}

bool readToken(std::istream& input, std::string& token)
{
    token.clear();
    char ch = '\0';
    while (input.get(ch)) {
        const auto byte = static_cast<unsigned char>(ch);
        if (std::isspace(byte) != 0) {
            continue;
        }
        if (ch == '#') {
            input.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            continue;
        }
        token.push_back(ch);
        break;
    }

    if (token.empty()) {
        return false;
    }

    while (input.get(ch)) {
        const auto byte = static_cast<unsigned char>(ch);
        if (std::isspace(byte) != 0) {
            break;
        }
        if (ch == '#') {
            input.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            break;
        }
        token.push_back(ch);
    }
    return true;
}

bool parsePositiveInt(const std::string& token, int& value)
{
    value = 0;
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    const auto parsed = std::from_chars(begin, end, value);
    return parsed.ec == std::errc{} && parsed.ptr == end && value > 0;
}

bool loadPpmFrame(const std::filesystem::path& path, PpmFrame& frame, std::string& error)
{
    frame = {};
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Unable to open preview source frame: " + path.string();
        return false;
    }

    std::string token;
    if (!readToken(input, token) || token != "P6") {
        error = "Preview source is not a binary PPM frame: " + path.string();
        return false;
    }
    if (!readToken(input, token) || !parsePositiveInt(token, frame.width)) {
        error = "Preview source has an invalid PPM width: " + path.string();
        return false;
    }
    if (!readToken(input, token) || !parsePositiveInt(token, frame.height)) {
        error = "Preview source has an invalid PPM height: " + path.string();
        return false;
    }

    int maxValue = 0;
    if (!readToken(input, token) || !parsePositiveInt(token, maxValue) || maxValue != 255) {
        error = "Preview source PPM must use max value 255: " + path.string();
        return false;
    }

    constexpr std::uint64_t kMaxPreviewSourcePixels = 200000000ULL;
    const std::uint64_t pixelCount = static_cast<std::uint64_t>(frame.width) *
                                     static_cast<std::uint64_t>(frame.height);
    if (pixelCount == 0 || pixelCount > kMaxPreviewSourcePixels) {
        error = "Preview source frame dimensions are too large: " + path.string();
        return false;
    }

    const std::size_t byteCount = static_cast<std::size_t>(pixelCount * 3ULL);
    frame.pixels.assign(byteCount, 0);
    input.read(reinterpret_cast<char*>(frame.pixels.data()), static_cast<std::streamsize>(frame.pixels.size()));
    if (input.gcount() != static_cast<std::streamsize>(frame.pixels.size())) {
        error = "Preview source PPM pixel payload is truncated: " + path.string();
        frame = {};
        return false;
    }
    return true;
}

void writeU16(std::ofstream& output, std::uint16_t value)
{
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
}

void writeU32(std::ofstream& output, std::uint32_t value)
{
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
    output.put(static_cast<char>((value >> 16U) & 0xFFU));
    output.put(static_cast<char>((value >> 24U) & 0xFFU));
}

bool writeBmp(const std::filesystem::path& outputBmp,
              const std::vector<std::uint8_t>& rgb,
              int width,
              int height,
              std::string& error)
{
    if (width <= 0 || height <= 0 || rgb.empty()) {
        error = "Preview BMP dimensions are invalid.";
        return false;
    }

    const std::uint64_t rowBytes = static_cast<std::uint64_t>(width) * 3ULL;
    const std::uint64_t rowStride = ((rowBytes + 3ULL) / 4ULL) * 4ULL;
    const std::uint64_t pixelBytes = rowStride * static_cast<std::uint64_t>(height);
    constexpr std::uint64_t kBmpHeaderBytes = 54ULL;
    constexpr std::uint64_t kMaxBmpBytes = 0xFFFFFFFFULL;
    if (pixelBytes + kBmpHeaderBytes > kMaxBmpBytes) {
        error = "Preview BMP is too large.";
        return false;
    }

    std::error_code ec;
    const std::filesystem::path parent = outputBmp.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = "Unable to create preview image directory: " + ec.message();
            return false;
        }
    }

    std::ofstream output(outputBmp, std::ios::binary);
    if (!output) {
        error = "Unable to write preview image: " + outputBmp.string();
        return false;
    }

    output.write("BM", 2);
    writeU32(output, static_cast<std::uint32_t>(pixelBytes + kBmpHeaderBytes));
    writeU16(output, 0);
    writeU16(output, 0);
    writeU32(output, static_cast<std::uint32_t>(kBmpHeaderBytes));

    writeU32(output, 40);
    writeU32(output, static_cast<std::uint32_t>(width));
    writeU32(output, static_cast<std::uint32_t>(height));
    writeU16(output, 1);
    writeU16(output, 24);
    writeU32(output, 0);
    writeU32(output, static_cast<std::uint32_t>(pixelBytes));
    writeU32(output, 2835);
    writeU32(output, 2835);
    writeU32(output, 0);
    writeU32(output, 0);

    const std::array<char, 3> padding{0, 0, 0};
    const std::size_t rowPadding = static_cast<std::size_t>(rowStride - rowBytes);
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t source = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                        static_cast<std::size_t>(x)) *
                                       3U;
            output.put(static_cast<char>(rgb[source + 2U]));
            output.put(static_cast<char>(rgb[source + 1U]));
            output.put(static_cast<char>(rgb[source]));
        }
        output.write(padding.data(), static_cast<std::streamsize>(rowPadding));
    }

    if (!output) {
        error = "Failed while writing preview image: " + outputBmp.string();
        return false;
    }
    return true;
}

void scaleFrameIntoSheet(const PpmFrame& frame,
                         int thumbWidth,
                         int thumbHeight,
                         int destX,
                         int destY,
                         int sheetWidth,
                         std::vector<std::uint8_t>& sheet)
{
    for (int y = 0; y < thumbHeight; ++y) {
        const int sourceY = std::min(frame.height - 1,
                                     static_cast<int>((static_cast<std::int64_t>(y) * frame.height) / thumbHeight));
        for (int x = 0; x < thumbWidth; ++x) {
            const int sourceX = std::min(frame.width - 1,
                                         static_cast<int>((static_cast<std::int64_t>(x) * frame.width) / thumbWidth));
            const std::size_t source = (static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(frame.width) +
                                        static_cast<std::size_t>(sourceX)) *
                                       3U;
            const std::size_t dest = (static_cast<std::size_t>(destY + y) * static_cast<std::size_t>(sheetWidth) +
                                      static_cast<std::size_t>(destX + x)) *
                                     3U;
            sheet[dest] = frame.pixels[source];
            sheet[dest + 1U] = frame.pixels[source + 1U];
            sheet[dest + 2U] = frame.pixels[source + 2U];
        }
    }
}

std::vector<std::filesystem::path> selectPreviewFrames(const std::vector<std::filesystem::path>& frames,
                                                       int maxFrames)
{
    std::vector<std::filesystem::path> selected;
    const std::size_t count = std::min(frames.size(), static_cast<std::size_t>(std::max(1, maxFrames)));
    selected.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t sourceIndex = count == 1U
                                            ? 0U
                                            : ((i * (frames.size() - 1U)) + ((count - 1U) / 2U)) / (count - 1U);
        selected.push_back(frames[sourceIndex]);
    }
    return selected;
}

} // namespace

bool writeFramePreviewContactSheet(const std::filesystem::path& framesDirectory,
                                   const std::filesystem::path& outputBmp,
                                   int maxFrames,
                                   int thumbnailWidth,
                                   PreviewImageResult& result,
                                   std::string& error)
{
    result = {};
    error.clear();
    result.outputPath = outputBmp;

    if (framesDirectory.empty()) {
        error = "Preview generation requires a frame directory.";
        return false;
    }

    const std::vector<std::filesystem::path> frames = collectFramePaths(framesDirectory);
    result.sourceFramesFound = clampToInt(frames.size());
    if (frames.empty()) {
        error = "No PPM frames were found for preview generation.";
        return false;
    }

    std::vector<PpmFrame> loadedFrames;
    loadedFrames.reserve(std::min<std::size_t>(frames.size(), static_cast<std::size_t>(std::max(1, maxFrames))));
    for (const std::filesystem::path& framePath : selectPreviewFrames(frames, maxFrames)) {
        PpmFrame frame;
        std::string loadError;
        if (loadPpmFrame(framePath, frame, loadError)) {
            loadedFrames.push_back(std::move(frame));
        } else if (error.empty()) {
            error = loadError;
        }
    }
    if (loadedFrames.empty()) {
        if (error.empty()) {
            error = "No valid PPM frames could be loaded for preview generation.";
        }
        return false;
    }

    const int cellWidth = std::clamp(thumbnailWidth <= 0 ? 220 : thumbnailWidth, 48, 640);
    std::vector<int> cellHeights;
    cellHeights.reserve(loadedFrames.size());
    int maxCellHeight = 1;
    for (const PpmFrame& frame : loadedFrames) {
        const int scaledHeight = std::max(1, static_cast<int>(
                                                 (static_cast<std::int64_t>(cellWidth) * frame.height +
                                                  (frame.width / 2)) /
                                                 frame.width));
        cellHeights.push_back(scaledHeight);
        maxCellHeight = std::max(maxCellHeight, scaledHeight);
    }

    constexpr int kPadding = 10;
    constexpr int kGutter = 8;
    const int usedFrames = clampToInt(loadedFrames.size());
    const int sheetWidth = (kPadding * 2) + (usedFrames * cellWidth) + ((usedFrames - 1) * kGutter);
    const int sheetHeight = (kPadding * 2) + maxCellHeight;
    const std::size_t sheetBytes = static_cast<std::size_t>(sheetWidth) *
                                   static_cast<std::size_t>(sheetHeight) *
                                   3U;
    std::vector<std::uint8_t> sheet(sheetBytes, 9U);
    for (std::size_t i = 0; i < sheet.size(); i += 3U) {
        sheet[i] = 7U;
        sheet[i + 1U] = 9U;
        sheet[i + 2U] = 15U;
    }

    for (std::size_t i = 0; i < loadedFrames.size(); ++i) {
        const int destX = kPadding + (static_cast<int>(i) * (cellWidth + kGutter));
        const int destY = kPadding + ((maxCellHeight - cellHeights[i]) / 2);
        scaleFrameIntoSheet(loadedFrames[i], cellWidth, cellHeights[i], destX, destY, sheetWidth, sheet);
    }

    if (!writeBmp(outputBmp, sheet, sheetWidth, sheetHeight, error)) {
        return false;
    }

    result.previewFramesUsed = usedFrames;
    result.width = sheetWidth;
    result.height = sheetHeight;
    result.written = true;
    return true;
}

} // namespace viz
