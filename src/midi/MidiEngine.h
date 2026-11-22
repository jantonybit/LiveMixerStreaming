#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include "../mixer/Mixer.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

// Spinlock minimo: este toolchain nao inclui o cabecalho <mutex>.
class SpinLock
{
public:
    void lock() noexcept
    {
        while (flag_.test_and_set(std::memory_order_acquire)) {}
    }
    void unlock() noexcept { flag_.clear(std::memory_order_release); }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

class SpinLockGuard
{
public:
    explicit SpinLockGuard(SpinLock& lock) noexcept : lock_(lock) { lock_.lock(); }
    ~SpinLockGuard() { lock_.unlock(); }
    SpinLockGuard(const SpinLockGuard&) = delete;
    SpinLockGuard& operator=(const SpinLockGuard&) = delete;

private:
    SpinLock& lock_;
};

// Alvo de um controle da interface que pode ser associado a um evento MIDI.
enum class MidiTargetType
{
    None = 0,
    ChannelFader,
    ChannelGain,
    ChannelPan,
    ChannelMute,
    ChannelSolo,
    ChannelSend1,
    ChannelSend2,
    ChannelHighpass,
    ChannelPhase,
    ChannelEqGain,
    MasterFader,
    AuxReturn,
    FxParam,
    // Acrescentado no fim para nao alterar o valor dos alvos ja salvos em midi.map.
    ChannelSend3
};

struct MidiTarget
{
    MidiTargetType type = MidiTargetType::None;
    int index = 0; // canal, aux, etc.
    int sub = 0;   // banda de EQ ou indice de parametro de efeito

    bool operator==(const MidiTarget& other) const noexcept
    {
        return type == other.type && index == other.index && sub == other.sub;
    }
    bool operator!=(const MidiTarget& other) const noexcept { return !(*this == other); }
    bool valid() const noexcept { return type != MidiTargetType::None; }
};

// Uma associacao aprendida: evento MIDI -> parametro do mixer.
struct MidiBinding
{
    std::wstring deviceName;
    int deviceIndex = 0;
    int channel = 0;
    int kind = 0; // 0=CC, 1=Note, 2=Pitch Bend
    int data = 0; // numero do CC ou da nota
    MidiTarget target;
    bool toggle = false; // botoes liga/desliga (latch)
    float minimum = 0.0f;
    float maximum = 1.0f;
};

// Decisao do usuario quando um controle aprendido ja esta associado a outro comando.
enum class MidiConflictChoice
{
    Replace,  // substitui a associacao anterior
    Together, // mantem a anterior e adiciona a nova (um evento controla varios parametros)
    Cancel    // descarta a nova associacao
};

// Motor MIDI: abre todos os controladores conectados ao mesmo tempo e permite
// habilitar/desabilitar cada um individualmente pela interface.
class MidiEngine
{
public:
    struct DeviceInfo
    {
        int index = 0;
        std::wstring name;
        bool enabled = true;
        bool opened = false;
        std::string error;
    };

    struct Event
    {
        int deviceIndex = 0;
        int channel = 0;
        int kind = 0;
        int data = 0;
        int value = 0;
    };

    explicit MidiEngine(Mixer& mixer);
    ~MidiEngine();
    MidiEngine(const MidiEngine&) = delete;
    MidiEngine& operator=(const MidiEngine&) = delete;

    void refresh();
    const std::vector<DeviceInfo>& devices() const { return devicesInfo_; }
    void setEnabled(int index, bool enabled);

    void beginLearn(const MidiTarget& target, float minimum, float maximum);
    void cancelLearn();
    bool isLearning() const { return learning_.load(std::memory_order_acquire); }
    MidiTarget learningTarget() const { return learnTarget_; }

    bool isBound(const MidiTarget& target) const;
    void clearBinding(const MidiTarget& target);
    void clearAllBindings();
    const std::vector<MidiBinding>& bindings() const { return bindings_; }

    // Aprendizado com conflito: o evento foi capturado, mas aguarda a decisao do usuario.
    bool hasPendingBinding() const { return pending_; }
    const MidiBinding& pendingBinding() const { return pendingBinding_; }
    const std::vector<MidiBinding>& pendingConflicts() const { return pendingConflicts_; }
    void resolvePending(MidiConflictChoice choice);

    // Chamado uma vez por frame na thread de UI: aplica os eventos recebidos.
    void poll();

    bool save(const std::string& path) const;
    bool load(const std::string& path);

private:
    struct Device;

    static void CALLBACK inputCallback(HMIDIIN handle, UINT message,
                                       DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR);
    void pushEvent(const Event& event);
    void applyEvent(const Event& event);
    dsp::Param* resolve(const MidiTarget& target) const;
    static bool isBooleanTarget(MidiTargetType type);

    Mixer& mixer_;
    std::vector<std::unique_ptr<Device>> devices_;
    std::vector<DeviceInfo> devicesInfo_;
    std::vector<MidiBinding> bindings_;
    std::vector<std::wstring> disabledDevices_;

    std::vector<Event> queue_;
    SpinLock queueLock_;

    std::atomic<bool> learning_{false};
    MidiTarget learnTarget_;
    float learnMin_ = 0.0f;
    float learnMax_ = 1.0f;

    bool pending_ = false;
    MidiBinding pendingBinding_;
    std::vector<MidiBinding> pendingConflicts_;
};

// Instancia unica usada pelo callback estatico do winmm.
MidiEngine* midiEngineInstance();
