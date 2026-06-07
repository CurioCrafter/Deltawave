#include "Visualizer/Performance/FramePerformanceTracker.hpp"

#include <algorithm>

namespace viz {
namespace {

double nonNegative(double value)
{
    return std::max(0.0, value);
}

double smooth(double previous, double next, bool initialized)
{
    return initialized ? (previous * 0.9) + (next * 0.1) : next;
}

} // namespace

int countPrimitives(const GeometryFrame& frame)
{
    int count = static_cast<int>(frame.rings.size() + frame.beams.size() + frame.particles.size());
    for (const Polyline& line : frame.polylines) {
        count += static_cast<int>(line.points.size());
    }
    return count;
}

void FramePerformanceTracker::reset()
{
    stats_ = {};
    stats_.qualityScale = 1.0f;
    initialized_ = false;
}

PerformanceStats FramePerformanceTracker::recordFrame(double frameMilliseconds,
                                                      int primitiveCount,
                                                      bool adaptiveQualityEnabled,
                                                      float requestedQualityScale)
{
    FrameTimingBreakdown timings;
    timings.frameMs = frameMilliseconds;
    return recordFrame(timings, primitiveCount, adaptiveQualityEnabled, requestedQualityScale);
}

PerformanceStats FramePerformanceTracker::recordFrame(const FrameTimingBreakdown& timings,
                                                      int primitiveCount,
                                                      bool adaptiveQualityEnabled,
                                                      float requestedQualityScale)
{
    const double frameMilliseconds = nonNegative(timings.frameMs);
    stats_.lastFrameMs = frameMilliseconds;
    stats_.lastAnalysisMs = nonNegative(timings.analysisMs);
    stats_.lastGeometryMs = nonNegative(timings.geometryMs);
    stats_.lastRenderMs = nonNegative(timings.renderMs);
    stats_.lastRecordMs = nonNegative(timings.recordMs);
    stats_.primitiveCount = primitiveCount;

    stats_.averageFrameMs = smooth(stats_.averageFrameMs, stats_.lastFrameMs, initialized_);
    stats_.averageAnalysisMs = smooth(stats_.averageAnalysisMs, stats_.lastAnalysisMs, initialized_);
    stats_.averageGeometryMs = smooth(stats_.averageGeometryMs, stats_.lastGeometryMs, initialized_);
    stats_.averageRenderMs = smooth(stats_.averageRenderMs, stats_.lastRenderMs, initialized_);
    stats_.averageRecordMs = smooth(stats_.averageRecordMs, stats_.lastRecordMs, initialized_);
    stats_.averageCoreMs = stats_.averageAnalysisMs + stats_.averageGeometryMs;
    initialized_ = true;

    stats_.fps = stats_.averageFrameMs > 0.001 ? 1000.0 / stats_.averageFrameMs : 0.0;
    stats_.renderShare = stats_.averageFrameMs > 0.001
                              ? std::clamp(stats_.averageRenderMs / stats_.averageFrameMs, 0.0, 1.0)
                              : 0.0;

    if (adaptiveQualityEnabled) {
        constexpr double targetFrameMs = 16.7;
        if (stats_.averageFrameMs > targetFrameMs * 1.25) {
            stats_.qualityScale = std::max(0.45f, stats_.qualityScale - 0.035f);
        } else if (stats_.averageFrameMs < targetFrameMs * 0.72) {
            stats_.qualityScale = std::min(1.0f, stats_.qualityScale + 0.02f);
        }
    } else {
        stats_.qualityScale = std::clamp(requestedQualityScale, 0.45f, 1.0f);
    }

    stats_.adaptiveQualityActive = adaptiveQualityEnabled && stats_.qualityScale < 0.995f;
    return stats_;
}

} // namespace viz
