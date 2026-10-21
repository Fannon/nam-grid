#pragma once

namespace grid_layout
{
inline constexpr int width = 1000, height = 720;
inline constexpr int cardTop = 64, rowPitch = 104, cardHeight = 80;
inline constexpr int editorTop = 480, editorBottom = height - 24;
inline constexpr int knobTop = 586, knobBottom = 684;
inline constexpr int knobGestureY = 632, toggleY = 642, moduleButtonY = 561;
inline constexpr int knobValueY = 679, plotToggleY = 510;
inline constexpr float parameterLeft = 42.f, parameterRight = 956.f;
inline constexpr float parameterWidth = (parameterRight - parameterLeft) / 8.f;
inline constexpr int KnobX(int position) { return static_cast<int>(parameterLeft + (position + .5f) * parameterWidth); }
}
