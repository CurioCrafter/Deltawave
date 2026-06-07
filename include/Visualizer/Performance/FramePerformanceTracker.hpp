#pragma once

#include "Visualizer/Visualization/VisualizerEngine.hpp"

namespace viz {

struct FrameTimingBreakdown {
    double frameMs = 0.0;
    double analysisMs = 0.0;
    double geometryMs = 0.0;
    double renderMs = 0.0;
    double recordMs = 0.0;
};

struct PerformanceStats {
    double lastFrameMs = 0.0;
    double averageFrameMs = 0.0;
    double lastAnalysisMs = 0.0;
    double averageAnalysisMs = 0.0;
    double lastGeometryMs = 0.0;
    double averageGeometryMs = 0.0;
    double lastRenderMs = 0.0;
    double averageRenderMs = 0.0;
    double lastRecordMs = 0.0;
    double averageRecordMs = 0.0;
    double averageCoreMs = 0.0;
    double renderShare = 0.0;
    double fps = 0.0;
    float qualityScale = 1.0f;
    int primitiveCount = 0;
    bool adaptiveQualityActive = false;
};

int countPrimitives(const GeometryFrame& frame);

class FramePerformanceTracker {
public:
    [[nodiscard]] const PerformanceStats& stats() const noexcept { return stats_; }

    void reset();
    PerformanceStats recordFrame(double frameMilliseconds,
                                 int primitiveCount,
                                 bool adaptiveQualityEnabled,
                                 float requestedQualityScale = 1.0f);
    PerformanceStats recordFrame(const FrameTimingBreakdown& timings,
                                 int primitiveCount,
                                 bool adaptiveQualityEnabled,
                                 float requestedQualityScale = 1.0f);

private:
    PerformanceStats stats_{};
    bool initialized_ = false;
};

} // namespace viz
