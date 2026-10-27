#pragma once
#include <algorithm>
#include <array>

// Display labels may evolve. Stored integer identities below must never change:
// append modes, retain old implementations, and never reuse an ID. Algorithm
// selectors are saved state, not automatable parameters; no spare choices exist.
namespace effect_modes
{
enum class PedalStyle
{
  Screamer = 0,
  Rodent = 1,
  BassEdge = 2,
  ParallelOD = 3,
  SustainFuzz = 4,
  CascadedPreamp = 5,
  TrebleDrive = 6,
  BluesBreaker = 7,
  TransparentOD = 8,
  VintageFuzz = 9,
  Count
};
inline constexpr int pedalCount = static_cast<int>(PedalStyle::Count);
inline constexpr std::array<const char*, pedalCount> pedalNames{
  "Screamer", "Rodent", "Bass Edge", "Parallel OD", "Sustain Fuzz", "Cascaded Preamp", "Treble Drive",
  "Blues OD", "Transparent OD", "Vintage Fuzz"};
inline constexpr std::array<const char*, pedalCount> pedalHelp{
  "Screamer: mid-focused soft drive with reduced bass into clipping. Useful before a driven amp.",
  "Rodent: filtered high gain, slew rounding and hard clipping for crunch and heavy sustain.",
  "Bass Edge: asymmetric saturation with upper-mid attack. Low keep can retain clean bass.",
  "Parallel OD: Drive increases clipping while reducing an internal clean path. Mix remains independent.",
  "Sustain Fuzz: two clipping stages with interstage filtering and scooped mids for dense sustain.",
  "Cascaded Preamp: two gently asymmetric stages with interstage filtering for responsive breakup.",
  "Treble Drive: bass-light, colored boost with asymmetric grit. Try it before a crunchy amp.",
  "Blues OD: two-stage, lightly voiced overdrive. Clipping moves from open to harder distortion.",
  "Transparent OD: restrained voicing, pre-drive bass shaping and post-drive treble filtering. Clipping changes compression and symmetry.",
  "Vintage Fuzz: coupled, inverting stages for responsive fuzz. Bias moves from rounder sustain to pinched, gated decay."};
inline constexpr std::array<const char*, pedalCount> pedalCharacterNames{
  "Focus", "Bite", "Attack", "Blend", "Mids", "Asymmetry", "Focus", "Clipping", "Clipping", "Bias"};
inline constexpr std::array<const char*, pedalCount> pedalCharacterHelp{
  "Focus: negative restores more bass into clipping; positive tightens the drive's bass and emphasizes mids. Center keeps the original voice.",
  "Bite: negative rounds the upper-mid gain and attack; positive increases bite and slew speed. Center keeps the original voice.",
  "Attack: negative softens upper-mid emphasis; positive brings out pick attack. Center keeps the original voice.",
  "Blend: negative adds internal clean sound; positive adds clipped sound. Drive still affects this blend; outer Mix remains independent.",
  "Mids: negative deepens the scoop; positive fills in the mids. Center keeps the original scoop.",
  "Asymmetry: negative makes the stages more symmetric; positive increases uneven clipping. Center keeps the original bias.",
  "Focus: negative admits more low end; positive shifts emphasis toward higher frequencies. Center keeps the original treble drive.",
  "Clipping: negative opens up soft clipping; center is the base soft drive; positive adds harder output clipping.",
  "Clipping: negative compresses with symmetric clipping; center has open symmetric clipping; positive adds asymmetry.",
  "Bias: negative gives rounder, more symmetric fuzz; positive pinches the waveform and gates quiet decays. Center is the base fuzz."};
inline constexpr std::array<const char*, 2> blackNames{"Zero crossing", "Hysteresis"};
inline constexpr std::array<const char*, 8> deskNames{"Dark", "Rock", "Lush", "Vibe", "Holo", "Punch", "Steel", "Tube"};

enum class DistortionStyle
{
  Soft = 0,
  Hard = 1,
  Fuzz = 2,
  Foldback = 3,
  Diode = 4,
  Rectify = 5,
  Count
};
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
enum class CosmicStyle
{
  Gemini = 0,
  Sagittarius = 1,
  Virgo = 2,
  Centaurus = 3,
  GreatAnnihilator = 4,
  Lyra = 5,
  AuroraHall = 6,
  Pulsar = 7,
  Nebula = 8,
  PhotonPlate = 9,
  Count
};
inline constexpr int cosmicCount = static_cast<int>(CosmicStyle::Count);
inline constexpr std::array<const char*, cosmicCount> cosmicNames{
  "Gemini", "Sagittarius", "Virgo", "Centaurus", "Great Annihilator", "Lyra", "Aurora Hall", "Pulsar Echo", "Binary Bloom", "Photon Plate"};
inline constexpr std::array<const char*, cosmicCount> cosmicTopology{
  "16-line diffusion", "8-stage stereo bloom", "Filtered feedback echoes",
  "4-stage matrix bloom", "Global feedback tank", "4-line sparse echoes", "8-loop diffused hall", "Stereo echo ladder", "Coupled short/long tanks", "Multi-tap figure-eight plate"};
inline constexpr std::array<const char*, cosmicCount> cosmicHelp{
  "Gemini: dense, smooth ambience from sixteen scattering delays. Valhalla Supermassive-inspired Faust model. Style changes fade and clear the old tail.",
  "Sagittarius: eight stereo allpasses create slow buildup; Feedback changes attack and decay. Valhalla Supermassive-inspired Faust model. Style changes fade and clear the old tail.",
  "Virgo: four delays with filtering inside feedback create dark, sparse echoes. Valhalla Supermassive-inspired Faust model. Style changes fade and clear the old tail.",
  "Centaurus: four serial four-channel allpasses build swelling ambience; Feedback changes the onset. Valhalla Supermassive-inspired Faust model. Style changes fade and clear the old tail.",
  "Great Annihilator: a delay stage and three matrix allpasses inside global feedback create long, evolving tails. Valhalla Supermassive-inspired Faust model. Style changes fade and clear the old tail.",
  "Lyra: four scattering delays with summed output create clear, sparse echoes; High cut shapes the output. Valhalla Supermassive-inspired Faust model. Style changes fade and clear the old tail.",
  "Aurora Hall: clustered echoes become a diffused guitar hall as Density rises. High cut damps each repeat; raise Feedback for long washes. Try Time 100-250 ms for a hall. Style changes fade and clear the old tail.",
  "Pulsar Echo: four serial stereo echoes with cross-channel regeneration. Low Density keeps the pulse pattern clear; high Density diffuses and rotates each stage. Time sets the scale of the whole pattern. Style changes fade and clear the old tail.",
  "Binary Bloom: coupled short and long delay tanks create an initial space and a broader, slower tail. Density controls scattering and transfer into the long tank. Try sustained chords and swells. Style changes fade and clear the old tail.",
  "Photon Plate: input diffusion feeds a cross-coupled figure-eight tank with seven output taps per channel. Try Time 100 ms, high Density and low Modulation for a quick, dense digital plate; Warp changes the tank asymmetry. Style changes fade and clear the old tail."};
} // namespace effect_modes
