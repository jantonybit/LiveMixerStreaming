#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct IASIO;
struct ASIOBufferInfo;
struct ASIOCallbacks;
struct ASIOTime;
class AsioDriverList;

// Host ASIO minimalista: enumera drivers, abre o dispositivo, converte os formatos
// de amostra e chama o callback de DSP na thread de audio do driver (baixa latencia).
class AsioDevice
{
public:
    // inputs: canais de entrada ja convertidos para float [-1, 1]
    // outputs: canais de saida a serem preenchidos com float [-1, 1]
    using ProcessCallback = std::function<void(float* const* inputs, float** outputs, int frames)>;
    using RateCallback = std::function<void(double)>;

    struct Info
    {
        double sampleRate = 0.0;
        long bufferSize = 0;
        long inputLatency = 0;
        long outputLatency = 0;
        int numInputs = 0;
        int numOutputs = 0;
        std::vector<std::string> inputNames;
        std::vector<std::string> outputNames;
    };

    static std::vector<std::string> enumerateDrivers();

    AsioDevice();
    ~AsioDevice();
    AsioDevice(const AsioDevice&) = delete;
    AsioDevice& operator=(const AsioDevice&) = delete;

    bool open(int driverIndex, double sampleRate, long bufferSize,
              int numInputs, int numOutputs, void* systemWindow, std::string* error);
    void close();
    bool start(std::string* error);
    void stop();
    bool controlPanel();

    void setProcessCallback(ProcessCallback cb) { callback_ = std::move(cb); }
    void setRateCallback(RateCallback cb) { rateCallback_ = std::move(cb); }

    bool isRunning() const { return running_.load(std::memory_order_acquire); }
    bool isOpen() const { return driver_ != nullptr; }
    const Info& info() const { return info_; }
    float dspLoad() const { return dspLoad_.load(std::memory_order_relaxed); }
    bool consumeResetRequest();

private:
    static void bufferSwitchTrampoline(long doubleBufferIndex, long directProcess);
    static ASIOTime* bufferSwitchTimeInfoTrampoline(ASIOTime* params, long doubleBufferIndex,
                                                    long directProcess);
    static void sampleRateTrampoline(double rate);
    static long asioMessageTrampoline(long selector, long value, void* message, double* opt);

    void bufferSwitch(long index);
    long asioMessage(long selector, long value);

    std::unique_ptr<AsioDriverList> list_;
    // O driver guarda o ponteiro de callbacks ate disposeBuffers(); precisa viver
    // enquanto o driver estiver aberto (nao pode ser local de open()).
    std::unique_ptr<ASIOCallbacks> callbacks_;
    IASIO* driver_ = nullptr;
    int driverIndex_ = -1;

    std::vector<ASIOBufferInfo> bufferInfos_;
    std::vector<std::vector<float>> inputScratch_;
    std::vector<std::vector<float>> outputScratch_;
    std::vector<float*> inputPointers_;
    std::vector<float*> outputPointers_;
    std::vector<int> inputTypes_;
    std::vector<int> outputTypes_;

    Info info_;
    ProcessCallback callback_;
    RateCallback rateCallback_;

    std::atomic<bool> running_{false};
    std::atomic<bool> resetRequested_{false};
    std::atomic<float> dspLoad_{0.0f};
    double performanceFrequency_ = 1.0;
};
