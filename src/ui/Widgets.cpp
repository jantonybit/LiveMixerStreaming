#include "Widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {

// Cores dos indicadores de nivel: verde ate 80%, amarelo de 81 a 93%, vermelho a partir de 94%.
const ImVec4 kLevelGreen  = ImVec4(60.0f / 255.0f, 220.0f / 255.0f, 90.0f / 255.0f, 1.0f);
const ImVec4 kLevelYellow = ImVec4(255.0f / 255.0f, 205.0f / 255.0f, 45.0f / 255.0f, 1.0f);
const ImVec4 kLevelRed    = ImVec4(235.0f / 255.0f, 65.0f / 255.0f, 55.0f / 255.0f, 1.0f);

ImU32 levelColor(float normalized)
{
    const float percent = normalized * 100.0f;
    if (percent <= 80.0f) return ImGui::GetColorU32(kLevelGreen);
    if (percent < 94.0f) return ImGui::GetColorU32(kLevelYellow);
    return ImGui::GetColorU32(kLevelRed);
}

} // namespace

namespace widgets {

bool Knob(const char* label, float* value, float minimum, float maximum,
          const char* format, float radius, float defaultValue,
          const RightClickCallback& onRightClick, const ImVec4& accent)
{
    ImGui::PushID(label);

    const ImVec2 topLeft = ImGui::GetCursorScreenPos();
    const ImVec2 center(topLeft.x + radius, topLeft.y + radius);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const float normalized = dsp::clamp((*value - minimum) / (maximum - minimum), 0.0f, 1.0f);
    const float startAngle = dsp::kPi * 0.75f;
    const float endAngle = dsp::kPi * 2.25f;
    const float angle = startAngle + normalized * (endAngle - startAngle);

    const bool hasAccent = accent.w > 0.0f;
    const ImU32 valueColor = hasAccent ? ImGui::GetColorU32(accent)
                                       : levelColor(normalized);
    const ImU32 needleColor = hasAccent ? valueColor
                                        : ImGui::GetColorU32(ImGuiCol_Text);

    drawList->PathArcTo(center, radius, startAngle, endAngle, 24);
    drawList->PathStroke(ImGui::GetColorU32(ImGuiCol_FrameBg), false, 3.0f);
    if (normalized > 0.001f) {
        drawList->PathArcTo(center, radius, startAngle, angle, 24);
        drawList->PathStroke(valueColor, false, 3.0f);
    }

    char text[48];
    std::snprintf(text, sizeof(text), format, *value);

    // O valor SEMPRE fica dentro do knob, com fonte reduzida proporcional ao
    // raio. Texto abaixo de cada controle custava ~27 px por linha e fazia o
    // canal estourar a janela em telas de 768p / escala Windows 125%.
    ImFont* font = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize();
    const float maxTextWidth = radius * 2.0f - 2.0f;
    const float infinite = std::numeric_limits<float>::max();
    ImVec2 textSize = font->CalcTextSizeA(fontSize, infinite, 0.0f, text);
    if (textSize.x > maxTextWidth) fontSize *= maxTextWidth / textSize.x;
    fontSize = std::max(7.0f, fontSize);
    textSize = font->CalcTextSizeA(fontSize, infinite, 0.0f, text);

    const ImVec2 base(center.x + std::cos(angle) * (radius * 0.65f),
                      center.y + std::sin(angle) * (radius * 0.65f));
    const ImVec2 tip(center.x + std::cos(angle) * (radius - 2.0f),
                     center.y + std::sin(angle) * (radius - 2.0f));
    drawList->AddLine(base, tip, needleColor, 1.5f);
    drawList->AddText(font, fontSize,
                      ImVec2(center.x - textSize.x * 0.5f,
                             center.y - textSize.y * 0.5f),
                      ImGui::GetColorU32(ImGuiCol_Text), text);

    const ImVec2 area(radius * 2.0f, radius * 2.0f);
    ImGui::SetCursorScreenPos(topLeft);
    ImGui::InvisibleButton("knob", area);
    if (onRightClick && ImGui::IsItemClicked(ImGuiMouseButton_Right)) onRightClick();

    bool changed = false;
    if (ImGui::IsItemActive()) {
        const float delta = ImGui::GetIO().MouseDelta.y;
        if (delta != 0.0f) {
            float step = (maximum - minimum) / 180.0f;
            if (ImGui::GetIO().KeyShift) step *= 0.15f;
            *value = dsp::clamp(*value - delta * step, minimum, maximum);
            changed = true;
        }
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        *value = defaultValue;
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s: %s", label, text);
    }

    // Dummy() é um item de verdade: evita o erro do ImGui sobre mover o cursor
    // para fora dos limites da janela sem submeter um item depois.
    // Ele precisa ser submetido na borda direita do knob: o InvisibleButton
    // devolve o cursor para a margem esquerda da linha, e um Dummy(0,4) feito
    // ali reescreve "CursorPosPrevLine.x" para essa margem. O SameLine()
    // seguinte voltaria ao inicio da linha e empilharia todos os knobs no
    // mesmo X. Reposicionar antes do Dummy preserva a largura ocupada e o
    // avanco vertical, mas mantem o X correto para o proximo controle.
    ImGui::SetCursorScreenPos(ImVec2(topLeft.x + area.x,
                                     topLeft.y + area.y + ImGui::GetStyle().ItemSpacing.y));
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::PopID();
    return changed;
}

void VerticalMeter(float amplitude, float peakHold, float width, float height)
{
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(width, height);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(p, ImVec2(p.x + width, p.y + height),
                            ImGui::GetColorU32(ImVec4(0.04f, 0.05f, 0.06f, 1.0f)), 2.0f);

    auto dbToNorm = [](float lin) {
        return dsp::clamp((dsp::linToDb(lin) + 60.0f) / 60.0f, 0.0f, 1.0f);
    };

    const float level = dbToNorm(amplitude);
    const int rows = 36;
    const float rowHeight = (height - 4.0f) / static_cast<float>(rows);
    for (int i = 0; i < rows; ++i) {
        const float t = static_cast<float>(i + 1) / static_cast<float>(rows);
        if (t > level) break;
        drawList->AddRectFilled(
            ImVec2(p.x + 2.0f, p.y + height - 2.0f - static_cast<float>(i + 1) * rowHeight),
            ImVec2(p.x + width - 2.0f, p.y + height - 2.0f - static_cast<float>(i) * rowHeight),
            levelColor(t));
    }

    const float hold = dbToNorm(peakHold);
    if (hold > 0.002f) {
        const float y = p.y + height - 2.0f - hold * (height - 4.0f);
        drawList->AddLine(ImVec2(p.x + 1.0f, y), ImVec2(p.x + width - 1.0f, y),
                          ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.9f)), 2.0f);
    }

    const float ticks[] = { 0.0f, 1.0f / 6.0f, 2.0f / 6.0f, 3.0f / 6.0f, 4.0f / 6.0f, 5.0f / 6.0f };
    for (float t : ticks) {
        const float y = p.y + height - 2.0f - t * (height - 4.0f);
        drawList->AddLine(ImVec2(p.x + width, y - 1.0f), ImVec2(p.x + width + 4.0f, y - 1.0f),
                          ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 0.7f)), 1.0f);
    }

    ImGui::Dummy(size);
}

bool Fader(const char* label, float* value, float minimum, float maximum, float height,
           const RightClickCallback& onRightClick)
{
    ImGui::PushID(label);
    const float width = 26.0f;
    const bool changed = ImGui::VSliderFloat("##fader", ImVec2(width, height),
                                             value, minimum, maximum, "", ImGuiSliderFlags_AlwaysClamp);
    if (onRightClick && ImGui::IsItemClicked(ImGuiMouseButton_Right)) onRightClick();
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f", *value);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - ImGui::CalcTextSize(text).x) * 0.5f);
    ImGui::TextUnformatted(text);
    ImGui::PopID();
    return changed;
}

bool ToggleButton(const char* label, bool active, ImVec4 activeColor, float width,
                  const RightClickCallback& onRightClick)
{
    if (width > 0.0f) ImGui::PushItemWidth(width);
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeColor);
    }
    const bool pressed = ImGui::Button(label, ImVec2(width, 0.0f));
    if (onRightClick && ImGui::IsItemClicked(ImGuiMouseButton_Right)) onRightClick();
    if (active) ImGui::PopStyleColor(2);
    if (width > 0.0f) ImGui::PopItemWidth();
    return pressed;
}

} // namespace widgets
