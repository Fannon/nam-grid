#pragma once

namespace grid_layout
{
inline constexpr int width = 1000, height = 720;
// Header: one 28 px row. Scenes and the rig preset on the left, the preset file actions in the
// middle, and the views (Modulation, Tuner, Settings) on the right.
inline constexpr int headerTop = 14, headerBottom = 42, headerY = (headerTop + headerBottom) / 2;
inline constexpr int sceneLeft = 165, sceneWidth = 28, scenePitch = 32;
inline constexpr int presetLeft = 269, presetRight = 528;
inline constexpr int previousLeft = 534, nextLeft = 566, arrowWidth = 28;
inline constexpr int newLeft = 606, saveLeft = 640, fileButtonWidth = 28;
inline constexpr int modulationLeft = 680, modulationRight = 790;
inline constexpr int tunerLeft = 796, tunerRight = 872;
inline constexpr int settingsLeft = 878, settingsRight = width - headerTop;
inline constexpr int SceneX(int scene) { return sceneLeft + scenePitch * scene + sceneWidth / 2; }
inline constexpr int cardTop = 64, rowPitch = 104, cardHeight = 80;
inline constexpr int editorTop = 480, editorBottom = height - 24;
// The visualization sits equally close to the editor's top, right and bottom edges.
inline constexpr int visualizationInset = 4, visualizationTop = editorTop + visualizationInset;
inline constexpr int visualizationRight = width - visualizationInset;
inline constexpr int visualizationBottom = height - visualizationInset;
// Inside the visualization, from the top down: title, description, chart, module legend and
// the shared level meters. The text rows sit close together so the chart keeps its height.
inline constexpr int plotTitleTop = 488, plotTitleBottom = 507;
inline constexpr int plotDetailTop = 508, plotDetailBottom = 526;
inline constexpr int plotTop = 530;
inline constexpr int plotMetersTop = visualizationBottom - 18, plotMetersBottom = visualizationBottom - 2;
inline constexpr int plotFooterTop = plotMetersTop - 20, plotFooterBottom = plotMetersTop - 2;
inline constexpr int plotBottom = plotFooterTop - 4;
// Module editor rows: the header (slot ID, badges, module name and its actions), the
// description with the module's options on the right, then two rows of knobs.
inline constexpr int moduleHeaderTop = 488, moduleHeaderBottom = 512;
inline constexpr int descriptionTop = 516, descriptionBottom = 536;
inline constexpr int optionsTop = descriptionTop, optionsBottom = descriptionBottom;
inline constexpr int knobTop = 542, knobBottom = 620, knobRowPitch = 86;
inline constexpr int enabledX = 608, enabledY = (moduleHeaderTop + moduleHeaderBottom) / 2;
inline constexpr int moduleButtonY = (moduleHeaderTop + moduleHeaderBottom) / 2, optionsY = (optionsTop + optionsBottom) / 2;
inline constexpr int qualityLeft = 520, tempoLeft = 380, modeLeft = 496;
inline constexpr int KnobGestureY(int position) { return knobTop + (position / 4) * knobRowPitch + 43; }
inline constexpr int ToggleY(int position) { return knobTop + (position / 4) * knobRowPitch + 55; }
inline constexpr int KnobValueY(int position) { return knobBottom + (position / 4) * knobRowPitch - 6; }
inline constexpr float parameterLeft = 24.f, parameterRight = 620.f;
inline constexpr float parameterWidth = (parameterRight - parameterLeft) / 4.f;
// Four controls per row leave a full-height visualization beside the editor.
inline constexpr int KnobX(int position) { return static_cast<int>(parameterLeft + (position % 4 + .5f) * parameterWidth); }
}
