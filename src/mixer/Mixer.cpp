#include "Mixer.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>

namespace {

float toFloat(const std::string& text, float fallback)
{
    try { return std::stof(text); } catch (...) { return fallback; }
}

int toInt(const std::string& text, int fallback)
{
    try { return std::stoi(text); } catch (...) { return fallback; }
}

std::string valueOf(const std::map<std::string, std::string>& values,
                    const std::string& key, const std::string& fallback)
{
    auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

} // namespace

ChannelStrip::ChannelStrip()
{
    name = "Canal";
}

void ChannelStrip::prepare(double sampleRate)
{
    updateFilters(sampleRate);
    gainSmooth.reset(dsp::dbToLin(gainDb.get()), sampleRate);
    faderSmooth.reset(dsp::dbToLin(faderDb.get()), sampleRate);
    panSmooth.reset(dsp::clamp(pan.get(), -1.0f, 1.0f), sampleRate);
    send1Smooth.reset(dsp::dbToLin(send1Db.get()), sampleRate);
    send2Smooth.reset(dsp::dbToLin(send2Db.get()), sampleRate);
    send3Smooth.reset(dsp::dbToLin(send3Db.get()), sampleRate);
}

void ChannelStrip::updateFilters(double sampleRate)
{
    const float hpValue = highpass.get();
    if (cachedHp != hpValue) {
        hp.set(dsp::Biquad::Type::HighPass, static_cast<float>(sampleRate), 80.0f, 0.707f, 0.0f);
        hp.reset();
        cachedHp = hpValue;
    }

    auto updatePeaking = [&](dsp::Biquad& filter, float gain, float freq,
                             float& cachedGain, float& cachedFreq) {
        if (cachedGain != gain || cachedFreq != freq) {
            filter.set(dsp::Biquad::Type::Peaking, static_cast<float>(sampleRate), freq, 1.0f, gain);
            filter.reset();
            cachedGain = gain;
            cachedFreq = freq;
        }
    };
    auto updateShelf = [&](dsp::Biquad& filter, dsp::Biquad::Type type, float gain, float freq,
                           float& cachedGain, float& cachedFreq) {
        if (cachedGain != gain || cachedFreq != freq) {
            filter.set(type, static_cast<float>(sampleRate), freq, 0.707f, gain);
            filter.reset();
            cachedGain = gain;
            cachedFreq = freq;
        }
    };

    updateShelf(low, dsp::Biquad::Type::LowShelf, eqLowDb.get(), eqLowFreq.get(), cachedLowDb, cachedLowFreq);
    updatePeaking(lowMid, eqLowMidDb.get(), eqLowMidFreq.get(), cachedLowMidDb, cachedLowMidFreq);
    updatePeaking(highMid, eqHighMidDb.get(), eqHighMidFreq.get(), cachedHighMidDb, cachedHighMidFreq);
    updateShelf(high, dsp::Biquad::Type::HighShelf, eqHighDb.get(), eqHighFreq.get(), cachedHighDb, cachedHighFreq);
}

Mixer::Mixer()
{
    channels_.reserve(kMaxChannels);
    for (int i = 0; i < kMaxChannels; ++i) {
        auto strip = std::make_unique<ChannelStrip>();
        strip->name = "CH " + std::to_string(i + 1);
        strip->inputChannel.set(static_cast<float>(i));
        channels_.push_back(std::move(strip));
    }
    aux_[0].setType(FxProcessor::Type::Delay);
    aux_[1].setType(FxProcessor::Type::Reverb);
    aux_[2].setType(FxProcessor::Type::Chorus);
}

void Mixer::setChannelCount(int count)
{
    channelCount_ = dsp::clamp(static_cast<float>(count), 1.0f, static_cast<float>(kMaxChannels));
}

void Mixer::prepare(double sampleRate, int maxFrames)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxFrames_ = std::max(32, maxFrames);

    for (int i = 0; i < kMaxChannels; ++i) channels_[i]->prepare(sampleRate_);
    for (int a = 0; a < kNumAux; ++a) aux_[a].prepare(sampleRate_);
    masterSmooth_.reset(dsp::dbToLin(masterFaderDb_.get()), sampleRate_);

    masterL_.assign(static_cast<size_t>(maxFrames_), 0.0f);
    masterR_.assign(static_cast<size_t>(maxFrames_), 0.0f);
    for (int a = 0; a < kNumAux; ++a) {
        auxInL_[a].assign(static_cast<size_t>(maxFrames_), 0.0f);
        auxInR_[a].assign(static_cast<size_t>(maxFrames_), 0.0f);
        auxOutL_[a].assign(static_cast<size_t>(maxFrames_), 0.0f);
        auxOutR_[a].assign(static_cast<size_t>(maxFrames_), 0.0f);
    }
}

void Mixer::resetMeters()
{
    masterMeter_.reset();
    for (int a = 0; a < kNumAux; ++a) auxMeter_[a].reset();
    for (int i = 0; i < kMaxChannels; ++i) {
        channels_[i]->inputMeter.reset();
        channels_[i]->outputMeter.reset();
    }
}

void Mixer::process(float* const* inputs, float** outputs, int inputCount, int outputCount, int frames)
{
    if (frames <= 0 || outputCount <= 0) return;
    frames = std::min(frames, maxFrames_);

    std::fill(masterL_.begin(), masterL_.begin() + frames, 0.0f);
    std::fill(masterR_.begin(), masterR_.begin() + frames, 0.0f);
    for (int a = 0; a < kNumAux; ++a) {
        std::fill(auxInL_[a].begin(), auxInL_[a].begin() + frames, 0.0f);
        std::fill(auxInR_[a].begin(), auxInR_[a].begin() + frames, 0.0f);
    }

    bool anySolo = false;
    for (int c = 0; c < channelCount_; ++c)
        anySolo = anySolo || (channels_[c]->solo.get() > 0.5f);

    for (int c = 0; c < channelCount_; ++c) {
        ChannelStrip& strip = *channels_[static_cast<size_t>(c)];
        strip.updateFilters(sampleRate_);

        const bool audible = strip.mute.get() < 0.5f && (!anySolo || strip.solo.get() > 0.5f);
        const float gainTarget = dsp::dbToLin(strip.gainDb.get()) * (audible ? 1.0f : 0.0f);
        const float faderTarget = dsp::dbToLin(strip.faderDb.get());
        const float panTarget = dsp::clamp(strip.pan.get(), -1.0f, 1.0f);
        const float send1Target = dsp::dbToLin(strip.send1Db.get());
        const float send2Target = dsp::dbToLin(strip.send2Db.get());
        const float send3Target = dsp::dbToLin(strip.send3Db.get());
        const bool invert = strip.phase.get() > 0.5f;
        const bool useHp = strip.highpass.get() > 0.5f;
        const float send1Gain = strip.send1Smooth.process(send1Target);
        const float send2Gain = strip.send2Smooth.process(send2Target);
        const float send3Gain = strip.send3Smooth.process(send3Target);
        const int source = static_cast<int>(strip.inputChannel.get());
        const float* input = (source >= 0 && source < inputCount) ? inputs[source] : nullptr;

        for (int i = 0; i < frames; ++i) {
            float x = input ? input[i] : 0.0f;
            x *= strip.gainSmooth.process(gainTarget);
            if (invert) x = -x;
            if (useHp) x = strip.hp.process(x);
            x = strip.low.process(x);
            x = strip.lowMid.process(x);
            x = strip.highMid.process(x);
            x = strip.high.process(x);
            strip.inputMeter.update(x, x);

            const float post = x * strip.faderSmooth.process(faderTarget);
            const float panValue = strip.panSmooth.process(panTarget);
            const float angle = (panValue + 1.0f) * 0.25f * dsp::kPi;
            const float panLeft = std::cos(angle);
            const float panRight = std::sin(angle);
            const float left = post * panLeft;
            const float right = post * panRight;
            strip.outputMeter.update(left, right);

            masterL_[static_cast<size_t>(i)] += left;
            masterR_[static_cast<size_t>(i)] += right;

            // Envios sempre apos o fader. O pan tambem se aplica aos envios:
            // sem isso, o retorno do efeito tocaria no lado oposto mesmo com o
            // pan totalmente girado, "preenchendo" o canal que deveria estar mudo.
            auxInL_[0][static_cast<size_t>(i)] += post * send1Gain * 0.707f * panLeft;
            auxInR_[0][static_cast<size_t>(i)] += post * send1Gain * 0.707f * panRight;
            auxInL_[1][static_cast<size_t>(i)] += post * send2Gain * 0.707f * panLeft;
            auxInR_[1][static_cast<size_t>(i)] += post * send2Gain * 0.707f * panRight;
            auxInL_[2][static_cast<size_t>(i)] += post * send3Gain * 0.707f * panLeft;
            auxInR_[2][static_cast<size_t>(i)] += post * send3Gain * 0.707f * panRight;
        }

        strip.inputMeter.commit(frames, sampleRate_);
        strip.outputMeter.commit(frames, sampleRate_);
    }

    for (int a = 0; a < kNumAux; ++a) {
        aux_[a].process(auxInL_[a].data(), auxInR_[a].data(),
                        auxOutL_[a].data(), auxOutR_[a].data(), frames);
        const float returnGain = dsp::dbToLin(auxReturnDb_[a].get());
        for (int i = 0; i < frames; ++i) {
            masterL_[static_cast<size_t>(i)] += auxOutL_[a][static_cast<size_t>(i)] * returnGain;
            masterR_[static_cast<size_t>(i)] += auxOutR_[a][static_cast<size_t>(i)] * returnGain;
            auxMeter_[a].update(auxOutL_[a][static_cast<size_t>(i)], auxOutR_[a][static_cast<size_t>(i)]);
        }
        auxMeter_[a].commit(frames, sampleRate_);
    }

    const float limiterDrive = 1.25f;
    const float limiterNorm = 1.0f / std::tanh(limiterDrive);
    for (int i = 0; i < frames; ++i) {
        const float mg = masterSmooth_.process(dsp::dbToLin(masterFaderDb_.get()));
        float left = masterL_[static_cast<size_t>(i)] * mg;
        float right = masterR_[static_cast<size_t>(i)] * mg;
        left = std::tanh(left * limiterDrive) * limiterNorm;
        right = std::tanh(right * limiterDrive) * limiterNorm;

        outputs[0][i] = outputCount == 1 ? (left + right) * 0.5f : left;
        if (outputCount > 1) outputs[1][i] = right;
        for (int out = 2; out < outputCount; ++out) outputs[out][i] = 0.0f;

        masterMeter_.update(left, right);
    }
    masterMeter_.commit(frames, sampleRate_);
}

bool Mixer::save(const std::string& path) const
{
    std::map<std::string, std::string> values;
    values["channels"] = std::to_string(channelCount_);
    values["master"] = std::to_string(masterFaderDb_.get());
    for (int a = 0; a < kNumAux; ++a) {
        const std::string p = "aux" + std::to_string(a) + ".";
        values[p + "type"] = std::to_string(static_cast<int>(aux_[a].type()));
        values[p + "return"] = std::to_string(auxReturnDb_[a].get());
        values[p + "delay.time"] = std::to_string(aux_[a].delay.timeMs.get());
        values[p + "delay.feedback"] = std::to_string(aux_[a].delay.feedback.get());
        values[p + "delay.mix"] = std::to_string(aux_[a].delay.mix.get());
        values[p + "reverb.size"] = std::to_string(aux_[a].reverb.size.get());
        values[p + "reverb.damp"] = std::to_string(aux_[a].reverb.damp.get());
        values[p + "reverb.mix"] = std::to_string(aux_[a].reverb.mix.get());
        values[p + "chorus.rate"] = std::to_string(aux_[a].chorus.rate.get());
        values[p + "chorus.depth"] = std::to_string(aux_[a].chorus.depth.get());
        values[p + "chorus.mix"] = std::to_string(aux_[a].chorus.mix.get());
    }

    for (int c = 0; c < kMaxChannels; ++c) {
        const ChannelStrip& s = *channels_[static_cast<size_t>(c)];
        const std::string p = "ch" + std::to_string(c) + ".";
        values[p + "name"] = s.name;
        values[p + "input"] = std::to_string(s.inputChannel.get());
        values[p + "inputname"] = s.inputName;
        values[p + "gain"] = std::to_string(s.gainDb.get());
        values[p + "fader"] = std::to_string(s.faderDb.get());
        values[p + "pan"] = std::to_string(s.pan.get());
        values[p + "hp"] = std::to_string(s.highpass.get());
        values[p + "phase"] = std::to_string(s.phase.get());
        values[p + "mute"] = std::to_string(s.mute.get());
        values[p + "solo"] = std::to_string(s.solo.get());
        values[p + "low.db"] = std::to_string(s.eqLowDb.get());
        values[p + "low.freq"] = std::to_string(s.eqLowFreq.get());
        values[p + "lowmid.db"] = std::to_string(s.eqLowMidDb.get());
        values[p + "lowmid.freq"] = std::to_string(s.eqLowMidFreq.get());
        values[p + "highmid.db"] = std::to_string(s.eqHighMidDb.get());
        values[p + "highmid.freq"] = std::to_string(s.eqHighMidFreq.get());
        values[p + "high.db"] = std::to_string(s.eqHighDb.get());
        values[p + "high.freq"] = std::to_string(s.eqHighFreq.get());
        values[p + "send1"] = std::to_string(s.send1Db.get());
        values[p + "send2"] = std::to_string(s.send2Db.get());
        values[p + "send3"] = std::to_string(s.send3Db.get());
    }

    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const auto& item : values)
        out << item.first << '=' << item.second << '\n';
    return out.good();
}

bool Mixer::load(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(in, line)) {
        const size_t equal = line.find('=');
        if (equal == std::string::npos) continue;
        values[line.substr(0, equal)] = line.substr(equal + 1);
    }
    if (values.empty()) return false;

    setChannelCount(toInt(valueOf(values, "channels", "8"), 8));
    masterFaderDb_.set(toFloat(valueOf(values, "master", "0"), 0.0f));

    for (int a = 0; a < kNumAux; ++a) {
        const std::string p = "aux" + std::to_string(a) + ".";
        // Efeitos fixos: o tipo salvo e ignorado para manter a ordem Delay/Reverb/Chorus.
        aux_[a].setType(static_cast<FxProcessor::Type>(a + 1));
        auxReturnDb_[a].set(toFloat(valueOf(values, p + "return", "-12"), -12.0f));
        aux_[a].delay.timeMs.set(toFloat(valueOf(values, p + "delay.time", "250"), 250.0f));
        aux_[a].delay.feedback.set(toFloat(valueOf(values, p + "delay.feedback", "0.35"), 0.35f));
        aux_[a].delay.mix.set(toFloat(valueOf(values, p + "delay.mix", "0.5"), 0.5f));
        aux_[a].reverb.size.set(toFloat(valueOf(values, p + "reverb.size", "0.65"), 0.65f));
        aux_[a].reverb.damp.set(toFloat(valueOf(values, p + "reverb.damp", "0.35"), 0.35f));
        aux_[a].reverb.mix.set(toFloat(valueOf(values, p + "reverb.mix", "0.5"), 0.5f));
        aux_[a].chorus.rate.set(toFloat(valueOf(values, p + "chorus.rate", "0.6"), 0.6f));
        aux_[a].chorus.depth.set(toFloat(valueOf(values, p + "chorus.depth", "3.5"), 3.5f));
        aux_[a].chorus.mix.set(toFloat(valueOf(values, p + "chorus.mix", "0.5"), 0.5f));
    }

    for (int c = 0; c < kMaxChannels; ++c) {
        ChannelStrip& s = *channels_[static_cast<size_t>(c)];
        const std::string p = "ch" + std::to_string(c) + ".";
        s.name = valueOf(values, p + "name", s.name);
        s.inputName = valueOf(values, p + "inputname", "");
        s.inputChannel.set(toFloat(valueOf(values, p + "input", std::to_string(c)), static_cast<float>(c)));
        s.gainDb.set(toFloat(valueOf(values, p + "gain", "0"), 0.0f));
        s.faderDb.set(toFloat(valueOf(values, p + "fader", "0"), 0.0f));
        s.pan.set(toFloat(valueOf(values, p + "pan", "0"), 0.0f));
        s.highpass.set(toFloat(valueOf(values, p + "hp", "0"), 0.0f));
        s.phase.set(toFloat(valueOf(values, p + "phase", "0"), 0.0f));
        s.mute.set(toFloat(valueOf(values, p + "mute", "0"), 0.0f));
        s.solo.set(toFloat(valueOf(values, p + "solo", "0"), 0.0f));
        s.eqLowDb.set(toFloat(valueOf(values, p + "low.db", "0"), 0.0f));
        s.eqLowFreq.set(toFloat(valueOf(values, p + "low.freq", "120"), 120.0f));
        s.eqLowMidDb.set(toFloat(valueOf(values, p + "lowmid.db", "0"), 0.0f));
        s.eqLowMidFreq.set(toFloat(valueOf(values, p + "lowmid.freq", "450"), 450.0f));
        s.eqHighMidDb.set(toFloat(valueOf(values, p + "highmid.db", "0"), 0.0f));
        s.eqHighMidFreq.set(toFloat(valueOf(values, p + "highmid.freq", "2500"), 2500.0f));
        s.eqHighDb.set(toFloat(valueOf(values, p + "high.db", "0"), 0.0f));
        s.eqHighFreq.set(toFloat(valueOf(values, p + "high.freq", "8000"), 8000.0f));
        s.send1Db.set(toFloat(valueOf(values, p + "send1", "-60"), -60.0f));
        s.send2Db.set(toFloat(valueOf(values, p + "send2", "-60"), -60.0f));
        s.send3Db.set(toFloat(valueOf(values, p + "send3", "-60"), -60.0f));
    }
    return true;
}

void Mixer::resolveInputNames(const std::vector<std::string>& names)
{
    if (names.empty()) return;
    for (int c = 0; c < channelCount_; ++c) {
        ChannelStrip& strip = *channels_[static_cast<size_t>(c)];
        if (strip.inputName.empty()) continue;
        for (size_t i = 0; i < names.size(); ++i) {
            if (names[i] != strip.inputName) continue;
            strip.inputChannel.set(static_cast<float>(i));
            break;
        }
    }
}
