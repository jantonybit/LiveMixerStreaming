#include "AsioDevice.h"
#include "../dsp/DspUtils.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>

#include "asiosys.h"
#include "asio.h"
#ifndef interface
#define interface struct
#endif
#include "iasiodrv.h"
#include "asiolist.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

AsioDevice* g_activeDevice = nullptr;

inline uint32_t bswap32(uint32_t v) noexcept
{
    return (v >> 24) | ((v >> 8) & 0x0000ff00u) |
           ((v << 8) & 0x00ff0000u) | (v << 24);
}

inline uint64_t bswap64(uint64_t v) noexcept
{
    return (v >> 56) | ((v >> 40) & 0x000000000000ff00ULL) |
           ((v >> 24) & 0x0000000000ff0000ULL) | ((v >> 8) & 0x00000000ff000000ULL) |
           ((v << 8) & 0x000000ff00000000ULL) | ((v << 24) & 0x0000ff0000000000ULL) |
           ((v << 40) & 0x00ff000000000000ULL) | (v << 56);
}

inline int sampleWidth(int type) noexcept
{
    switch (type) {
    case ASIOSTInt16LSB: case ASIOSTInt16MSB:
    case ASIOSTDSDInt8LSB1: case ASIOSTDSDInt8MSB1:
        return 2;
    case ASIOSTInt24LSB: case ASIOSTInt24MSB:
        return 3;
    case ASIOSTFloat64LSB: case ASIOSTFloat64MSB:
        return 8;
    default:
        return 4;
    }
}

inline float scaledInt(int32_t v, int bits) noexcept
{
    return static_cast<float>(v) / static_cast<float>(1LL << bits);
}

inline int32_t quantize(float x, int bits) noexcept
{
    const double scale = static_cast<double>((1LL << bits) - 1);
    return static_cast<int32_t>(std::lround(static_cast<double>(x) * scale));
}

float sampleToFloat(const void* ptr, int type) noexcept
{
    const auto* b = static_cast<const unsigned char*>(ptr);
    int32_t i = 0;
    uint32_t u = 0;
    float f = 0.0f;
    uint64_t u64 = 0;
    double d = 0.0;

    switch (type) {
    case ASIOSTInt16LSB:
        std::memcpy(&i, b, 2);
        return scaledInt(static_cast<int16_t>(static_cast<uint16_t>(i)), 15);
    case ASIOSTInt16MSB:
        i = (static_cast<int32_t>(static_cast<int8_t>(b[0])) << 8) | b[1];
        return scaledInt(i, 15);
    case ASIOSTInt24LSB:
        i = static_cast<int32_t>(static_cast<uint32_t>(b[0]) |
            (static_cast<uint32_t>(b[1]) << 8) |
            (static_cast<uint32_t>(static_cast<int8_t>(b[2])) << 16));
        return scaledInt(i, 23);
    case ASIOSTInt24MSB:
        i = (static_cast<int32_t>(static_cast<int8_t>(b[0])) << 16) |
            (static_cast<uint32_t>(b[1]) << 8) | b[2];
        return scaledInt(i, 23);
    case ASIOSTInt32LSB:
        std::memcpy(&i, b, 4);
        return scaledInt(i, 31);
    case ASIOSTInt32MSB:
        std::memcpy(&u, b, 4);
        return scaledInt(static_cast<int32_t>(bswap32(u)), 31);

    case ASIOSTInt32LSB16: case ASIOSTInt32MSB16:
    case ASIOSTInt32LSB18: case ASIOSTInt32MSB18:
    case ASIOSTInt32LSB20: case ASIOSTInt32MSB20:
    case ASIOSTInt32LSB24: case ASIOSTInt32MSB24: {
        std::memcpy(&u, b, 4);
        const bool msb = (type == ASIOSTInt32MSB16 || type == ASIOSTInt32MSB18 ||
                          type == ASIOSTInt32MSB20 || type == ASIOSTInt32MSB24);
        i = static_cast<int32_t>(msb ? bswap32(u) : u);
        int bits = 23;
        if (type == ASIOSTInt32LSB16 || type == ASIOSTInt32MSB16) bits = 15;
        else if (type == ASIOSTInt32LSB18 || type == ASIOSTInt32MSB18) bits = 17;
        else if (type == ASIOSTInt32LSB20 || type == ASIOSTInt32MSB20) bits = 19;
        return scaledInt(i, bits);
    }

    case ASIOSTFloat32LSB:
        std::memcpy(&f, b, 4);
        return f;
    case ASIOSTFloat32MSB:
        std::memcpy(&u, b, 4);
        u = bswap32(u);
        std::memcpy(&f, &u, 4);
        return f;
    case ASIOSTFloat64LSB:
        std::memcpy(&d, b, 8);
        return static_cast<float>(d);
    case ASIOSTFloat64MSB:
        std::memcpy(&u64, b, 8);
        u64 = bswap64(u64);
        std::memcpy(&d, &u64, 8);
        return static_cast<float>(d);
    default:
        return 0.0f;
    }
}

void floatToSample(float value, void* ptr, int type) noexcept
{
    auto* b = static_cast<unsigned char*>(ptr);
    const float x = dsp::clamp(value, -1.0f, 1.0f);
    int32_t i = 0;
    uint32_t u = 0;
    uint64_t u64 = 0;

    switch (type) {
    case ASIOSTInt16LSB: {
        const int16_t v = static_cast<int16_t>(quantize(x, 15));
        std::memcpy(b, &v, 2);
        break;
    }
    case ASIOSTInt16MSB: {
        const int16_t v = static_cast<int16_t>(quantize(x, 15));
        b[0] = static_cast<unsigned char>(v >> 8);
        b[1] = static_cast<unsigned char>(v);
        break;
    }
    case ASIOSTInt24LSB:
        i = quantize(x, 23);
        b[0] = static_cast<unsigned char>(i);
        b[1] = static_cast<unsigned char>(i >> 8);
        b[2] = static_cast<unsigned char>(i >> 16);
        break;
    case ASIOSTInt24MSB:
        i = quantize(x, 23);
        b[0] = static_cast<unsigned char>(i >> 16);
        b[1] = static_cast<unsigned char>(i >> 8);
        b[2] = static_cast<unsigned char>(i);
        break;
    case ASIOSTInt32LSB:
        i = quantize(x, 31);
        std::memcpy(b, &i, 4);
        break;
    case ASIOSTInt32MSB:
        i = quantize(x, 31);
        u = bswap32(static_cast<uint32_t>(i));
        std::memcpy(b, &u, 4);
        break;

    case ASIOSTInt32LSB16: case ASIOSTInt32MSB16:
    case ASIOSTInt32LSB18: case ASIOSTInt32MSB18:
    case ASIOSTInt32LSB20: case ASIOSTInt32MSB20:
    case ASIOSTInt32LSB24: case ASIOSTInt32MSB24: {
        int bits = 23;
        if (type == ASIOSTInt32LSB16 || type == ASIOSTInt32MSB16) bits = 15;
        else if (type == ASIOSTInt32LSB18 || type == ASIOSTInt32MSB18) bits = 17;
        else if (type == ASIOSTInt32LSB20 || type == ASIOSTInt32MSB20) bits = 19;
        const bool msb = (type == ASIOSTInt32MSB16 || type == ASIOSTInt32MSB18 ||
                          type == ASIOSTInt32MSB20 || type == ASIOSTInt32MSB24);
        i = quantize(x, bits) << (31 - bits);
        u = static_cast<uint32_t>(i);
        if (msb) u = bswap32(u);
        std::memcpy(b, &u, 4);
        break;
    }

    case ASIOSTFloat32LSB:
        std::memcpy(b, &x, 4);
        break;
    case ASIOSTFloat32MSB:
        std::memcpy(&u, &x, 4);
        u = bswap32(u);
        std::memcpy(b, &u, 4);
        break;
    case ASIOSTFloat64LSB: {
        const double v = x;
        std::memcpy(b, &v, 8);
        break;
    }
    case ASIOSTFloat64MSB: {
        const double v = x;
        std::memcpy(&u64, &v, 8);
        u64 = bswap64(u64);
        std::memcpy(b, &u64, 8);
        break;
    }
    default:
        break;
    }
}

} // namespace

std::vector<std::string> AsioDevice::enumerateDrivers()
{
    std::vector<std::string> result;
    AsioDriverList list;
    const long count = list.asioGetNumDev();
    if (count > 0) result.reserve(static_cast<size_t>(count));
    for (long i = 0; i < count; ++i) {
        char name[128] = {};
        if (list.asioGetDriverName(static_cast<int>(i), name, static_cast<int>(sizeof(name))) == 0)
            result.emplace_back(name);
    }
    return result;
}

AsioDevice::AsioDevice()
{
    LARGE_INTEGER freq{};
    if (QueryPerformanceFrequency(&freq) && freq.QuadPart > 0)
        performanceFrequency_ = static_cast<double>(freq.QuadPart);
}

AsioDevice::~AsioDevice()
{
    close();
}

bool AsioDevice::open(int driverIndex, double sampleRate, long bufferSize,
                      int numInputs, int numOutputs, void* systemWindow, std::string* error)
{
    close();

    auto fail = [error](const std::string& message) {
        if (error) *error = message;
        return false;
    };

    try {
        list_ = std::make_unique<AsioDriverList>();
    } catch (...) {
        return fail("Falha ao enumerar os drivers ASIO.");
    }

    if (driverIndex < 0 || driverIndex >= list_->asioGetNumDev())
        return fail("Indice de driver ASIO invalido.");

    void* raw = nullptr;
    const LONG openResult = list_->asioOpenDriver(driverIndex, &raw);
    if (openResult != 0 || raw == nullptr)
        return fail("Nao foi possivel abrir o driver ASIO (codigo " + std::to_string(openResult) + ").");

    driver_ = static_cast<IASIO*>(raw);
    driverIndex_ = driverIndex;

    if (!driver_->init(systemWindow)) {
        fail("O driver ASIO rejeitou a inicializacao.");
        close();
        return false;
    }

    long maxInputs = 0, maxOutputs = 0;
    if (driver_->getChannels(&maxInputs, &maxOutputs) != ASE_OK) {
        fail("Nao foi possivel consultar os canais do driver.");
        close();
        return false;
    }

    numInputs = std::max(0, std::min(numInputs, static_cast<int>(maxInputs)));
    numOutputs = std::max(0, std::min(numOutputs, static_cast<int>(maxOutputs)));
    if (numInputs <= 0 && numOutputs <= 0) {
        fail("O driver nao expoe canais de entrada ou saida.");
        close();
        return false;
    }

    long minSize = 0, maxSize = 0, preferred = 0, granularity = 0;
    driver_->getBufferSize(&minSize, &maxSize, &preferred, &granularity);
    long chosenSize = bufferSize > 0 ? bufferSize : preferred;
    if (chosenSize <= 0) chosenSize = preferred > 0 ? preferred : 256;
    chosenSize = std::max(chosenSize, minSize);
    chosenSize = std::min(chosenSize, maxSize);
    if (granularity > 1 && minSize > 0 && chosenSize > minSize)
        chosenSize = minSize + ((chosenSize - minSize) / granularity) * granularity;

    double actualRate = sampleRate;
    if (driver_->canSampleRate(sampleRate) != ASE_OK) {
        driver_->getSampleRate(&actualRate);
        if (actualRate <= 0.0) actualRate = sampleRate;
    } else if (driver_->setSampleRate(sampleRate) != ASE_OK) {
        driver_->getSampleRate(&actualRate);
        if (actualRate <= 0.0) actualRate = sampleRate;
    }

    info_ = Info{};
    info_.sampleRate = actualRate;
    info_.bufferSize = chosenSize;
    info_.numInputs = numInputs;
    info_.numOutputs = numOutputs;

    bufferInfos_.clear();
    bufferInfos_.reserve(static_cast<size_t>(numInputs + numOutputs));
    inputTypes_.clear();
    outputTypes_.clear();

    for (int ch = 0; ch < numInputs; ++ch) {
        ASIOChannelInfo ci{};
        ci.channel = ch;
        ci.isInput = ASIOTrue;
        driver_->getChannelInfo(&ci);
        ASIOBufferInfo bi{};
        bi.isInput = ASIOTrue;
        bi.channelNum = ch;
        bufferInfos_.push_back(bi);
        inputTypes_.push_back(static_cast<int>(ci.type));
        std::string name = ci.name[0] ? std::string(ci.name) : ("Entrada " + std::to_string(ch + 1));
        info_.inputNames.push_back(name);
    }
    for (int ch = 0; ch < numOutputs; ++ch) {
        ASIOChannelInfo ci{};
        ci.channel = ch;
        ci.isInput = ASIOFalse;
        driver_->getChannelInfo(&ci);
        ASIOBufferInfo bi{};
        bi.isInput = ASIOFalse;
        bi.channelNum = ch;
        bufferInfos_.push_back(bi);
        outputTypes_.push_back(static_cast<int>(ci.type));
        std::string name = ci.name[0] ? std::string(ci.name) : ("Saida " + std::to_string(ch + 1));
        info_.outputNames.push_back(name);
    }

    // O ponteiro de callbacks e mantido pelo driver; deve permanecer valido.
    callbacks_ = std::make_unique<ASIOCallbacks>();
    ASIOCallbacks& callbacks = *callbacks_;
    callbacks.bufferSwitch = &AsioDevice::bufferSwitchTrampoline;
    callbacks.sampleRateDidChange = &AsioDevice::sampleRateTrampoline;
    callbacks.asioMessage = &AsioDevice::asioMessageTrampoline;
    callbacks.bufferSwitchTimeInfo = &AsioDevice::bufferSwitchTimeInfoTrampoline;

    const ASIOError createResult = driver_->createBuffers(
        bufferInfos_.data(), static_cast<long>(bufferInfos_.size()), chosenSize, &callbacks);
    if (createResult != ASE_OK) {
        fail("createBuffers falhou (codigo ASIO " + std::to_string(createResult) + ").");
        close();
        return false;
    }

    const size_t frames = static_cast<size_t>(chosenSize);
    inputScratch_.assign(static_cast<size_t>(numInputs), std::vector<float>(frames * 2, 0.0f));
    outputScratch_.assign(static_cast<size_t>(numOutputs), std::vector<float>(frames * 2, 0.0f));
    inputPointers_.assign(static_cast<size_t>(numInputs), nullptr);
    outputPointers_.assign(static_cast<size_t>(numOutputs), nullptr);

    long inLatency = 0, outLatency = 0;
    driver_->getLatencies(&inLatency, &outLatency);
    info_.inputLatency = inLatency;
    info_.outputLatency = outLatency;

    g_activeDevice = this;
    return true;
}

void AsioDevice::close()
{
    stop();

    if (driver_) {
        driver_->disposeBuffers();
        if (list_) list_->asioCloseDriver(driverIndex_);
    }
    driver_ = nullptr;
    driverIndex_ = -1;
    callbacks_.reset();
    list_.reset();
    bufferInfos_.clear();
    inputScratch_.clear();
    outputScratch_.clear();
    inputPointers_.clear();
    outputPointers_.clear();
    inputTypes_.clear();
    outputTypes_.clear();
    info_ = Info{};
    if (g_activeDevice == this) g_activeDevice = nullptr;
}

bool AsioDevice::start(std::string* error)
{
    if (!driver_) {
        if (error) *error = "Nenhum driver ASIO aberto.";
        return false;
    }
    running_.store(true, std::memory_order_release);
    const ASIOError result = driver_->start();
    if (result != ASE_OK) {
        running_.store(false, std::memory_order_release);
        if (error) *error = "Falha ao iniciar o motor ASIO (codigo " + std::to_string(result) + ").";
        return false;
    }
    return true;
}

void AsioDevice::stop()
{
    if (driver_ && running_.exchange(false, std::memory_order_acq_rel))
        driver_->stop();
}

bool AsioDevice::controlPanel()
{
    return driver_ ? driver_->controlPanel() == ASE_OK : false;
}

bool AsioDevice::consumeResetRequest()
{
    return resetRequested_.exchange(false, std::memory_order_acq_rel);
}

void AsioDevice::bufferSwitchTrampoline(long doubleBufferIndex, long)
{
    if (g_activeDevice) g_activeDevice->bufferSwitch(doubleBufferIndex);
}

// Alguns drivers (ex.: UMC/Behringer) chamam este callback mesmo quando o host
// nao anunciou suporte a time info. Deixa-lo nulo derruba o aplicativo na thread
// de audio; redirecionamos para o mesmo processamento do bufferSwitch.
ASIOTime* AsioDevice::bufferSwitchTimeInfoTrampoline(ASIOTime* params, long doubleBufferIndex, long)
{
    if (g_activeDevice) g_activeDevice->bufferSwitch(doubleBufferIndex);
    return params;
}

void AsioDevice::sampleRateTrampoline(double rate)
{
    if (!g_activeDevice) return;
    g_activeDevice->info_.sampleRate = rate;
    if (g_activeDevice->rateCallback_) g_activeDevice->rateCallback_(rate);
}

long AsioDevice::asioMessageTrampoline(long selector, long value, void*, double*)
{
    return g_activeDevice ? g_activeDevice->asioMessage(selector, value) : 0L;
}

long AsioDevice::asioMessage(long selector, long value)
{
    switch (selector) {
    case kAsioSelectorSupported:
        return (value == kAsioEngineVersion || value == kAsioResetRequest ||
                value == kAsioResyncRequest || value == kAsioLatenciesChanged ||
                value == kAsioOverload || value == kAsioSelectorSupported) ? 1L : 0L;
    case kAsioEngineVersion:
        return 2L;
    case kAsioResetRequest:
        resetRequested_.store(true, std::memory_order_release);
        return 1L;
    case kAsioResyncRequest:
    case kAsioLatenciesChanged:
    case kAsioOverload:
        return 1L;
    default:
        return 0L;
    }
}

void AsioDevice::bufferSwitch(long index)
{
    if (!running_.load(std::memory_order_acquire) || !driver_ || !callback_) return;

    LARGE_INTEGER t0{}, t1{};
    QueryPerformanceCounter(&t0);

    const size_t half = (index == 0) ? 0u : 1u;
    const int frames = static_cast<int>(info_.bufferSize);
    const size_t framesU = static_cast<size_t>(frames);

    for (size_t ch = 0; ch < inputPointers_.size(); ++ch) {
        float* dst = inputScratch_[ch].data() + half * framesU;
        const auto* src = static_cast<const unsigned char*>(bufferInfos_[ch].buffers[half]);
        const int width = sampleWidth(inputTypes_[ch]);
        for (int i = 0; i < frames; ++i)
            dst[i] = sampleToFloat(src + static_cast<size_t>(i) * width, inputTypes_[ch]);
        inputPointers_[ch] = dst;
    }

    for (size_t ch = 0; ch < outputPointers_.size(); ++ch) {
        float* dst = outputScratch_[ch].data() + half * framesU;
        std::fill(dst, dst + frames, 0.0f);
        outputPointers_[ch] = dst;
    }

    callback_(inputPointers_.data(), outputPointers_.data(), frames);

    for (size_t ch = 0; ch < outputPointers_.size(); ++ch) {
        const float* src = outputPointers_[ch];
        auto* dst = static_cast<unsigned char*>(bufferInfos_[inputPointers_.size() + ch].buffers[half]);
        const int width = sampleWidth(outputTypes_[ch]);
        for (int i = 0; i < frames; ++i)
            floatToSample(src[i], dst + static_cast<size_t>(i) * width, outputTypes_[ch]);
    }

    QueryPerformanceCounter(&t1);
    const double elapsed = static_cast<double>(t1.QuadPart - t0.QuadPart) / performanceFrequency_;
    const double budget = static_cast<double>(frames) / std::max(1.0, info_.sampleRate);
    if (budget > 0.0)
        dspLoad_.store(static_cast<float>(100.0 * elapsed / budget), std::memory_order_relaxed);

    driver_->outputReady();
}
