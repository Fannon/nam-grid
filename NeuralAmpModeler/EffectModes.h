#pragma once
#include <algorithm>
#include <array>

// Only implemented choices are exposed to hosts. Keep mode names/counts shared
// by parameter registration, DSP, the editor and tests; no selectable spare slots.
namespace effect_modes
{
enum class DistortionStyle { Soft, Hard, Fuzz, Foldback, Diode, Rectify, Count };
inline constexpr int distortionCount = static_cast<int>(DistortionStyle::Count);
inline constexpr std::array<const char*, distortionCount> distortionNames{
  "Soft clip", "Hard clip", "Fuzz", "Foldback", "Diode", "Rectify"};
inline constexpr std::array<const char*, distortionCount> distortionHelp{
  "Soft clip rounds peaks for smooth compression and predominantly odd harmonics.",
  "Hard clip flattens peaks abruptly for a sharper, brighter edge.",
  "Fuzz saturates strongly even at low Drive, making notes dense and compressed.",
  "Foldback folds peaks back on themselves; Drive changes the harmonic pattern.",
  "Diode clips negative peaks sooner than positive peaks, adding even harmonics.",
  "Rectify turns both half-cycles upward for an octave-like buzz; DC is removed."};
inline constexpr std::array<const char*, 3> junoNames{"I", "I+II", "II"};
inline constexpr std::array<const char*, 2> gateNames{"Natural", "Tight"};
inline constexpr std::array<const char*, 3> filterNames{"Low-pass", "High-pass", "Band-pass"};
inline constexpr std::array<const char*, 8> tapeHeadNames{
  "Single", "Dual", "Syncopated", "All heads", "Early pair", "Late pair", "Stereo dual", "Stereo cascade"};
struct TapeHeadPattern
{
  std::array<std::array<double, 4>, 2> gains;
  bool crossFeedback = false;
  constexpr TapeHeadPattern(std::array<double,4> left, std::array<double,4> right, bool cross=false)
  : gains{left,right}, crossFeedback(cross) {}
};
inline constexpr std::array<TapeHeadPattern, tapeHeadNames.size()> tapeHeadPatterns{{
  {{0.,0.,0.,1.}, {0.,0.,0.,1.}},
  {{0.,.5,0.,.5}, {0.,.5,0.,.5}},
  {{1./3.,0.,1./3.,1./3.}, {1./3.,0.,1./3.,1./3.}},
  {{.25,.25,.25,.25}, {.25,.25,.25,.25}},
  {{.5,.5,0.,0.}, {.5,.5,0.,0.}},
  {{0.,0.,.5,.5}, {0.,0.,.5,.5}},
  {{0.,1.,0.,0.}, {0.,0.,0.,1.}, true},
  {{.5,0.,.5,0.}, {0.,.5,0.,.5}, true}
}};
inline const TapeHeadPattern& TapeHeads(int index)
{ return tapeHeadPatterns[std::clamp(index, 0, static_cast<int>(tapeHeadPatterns.size())-1)]; }
inline constexpr std::array<const char*, tapeHeadNames.size()> tapeHeadHelp{
  "Single: the last head, at full Time. Feedback always comes from the last head.",
  "Dual: heads at half and full Time, equally weighted. Feedback comes from the last head.",
  "Syncopated: quarter, three-quarter and full Time, equally weighted. Feedback comes from the last head.",
  "All heads: quarter, half, three-quarter and full Time. Extra heads add rhythm without increasing feedback gain.",
  "Early pair: quarter and half Time, equally weighted. Feedback still comes from the unselected last head, at full Time.",
  "Late pair: three-quarter and full Time, equally weighted. Feedback comes from the last head.",
  "Stereo dual: half Time on the left, full Time on the right. Last-head feedback crosses between channels; dry stereo stays intact.",
  "Stereo cascade: quarter/three-quarter Time on the left, half/full Time on the right. Last-head feedback crosses channels; dry stereo stays intact."};
inline constexpr std::array<const char*, 3> cosmicNames{"Smooth", "Bloom", "Sparse"};
inline constexpr std::array<const char*, 3> cosmicHelp{
  "Smooth: Faust's Gemini model, with sixteen scattering delays for dense ambience. Changing Style fades and clears the old tail.",
  "Bloom: Faust's Sagittarius model, with eight stereo allpasses for slow buildup. Feedback changes both attack and decay. Changing Style fades and clears the old tail.",
  "Sparse: Faust's Virgo model, with four filtered feedback delays for complex echoes. Changing Style fades and clears the old tail."};
} // namespace effect_modes
