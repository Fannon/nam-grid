#pragma once

namespace grid_layout
{
inline constexpr int width = 1000, height = 720;
// Header: one 28 px row. Scenes and the rig preset on the left, the preset file actions in the
// middle, and the views (Modulation, Tuner, Library & settings) on the right.
inline constexpr int headerTop = 14, headerBottom = 42, headerY = (headerTop + headerBottom) / 2;
inline constexpr int sceneLeft = 165, sceneWidth = 28, scenePitch = 32;
inline constexpr int presetLeft = 269, presetRight = 510;
inline constexpr int previousLeft = 516, nextLeft = 548, arrowWidth = 28;
inline constexpr int newLeft = 588, saveLeft = 642, fileButtonWidth = 48;
inline constexpr int modulationLeft = 702, modulationRight = 792;
inline constexpr int tunerLeft = 798, tunerRight = 862;
inline constexpr int settingsLeft = 868, settingsRight = 976;
inline constexpr int SceneX(int scene) { return sceneLeft + scenePitch * scene + sceneWidth / 2; }
inline constexpr int cardTop = 64, rowPitch = 104, cardHeight = 80;
inline constexpr int editorTop = 480, editorBottom = height - 24;
inline constexpr int knobTop = 536, knobBottom = 614, knobRowPitch = 82;
// Header option strip (Tempo sync, modes, Quality) between the module row and the knob labels.
inline constexpr int moduleButtonY = 496, optionsTop = 510, optionsBottom = 530, optionsY = (optionsTop + optionsBottom) / 2;
inline constexpr int KnobGestureY(int position) { return knobTop + (position / 4) * knobRowPitch + 43; }
inline constexpr int ToggleY(int position) { return knobTop + (position / 4) * knobRowPitch + 55; }
inline constexpr int KnobValueY(int position) { return knobTop + (position / 4) * knobRowPitch + 72; }
inline constexpr float parameterLeft = 24.f, parameterRight = 620.f;
inline constexpr float parameterWidth = (parameterRight - parameterLeft) / 4.f;
// Four controls per row leave a full-height visualization beside the editor.
inline constexpr int KnobX(int position) { return static_cast<int>(parameterLeft + (position % 4 + .5f) * parameterWidth); }
}
