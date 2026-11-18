#pragma once
#include "ExpandedDelayPreview.h"

void TestExpandedDelayPreview()
{
  using namespace delay_preview;
  const std::array<Tap, 4> taps{{{.25, 1., -1.}, {.5, 0., 0.}, {.75, 0., 0.}, {1., 0., 0.}}};
  const int firstTap = DelayBin(.25, 3.), lastTap = DelayBin(1., 3.);
  const auto repeat = PatternReturns(taps, 0, .5, 1.);
  const auto swell = PatternReturns(taps, 1, .5, 1.);
  const auto matrix = PatternReturns(taps, 2, .5, 1.);
  // Repeat listens to tap 4 even at zero level. Swell returns from the one
  // audible tap. Matrix puts a negative contribution back into the same line.
  assert(repeat[0][firstTap] == 0. && repeat[0][firstTap + lastTap] == .5);
  assert(swell[0][2 * firstTap] == .5 && matrix[0][2 * firstTap] == -.25);
  for (int i = 0; i < bins; ++i)
    assert(repeat[1][i] == 0. && swell[1][i] == 0. && matrix[1][i] == 0.);
  const auto mutedFeedback = PatternReturns(taps, 1, 0., 1.);
  for (const auto& channel : mutedFeedback)
    for (double sample : channel) assert(sample == 0.);

  const auto single = TapeRepeats(0, .5, .3, 1.);
  const int head = DelayBin(.1, 1.);
  assert(single[head] == 1. && single[2 * head] == .5);
  assert(BBDBandwidth(1, 500., 10000.) < BBDBandwidth(1, 100., 10000.));
  // Repeats decay with Feedback, lose treble on every pass, and are darker at longer Times.
  const auto shortTrain = BBDRepeatTrain(1, 150., 12000., .5, 1., 48000., 2.);
  const auto longTrain = BBDRepeatTrain(1, 600., 12000., .5, 1., 48000., 2.);
  assert(shortTrain.count == 12 && longTrain.count == 3 && std::abs(longTrain.repeats[2].seconds - 1.8) < 1.e-12);
  assert(shortTrain.repeats[1].level == .5 && shortTrain.repeats[1].treble < shortTrain.repeats[0].treble);
  assert(longTrain.repeats[0].treble < shortTrain.repeats[0].treble);
  assert(BBDRepeatTrain(3, 300., 2000., .5, 1., 48000., 2.).cutoff == 2000.); // High cut caps Clear.
  // Granular: without scatter, forward grains return the note around Time; more density
  // gives more, shorter fragments; feedback adds later passes; Glitch marks latched repeats.
  const auto plain = GrainFragments(0, 400., 80., .5, 0., 0., 0., 2.);
  assert(plain.firstPass >= 2 && plain.first >= .35 && plain.last <= .7 && std::abs(plain.hop - .5 * plain.length) < 1.e-12);
  const auto dense = GrainFragments(0, 400., 80., 1., 0., 0., 0., 2.);
  assert(dense.firstPass > plain.firstPass && dense.length < plain.length);
  const auto repeated = GrainFragments(0, 400., 80., .5, 0., 0., .6, 2.);
  bool second = false;
  for (int i = 0; i < repeated.count; ++i) second |= repeated.fragments[i].pass == 2 && repeated.fragments[i].start > .7;
  assert(second);
  const auto glitch = GrainFragments(2, 400., 60., .5, 1., 0., .4, 3.);
  bool latched = false;
  for (int i = 0; i < glitch.count; ++i) latched |= glitch.fragments[i].latched;
  assert(latched);
  const auto longGrains = GrainFragments(1, 4000., 1000., 0., 1., 12., .6, 10.);
  assert(longGrains.count <= static_cast<int>(longGrains.fragments.size()) && longGrains.length == 1.25);

  const auto pitch = PitchVoices(.5, .5);
  assert(pitch[1].weights[0] == .125 && pitch[1].weights[1] == .25 && pitch[1].weights[2] == .125);
  const auto onlyShifted = PitchVoices(1., .5);
  assert(onlyShifted[2].weights[0] == 0. && onlyShifted[2].weights[3] == .25);
  assert(FilterEffect(0, 400., 48000., 550., 0., .9) == 1.);
  assert(FilterEffect(1, 400., 48000., 550., 0., .9) == 1.);
}
