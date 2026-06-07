#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Visualizer/Audio/WavFile.hpp"

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace viz {
namespace {

constexpr DWORD kAllSourceReaderStreams = static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS);
constexpr DWORD kFirstAudioSourceReaderStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);

template <typename T>
void safeRelease(T*& value)
{
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

std::string formatHresult(const char* action, HRESULT hr)
{
    std::ostringstream output;
    output << action << " failed with HRESULT 0x"
           << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
           << static_cast<unsigned long>(static_cast<std::uint32_t>(hr)) << ".";
    return output.str();
}

float clampSample(float sample)
{
    return std::clamp(sample, -1.0f, 1.0f);
}

class ComScope {
public:
    ComScope()
    {
        hr_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr_ == RPC_E_CHANGED_MODE) {
            hr_ = S_OK;
            return;
        }
        uninitialize_ = SUCCEEDED(hr_);
    }

    ~ComScope()
    {
        if (uninitialize_) {
            CoUninitialize();
        }
    }

    [[nodiscard]] HRESULT status() const { return hr_; }

private:
    HRESULT hr_ = S_OK;
    bool uninitialize_ = false;
};

class MediaFoundationScope {
public:
    MediaFoundationScope()
    {
        hr_ = MFStartup(MF_VERSION);
        started_ = SUCCEEDED(hr_);
    }

    ~MediaFoundationScope()
    {
        if (started_) {
            MFShutdown();
        }
    }

    [[nodiscard]] HRESULT status() const { return hr_; }

private:
    HRESULT hr_ = S_OK;
    bool started_ = false;
};

bool setFloatOutput(IMFSourceReader* reader, std::string& error)
{
    HRESULT hr = reader->SetStreamSelection(kAllSourceReaderStreams, FALSE);
    if (FAILED(hr)) {
        error = formatHresult("Selecting audio streams", hr);
        return false;
    }

    hr = reader->SetStreamSelection(kFirstAudioSourceReaderStream, TRUE);
    if (FAILED(hr)) {
        error = formatHresult("Selecting first audio stream", hr);
        return false;
    }

    IMFMediaType* outputType = nullptr;
    hr = MFCreateMediaType(&outputType);
    if (FAILED(hr)) {
        error = formatHresult("Creating audio output type", hr);
        return false;
    }

    hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (SUCCEEDED(hr)) {
        hr = outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    }
    if (SUCCEEDED(hr)) {
        hr = reader->SetCurrentMediaType(kFirstAudioSourceReaderStream, nullptr, outputType);
    }

    safeRelease(outputType);
    if (FAILED(hr)) {
        error = formatHresult("Configuring float audio decoding", hr);
        return false;
    }
    return true;
}

bool readAudioFormat(IMFSourceReader* reader, WavAudio& audio, std::string& error)
{
    IMFMediaType* actualType = nullptr;
    const HRESULT hr = reader->GetCurrentMediaType(kFirstAudioSourceReaderStream, &actualType);
    if (FAILED(hr)) {
        error = formatHresult("Reading decoded audio format", hr);
        return false;
    }

    const UINT32 sampleRate = MFGetAttributeUINT32(actualType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
    const UINT32 channelCount = MFGetAttributeUINT32(actualType, MF_MT_AUDIO_NUM_CHANNELS, 0);
    const UINT32 bitsPerSample = MFGetAttributeUINT32(actualType, MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
    safeRelease(actualType);

    if (sampleRate == 0 || channelCount == 0) {
        error = "Decoded audio stream has invalid sample rate or channel count.";
        return false;
    }

    audio.sampleRate = static_cast<int>(sampleRate);
    audio.channelCount = static_cast<int>(channelCount);
    audio.bitsPerSample = static_cast<int>(bitsPerSample == 0 ? 32 : bitsPerSample);
    return true;
}

bool appendDecodedSample(IMFSample* sample, std::vector<float>& samples, std::string& error)
{
    IMFMediaBuffer* buffer = nullptr;
    HRESULT hr = sample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr)) {
        error = formatHresult("Reading decoded audio buffer", hr);
        return false;
    }

    BYTE* data = nullptr;
    DWORD maxLength = 0;
    DWORD currentLength = 0;
    hr = buffer->Lock(&data, &maxLength, &currentLength);
    (void)maxLength;
    if (FAILED(hr)) {
        safeRelease(buffer);
        error = formatHresult("Locking decoded audio buffer", hr);
        return false;
    }

    const DWORD sampleBytes = static_cast<DWORD>(sizeof(float));
    const DWORD sampleCount = currentLength / sampleBytes;
    const std::size_t oldSize = samples.size();
    samples.resize(oldSize + sampleCount);
    for (DWORD index = 0; index < sampleCount; ++index) {
        float value = 0.0f;
        std::memcpy(&value, data + (index * sampleBytes), sizeof(float));
        samples[oldSize + index] = clampSample(value);
    }

    buffer->Unlock();
    safeRelease(buffer);
    return true;
}

bool readAllSamples(IMFSourceReader* reader, WavAudio& audio, std::string& error)
{
    while (true) {
        DWORD flags = 0;
        IMFSample* sample = nullptr;
        const HRESULT hr = reader->ReadSample(kFirstAudioSourceReaderStream,
                                              0,
                                              nullptr,
                                              &flags,
                                              nullptr,
                                              &sample);
        if (FAILED(hr)) {
            error = formatHresult("Decoding audio sample", hr);
            return false;
        }

        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0U) {
            safeRelease(sample);
            break;
        }
        if ((flags & MF_SOURCE_READERF_ERROR) != 0U) {
            safeRelease(sample);
            error = "Media Foundation reported a stream decoding error.";
            return false;
        }
        if (sample != nullptr) {
            if (!appendDecodedSample(sample, audio.samples, error)) {
                safeRelease(sample);
                return false;
            }
            safeRelease(sample);
        }
    }

    if (audio.samples.empty()) {
        error = "Decoded audio stream did not produce samples.";
        return false;
    }

    const std::size_t channels = static_cast<std::size_t>(audio.channelCount);
    const std::size_t completeFrames = audio.samples.size() / channels;
    audio.samples.resize(completeFrames * channels);
    if (completeFrames == 0) {
        error = "Decoded audio stream has no complete frames.";
        return false;
    }

    audio.durationSeconds = static_cast<double>(completeFrames) / static_cast<double>(audio.sampleRate);
    return true;
}

} // namespace

std::optional<WavAudio> loadAudioFileWithMediaFoundation(const std::filesystem::path& path,
                                                         std::string& error)
{
    error.clear();

    const ComScope com;
    if (FAILED(com.status())) {
        error = formatHresult("Initializing COM", com.status());
        return std::nullopt;
    }

    const MediaFoundationScope mediaFoundation;
    if (FAILED(mediaFoundation.status())) {
        error = formatHresult("Starting Media Foundation", mediaFoundation.status());
        return std::nullopt;
    }

    IMFSourceReader* reader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL(path.wstring().c_str(), nullptr, &reader);
    if (FAILED(hr)) {
        error = formatHresult("Opening audio file", hr);
        return std::nullopt;
    }

    WavAudio audio{};
    audio.sourcePath = path;
    if (!setFloatOutput(reader, error) ||
        !readAudioFormat(reader, audio, error) ||
        !readAllSamples(reader, audio, error)) {
        safeRelease(reader);
        return std::nullopt;
    }

    safeRelease(reader);
    return audio;
}

} // namespace viz
