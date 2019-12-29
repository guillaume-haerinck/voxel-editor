#pragma once

#include <imgui/imgui.h>

class UIStyle {
public:
    UIStyle() {};

    ImFont* fontIconLarge() const { return m_fontIconLarge; }

    const ImVec2& largeButtonSize() const { return m_largeButtonSize; }
    const float largeButtonRounding() const { return m_largeButtonRounding; }
    const ImVec4& selectColor() const { return m_selectColor; }

private:
    ImFont* m_fontIconLarge;

    ImVec2 m_largeButtonSize = ImVec2(50, 50);
    float m_largeButtonRounding = 10.0f;
    ImVec4 m_selectColor = ImVec4(0.18f, 0.46f, 0.80f, 1.00f);

private:
    friend class App;
};