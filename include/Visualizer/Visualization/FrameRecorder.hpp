#pragma once

#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace viz {

struct FrameRenderOptions {
    bool trails = false;
    float trailPersistence = 0.84f;
};

class FrameRecorder {
public:
    bool start(const std::filesystem::path& captureRoot,
               int width,
               int height,
               std::string& error);
    bool startSession(const std::filesystem::path& sessionPath,
                      int width,
                      int height,
                      std::string& error);
    void stop();

    [[nodiscard]] bool isRecording() const noexcept { return recording_; }
    [[nodiscard]] std::size_t frameCount() const noexcept { return frameIndex_; }
    [[nodiscard]] const std::filesystem::path& sessionPath() const noexcept { return sessionPath_; }

    bool writeFrame(const GeometryFrame& frame, std::string& error);
    bool writeFrame(const GeometryFrame& frame, const FrameRenderOptions& options, std::string& error);

private:
    int width_ = 0;
    int height_ = 0;
    bool recording_ = false;
    std::size_t frameIndex_ = 0;
    std::filesystem::path sessionPath_;
    std::vector<unsigned char> pixels_;

    void clear(ColorRGBA color);
    void fadeToward(ColorRGBA color, float persistence);
    void blendPixel(int x, int y, ColorRGBA color);
    void drawLine(Vec2 a, Vec2 b, float width, ColorRGBA color);
    void drawFilledPolygon(const std::vector<Vec2>& points, ColorRGBA color);
    void drawDisc(Vec2 center, float radius, ColorRGBA color);
    void drawRing(const Ring& ring);
    bool flushPpm(std::string& error);
};

} // namespace viz
