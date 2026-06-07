#include "WasapiLoopbackCapture.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <mmreg.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>

namespace viz {
namespace {

template <typename T>
void safeRelease(T*& pointer)
{
    if (pointer != nullptr) {
        pointer->Release();
        pointer = nullptr;
    }
}

std::uint16_t effectiveFormatTag(const WAVEFORMATEX* format)
{
    if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
        return format->wFormatTag;
    }
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    if (IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
        return WAVE_FORMAT_IEEE_FLOAT;
    }
    if (IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
        return WAVE_FORMAT_PCM;
    }
    return format->wFormatTag;
}

float pcmSample(const unsigned char* data, int bitsPerSample)
{
    switch (bitsPerSample) {
    case 16: {
        std::int16_t value = 0;
        std::memcpy(&value, data, sizeof(value));
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
        std::int32_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        return static_cast<float>(value) / 2147483648.0f;
    }
    default:
        return 0.0f;
    }
}

std::vector<float> convertPacket(const BYTE* data,
                                 UINT32 frameCount,
                                 DWORD flags,
                                 const WAVEFORMATEX* format)
{
    const int channels = static_cast<int>(format->nChannels);
    const int bitsPerSample = static_cast<int>(format->wBitsPerSample);
    const int bytesPerSample = std::max(1, bitsPerSample / 8);
    const int blockAlign = static_cast<int>(format->nBlockAlign);
    const std::uint16_t tag = effectiveFormatTag(format);

    std::vector<float> output(static_cast<std::size_t>(frameCount) * static_cast<std::size_t>(channels), 0.0f);
    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr) {
        return output;
    }

    for (UINT32 frame = 0; frame < frameCount; ++frame) {
        const BYTE* frameData = data + (static_cast<std::size_t>(frame) * static_cast<std::size_t>(blockAlign));
        for (int channel = 0; channel < channels; ++channel) {
            const BYTE* sampleData = frameData + (static_cast<std::size_t>(channel) *
                                                  static_cast<std::size_t>(bytesPerSample));
            float sample = 0.0f;
            if (tag == WAVE_FORMAT_IEEE_FLOAT && bitsPerSample == 32) {
                std::memcpy(&sample, sampleData, sizeof(float));
            } else if (tag == WAVE_FORMAT_PCM) {
                sample = pcmSample(sampleData, bitsPerSample);
            }
            output[(static_cast<std::size_t>(frame) * static_cast<std::size_t>(channels)) +
                   static_cast<std::size_t>(channel)] = std::clamp(sample, -1.0f, 1.0f);
        }
    }
    return output;
}

} // namespace

WasapiLoopbackCapture::~WasapiLoopbackCapture()
{
    stop();
}

bool WasapiLoopbackCapture::start()
{
    if (running_.load()) {
        return true;
    }
    {
        std::lock_guard lock(mutex_);
        lastError_.clear();
        ring_.clear();
    }
    running_.store(true);
    thread_ = std::thread(&WasapiLoopbackCapture::captureThread, this);
    return true;
}

void WasapiLoopbackCapture::stop()
{
    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::wstring WasapiLoopbackCapture::lastError() const
{
    std::lock_guard lock(mutex_);
    return lastError_;
}

std::vector<float> WasapiLoopbackCapture::latestFrames(std::size_t maxFrames,
                                                       int& sampleRate,
                                                       int& channelCount) const
{
    std::lock_guard lock(mutex_);
    sampleRate = sampleRate_;
    channelCount = channelCount_;
    if (ring_.empty() || channelCount_ <= 0) {
        return {};
    }

    const std::size_t availableFrames = ring_.size() / static_cast<std::size_t>(channelCount_);
    const std::size_t framesToCopy = std::min(maxFrames, availableFrames);
    const std::size_t samplesToCopy = framesToCopy * static_cast<std::size_t>(channelCount_);
    const std::size_t start = ring_.size() - samplesToCopy;

    std::vector<float> output;
    output.reserve(samplesToCopy);
    auto iterator = ring_.begin();
    std::advance(iterator, static_cast<std::ptrdiff_t>(start));
    output.insert(output.end(), iterator, ring_.end());
    return output;
}

void WasapiLoopbackCapture::captureThread()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        setError(L"WASAPI COM initialization failed.");
        running_.store(false);
        return;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* audioClient = nullptr;
    IAudioCaptureClient* captureClient = nullptr;
    WAVEFORMATEX* mixFormat = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                          nullptr,
                          CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) {
        setError(L"Unable to create WASAPI device enumerator.");
        running_.store(false);
    }

    if (SUCCEEDED(hr)) {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(hr)) {
            setError(L"Unable to find the default Windows audio output device.");
            running_.store(false);
        }
    }
    if (SUCCEEDED(hr)) {
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient));
        if (FAILED(hr)) {
            setError(L"Unable to activate WASAPI audio client.");
            running_.store(false);
        }
    }
    if (SUCCEEDED(hr)) {
        hr = audioClient->GetMixFormat(&mixFormat);
        if (FAILED(hr)) {
            setError(L"Unable to read WASAPI mix format.");
            running_.store(false);
        }
    }
    if (SUCCEEDED(hr)) {
        {
            std::lock_guard lock(mutex_);
            sampleRate_ = static_cast<int>(mixFormat->nSamplesPerSec);
            channelCount_ = static_cast<int>(mixFormat->nChannels);
            maxSamples_ = static_cast<std::size_t>(sampleRate_) *
                          static_cast<std::size_t>(std::max(1, channelCount_)) * 3U;
        }

        constexpr REFERENCE_TIME bufferDuration = 10000000;
        hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                     AUDCLNT_STREAMFLAGS_LOOPBACK,
                                     bufferDuration,
                                     0,
                                     mixFormat,
                                     nullptr);
        if (FAILED(hr)) {
            setError(L"Unable to initialize WASAPI loopback capture.");
            running_.store(false);
        }
    }
    if (SUCCEEDED(hr)) {
        hr = audioClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&captureClient));
        if (FAILED(hr)) {
            setError(L"Unable to create WASAPI capture client.");
            running_.store(false);
        }
    }
    if (SUCCEEDED(hr)) {
        hr = audioClient->Start();
        if (FAILED(hr)) {
            setError(L"Unable to start WASAPI loopback capture.");
            running_.store(false);
        }
    }

    while (running_.load() && SUCCEEDED(hr)) {
        UINT32 packetLength = 0;
        hr = captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) {
            setError(L"WASAPI packet query failed.");
            break;
        }

        while (packetLength != 0 && running_.load()) {
            BYTE* data = nullptr;
            UINT32 framesAvailable = 0;
            DWORD flags = 0;
            hr = captureClient->GetBuffer(&data, &framesAvailable, &flags, nullptr, nullptr);
            if (FAILED(hr)) {
                setError(L"WASAPI packet read failed.");
                break;
            }

            const std::vector<float> converted = convertPacket(data, framesAvailable, flags, mixFormat);
            pushSamples(converted.data(),
                        converted.size(),
                        static_cast<int>(mixFormat->nSamplesPerSec),
                        static_cast<int>(mixFormat->nChannels));

            captureClient->ReleaseBuffer(framesAvailable);
            hr = captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) {
                setError(L"WASAPI packet query failed.");
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (audioClient != nullptr) {
        audioClient->Stop();
    }
    if (mixFormat != nullptr) {
        CoTaskMemFree(mixFormat);
    }
    safeRelease(captureClient);
    safeRelease(audioClient);
    safeRelease(device);
    safeRelease(enumerator);
    CoUninitialize();
}

void WasapiLoopbackCapture::pushSamples(const float* samples,
                                        std::size_t sampleCount,
                                        int sampleRate,
                                        int channelCount)
{
    if (samples == nullptr || sampleCount == 0) {
        return;
    }

    std::lock_guard lock(mutex_);
    sampleRate_ = sampleRate;
    channelCount_ = std::max(1, channelCount);
    maxSamples_ = static_cast<std::size_t>(std::max(1, sampleRate_)) *
                  static_cast<std::size_t>(channelCount_) * 3U;
    for (std::size_t i = 0; i < sampleCount; ++i) {
        ring_.push_back(samples[i]);
    }
    while (ring_.size() > maxSamples_) {
        ring_.pop_front();
    }
}

void WasapiLoopbackCapture::setError(std::wstring error)
{
    std::lock_guard lock(mutex_);
    lastError_ = std::move(error);
}

} // namespace viz
