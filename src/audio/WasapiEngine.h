#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "StereoRing.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct IMMDeviceEnumerator;
struct IMMDeviceCollection;
struct IMMDevice;
struct IAudioClient;
struct IAudioCaptureClient;
struct IAudioRenderClient;

// Motor WASAPI para tres funcoes independentes:
//   - loopback de um dispositivo de reproducao (audio do computador) -> mixer
//   - captura de um dispositivo de entrada (microfone) -> mixer
//   - saida do mix para um dispositivo de reproducao (Master Out, capturavel
//     por qualquer aplicacao de gravacao/captura de audio)
//
// Cada stream cria, usa e libera todos os objetos COM dentro da sua propria thread STA.
// A thread de UI apenas configura o Stream, cria a thread e espera um handshake
// (startDone / finished). Isso evita RPC_E_WRONG_THREAD e garante que nada seja
// destruido enquanto a thread de audio ainda esta usando o IAudioClient.
class WasapiEngine
{
public:
    struct DeviceInfo
    {
        std::wstring id;
        std::wstring name;
    };

    using OutputCallback = std::function<void(float* left, float* right, int frames)>;

    static std::vector<DeviceInfo> enumerateDevices();
    static int defaultDeviceIndex(const std::vector<DeviceInfo>& devices);

    // Dispositivos de captura (microfones/entradas de linha) enumerados via WASAPI.
    static std::vector<DeviceInfo> enumerateCaptureDevices();

    WasapiEngine();
    ~WasapiEngine();
    WasapiEngine(const WasapiEngine&) = delete;
    WasapiEngine& operator=(const WasapiEngine&) = delete;

    // Captura o que esta sendo enviado ao dispositivo escolhido (loopback).
    // 'muteOutput' silencia o dispositivo no Windows: o som continua sendo
    // capturado, mas nao sai nas caixas (util para game/musica -> mixer).
    bool startLoopback(int deviceIndex, double sampleRate, bool muteOutput, std::string* error);
    void stopLoopback();
    bool isLoopbackRunning() const { return loopbackRunning_.load(std::memory_order_acquire); }
    bool isLoopbackMuted() const { return loopbackMuted_.load(std::memory_order_acquire); }
    int loopbackRate() const { return loopbackRate_.load(std::memory_order_relaxed); }

    // Entrada fisica via WASAPI: captura estereo de um microfone/dispositivo de
    // entrada. Funciona mesmo quando nao ha driver ASIO instalado.
    bool startMicrophone(int deviceIndex, double sampleRate, std::string* error);
    void stopMicrophone();
    bool isMicrophoneRunning() const { return microphoneRunning_.load(std::memory_order_acquire); }
    int microphoneRate() const { return microphoneRate_.load(std::memory_order_relaxed); }

    // Saida "sink": a thread do ASIO empurra o mix para a ring buffer.
    bool startOutputSink(int deviceIndex, double sampleRate, std::string* error);
    void writeOutput(const float* left, const float* right, int frames);

    // Saida como relogio mestre (modo sem ASIO): a thread WASAPI chama 'callback'.
    bool startOutputClock(int deviceIndex, double sampleRate, OutputCallback callback,
                          std::string* error);
    bool isClockRunning() const { return clockRunning_.load(std::memory_order_acquire); }
    int outputRate() const { return outputRate_.load(std::memory_order_relaxed); }

    void stopOutput();
    bool isOutputRunning() const { return outputRunning_.load(std::memory_order_acquire); }

    // Consumido pela thread de audio do mixer: copia o loopback capturado.
    bool readLoopback(float* left, float* right, int frames);

    // Consumido pela thread de audio do mixer: copia o microfone WASAPI capturado.
    bool readMicrophone(float* left, float* right, int frames);

    // Taxa efetiva do stream ativo (para exibicao).
    double actualRate() const { return static_cast<double>(outputRate_.load(std::memory_order_relaxed)); }

private:
    struct Stream;
    struct MicrophoneStream
    {
        std::unique_ptr<Stream> stream;
        StereoRing ring{8192};
    };

    static DWORD WINAPI loopbackThreadMain(LPVOID param);
    static DWORD WINAPI microphoneThreadMain(LPVOID param);
    static DWORD WINAPI outputThreadMain(LPVOID param);

    static HRESULT openClient(Stream& stream, char* error, size_t errorSize);
    static void writeError(char* error, size_t errorSize, const char* message, HRESULT hr);

    bool startOutput(int deviceIndex, double sampleRate, bool clock,
                     OutputCallback callback, std::string* error);

    std::unique_ptr<Stream> loopback_;
    std::unique_ptr<Stream> output_;
    std::unique_ptr<MicrophoneStream> microphone_;

    StereoRing loopbackRing_{8192};
    StereoRing outputRing_{8192};

    std::atomic<bool> loopbackRunning_{false};
    std::atomic<bool> loopbackMuted_{false};
    std::atomic<bool> outputRunning_{false};
    std::atomic<bool> clockRunning_{false};
    std::atomic<bool> microphoneRunning_{false};
    std::atomic<int> loopbackRate_{0};
    std::atomic<int> outputRate_{0};
    std::atomic<int> microphoneRate_{0};
};
