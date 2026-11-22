#pragma once

#include <atomic>
#include <cmath>
#include <algorithm>

namespace dsp {

constexpr float kPi = 3.14159265358979323846f;

inline float clamp(float v, float lo, float hi) noexcept
{
    return std::max(lo, std::min(v, hi));
}

inline float dbToLin(float db) noexcept
{
    if (db <= -120.0f) return 0.0f;
    return std::pow(10.0f, db * 0.05f);
}

inline float linToDb(float lin) noexcept
{
    return lin > 1.0e-9f ? 20.0f * std::log10(lin) : -120.0f;
}

// Parametro lido na thread de audio e escrito na thread de UI.
class Param
{
public:
    Param(float value = 0.0f) noexcept : value_(value) {}
    Param(const Param&) = delete;
    Param& operator=(const Param&) = delete;

    float get() const noexcept { return value_.load(std::memory_order_relaxed); }
    void set(float v) noexcept { value_.store(v, std::memory_order_relaxed); }

private:
    std::atomic<float> value_;
};

// Suavizacao de um polo para evitar clicks quando um controle e movido.
class Smoother
{
public:
    void reset(float value, float sampleRate, float tauSeconds = 0.012f) noexcept
    {
        value_ = value;
        coeff_ = makeCoeff(sampleRate, tauSeconds);
    }

    void setTau(float tauSeconds, float sampleRate) noexcept
    {
        coeff_ = makeCoeff(sampleRate, tauSeconds);
    }

    float process(float target) noexcept
    {
        const float delta = target - value_;
        // Sem este "snap", um filtro one-pole em float32 trava a ~1.7e-5 do
        // alvo (o passo fica menor que o ULP). Isso deixaria, por exemplo,
        // um residuo audivel no canal oposto quando o pan vai ao extremo.
        if (std::fabs(delta) < 1e-4f) {
            value_ = target;
            return value_;
        }
        value_ += coeff_ * delta;
        return value_;
    }

    float value() const noexcept { return value_; }

private:
    static float makeCoeff(float sampleRate, float tau) noexcept
    {
        if (sampleRate <= 0.0f || tau <= 0.0f) return 1.0f;
        return 1.0f - std::exp(-1.0f / (tau * sampleRate));
    }

    float value_ = 0.0f;
    float coeff_ = 1.0f;
};

} // namespace dsp
