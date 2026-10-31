#pragma once

namespace grid_layout
{
inline constexpr int width = 1020, height = 720;
inline constexpr int gridOffset = 20, cardLeft = 88 + gridOffset, columnPitch = 212;
inline constexpr int sidebarRight = 82;
inline constexpr float cardCornerRadius = 4.f;
// Header: one 28 px row. Scenes and the rig preset on the left, the preset file actions in the
// middle, and the views (Modulation, Tuner, Settings) on the right.
inline constexpr int headerTop = 14, headerBottom = 42, headerY = (headerTop + headerBottom) / 2;
inline constexpr int sceneLeft = 165, sceneWidth = 28, scenePitch = 32;
inline constexpr int presetLeft = 269, presetRight = 548;
inline constexpr int previousLeft = 554, nextLeft = 586, arrowWidth = 28;
inline constexpr int newLeft = 626, saveLeft = 660, fileButtonWidth = 28;
inline constexpr int modulationLeft = 700, modulationRight = 810;
inline constexpr int tunerLeft = 816, tunerRight = 892;
inline constexpr int settingsLeft = 898, settingsRight = width - headerTop;
inline constexpr int SceneX(int scene) { return sceneLeft + scenePitch * scene + sceneWidth / 2; }
inline constexpr int cardTop = 64, rowPitch = 104, cardHeight = 80;
inline constexpr int editorTop = 480, editorBottom = height - 24;
// The visualization sits equally close to the editor's top, right and bottom edges.
inline constexpr int visualizationInset = 4, visualizationTop = editorTop + visualizationInset;
inline constexpr int visualizationLeft = 564;
inline constexpr int visualizationRight = width - visualizationInset;
inline constexpr int visualizationBottom = height - visualizationInset;
// Inside the visualization: title, chart, axis legend and shared level meters.
// Explanations are hover help, leaving their former row available to the chart.
inline constexpr int plotTitleTop = 488, plotTitleBottom = 507;
inline constexpr int plotTop = 510;
inline constexpr int plotMetersTop = visualizationBottom - 18, plotMetersBottom = visualizationBottom - 2;
inline constexpr int plotFooterTop = plotMetersTop - 20, plotFooterBottom = plotMetersTop - 2;
inline constexpr int plotBottom = plotFooterTop - 4;
// Module editor rows: the header (slot ID, badges, module name and its actions), the
// module's options on the right, then two rows of knobs. The description is the name's hover help.
inline constexpr int moduleHeaderTop = 488, moduleHeaderBottom = 512;
inline constexpr int descriptionTop = 516, descriptionBottom = 536;
inline constexpr int optionsTop = descriptionTop, optionsBottom = descriptionBottom;
inline constexpr int knobTop = 542, knobBottom = 620, knobRowPitch = 86;
inline constexpr int enabledY = (moduleHeaderTop + moduleHeaderBottom) / 2;
inline constexpr int moduleBrowseX = visualizationLeft - 88, modulePreviousX = visualizationLeft - 54, moduleNextX = visualizationLeft - 24;
inline constexpr int moduleButtonY = (moduleHeaderTop + moduleHeaderBottom) / 2, optionsY = (optionsTop + optionsBottom) / 2;
inline constexpr int qualityLeft = visualizationLeft - 128, tempoLeft = visualizationLeft - 300, modeLeft = visualizationLeft - 176;
// Dial centre below a control's top. Switches and choices in the dial grid share it, so their
// boxes line up with the knobs beside them.
inline constexpr int knobCenter = 43, knobBoxHalfHeight = 17;
inline constexpr int KnobGestureY(int position) { return knobTop + (position / 4) * knobRowPitch + knobCenter; }
inline constexpr int ToggleY(int position) { return KnobGestureY(position); }
inline constexpr int KnobValueY(int position) { return knobBottom + (position / 4) * knobRowPitch - 6; }
inline constexpr float parameterLeft = 12.f, parameterRight = visualizationLeft - 12.f;
inline constexpr float parameterWidth = (parameterRight - parameterLeft) / 4.f;
// Four controls per row leave a full-height visualization beside the editor.
inline constexpr int KnobX(int position) { return static_cast<int>(parameterLeft + (position % 4 + .5f) * parameterWidth); }
}
