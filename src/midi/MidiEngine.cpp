#include "MidiEngine.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace {

MidiEngine* g_midiEngine = nullptr;

std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty()) return std::wstring();
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size > 0 ? size : 0), L'\0');
    if (size > 0)
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                            result.data(), size);
    return result;
}

std::string wideToUtf8(const std::wstring& text)
{
    if (text.empty()) return std::string();
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr, 0,
                                         nullptr, nullptr);
    std::string result(static_cast<size_t>(size > 0 ? size : 0), '\0');
    if (size > 0)
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                            result.data(), size, nullptr, nullptr);
    return result;
}

} // namespace

MidiEngine* midiEngineInstance() { return g_midiEngine; }

struct MidiEngine::Device
{
    int index = 0;
    std::wstring name;
    bool enabled = true;
    HMIDIIN handle = nullptr;
};

MidiEngine::MidiEngine(Mixer& mixer) : mixer_(mixer)
{
    g_midiEngine = this;
}

MidiEngine::~MidiEngine()
{
    for (auto& device : devices_) {
        if (device->handle) {
            midiInStop(device->handle);
            midiInClose(device->handle);
            device->handle = nullptr;
        }
    }
    devices_.clear();
    if (g_midiEngine == this) g_midiEngine = nullptr;
}

void MidiEngine::refresh()
{
    for (auto& device : devices_) {
        if (device->handle) {
            midiInStop(device->handle);
            midiInClose(device->handle);
            device->handle = nullptr;
        }
    }
    devices_.clear();
    devicesInfo_.clear();

    const UINT count = midiInGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        MIDIINCAPSW caps{};
        const MMRESULT result = midiInGetDevCapsW(i, &caps, sizeof(caps));
        auto device = std::make_unique<Device>();
        device->index = static_cast<int>(i);
        device->name = result == MMSYSERR_NOERROR ? caps.szPname : L"Dispositivo MIDI";
        device->enabled = std::find(disabledDevices_.begin(), disabledDevices_.end(),
                                    device->name) == disabledDevices_.end();

        DeviceInfo info;
        info.index = device->index;
        info.name = device->name;
        info.enabled = device->enabled;

        if (device->enabled) {
            const MMRESULT openResult = midiInOpen(
                &device->handle, i,
                reinterpret_cast<DWORD_PTR>(&MidiEngine::inputCallback),
                reinterpret_cast<DWORD_PTR>(device.get()),
                CALLBACK_FUNCTION);
            if (openResult == MMSYSERR_NOERROR) {
                if (midiInStart(device->handle) == MMSYSERR_NOERROR) {
                    info.opened = true;
                } else {
                    info.error = "Falha ao iniciar a entrada MIDI.";
                    midiInClose(device->handle);
                    device->handle = nullptr;
                }
            } else {
                info.error = "Falha ao abrir a entrada MIDI (codigo " +
                             std::to_string(openResult) + ").";
            }
        }

        devices_.push_back(std::move(device));
        devicesInfo_.push_back(std::move(info));
    }
}

void MidiEngine::setEnabled(int index, bool enabled)
{
    if (index < 0 || static_cast<size_t>(index) >= devices_.size()) return;
    Device& device = *devices_[static_cast<size_t>(index)];
    if (device.enabled == enabled) return;

    if (device.enabled && device.handle) {
        midiInStop(device.handle);
        midiInClose(device.handle);
        device.handle = nullptr;
    }
    device.enabled = enabled;
    if (enabled) {
        disabledDevices_.erase(std::remove(disabledDevices_.begin(), disabledDevices_.end(),
                                           device.name), disabledDevices_.end());
    } else if (std::find(disabledDevices_.begin(), disabledDevices_.end(), device.name) ==
               disabledDevices_.end()) {
        disabledDevices_.push_back(device.name);
    }
    DeviceInfo& info = devicesInfo_[static_cast<size_t>(index)];
    info.enabled = enabled;
    info.error.clear();

    if (enabled) {
        const MMRESULT openResult = midiInOpen(
            &device.handle, static_cast<UINT>(index),
            reinterpret_cast<DWORD_PTR>(&MidiEngine::inputCallback),
            reinterpret_cast<DWORD_PTR>(&device), CALLBACK_FUNCTION);
        if (openResult == MMSYSERR_NOERROR) {
            if (midiInStart(device.handle) == MMSYSERR_NOERROR)
                info.opened = true;
            else {
                info.error = "Falha ao iniciar a entrada MIDI.";
                midiInClose(device.handle);
                device.handle = nullptr;
            }
        } else {
            info.error = "Falha ao abrir a entrada MIDI (codigo " +
                         std::to_string(openResult) + ").";
        }
    } else {
        info.opened = false;
    }
}

void CALLBACK MidiEngine::inputCallback(HMIDIIN, UINT message,
                                        DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR)
{
    MidiEngine* engine = g_midiEngine;
    if (!engine || message != MIM_DATA) return;

    auto* device = reinterpret_cast<Device*>(instance);
    if (!device || !device->enabled) return;

    const DWORD raw = static_cast<DWORD>(param1);
    const int status = static_cast<int>(raw & 0xF0);
    const int channel = static_cast<int>(raw & 0x0F);
    const int data1 = static_cast<int>((raw >> 8) & 0x7F);
    const int data2 = static_cast<int>((raw >> 16) & 0x7F);

    Event event;
    event.deviceIndex = device->index;
    event.channel = channel;

    if (status == 0x80 || (status == 0x90 && data2 == 0)) {
        event.kind = 1; // Note Off
        event.data = data1;
        event.value = 0;
    } else if (status == 0x90) {
        event.kind = 1; // Note On
        event.data = data1;
        event.value = data2;
    } else if (status == 0xB0) {
        event.kind = 0; // Control Change
        event.data = data1;
        event.value = data2;
    } else if (status == 0xE0) {
        event.kind = 2; // Pitch Bend
        event.data = 0;
        event.value = (data2 << 7) | data1;
    } else {
        return;
    }

    engine->pushEvent(event);
}

void MidiEngine::pushEvent(const Event& event)
{
    SpinLockGuard guard(queueLock_);
    queue_.push_back(event);
}

void MidiEngine::beginLearn(const MidiTarget& target, float minimum, float maximum)
{
    if (!target.valid()) return;
    pending_ = false;
    pendingConflicts_.clear();
    learnTarget_ = target;
    learnMin_ = minimum;
    learnMax_ = maximum;
    learning_.store(true, std::memory_order_release);
}

void MidiEngine::cancelLearn()
{
    learning_.store(false, std::memory_order_release);
    pending_ = false;
    pendingConflicts_.clear();
}

bool MidiEngine::isBound(const MidiTarget& target) const
{
    return std::any_of(bindings_.begin(), bindings_.end(),
                       [&](const MidiBinding& binding) { return binding.target == target; });
}

void MidiEngine::clearBinding(const MidiTarget& target)
{
    bindings_.erase(std::remove_if(bindings_.begin(), bindings_.end(),
                                   [&](const MidiBinding& binding) { return binding.target == target; }),
                    bindings_.end());
}

void MidiEngine::clearAllBindings()
{
    bindings_.clear();
}

void MidiEngine::resolvePending(MidiConflictChoice choice)
{
    if (!pending_) return;

    if (choice == MidiConflictChoice::Replace) {
        const MidiBinding& selected = pendingBinding_;
        bindings_.erase(std::remove_if(bindings_.begin(), bindings_.end(),
            [&](const MidiBinding& old) {
                return old.deviceIndex == selected.deviceIndex && old.channel == selected.channel &&
                       old.kind == selected.kind && old.data == selected.data;
            }), bindings_.end());
        bindings_.push_back(pendingBinding_);
    } else if (choice == MidiConflictChoice::Together) {
        bindings_.push_back(pendingBinding_);
    }

    pending_ = false;
    pendingConflicts_.clear();
}

void MidiEngine::poll()
{
    std::vector<Event> events;
    {
        SpinLockGuard guard(queueLock_);
        events.swap(queue_);
    }
    for (const Event& event : events) applyEvent(event);
}

void MidiEngine::applyEvent(const Event& event)
{
    const bool wasLearning = isLearning();
    if (wasLearning) {
        MidiBinding binding;
        if (static_cast<size_t>(event.deviceIndex) < devicesInfo_.size())
            binding.deviceName = devicesInfo_[static_cast<size_t>(event.deviceIndex)].name;
        binding.deviceIndex = event.deviceIndex;
        binding.channel = event.channel;
        binding.kind = event.kind;
        binding.data = event.data;
        binding.target = learnTarget_;
        binding.toggle = isBooleanTarget(learnTarget_.type);
        binding.minimum = learnMin_;
        binding.maximum = learnMax_;

        // Se o mesmo parametro ja usa este evento, nao ha conflito real: encerra.
        const bool alreadySame = std::any_of(bindings_.begin(), bindings_.end(),
            [&](const MidiBinding& old) {
                return old.deviceIndex == binding.deviceIndex && old.channel == binding.channel &&
                       old.kind == binding.kind && old.data == binding.data &&
                       old.target == binding.target;
            });
        if (alreadySame) {
            learning_.store(false, std::memory_order_release);
            return;
        }

        // Guarda o evento e as associacoes que ele ja controla. A interface
        // apresentara as opcoes Substituir / Usar em conjunto / Cancelar.
        pendingConflicts_.clear();
        for (const MidiBinding& old : bindings_) {
            if (old.deviceIndex == binding.deviceIndex && old.channel == binding.channel &&
                old.kind == binding.kind && old.data == binding.data)
                pendingConflicts_.push_back(old);
        }
        pendingBinding_ = binding;
        pending_ = true;
        learning_.store(false, std::memory_order_release);
        return;
    }

    // O indice do dispositivo e decisivo: dois controladores genericos podem ter
    // exatamente o mesmo nome, e nao devem compartilhar associacoes.
    for (const MidiBinding& binding : bindings_) {
        if (binding.deviceIndex != event.deviceIndex) continue;
        if (binding.channel != event.channel || binding.kind != event.kind ||
            binding.data != event.data) continue;

        dsp::Param* param = resolve(binding.target);
        if (!param) continue;

        if (binding.toggle) {
            // Botao latch: so reage ao "pressionar", ignorando o soltar (valor 0).
            if (event.value > 0) param->set(param->get() > 0.5f ? 0.0f : 1.0f);
        } else {
            const float normalized = binding.kind == 2
                ? static_cast<float>(event.value) / 16383.0f
                : static_cast<float>(event.value) / 127.0f;
            param->set(binding.minimum + dsp::clamp(normalized, 0.0f, 1.0f) *
                       (binding.maximum - binding.minimum));
        }
        // Sem 'return': um evento pode controlar varios parametros quando o
        // usuario escolhe "Usar em conjunto" no dialogo de conflito.
    }
}

dsp::Param* MidiEngine::resolve(const MidiTarget& target) const
{
    if (target.index < 0) return nullptr;
    switch (target.type) {
    case MidiTargetType::ChannelFader:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).faderDb : nullptr;
    case MidiTargetType::ChannelGain:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).gainDb : nullptr;
    case MidiTargetType::ChannelPan:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).pan : nullptr;
    case MidiTargetType::ChannelMute:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).mute : nullptr;
    case MidiTargetType::ChannelSolo:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).solo : nullptr;
    case MidiTargetType::ChannelSend1:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).send1Db : nullptr;
    case MidiTargetType::ChannelSend2:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).send2Db : nullptr;
    case MidiTargetType::ChannelSend3:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).send3Db : nullptr;
    case MidiTargetType::ChannelHighpass:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).highpass : nullptr;
    case MidiTargetType::ChannelPhase:
        return target.index < kMaxChannels ? &mixer_.channel(target.index).phase : nullptr;
    case MidiTargetType::ChannelEqGain:
        if (target.index >= kMaxChannels || target.sub < 0 || target.sub > 3) return nullptr;
        switch (target.sub) {
        case 0: return &mixer_.channel(target.index).eqLowDb;
        case 1: return &mixer_.channel(target.index).eqLowMidDb;
        case 2: return &mixer_.channel(target.index).eqHighMidDb;
        default: return &mixer_.channel(target.index).eqHighDb;
        }
    case MidiTargetType::MasterFader:
        return &mixer_.masterFaderDb();
    case MidiTargetType::AuxReturn:
        return target.index < kNumAux ? &mixer_.auxReturnDb(target.index) : nullptr;
    case MidiTargetType::FxParam: {
        if (target.index < 0 || target.index >= kNumAux) return nullptr;
        FxProcessor& fx = mixer_.aux(target.index);
        switch (target.sub) {
        case 0: return &fx.delay.timeMs;
        case 1: return &fx.delay.feedback;
        case 2: return &fx.delay.mix;
        case 3: return &fx.reverb.size;
        case 4: return &fx.reverb.damp;
        case 5: return &fx.reverb.mix;
        case 6: return &fx.chorus.rate;
        case 7: return &fx.chorus.depth;
        case 8: return &fx.chorus.mix;
        default: return nullptr;
        }
    }
    case MidiTargetType::None:
    default:
        return nullptr;
    }
}

bool MidiEngine::isBooleanTarget(MidiTargetType type)
{
    return type == MidiTargetType::ChannelMute || type == MidiTargetType::ChannelSolo ||
           type == MidiTargetType::ChannelHighpass || type == MidiTargetType::ChannelPhase;
}

bool MidiEngine::save(const std::string& path) const
{
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const std::wstring& name : disabledDevices_)
        out << "#disabled\t" << wideToUtf8(name) << '\n';
    for (const MidiBinding& binding : bindings_) {
        out << wideToUtf8(binding.deviceName) << '\t' << binding.deviceIndex << '\t'
            << binding.channel << '\t' << binding.kind << '\t' << binding.data << '\t'
            << static_cast<int>(binding.target.type) << '\t' << binding.target.index << '\t'
            << binding.target.sub << '\t' << (binding.toggle ? 1 : 0) << '\t'
            << binding.minimum << '\t' << binding.maximum << '\n';
    }
    return out.good();
}

bool MidiEngine::load(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open()) return false;
    bindings_.clear();
    disabledDevices_.clear();
    learning_.store(false, std::memory_order_release);
    pending_ = false;
    pendingConflicts_.clear();
    std::string line;
    while (std::getline(in, line)) {
        std::vector<std::string> parts;
        std::istringstream stream(line);
        std::string part;
        while (std::getline(stream, part, '\t')) parts.push_back(part);
        if (parts.size() == 2 && parts[0] == "#disabled") {
            disabledDevices_.push_back(utf8ToWide(parts[1]));
            continue;
        }
        if (parts.size() < 11) continue;

        try {
            MidiBinding binding;
            binding.deviceName = utf8ToWide(parts[0]);
            binding.deviceIndex = std::stoi(parts[1]);
            binding.channel = std::stoi(parts[2]);
            binding.kind = std::stoi(parts[3]);
            binding.data = std::stoi(parts[4]);
            binding.target.type = static_cast<MidiTargetType>(std::stoi(parts[5]));
            binding.target.index = std::stoi(parts[6]);
            binding.target.sub = std::stoi(parts[7]);
            binding.toggle = std::stoi(parts[8]) != 0;
            binding.minimum = std::stof(parts[9]);
            binding.maximum = std::stof(parts[10]);
            if (binding.target.valid()) bindings_.push_back(std::move(binding));
        } catch (...) {
            continue;
        }
    }
    return true;
}
