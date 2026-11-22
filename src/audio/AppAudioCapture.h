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
#include <memory>
#include <string>
#include <vector>

// Captura o audio de aplicacoes especificas usando a API documentada do Windows
// ActivateAudioInterfaceAsync + AUDIOCLIENT_ACTIVATION_PARAMS (Win 10 build 20348+).
// Diferente do loopback de endpoint, a captura por processo nao depende de qual
// dispositivo de reproducao o app esta usando e nao captura o nosso proprio mix.
class AppAudioCapture
{
public:
    struct AppInfo
    {
        DWORD pid = 0;
        std::wstring name;  // nome do executavel (ou nome de sessao)
        float peak = 0.0f;
    };

    // Enumera sessoes de render ativas (apps que estao gerando audio).
    // 'excludePid' normalmente e o nosso proprio processo.
    static std::vector<AppInfo> enumerateApps(DWORD excludePid);

    AppAudioCapture();
    ~AppAudioCapture();
    AppAudioCapture(const AppAudioCapture&) = delete;
    AppAudioCapture& operator=(const AppAudioCapture&) = delete;

    // Sincroniza a lista: mantem capturas existentes, inicia novas e remove as
    // que terminaram. Seguro chamar com o motor parado ou em execucao.
    void refresh(DWORD excludePid, double sampleRate, std::string* error);
    void stopAll();

    int count() const;
    std::string name(int index) const;
    DWORD pid(int index) const;

    // Consumido pela thread de audio do mixer: preenche 'left'/'right'.
    bool read(int index, float* left, float* right, int frames);

    // "Desvio": silencia a sessao Windows dos apps capturados para que o som
    // nao saia nas caixas. A captura por processo le o streamantes do volume da
    // sessao, entao o nivel entregue ao mixer permanece cheio. O estado original
    // de mudo de cada app e restaurado em stopAll()/destrutor.
    void setAppsMuted(bool mute, std::string* error);
    bool appsMuted() const { return appsMuted_.load(std::memory_order_acquire); }

private:
    struct Entry;
    struct MuteState
    {
        DWORD pid = 0;
        bool original = false;
    };

    static DWORD WINAPI captureThreadMain(LPVOID param);
    static bool setSessionMute(DWORD pid, bool mute, bool* previous);
    void restoreMute(DWORD pid);

    // Protege apenas o vetor de entradas. A thread de audio mantem o lock por
    // poucas instrucoes; a destruicao de entradas (que espera a worker thread)
    // acontece sempre fora do lock.
    class Lock
    {
    public:
        Lock() noexcept = default;
        void lock() noexcept
        {
            while (flag_.test_and_set(std::memory_order_acquire)) {}
        }
        void unlock() noexcept { flag_.clear(std::memory_order_release); }

    private:
        std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    };

    mutable Lock lock_;
    std::vector<std::unique_ptr<Entry>> entries_;
    std::vector<MuteState> muteStates_;
    std::atomic<bool> appsMuted_{false};
};
