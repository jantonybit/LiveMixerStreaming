#pragma once

#include "../dsp/DspUtils.h"
#include "imgui.h"

#include <functional>

namespace widgets {

using RightClickCallback = std::function<void()>;

// Knob com arraste vertical (Shift = ajuste fino, duplo clique = valor inicial).
// "accent" colorea o arco de valor e a agulha; com alpha 0 mantem o comportamento
// antigo (cor baseada no nivel, verde/amarelo/vermelho).
bool Knob(const char* label, float* value, float minimum, float maximum,
          const char* format, float radius, float defaultValue,
          const RightClickCallback& onRightClick = {},
          const ImVec4& accent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

inline bool KnobParam(const char* label, dsp::Param& param, float minimum, float maximum,
                      const char* format = "%.1f", float radius = 18.0f,
                      float defaultValue = 0.0f,
                      const RightClickCallback& onRightClick = {},
                      const ImVec4& accent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f))
{
    float value = param.get();
    const bool changed = Knob(label, &value, minimum, maximum, format, radius,
                              defaultValue, onRightClick, accent);
    if (changed) param.set(value);
    return changed;
}

// Medidor vertical com escala em dB e linha de pico retido.
void VerticalMeter(float amplitude, float peakHold, float width, float height);

// Fader vertical estilizado.
bool Fader(const char* label, float* value, float minimum, float maximum, float height,
           const RightClickCallback& onRightClick = {});

inline bool FaderParam(const char* label, dsp::Param& param, float minimum, float maximum,
                       float height, const RightClickCallback& onRightClick = {})
{
    float value = param.get();
    const bool changed = Fader(label, &value, minimum, maximum, height, onRightClick);
    if (changed) param.set(value);
    return changed;
}

// Botao de alternancia compacto (M, S, PRE, ...).
bool ToggleButton(const char* label, bool active, ImVec4 activeColor,
                  float width = 0.0f, const RightClickCallback& onRightClick = {});

} // namespace widgets
