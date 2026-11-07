#pragma once

namespace grid_layout
{
// 3:2: wider than the original editor without taking a full 16:10 of the screen.
inline constexpr int width = 1080, height = 720;
// Sidebars beside the 4x4 grid: IN, its meter and the macros on the left; A/B, Routing, OUT
// and the output meter on the right. Both stay clear of the return wiring in the gutters.
inline constexpr int sidebarRight = 92;
inline constexpr int cardLeft = 118, cardWidth = 196, columnPitch = 220, cardGap = columnPitch - cardWidth;
inline constexpr int gridRight = cardLeft + 3 * columnPitch + cardWidth;
inline constexpr int returnLeft = cardLeft - 19, returnRight = gridRight + 16;
// The right column centres between the return wiring and the window edge. The stereo IN and OUT
// meters mirror each other beside their knobs, apart from Pre 1's input and Post 4's output strips.
inline constexpr int inCenterX = (4 + sidebarRight) / 2, inputMeterLeft = inCenterX + 31;
inline constexpr int sideCenterX = (returnRight + width) / 2, outputMeterLeft = sideCenterX - 38;
inline constexpr float cardCornerRadius = 4.f;
// Header: one 28 px row. Scenes and the rig preset on the left, the preset file actions in the
// middle, and the views (Modulation, Tuner, the Settings icon) pinned to the right edge.
inline constexpr int headerTop = 14, headerBottom = 42, headerY = (headerTop + headerBottom) / 2;
inline constexpr int sceneLeft = 165, sceneWidth = 28, scenePitch = 32;
inline constexpr int settingsRight = width - headerTop, settingsLeft = settingsRight - 28;
inline constexpr int tunerRight = settingsLeft - 6, tunerLeft = tunerRight - 76;
inline constexpr int modulationRight = tunerLeft - 6, modulationLeft = modulationRight - 110;
// The rig preset name takes the remaining width; its file actions keep a group gap before the views.
inline constexpr int presetLeft = 269, presetRight = modulationLeft - 164;
inline constexpr int previousLeft = presetRight + 6, nextLeft = previousLeft + 32, arrowWidth = 28;
inline constexpr int newLeft = nextLeft + 40, saveLeft = newLeft + 34, fileButtonWidth = 28;
// Full pages (Settings, Modulation, the library browser) share these margins and a Close button
// at the top right.
inline constexpr int pageLeft = 24, pageRight = width - 24, pageCloseX = pageRight - 40;
inline constexpr int SceneX(int scene) { return sceneLeft + scenePitch * scene + sceneWidth / 2; }
inline constexpr int cardTop = 64, rowPitch = 104, cardHeight = 80;
inline constexpr int editorTop = 480, editorBottom = height - 24;
// The visualization sits equally close to the editor's top, right and bottom edges.
inline constexpr int visualizationInset = 4, visualizationTop = editorTop + visualizationInset;
// The module editor and visualization split under the grid, at the end of the second column.
inline constexpr int visualizationLeft = cardLeft + columnPitch + cardWidth;
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
