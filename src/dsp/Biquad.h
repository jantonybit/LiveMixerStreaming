#pragma once

#include "DspUtils.h"

namespace dsp {

// Filtros RBJ "Audio EQ Cookbook" em precisao simples, forma direta II transposta.
class Biquad
{
public:
    enum class Type { LowPass, HighPass, Peaking, LowShelf, HighShelf };

    void set(Type type, float sampleRate, float freq, float q, float gainDb) noexcept
    {
        if (sampleRate <= 0.0f) return;

        const float f = clamp(freq, 10.0f, sampleRate * 0.45f);
        const float w0 = 2.0f * kPi * f / sampleRate;
        const float cw = std::cos(w0);
        const float sw = std::sin(w0);
        const float alpha = sw / (2.0f * std::max(0.05f, q));
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float sqrtA = std::sqrt(A);

        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

        switch (type) {
        case Type::LowPass:
            b0 = (1.0f - cw) * 0.5f;
            b1 = 1.0f - cw;
            b2 = (1.0f - cw) * 0.5f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cw;
            a2 = 1.0f - alpha;
            break;
        case Type::HighPass:
            b0 = (1.0f + cw) * 0.5f;
            b1 = -(1.0f + cw);
            b2 = (1.0f + cw) * 0.5f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cw;
            a2 = 1.0f - alpha;
            break;
        case Type::Peaking:
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cw;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cw;
            a2 = 1.0f - alpha / A;
            break;
        case Type::LowShelf:
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqrtA * alpha);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqrtA * alpha);
            a0 = (A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqrtA * alpha;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw);
            a2 = (A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqrtA * alpha;
            break;
        case Type::HighShelf:
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqrtA * alpha);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqrtA * alpha);
            a0 = (A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqrtA * alpha;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw);
            a2 = (A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqrtA * alpha;
            break;
        }

        b0_ = b0 / a0;
        b1_ = b1 / a0;
        b2_ = b2 / a0;
        a1_ = a1 / a0;
        a2_ = a2 / a0;
    }

    void reset() noexcept { x1_ = x2_ = y1_ = y2_ = 0.0f; }

    float process(float x) noexcept
    {
        const float y = b0_ * x + x1_;
        x1_ = b1_ * x - a1_ * y + x2_;
        x2_ = b2_ * x - a2_ * y;
        return y;
    }

private:
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    float x1_ = 0.0f, x2_ = 0.0f, y1_ = 0.0f, y2_ = 0.0f;
};

} // namespace dsp
