#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace viz {

class WasapiLoopbackCapture {
public:
    WasapiLoopbackCapture() = default;
    ~WasapiLoopbackCapture();

    WasapiLoopbackCapture(const WasapiLoopbackCapture&) = delete;
    WasapiLoopbackCapture& operator=(const WasapiLoopbackCapture&) = delete;

    bool start();
    void stop();

    [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }
    [[nodiscard]] std::wstring lastError() const;

    std::vector<float> latestFrames(std::size_t maxFrames, int& sampleRate, int& channelCount) const;

private:
    mutable std::mutex mutex_;
    std::deque<float> ring_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::wstring lastError_;
    int sampleRate_ = 48000;
    int channelCount_ = 2;
    std::size_t maxSamples_ = 48000 * 2 * 3;

    void captureThread();
    void pushSamples(const float* samples, std::size_t sampleCount, int sampleRate, int channelCount);
    void setError(std::wstring error);
};

} // namespace viz
