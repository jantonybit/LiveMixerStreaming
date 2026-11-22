#include "WasapiEngine.h"

#include "../dsp/DspUtils.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <propvarutil.h>

namespace {

std::wstring propVariantString(const PROPVARIANT& value)
{
    LPWSTR text = nullptr;
    if (SUCCEEDED(PropVariantToStringAlloc(value, &text)) && text) {
        const std::wstring result(text);
        CoTaskMemFree(text);
        return result;
    }
    return L"Dispositivo";
}

void copyFromWasapi(const BYTE* source, int channels, int bits, bool isFloat,
                    float* left, float* right, int frames)
{
    if (channels <= 0) return;
    const int outChannels = std::min(channels, 2);
    if (isFloat && bits == 32) {
        const float* src = reinterpret_cast<const float*>(source);
        for (int i = 0; i < frames; ++i) {
            const size_t base = static_cast<size_t>(i) * channels;
            left[i] = src[base];
            right[i] = outChannels > 1 ? src[base + 1] : left[i];
        }
    } else if (!isFloat && bits == 16) {
        const int16_t* src = reinterpret_cast<const int16_t*>(source);
        for (int i = 0; i < frames; ++i) {
            const size_t base = static_cast<size_t>(i) * channels;
            left[i] = static_cast<float>(src[base]) / 32768.0f;
            right[i] = outChannels > 1 ? static_cast<float>(src[base + 1]) / 32768.0f : left[i];
        }
    } else if (!isFloat && bits == 32) {
        const int32_t* src = reinterpret_cast<const int32_t*>(source);
        for (int i = 0; i < frames; ++i) {
            const size_t base = static_cast<size_t>(i) * channels;
            left[i] = static_cast<float>(src[base]) / 2147483648.0f;
            right[i] = outChannels > 1 ? static_cast<float>(src[base + 1]) / 2147483648.0f : left[i];
        }
    } else {
        std::fill(left, left + frames, 0.0f);
        std::fill(right, right + frames, 0.0f);
    }
}

void copyToWasapi(BYTE* destination, int channels, int bits, bool isFloat,
                  const float* left, const float* right, int frames)
{
    if (channels <= 0) return;
    // 2147483647.0f arredonda para 2^31 em float; usar o maior float abaixo disso
    // evita comportamento indefinido no cast para int32 em full scale.
    const float int32Scale = 2147483520.0f;
    if (isFloat && bits == 32) {
        float* dst = reinterpret_cast<float*>(destination);
        for (int i = 0; i < frames; ++i) {
            const size_t base = static_cast<size_t>(i) * channels;
            dst[base] = left[i];
            if (channels > 1) dst[base + 1] = right[i];
            for (int c = 2; c < channels; ++c) dst[base + c] = 0.0f;
        }
    } else if (!isFloat && bits == 16) {
        int16_t* dst = reinterpret_cast<int16_t*>(destination);
        for (int i = 0; i < frames; ++i) {
            const size_t base = static_cast<size_t>(i) * channels;
            dst[base] = static_cast<int16_t>(dsp::clamp(left[i], -1.0f, 1.0f) * 32767.0f);
            if (channels > 1)
                dst[base + 1] = static_cast<int16_t>(dsp::clamp(right[i], -1.0f, 1.0f) * 32767.0f);
            for (int c = 2; c < channels; ++c) dst[base + c] = 0;
        }
    } else if (!isFloat && bits == 32) {
        int32_t* dst = reinterpret_cast<int32_t*>(destination);
        for (int i = 0; i < frames; ++i) {
            const size_t base = static_cast<size_t>(i) * channels;
            dst[base] = static_cast<int32_t>(dsp::clamp(left[i], -1.0f, 1.0f) * int32Scale);
            if (channels > 1)
                dst[base + 1] = static_cast<int32_t>(dsp::clamp(right[i], -1.0f, 1.0f) * int32Scale);
            for (int c = 2; c < channels; ++c) dst[base + c] = 0;
        }
    } else {
        std::memset(destination, 0, static_cast<size_t>(frames) * channels * (bits / 8));
    }
}

} // namespace

struct WasapiEngine::Stream
{
    // Configuracao escrita pela thread de UI antes de CreateThread.
    std::wstring deviceId;
    long requestedRate = 48000;
    bool loopback = false;
    bool isCapture = false;         // true = endpoint de captura (microfone)
    bool clock = false;
    bool muteOutput = false;        // loopback: silencia o endpoint no Windows
    bool muteSaved = false;         // GetMute teve sucesso; valores abaixo sao validos
    bool muteApplied = false;       // SetMute(TRUE) teve sucesso
    BOOL previousMute = FALSE;
    OutputCallback callback;

    // Eventos criados na UI, sinalizados pela worker e fechados na UI apos o join.
    HANDLE audioEvent = nullptr;
    HANDLE startDone = nullptr;
    HANDLE finished = nullptr;
    std::atomic<bool> stop{false};
    std::atomic<HRESULT> startHr{E_FAIL};
    char startError[256] = {};

    // Preenchido pela worker antes de sinalizar startDone.
    int formatRate = 0;
    int channels = 2;
    int bits = 32;
    bool isFloat = true;
    UINT bufferFrames = 0;

    // Objetos COM: criados, usados e liberados exclusivamente na worker thread.
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    IAudioRenderClient* render = nullptr;
    IAudioEndpointVolume* endpointVolume = nullptr; // somente na thread do loopback
    WAVEFORMATEX* format = nullptr;

    HANDLE thread = nullptr;
    DWORD threadId = 0;

    ~Stream() { release(); }

    void releaseCom()
    {
        if (capture) { capture->Release(); capture = nullptr; }
        if (render) { render->Release(); render = nullptr; }
        if (endpointVolume) { endpointVolume->Release(); endpointVolume = nullptr; }
        if (client) { client->Release(); client = nullptr; }
        if (device) { device->Release(); device = nullptr; }
        if (enumerator) { enumerator->Release(); enumerator = nullptr; }
        if (format) { CoTaskMemFree(format); format = nullptr; }
    }

    // Retorna false se a thread ainda nao terminou: nesse caso nada e liberado.
    bool release()
    {
        if (thread) {
            const DWORD wait = WaitForSingleObject(finished, 5000);
            if (wait != WAIT_OBJECT_0) return false;
            CloseHandle(thread);
            thread = nullptr;
        }
        releaseCom();
        if (audioEvent) { CloseHandle(audioEvent); audioEvent = nullptr; }
        if (startDone) { CloseHandle(startDone); startDone = nullptr; }
        if (finished) { CloseHandle(finished); finished = nullptr; }
        return true;
    }
};

void WasapiEngine::writeError(char* error, size_t errorSize,
                              const char* message, HRESULT hr)
{
    if (!error || errorSize == 0) return;
    std::snprintf(error, errorSize, "%s (codigo 0x%08lX)", message,
                  static_cast<unsigned long>(hr));
}

namespace {

std::vector<WasapiEngine::DeviceInfo> enumerateEndpoints(EDataFlow flow, const wchar_t* fallback)
{
    std::vector<WasapiEngine::DeviceInfo> result;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumerator)))
        return result;

    IMMDeviceCollection* collection = nullptr;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection))) {
        UINT count = 0;
        collection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* device = nullptr;
            if (FAILED(collection->Item(i, &device))) continue;

            WasapiEngine::DeviceInfo info;
            LPWSTR id = nullptr;
            if (SUCCEEDED(device->GetId(&id)) && id) {
                info.id = id;
                CoTaskMemFree(id);
            }
            IPropertyStore* properties = nullptr;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties))) {
                PROPVARIANT value;
                PropVariantInit(&value);
                if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)))
                    info.name = propVariantString(value);
                PropVariantClear(&value);
                properties->Release();
            }
            if (info.name.empty()) info.name = fallback;
            result.push_back(std::move(info));
            device->Release();
        }
        collection->Release();
    }
    enumerator->Release();
    return result;
}

} // namespace

std::vector<WasapiEngine::DeviceInfo> WasapiEngine::enumerateDevices()
{
    return enumerateEndpoints(eRender, L"Dispositivo de reproducao");
}

std::vector<WasapiEngine::DeviceInfo> WasapiEngine::enumerateCaptureDevices()
{
    return enumerateEndpoints(eCapture, L"Microfone");
}

int WasapiEngine::defaultDeviceIndex(const std::vector<DeviceInfo>& devices)
{
    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumerator)))
        return 0;
    IMMDevice* device = nullptr;
    int result = 0;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device))) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(device->GetId(&id)) && id) {
            for (size_t i = 0; i < devices.size(); ++i) {
                if (devices[i].id == id) { result = static_cast<int>(i); break; }
            }
            CoTaskMemFree(id);
        }
        device->Release();
    }
    enumerator->Release();
    return result;
}

WasapiEngine::WasapiEngine() = default;

WasapiEngine::~WasapiEngine()
{
    stopLoopback();
    stopMicrophone();
    stopOutput();
}

// Executada na worker thread STA. Em caso de erro, os objetos ja criados ficam no
// Stream e sao liberados por releaseCom() no fim da thread (sem vazamento).
HRESULT WasapiEngine::openClient(Stream& stream, char* error, size_t errorSize)
{
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&stream.enumerator);
    if (FAILED(hr)) {
        writeError(error, errorSize, "Falha ao criar o enumerador WASAPI.", hr);
        return hr;
    }

    if (stream.deviceId.empty()) {
        const EDataFlow flow = stream.isCapture ? eCapture : eRender;
        hr = stream.enumerator->GetDefaultAudioEndpoint(flow, eMultimedia, &stream.device);
    } else {
        hr = stream.enumerator->GetDevice(stream.deviceId.c_str(), &stream.device);
    }
    if (FAILED(hr)) {
        writeError(error, errorSize, stream.isCapture
                                        ? "Dispositivo de captura nao encontrado."
                                        : "Dispositivo de reproducao nao encontrado.", hr);
        return hr;
    }

    hr = stream.device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 (void**)&stream.client);
    if (FAILED(hr)) {
        writeError(error, errorSize, "Falha ao abrir o IAudioClient.", hr);
        return hr;
    }

    hr = stream.client->GetMixFormat(&stream.format);
    if (FAILED(hr) || !stream.format) {
        writeError(error, errorSize, "Falha ao obter o formato do dispositivo.", hr);
        return hr;
    }

    stream.formatRate = static_cast<int>(stream.format->nSamplesPerSec);
    stream.channels = std::max(1, static_cast<int>(stream.format->nChannels));
    stream.bits = static_cast<int>(stream.format->wBitsPerSample);
    if (stream.format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        stream.isFloat = true;
    } else if (stream.format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(stream.format);
        stream.isFloat = extended->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    } else {
        stream.isFloat = false;
    }

    // Em modo compartilhado com EVENTCALLBACK, MSDN exige duracao e periodicidade zero.
    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (stream.loopback) flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    hr = stream.client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0,
                                   stream.format, nullptr);
    if (FAILED(hr)) {
        writeError(error, errorSize, "Falha ao inicializar o WASAPI.", hr);
        return hr;
    }

    hr = stream.client->SetEventHandle(stream.audioEvent);
    if (FAILED(hr)) {
        writeError(error, errorSize, "Falha ao configurar o evento WASAPI.", hr);
        return hr;
    }

    hr = stream.client->GetBufferSize(&stream.bufferFrames);
    if (FAILED(hr)) {
        writeError(error, errorSize, "Falha ao consultar o buffer WASAPI.", hr);
        return hr;
    }
    return S_OK;
}

bool WasapiEngine::startLoopback(int deviceIndex, double sampleRate, bool muteOutput,
                                 std::string* error)
{
    stopLoopback();
    const auto devices = enumerateDevices();
    if (deviceIndex < 0 || static_cast<size_t>(deviceIndex) >= devices.size()) {
        if (error) *error = "Dispositivo de loopback invalido.";
        return false;
    }

    auto stream = std::make_unique<Stream>();
    stream->deviceId = devices[static_cast<size_t>(deviceIndex)].id;
    stream->requestedRate = static_cast<long>(sampleRate);
    stream->loopback = true;
    stream->isCapture = true;
    stream->muteOutput = muteOutput;
    stream->audioEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    stream->startDone = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    stream->finished = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!stream->audioEvent || !stream->startDone || !stream->finished) {
        if (error) *error = "Falha ao criar os eventos do loopback.";
        return false;
    }

    loopback_ = std::move(stream);
    loopbackRing_.reset();

    loopback_->thread = CreateThread(nullptr, 0, &WasapiEngine::loopbackThreadMain,
                                     this, 0, &loopback_->threadId);
    if (!loopback_->thread) {
        if (error) *error = "Falha ao criar a thread de loopback.";
        stopLoopback();
        return false;
    }

    const DWORD wait = WaitForSingleObject(loopback_->startDone, 5000);
    const HRESULT hr = loopback_->startHr.load(std::memory_order_acquire);
    if (wait != WAIT_OBJECT_0 || FAILED(hr)) {
        if (error) *error = loopback_->startError[0] ? loopback_->startError
                                                     : "Falha ao iniciar o loopback.";
        stopLoopback();
        return false;
    }

    loopbackRate_.store(loopback_->formatRate, std::memory_order_relaxed);
    loopbackMuted_.store(loopback_->muteApplied, std::memory_order_release);
    loopbackRunning_.store(true, std::memory_order_release);
    return true;
}

void WasapiEngine::stopLoopback()
{
    if (!loopback_) return;
    loopback_->stop.store(true, std::memory_order_release);
    if (loopback_->audioEvent) SetEvent(loopback_->audioEvent);
    loopbackRunning_.store(false, std::memory_order_release);
    loopbackMuted_.store(false, std::memory_order_release);
    loopbackRate_.store(0, std::memory_order_relaxed);
    if (loopback_->release()) loopback_.reset();
}

DWORD WINAPI WasapiEngine::loopbackThreadMain(LPVOID param)
{
    auto* self = static_cast<WasapiEngine*>(param);
    Stream* stream = self->loopback_.get();
    if (!stream) return 0;

    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comOk = SUCCEEDED(comHr);
    HRESULT hr = comOk ? self->openClient(*stream, stream->startError,
                                          sizeof(stream->startError)) : comHr;

    // Silencia o endpoint no Windows. O WASAPI loopback captura o stream antes
    // do controle de volume, entao o audio continua chegando ao mixer com o
    // nivel original mesmo com a saida muda (game/musica -> mixer sem ouvir).
    // O estado anterior e guardado e restaurado quando a thread termina.
    if (SUCCEEDED(hr) && stream->muteOutput) {
        HRESULT muteHr = stream->device->Activate(__uuidof(IAudioEndpointVolume),
                                                  CLSCTX_ALL, nullptr,
                                                  (void**)&stream->endpointVolume);
        if (SUCCEEDED(muteHr)) muteHr = stream->endpointVolume->GetMute(&stream->previousMute);
        if (SUCCEEDED(muteHr)) {
            stream->muteSaved = true;
            muteHr = stream->endpointVolume->SetMute(TRUE, nullptr);
        }
        stream->muteApplied = SUCCEEDED(muteHr);
    }

    if (SUCCEEDED(hr))
        hr = stream->client->GetService(__uuidof(IAudioCaptureClient),
                                        (void**)&stream->capture);
    if (SUCCEEDED(hr))
        hr = stream->client->Start();

    stream->startHr.store(hr, std::memory_order_release);
    SetEvent(stream->startDone);

    if (SUCCEEDED(hr)) {
        std::vector<float> left(16384), right(16384);
        while (!stream->stop.load(std::memory_order_acquire)) {
            const DWORD wait = WaitForSingleObject(stream->audioEvent, 250);
            if (wait == WAIT_FAILED || wait == WAIT_ABANDONED) break;
            if (wait == WAIT_TIMEOUT) continue;

            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            UINT64 position = 0, timestamp = 0;
            const HRESULT result = stream->capture->GetBuffer(&data, &frames,
                                                              &flags, &position, &timestamp);
            if (FAILED(result) || frames == 0) continue;

            const size_t count = std::min<size_t>(frames, left.size());
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                std::fill(left.begin(), left.begin() + count, 0.0f);
                std::fill(right.begin(), right.begin() + count, 0.0f);
            } else {
                copyFromWasapi(data, stream->channels, stream->bits, stream->isFloat,
                               left.data(), right.data(), static_cast<int>(count));
            }
            self->loopbackRing_.write(left.data(), right.data(), count);
            stream->capture->ReleaseBuffer(frames);
        }
        stream->client->Stop();
    }

    if (stream->muteSaved && stream->endpointVolume)
        stream->endpointVolume->SetMute(stream->previousMute, nullptr);

    stream->releaseCom();
    if (comOk) CoUninitialize();
    SetEvent(stream->finished);
    return 0;
}

bool WasapiEngine::startMicrophone(int deviceIndex, double sampleRate, std::string* error)
{
    stopMicrophone();
    const auto devices = enumerateCaptureDevices();
    if (deviceIndex < 0 || static_cast<size_t>(deviceIndex) >= devices.size()) {
        if (error) *error = "Dispositivo de microfone invalido.";
        return false;
    }

    auto holder = std::make_unique<MicrophoneStream>();
    auto stream = std::make_unique<Stream>();
    stream->deviceId = devices[static_cast<size_t>(deviceIndex)].id;
    stream->requestedRate = static_cast<long>(sampleRate);
    stream->isCapture = true;
    stream->loopback = false;
    stream->audioEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    stream->startDone = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    stream->finished = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!stream->audioEvent || !stream->startDone || !stream->finished) {
        if (error) *error = "Falha ao criar os eventos do microfone.";
        return false;
    }

    holder->stream = std::move(stream);
    microphone_ = std::move(holder);
    microphone_->ring.reset();

    microphone_->stream->thread = CreateThread(nullptr, 0, &WasapiEngine::microphoneThreadMain,
                                               this, 0, &microphone_->stream->threadId);
    if (!microphone_->stream->thread) {
        if (error) *error = "Falha ao criar a thread do microfone.";
        stopMicrophone();
        return false;
    }

    const DWORD wait = WaitForSingleObject(microphone_->stream->startDone, 5000);
    const HRESULT hr = microphone_->stream->startHr.load(std::memory_order_acquire);
    if (wait != WAIT_OBJECT_0 || FAILED(hr)) {
        if (error) *error = microphone_->stream->startError[0]
                                ? microphone_->stream->startError
                                : "Falha ao iniciar o microfone WASAPI.";
        stopMicrophone();
        return false;
    }

    microphoneRate_.store(microphone_->stream->formatRate, std::memory_order_relaxed);
    microphoneRunning_.store(true, std::memory_order_release);
    return true;
}

void WasapiEngine::stopMicrophone()
{
    if (!microphone_) return;
    Stream* stream = microphone_->stream.get();
    if (stream) {
        stream->stop.store(true, std::memory_order_release);
        if (stream->audioEvent) SetEvent(stream->audioEvent);
    }
    microphoneRunning_.store(false, std::memory_order_release);
    microphoneRate_.store(0, std::memory_order_relaxed);
    if (!stream || stream->release()) microphone_.reset();
}

DWORD WINAPI WasapiEngine::microphoneThreadMain(LPVOID param)
{
    auto* self = static_cast<WasapiEngine*>(param);
    if (!self->microphone_ || !self->microphone_->stream) return 0;
    Stream* stream = self->microphone_->stream.get();

    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comOk = SUCCEEDED(comHr);
    HRESULT hr = comOk ? self->openClient(*stream, stream->startError,
                                          sizeof(stream->startError)) : comHr;
    if (SUCCEEDED(hr))
        hr = stream->client->GetService(__uuidof(IAudioCaptureClient),
                                        (void**)&stream->capture);
    if (SUCCEEDED(hr)) hr = stream->client->Start();

    stream->startHr.store(hr, std::memory_order_release);
    SetEvent(stream->startDone);

    if (SUCCEEDED(hr)) {
        std::vector<float> left(16384), right(16384);
        while (!stream->stop.load(std::memory_order_acquire)) {
            const DWORD wait = WaitForSingleObject(stream->audioEvent, 250);
            if (wait == WAIT_FAILED || wait == WAIT_ABANDONED) break;
            if (wait == WAIT_TIMEOUT) continue;

            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            UINT64 position = 0, timestamp = 0;
            const HRESULT result = stream->capture->GetBuffer(&data, &frames,
                                                              &flags, &position, &timestamp);
            if (FAILED(result) || frames == 0) continue;

            const size_t count = std::min<size_t>(frames, left.size());
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                std::fill(left.begin(), left.begin() + count, 0.0f);
                std::fill(right.begin(), right.begin() + count, 0.0f);
            } else {
                copyFromWasapi(data, stream->channels, stream->bits, stream->isFloat,
                               left.data(), right.data(), static_cast<int>(count));
            }
            self->microphone_->ring.write(left.data(), right.data(), count);
            stream->capture->ReleaseBuffer(frames);
        }
        stream->client->Stop();
    }

    stream->releaseCom();
    if (comOk) CoUninitialize();
    SetEvent(stream->finished);
    return 0;
}

bool WasapiEngine::startOutputSink(int deviceIndex, double sampleRate, std::string* error)
{
    return startOutput(deviceIndex, sampleRate, false, {}, error);
}

bool WasapiEngine::startOutputClock(int deviceIndex, double sampleRate,
                                    OutputCallback callback, std::string* error)
{
    return startOutput(deviceIndex, sampleRate, true, std::move(callback), error);
}

bool WasapiEngine::startOutput(int deviceIndex, double sampleRate, bool clock,
                               OutputCallback callback, std::string* error)
{
    stopOutput();
    const auto devices = enumerateDevices();
    if (deviceIndex < 0 || static_cast<size_t>(deviceIndex) >= devices.size()) {
        if (error) *error = "Dispositivo de saida invalido.";
        return false;
    }

    auto stream = std::make_unique<Stream>();
    stream->deviceId = devices[static_cast<size_t>(deviceIndex)].id;
    stream->requestedRate = static_cast<long>(sampleRate);
    stream->loopback = false;
    stream->clock = clock;
    stream->callback = std::move(callback);
    stream->audioEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    stream->startDone = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    stream->finished = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!stream->audioEvent || !stream->startDone || !stream->finished) {
        if (error) *error = "Falha ao criar os eventos da saida.";
        return false;
    }

    output_ = std::move(stream);
    outputRing_.reset();

    output_->thread = CreateThread(nullptr, 0, &WasapiEngine::outputThreadMain,
                                   this, 0, &output_->threadId);
    if (!output_->thread) {
        if (error) *error = "Falha ao criar a thread de saida.";
        stopOutput();
        return false;
    }

    const DWORD wait = WaitForSingleObject(output_->startDone, 5000);
    const HRESULT hr = output_->startHr.load(std::memory_order_acquire);
    if (wait != WAIT_OBJECT_0 || FAILED(hr)) {
        if (error) *error = output_->startError[0] ? output_->startError
                                                   : "Falha ao iniciar a saida WASAPI.";
        stopOutput();
        return false;
    }

    outputRate_.store(output_->formatRate, std::memory_order_relaxed);
    outputRunning_.store(true, std::memory_order_release);
    clockRunning_.store(clock, std::memory_order_release);
    return true;
}

void WasapiEngine::stopOutput()
{
    if (!output_) return;
    output_->stop.store(true, std::memory_order_release);
    if (output_->audioEvent) SetEvent(output_->audioEvent);
    outputRunning_.store(false, std::memory_order_release);
    clockRunning_.store(false, std::memory_order_release);
    outputRate_.store(0, std::memory_order_relaxed);
    if (output_->release()) output_.reset();
}

DWORD WINAPI WasapiEngine::outputThreadMain(LPVOID param)
{
    auto* self = static_cast<WasapiEngine*>(param);
    Stream* stream = self->output_.get();
    if (!stream) return 0;

    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comOk = SUCCEEDED(comHr);
    HRESULT hr = comOk ? self->openClient(*stream, stream->startError,
                                          sizeof(stream->startError)) : comHr;
    if (SUCCEEDED(hr))
        hr = stream->client->GetService(__uuidof(IAudioRenderClient),
                                        (void**)&stream->render);
    if (SUCCEEDED(hr))
        hr = stream->client->Start();

    stream->startHr.store(hr, std::memory_order_release);
    SetEvent(stream->startDone);

    if (SUCCEEDED(hr)) {
        std::vector<float> left(16384), right(16384);
        while (!stream->stop.load(std::memory_order_acquire)) {
            WaitForSingleObject(stream->audioEvent, 100);
            if (stream->stop.load(std::memory_order_acquire)) break;

            UINT32 padding = 0;
            if (FAILED(stream->client->GetCurrentPadding(&padding))) break;
            const UINT32 available = stream->bufferFrames > padding
                                     ? stream->bufferFrames - padding : 0;
            if (available == 0) continue;

            BYTE* data = nullptr;
            if (FAILED(stream->render->GetBuffer(available, &data))) continue;
            const size_t count = std::min<size_t>(available, left.size());

            if (stream->clock) {
                if (stream->callback)
                    stream->callback(left.data(), right.data(), static_cast<int>(count));
                else {
                    std::fill(left.begin(), left.begin() + count, 0.0f);
                    std::fill(right.begin(), right.begin() + count, 0.0f);
                }
            } else {
                self->outputRing_.read(left.data(), right.data(), count);
            }

            copyToWasapi(data, stream->channels, stream->bits, stream->isFloat,
                         left.data(), right.data(), static_cast<int>(count));
            stream->render->ReleaseBuffer(available, 0);
        }
        stream->client->Stop();
    }

    stream->releaseCom();
    if (comOk) CoUninitialize();
    SetEvent(stream->finished);
    return 0;
}

void WasapiEngine::writeOutput(const float* left, const float* right, int frames)
{
    if (frames <= 0) return;
    if (!outputRunning_.load(std::memory_order_acquire)) return;
    if (!output_ || output_->clock) return;
    outputRing_.write(left, right, static_cast<size_t>(frames));
}

bool WasapiEngine::readLoopback(float* left, float* right, int frames)
{
    if (frames <= 0) return false;
    const size_t count = loopbackRing_.read(left, right, static_cast<size_t>(frames));
    return count > 0;
}

bool WasapiEngine::readMicrophone(float* left, float* right, int frames)
{
    if (frames <= 0 || !microphone_) return false;
    const size_t count = microphone_->ring.read(left, right, static_cast<size_t>(frames));
    return count > 0;
}
