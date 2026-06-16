#include "Visualizer/Visualization/FrameRecorder.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace viz {
namespace {

constexpr float kPi = 3.14159265358979323846f;

unsigned char toByte(float value)
{
    return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

Vec2 polar(Vec2 center, float radius, float angle)
{
    return Vec2{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
}

ColorRGBA edgeTintForFilledMaterial(ColorRGBA color)
{
    color.a = std::clamp(color.a * 0.18f, 0.0f, 0.16f);
    return color;
}

std::string timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif
    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y%m%d_%H%M%S");
    return stream.str();
}

} // namespace

bool FrameRecorder::start(const std::filesystem::path& captureRoot,
                          int width,
                          int height,
                          std::string& error)
{
    error.clear();
    return startSession(captureRoot / ("visualizer_" + timestamp()), width, height, error);
}

bool FrameRecorder::startSession(const std::filesystem::path& sessionPath,
                                 int width,
                                 int height,
                                 std::string& error)
{
    error.clear();

    if (width <= 0 || height <= 0) {
        error = "Capture dimensions are invalid.";
        return false;
    }

    width_ = width;
    height_ = height;
    frameIndex_ = 0;
    sessionPath_ = sessionPath;

    std::error_code ec;
    std::filesystem::create_directories(sessionPath_, ec);
    if (ec) {
        error = "Unable to create capture directory: " + ec.message();
        return false;
    }

    pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 3U, 0);
    recording_ = true;
    return true;
}

void FrameRecorder::stop()
{
    recording_ = false;
}

bool FrameRecorder::writeFrame(const GeometryFrame& frame, std::string& error)
{
    return writeFrame(frame, FrameRenderOptions{}, error);
}

bool FrameRecorder::writeFrame(const GeometryFrame& frame, const FrameRenderOptions& options, std::string& error)
{
    if (!recording_) {
        return true;
    }

    if (options.trails && frameIndex_ > 0) {
        fadeToward(frame.background, options.trailPersistence);
    } else {
        clear(frame.background);
    }

    const Vec2 center{static_cast<float>(width_) * 0.5f, static_cast<float>(height_) * 0.5f};
    for (const Beam& beam : frame.beams) {
        drawLine(center, polar(center, beam.length, beam.angle), beam.width, beam.color);
    }
    for (const Ring& ring : frame.rings) {
        drawRing(ring);
    }
    for (const Polyline& line : frame.polylines) {
        if (line.points.size() < 2) {
            continue;
        }
        if (line.filled && line.closed && line.points.size() >= 3U) {
            drawFilledPolygon(line.points, line.color);
        }
        const ColorRGBA strokeColor = line.filled ? edgeTintForFilledMaterial(line.color) : line.color;
        const float strokeWidth = line.filled ? std::max(0.35f, line.strokeWidth * 0.26f) : line.strokeWidth;
        for (std::size_t i = 1; i < line.points.size(); ++i) {
            drawLine(line.points[i - 1], line.points[i], strokeWidth, strokeColor);
        }
        if (line.closed) {
            drawLine(line.points.back(), line.points.front(), strokeWidth, strokeColor);
        }
    }
    for (const Particle& particle : frame.particles) {
        drawDisc(particle.position, particle.radius, particle.color);
    }
    if (frame.flash > 0.0f) {
        const ColorRGBA flash{1.0f, 1.0f, 1.0f, std::min(frame.flash, 0.28f)};
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                blendPixel(x, y, flash);
            }
        }
    }

    return flushPpm(error);
}

void FrameRecorder::clear(ColorRGBA color)
{
    for (std::size_t i = 0; i < pixels_.size(); i += 3) {
        pixels_[i] = toByte(color.r);
        pixels_[i + 1] = toByte(color.g);
        pixels_[i + 2] = toByte(color.b);
    }
}

void FrameRecorder::fadeToward(ColorRGBA color, float persistence)
{
    const float clampedPersistence = std::clamp(persistence, 0.55f, 0.96f);
    const float backgroundWeight = 1.0f - clampedPersistence;
    for (std::size_t i = 0; i < pixels_.size(); i += 3) {
        pixels_[i] = toByte((static_cast<float>(pixels_[i]) / 255.0f) * clampedPersistence +
                            color.r * backgroundWeight);
        pixels_[i + 1] = toByte((static_cast<float>(pixels_[i + 1]) / 255.0f) * clampedPersistence +
                                color.g * backgroundWeight);
        pixels_[i + 2] = toByte((static_cast<float>(pixels_[i + 2]) / 255.0f) * clampedPersistence +
                                color.b * backgroundWeight);
    }
}

void FrameRecorder::blendPixel(int x, int y, ColorRGBA color)
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }

    const float alpha = std::clamp(color.a, 0.0f, 1.0f);
    const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                               static_cast<std::size_t>(x)) * 3U;
    pixels_[index] = toByte((static_cast<float>(pixels_[index]) / 255.0f) * (1.0f - alpha) + color.r * alpha);
    pixels_[index + 1] = toByte((static_cast<float>(pixels_[index + 1]) / 255.0f) * (1.0f - alpha) + color.g * alpha);
    pixels_[index + 2] = toByte((static_cast<float>(pixels_[index + 2]) / 255.0f) * (1.0f - alpha) + color.b * alpha);
}

void FrameRecorder::drawLine(Vec2 a, Vec2 b, float width, ColorRGBA color)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(dx), std::abs(dy)))));
    const int radius = std::max(0, static_cast<int>(std::ceil(width * 0.5f)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const int x = static_cast<int>(std::round(a.x + dx * t));
        const int y = static_cast<int>(std::round(a.y + dy * t));
        for (int oy = -radius; oy <= radius; ++oy) {
            for (int ox = -radius; ox <= radius; ++ox) {
                if ((ox * ox + oy * oy) <= radius * radius) {
                    blendPixel(x + ox, y + oy, color);
                }
            }
        }
    }
}

void FrameRecorder::drawFilledPolygon(const std::vector<Vec2>& points, ColorRGBA color)
{
    if (points.size() < 3U) {
        return;
    }

    float minX = points.front().x;
    float maxX = minX;
    float minY = points.front().y;
    float maxY = minY;
    for (Vec2 point : points) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }

    const int left = std::max(0, static_cast<int>(std::floor(minX)));
    const int right = std::min(width_ - 1, static_cast<int>(std::ceil(maxX)));
    const int top = std::max(0, static_cast<int>(std::floor(minY)));
    const int bottom = std::min(height_ - 1, static_cast<int>(std::ceil(maxY)));
    if (left > right || top > bottom) {
        return;
    }

    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            bool inside = false;
            for (std::size_t i = 0, j = points.size() - 1U; i < points.size(); j = i++) {
                const Vec2 a = points[i];
                const Vec2 b = points[j];
                const bool crosses = ((a.y > py) != (b.y > py)) &&
                                     (px < (b.x - a.x) * (py - a.y) / ((b.y - a.y) + 0.0001f) + a.x);
                if (crosses) {
                    inside = !inside;
                }
            }
            if (inside) {
                blendPixel(x, y, color);
            }
        }
    }
}

void FrameRecorder::drawDisc(Vec2 center, float radius, ColorRGBA color)
{
    const int r = std::max(1, static_cast<int>(std::ceil(radius)));
    const int cx = static_cast<int>(std::round(center.x));
    const int cy = static_cast<int>(std::round(center.y));
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            const int distanceSquared = x * x + y * y;
            if (distanceSquared <= r * r) {
                const float falloff = 1.0f - (static_cast<float>(distanceSquared) / static_cast<float>(r * r));
                ColorRGBA shaded = color;
                shaded.a *= std::clamp(falloff * 1.2f, 0.0f, 1.0f);
                blendPixel(cx + x, cy + y, shaded);
            }
        }
    }
}

void FrameRecorder::drawRing(const Ring& ring)
{
    const int sides = std::max(3, ring.sides);
    Vec2 previous = polar(ring.center, ring.radius, ring.rotation);
    for (int i = 1; i <= sides; ++i) {
        const float angle = ring.rotation + (static_cast<float>(i) / static_cast<float>(sides)) * 2.0f * kPi;
        const Vec2 next = polar(ring.center, ring.radius, angle);
        drawLine(previous, next, ring.strokeWidth, ring.color);
        previous = next;
    }
}

bool FrameRecorder::flushPpm(std::string& error)
{
    std::ostringstream name;
    name << "frame_" << std::setw(6) << std::setfill('0') << frameIndex_ << ".ppm";
    const std::filesystem::path outputPath = sessionPath_ / name.str();

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        error = "Unable to write capture frame.";
        recording_ = false;
        return false;
    }
    output << "P6\n" << width_ << " " << height_ << "\n255\n";
    output.write(reinterpret_cast<const char*>(pixels_.data()), static_cast<std::streamsize>(pixels_.size()));
    if (!output) {
        error = "Failed while writing capture frame.";
        recording_ = false;
        return false;
    }

    ++frameIndex_;
    return true;
}

} // namespace viz
