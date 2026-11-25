#pragma once
#include "AIGate.h"
#include "GridPerformance.h"
#include "ModulationMatrix.h"
#include "RoadmapEffects.h"
#include "SlotAirReverb.h"
#include "SlotEQ.h"
#include "SlotHQEffects.h"
#include "SlotOutboard.h"
#include "SlotTimeEffects.h"
#include "SlotUtility.h"
#include "StereoUtility.h"
#include "dsp/airwindows/outboard/ButterComp2.h"
#include "dsp/airwindows/outboard/CStrip2.h"
#include "dsp/reverb/CosmicReverb.h"
#include "dsp/reverb/ReverbPrimitives.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

namespace dsp_safety_tests
{
struct SafetyReport
{
  int failures = 0;
  void Check(bool condition, const char* label)
  {
    if (!condition)
    {
      std::cerr << "[FAIL] " << label << '\n';
      ++failures;
    }
  }
};

inline void TestGroup1SourceBankAndMatrix(SafetyReport& report)
{
  using namespace grid_performance;
  using namespace grid_modulation;
  const double kNaN = std::numeric_limits<double>::quiet_NaN();
  const double kInf = std::numeric_limits<double>::infinity();

  // 1a. Tempo == 0., negative, NaN, +Inf in Song-Sync mode must not yield NaN/UB.
  for (double badTempo : {0., -120., kNaN, kInf})
  {
    SourceBank bank(0);
    bank.Reset(48000.);
    Values v = Defaults();
    v[LFO1Sync] = 1.;
    v[LFO1Retrigger] = RetriggerSong;
    v[LFO1Shape] = 5.; // Random -> exercises SongRandom
    v[Seq1Sync] = 1.;
    v[Seq1Retrigger] = RetriggerSong;
    std::array<double, 64> l{}, r{};
    for (int i = 0; i < 64; ++i) l[i] = .2 * std::sin(i * .1);
    bank.Advance(l.data(), r.data(), 64, v, badTempo, nullptr, 0, {true, 0.});
    const auto mon = bank.Read();
    report.Check(std::isfinite(mon.phase) && mon.phase >= 0.f && mon.phase <= 1.f,
                 "Group1: SourceBank tempo<=0/non-finite song phase");
    report.Check(std::isfinite(mon.lfo), "Group1: SourceBank tempo<=0/non-finite song LFO");
    report.Check(mon.step >= 0 && mon.step <= 7, "Group1: SourceBank tempo<=0/non-finite step range");
    report.Check(std::isfinite(bank.Value(SourceLFO, v)), "Group1: SourceBank Value(LFO) finite");
    const double seq = bank.Value(SourceSequencer, v);
    report.Check(std::isfinite(seq) && seq >= -1. && seq <= 1., "Group1: SourceBank Value(Seq) finite in [-1,1]");
  }

  // 1b. Non-finite or huge transport.beats in RetriggerSong must stay finite & bounded.
  for (double badBeats : {kNaN, kInf, -kInf, 1.e308, -1.e308})
  {
    SourceBank bank(1);
    bank.Reset(48000.);
    Values v = Defaults();
    v[LFO2Retrigger] = RetriggerSong;
    v[LFO2Shape] = 5.;
    v[Seq2Retrigger] = RetriggerSong;
    std::array<double, 64> audio{};
    audio.fill(.1);
    bank.Advance(audio.data(), audio.data(), 64, v, 120., nullptr, 0, {true, badBeats});
    const auto mon = bank.Read();
    report.Check(std::isfinite(mon.phase) && std::isfinite(mon.lfo), "Group1: SourceBank bad transport.beats LFO");
    report.Check(mon.step >= 0 && mon.step <= 7, "Group1: SourceBank bad transport.beats step");
    report.Check(std::isfinite(bank.Value(SourceLFO, v)) && std::isfinite(bank.Value(SourceSequencer, v)),
                 "Group1: SourceBank bad transport.beats Value()");
  }

  // 1c. NaN / +Inf audio sample must not permanently poison envelope/onset followers,
  // and silence decay must flush denormals to 0.
  {
    SourceBank bank(0);
    bank.Reset(48000.);
    Values v = Defaults();
    v[LFO1Retrigger] = RetriggerOnset;
    std::array<double, 64> badL{}, badR{};
    badL.fill(.2);
    badR.fill(.2);
    badL[10] = kNaN;
    badR[20] = kInf;
    bank.Advance(badL.data(), badR.data(), 64, v, 120.);
    std::array<double, 64> good{};
    for (int i = 0; i < 64; ++i) good[i] = .3 * std::sin(i * .2);
    bank.Advance(good.data(), good.data(), 64, v, 120.);
    const auto mon = bank.Read();
    report.Check(std::isfinite(mon.input) && std::isfinite(mon.envelope),
                 "Group1: SourceBank envelope recovery after NaN/Inf audio");
    report.Check(std::isfinite(bank.Value(SourceEnvelope, v)),
                 "Group1: SourceBank Value(Envelope) after NaN/Inf audio");

    // Decay on silence: must flush below 1e-30 cleanly to 0 without NaN.
    v[Env1Release] = 1.; // 1 ms release -> fast decay
    std::array<double, 512> silence{};
    for (int b = 0; b < 40; ++b)
      bank.Advance(silence.data(), silence.data(), 512, v, 120.);
    const auto quiet = bank.Read();
    report.Check(quiet.input == 0.f && quiet.envelope == 0.f,
                 "Group1: SourceBank envelope denormal flush on silence");
  }

  // 1d. Matrix::Apply with non-finite offset on GridBlend and nullptr slot shape.
  {
    Matrix matrix;
    std::array<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> animated{};
    Routing routing{};
    Matrix::Targets targets{};
    for (double badOffset : {kNaN, kInf, -kInf})
    {
      double blend = 0.5;
      targets[0] = {gridSlot, GridBlend, badOffset};
      targets[1] = {0, parameter_layout::kSlotIn, badOffset};
      matrix.Apply(animated, blend, routing, targets, 2);
      report.Check(std::isfinite(blend) && blend == 0.5, "Group1: Matrix::Apply GridBlend non-finite offset");
      report.Check(std::isfinite(animated[0].inputDB), "Group1: Matrix::Apply nullptr slot shape");
    }
  }

  // 1e. Valid-audio invariance: finite tempo, beats, audio and finite GridBlend offset.
  {
    SourceBank bank(0);
    bank.Reset(48000.);
    Values v = Defaults();
    v[LFO1Sync] = 1.;
    v[LFO1Retrigger] = RetriggerSong;
    v[Seq1Retrigger] = RetriggerSong;
    std::array<double, 128> audio{};
    for (int i = 0; i < 128; ++i) audio[i] = .25 * std::sin(i * .11);
    bank.Advance(audio.data(), audio.data(), 128, v, 120., nullptr, 0, {true, 4.25});
    const auto mon = bank.Read();
    // At 120 BPM, 48 kHz, 128 samples -> beats at i=127: 4.25 + 127 * 2 / 48000.
    // Held at multiples of quantum (16 frames): last hold at i = 127 (8th quantum).
    // LFO1Division defaults to 5 (1/4 note = 1 beat), so lfoPerBeat = 1.0.
    const double expectedBeats = 4.25 + 127. * (120. / (60. * 48000.));
    const double expectedPhase = expectedBeats - std::floor(expectedBeats);
    report.Check(std::abs(mon.phase - static_cast<float>(expectedPhase)) < 1.e-6f,
                 "Group1: Valid-audio SourceBank song phase invariance");

    Matrix matrix;
    std::array<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> animated{};
    Routing routing{};
    Matrix::Targets targets{};
    targets[0] = {gridSlot, GridBlend, 0.25};
    double blend = 0.3;
    matrix.Apply(animated, blend, routing, targets, 1);
    report.Check(blend == 0.55, "Group1: Valid-audio Matrix::Apply GridBlend invariance");
  }
}

inline void TestGroup2DelayClampsAndSmoothing(SafetyReport& report)
{
  const double kNaN = std::numeric_limits<double>::quiet_NaN();
  const double kInf = std::numeric_limits<double>::infinity();

  // 2a. BasicDelayLine::Read NaN / out-of-range clamps
  {
    grid_fx::DelayLine line, ref;
    line.Reset(32);
    ref.Reset(32);
    for (int i = 1; i <= 20; ++i)
    {
      const double x = 0.05 * i;
      line.Push(x);
      ref.Push(x);
    }
    const double outNaN = line.Read(kNaN);
    const double outNeg = line.Read(-5.);
    const double outInf = line.Read(kInf);
    const double outNInf = line.Read(-kInf);
    report.Check(std::isfinite(outNaN) && outNaN == ref.Read(1.), "Group2: BasicDelayLine::Read(NaN) == Read(1.)");
    report.Check(std::isfinite(outNeg) && outNeg == ref.Read(1.), "Group2: BasicDelayLine::Read(-5.) == Read(1.)");
    report.Check(std::isfinite(outNInf) && outNInf == ref.Read(1.), "Group2: BasicDelayLine::Read(-Inf) == Read(1.)");
    report.Check(std::isfinite(outInf) && outInf == ref.Read(ref.MaxDelay()),
                 "Group2: BasicDelayLine::Read(+Inf) == Read(MaxDelay)");
    report.Check(line.Read(4.375) == ref.Read(4.375), "Group2: BasicDelayLine valid read invariant");
  }

  // 2b. MovingDelay::Read NaN on both mFirst=true and mFirst=false
  {
    grid_reverb::MovingDelay firstNaN, settledNaN;
    firstNaN.Reset(48000., .05);
    settledNaN.Reset(48000., .05);
    for (int i = 1; i <= 32; ++i)
    {
      firstNaN.Push(0.02 * i);
      settledNaN.Push(0.02 * i);
    }
    const double r1 = firstNaN.Read(kNaN);
    report.Check(std::isfinite(r1) && firstNaN.Clipped(), "Group2: MovingDelay::Read(NaN) mFirst=true finite & clipped");
    (void)settledNaN.Read(8.5);
    report.Check(!settledNaN.Clipped(), "Group2: MovingDelay valid read not clipped");
    const double r2 = settledNaN.Read(kNaN);
    report.Check(std::isfinite(r2) && settledNaN.Clipped(), "Group2: MovingDelay::Read(NaN) mFirst=false finite & clipped");
  }

  // 2c. grid_cosmic::Line::ReadTap & Tick NaN clamps
  {
    grid_cosmic::Line line, ref;
    line.Reset(48000.);
    ref.Reset(48000.);
    for (int i = 1; i <= 24; ++i)
    {
      const double a = line.Tick(0.1 * i, 5.25);
      const double b = ref.Tick(0.1 * i, 5.25);
      report.Check(a == b, "Group2: Cosmic Line valid Tick invariant");
    }
    const double tapNaN = line.ReadTap(kNaN);
    const double tapRef = ref.ReadTap(1.);
    report.Check(std::isfinite(tapNaN) && tapNaN == tapRef, "Group2: Cosmic Line::ReadTap(NaN) == ReadTap(1.)");
    const double tickNaN = line.Tick(0.25, kNaN);
    const double tickRef = ref.Tick(0.25, 2.);
    report.Check(std::isfinite(tickNaN) && tickNaN == tickRef, "Group2: Cosmic Line::Tick(NaN) == Tick(2.)");
  }

  // 2d. SmoothControls::Target (both overloads) with NaN and synced +/-Inf
  {
    grid_fx::SmoothControls<4> sm;
    sm.Reset(48000.);
    // First update with NaN -> falls back to parameter initial
    sm.Target({kNaN, kNaN, kNaN, kNaN}, uDelayTime, false);
    auto v = sm.Tick();
    for (size_t j = 0; j < 4; ++j)
      report.Check(std::isfinite(v[j]) && v[j] == kEffectParameters[uDelayTime + j].initial,
                   "Group2: SmoothControls::Target(offset) mFirst NaN fallback");

    // Synced first update with +Inf on j==0
    sm.Reset(48000.);
    sm.Target({kInf, 25., 5000., 30.}, uDelayTime, true);
    v = sm.Tick();
    report.Check(std::isfinite(v[0]) && v[0] == kEffectParameters[uDelayTime].initial,
                 "Group2: SmoothControls::Target(offset, synced) +Inf fallback");

    // Settled update with NaN -> retains previous valid target
    sm.Target({350., 40., 4000., 50.}, uDelayTime, false);
    for (int i = 0; i < 4000; ++i) (void)sm.Tick();
    const double settled0 = sm.Value(0);
    sm.Target({kNaN, kNaN, kNaN, kNaN}, uDelayTime, false);
    v = sm.Tick();
    report.Check(std::isfinite(v[0]) && std::abs(v[0] - settled0) < 1.e-6,
                 "Group2: SmoothControls::Target(offset) settled NaN retains target");

    // Explicit ids overload
    grid_fx::SmoothControls<3> idsSm;
    idsSm.Reset(48000.);
    const std::array<int, 3> ids{uSwellCurve, uSwellHold, uSwellMix};
    idsSm.Target({kNaN, kNaN, kNaN}, ids);
    const auto vIds = idsSm.Tick();
    for (size_t j = 0; j < 3; ++j)
      report.Check(std::isfinite(vIds[j]) && vIds[j] == kEffectParameters[ids[j]].initial,
                   "Group2: SmoothControls::Target(ids) mFirst NaN fallback");
  }

  // 2e. SlotDelay::Process with NaN delay[0]
  {
    SlotDelay delay;
    delay.Reset(48000.);
    UtilityControls c;
    c.delay = {kNaN, 30., 4000., 0., 50.};
    c.extra.delayTimeChange = 1.;
    std::array<double, 64> buf{};
    for (int i = 0; i < 64; ++i) buf[i] = .2 * std::sin(i * .1);
    delay.Process(buf.data(), 64, SlotEffect::DigitalDelay, c);
    bool allFinite = true;
    for (double s : buf) allFinite &= std::isfinite(s);
    report.Check(allFinite, "Group2: SlotDelay::Process NaN delay[0] stays finite");
  }
}

inline void TestGroup3CaptureFade(SafetyReport& report)
{
  // Stutter at 8000 Hz and 1000 Hz with minimum 0.1 ms fade (rate * .001 * .1 < 1.)
  for (double rate : {8000., 1000.})
  {
    grid_roadmap::Capture cap;
    cap.Reset(rate);
    UtilityControls c;
    c.stutter = {10., 75., .1, 100.};
    std::array<double, 128> l{}, r{};
    for (int b = 0; b < 4; ++b)
    {
      for (int i = 0; i < 128; ++i)
      {
        l[i] = .25 * std::sin((b * 128 + i) * .17);
        r[i] = .25 * std::cos((b * 128 + i) * .13);
      }
      cap.Process(l.data(), r.data(), 128, SlotEffect::Stutter, c);
      bool finite = true;
      for (int i = 0; i < 128; ++i) finite &= std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i]) <= 1. && std::abs(r[i]) <= 1.;
      report.Check(finite, "Group3: Capture Stutter low-rate fade>=1 finite");
      if (!finite) break;
    }
  }

  // Freeze at 500 Hz and ReverseDelay across small frame counts
  {
    grid_roadmap::Capture freeze;
    freeze.Reset(500.);
    UtilityControls c;
    c.freeze = {1., 50., 1., 6000., 100.};
    for (int n : {1, 2, 3, 7, 16, 32})
    {
      std::vector<double> buf(n, .2);
      freeze.Process(buf.data(), nullptr, n, SlotEffect::Freeze, c);
      bool finite = true;
      for (double s : buf) finite &= std::isfinite(s);
      report.Check(finite, "Group3: Capture Freeze low-rate finite");
    }
  }
}

inline void TestGroup4SoftLimit(SafetyReport& report)
{
  using grid_hq::SoftLimit;
  const double kNaN = std::numeric_limits<double>::quiet_NaN();
  const double kInf = std::numeric_limits<double>::infinity();

  report.Check(SoftLimit(10.) == 1., "Group4: SoftLimit(10.) == 1.");
  report.Check(SoftLimit(-10.) == -1., "Group4: SoftLimit(-10.) == -1.");
  report.Check(SoftLimit(1.e6) == 1. && SoftLimit(-1.e6) == -1., "Group4: SoftLimit(+-1e6) == +-1.");
  report.Check(SoftLimit(1.e200) == 1. && SoftLimit(-1.e200) == -1., "Group4: SoftLimit(+-1e200) == +-1.");
  report.Check(SoftLimit(kInf) == 1. && SoftLimit(-kInf) == -1., "Group4: SoftLimit(+-Inf) == +-1.");
  report.Check(SoftLimit(kNaN) == 0., "Group4: SoftLimit(NaN) == 0.");
  report.Check(SoftLimit(3.) == 1. && SoftLimit(-3.) == -1., "Group4: SoftLimit(+-3.) == +-1.");

  // Bit-identical across (-3., 3.)
  bool sweepExact = true;
  for (int i = -2999; i <= 2999; ++i)
  {
    const double x = i * 0.001;
    const double x2 = x * x;
    const double expected = x * (27. + x2) / (27. + 9. * x2);
    if (SoftLimit(x) != expected) { sweepExact = false; break; }
  }
  report.Check(sweepExact, "Group4: SoftLimit (-3., 3.) bit-identical");

  // JunoChorus & FrequencyShifter fed +/-100. stay finite
  {
    grid_hq::JunoChorus chorus;
    chorus.Reset(48000.);
    grid_hq::FrequencyShifter shifter;
    shifter.Reset(48000.);
    UtilityControls c;
    c.juno = {1., 100., 100.};
    c.shift = {120., 95., 120., 100., 8000., 0., 100.};
    std::array<double, 128> l{}, r{};
    for (int i = 0; i < 128; ++i) { l[i] = (i % 2 ? 100. : -100.); r[i] = -l[i]; }
    auto cl = l, cr = r;
    chorus.Process(cl.data(), cr.data(), 128, c);
    shifter.Process(l.data(), r.data(), 128, c);
    bool finite = true;
    for (int i = 0; i < 128; ++i)
      finite &= std::isfinite(cl[i]) && std::isfinite(cr[i]) && std::isfinite(l[i]) && std::isfinite(r[i]);
    report.Check(finite, "Group4: JunoChorus & FrequencyShifter +-100 input finite");
  }
}

inline void TestGroup5ScalarFilter(SafetyReport& report)
{
  const double kNaN = std::numeric_limits<double>::quiet_NaN();
  const double kInf = std::numeric_limits<double>::infinity();

  for (double bad : {kNaN, kInf, -kInf})
  {
    ScalarFilter<recursive_linear_filter::Peaking> filter, fresh;
    const recursive_linear_filter::BiquadParams params(48000., 1000., .7, 6.);
    filter.SetParams(params);
    fresh.SetParams(params);
    for (int i = 0; i < 64; ++i) (void)filter.Tick(.3 * std::sin(i * .13));
    const double outBad1 = filter.Tick(bad);
    const double outBad2 = filter.Tick(bad);
    report.Check(outBad1 == 0. && outBad2 == 0., "Group5: ScalarFilter::Tick(non-finite) returns 0.");
    bool recoveredExact = true;
    for (int i = 0; i < 256; ++i)
    {
      const double x = .3 * std::sin((i + 66) * .13);
      const double y = filter.Tick(x);
      const double yRef = fresh.Tick(x);
      if (!std::isfinite(y) || y != yRef) { recoveredExact = false; break; }
    }
    report.Check(recoveredExact, "Group5: ScalarFilter::Tick recovers bit-identically after non-finite");
  }

  // Valid audio invariance on SlotEQ
  {
    SlotEQ eq;
    eq.Reset(48000.);
    EQControls c;
    c.bass = 3.; c.mid = -2.; c.treble = 4.; c.highPass = 80.; c.lowPass = 12000.; c.presence = 2.;
    std::array<double, 128> buf{};
    for (int i = 0; i < 128; ++i) buf[i] = .25 * std::sin(i * .09);
    eq.Process(buf.data(), 128, true, c);
    bool finite = true;
    for (double s : buf) finite &= std::isfinite(s);
    report.Check(finite, "Group5: SlotEQ valid audio finite");
  }
}

inline void TestGroup6OutboardDC(SafetyReport& report)
{
  // 6a. CStrip2 sustained -1.0 and +1.0 DC at max Compress & Speed (overflows ~42,155 samples on main)
  for (double dc : {-1.0, 1.0})
  {
    airwindows_outboard::CStrip2::Core core;
    core.Reset(44100.);
    core.Set({.5, .5, .5, .5, .5, 1., 0., 1., 1., 1. / 3.});
    bool finite = true;
    for (int i = 0; i < 60000; ++i)
    {
      double l = dc, r = dc;
      core.Tick(l, r);
      if (!std::isfinite(l) || !std::isfinite(r)) { finite = false; break; }
    }
    for (int i = 0; i < 1000 && finite; ++i)
    {
      double l = .5 * std::sin(i * .1), r = l;
      core.Tick(l, r);
      if (!std::isfinite(l) || !std::isfinite(r)) { finite = false; break; }
    }
    report.Check(finite, dc < 0. ? "Group6: CStrip2 sustained -1.0 DC" : "Group6: CStrip2 sustained +1.0 DC");
  }

  // 6b. ButterComp2 sustained -1.0 and +1.0 DC at A=1.0 (overflows ~3.99M samples on main)
  for (double dc : {-1.0, 1.0})
  {
    airwindows_outboard::ButterComp2::Core core;
    core.Reset(44100.);
    core.Set({1., 0., 1.});
    bool finite = true;
    for (int i = 0; i < 4200000; ++i)
    {
      double l = dc, r = dc;
      core.Tick(l, r);
      if (!std::isfinite(l) || !std::isfinite(r)) { finite = false; break; }
    }
    for (int i = 0; i < 1000 && finite; ++i)
    {
      double l = .5 * std::sin(i * .1), r = l;
      core.Tick(l, r);
      if (!std::isfinite(l) || !std::isfinite(r)) { finite = false; break; }
    }
    report.Check(finite, dc < 0. ? "Group6: ButterComp2 sustained -1.0 DC" : "Group6: ButterComp2 sustained +1.0 DC");
  }
}

inline void TestGroup7ResetAndBounds(SafetyReport& report, bool includeCrashingOnMain)
{
  const double kNaN = std::numeric_limits<double>::quiet_NaN();
  const double kInf = std::numeric_limits<double>::infinity();

  // 7a. AIGate::Reset bad rates
  for (double rate : {0., -48000., kNaN, kInf})
  {
    ai_gate::Processor gate;
    gate.Reset(rate);
    ai_gate::Controls c;
    c.reduction = 80.;
    std::array<double, 512> l{}, r{};
    for (int i = 0; i < 512; ++i) { l[i] = .1 * std::sin(i * .07); r[i] = l[i]; }
    gate.Process(l.data(), r.data(), 512, c);
    bool finite = true;
    for (int i = 0; i < 512; ++i) finite &= std::isfinite(l[i]) && std::isfinite(r[i]);
    report.Check(finite, "Group7: AIGate::Reset bad rate finite output");
  }

  // 7b. SlotTape::Reset bad rates + extreme low rate
  for (double rate : {0., -48000., kNaN, kInf, 1.})
  {
    SlotTape tape;
    tape.Reset(rate);
    UtilityControls c;
    c.tape = {6., 50., 100., 0.};
    std::array<double, 512> l{}, r{};
    for (int i = 0; i < 512; ++i) { l[i] = .25 * std::sin(i * .09); r[i] = l[i]; }
    tape.Process(l.data(), 512, c);
    tape.Process(r.data(), 512, c);
    bool finite = true;
    for (int i = 0; i < 512; ++i) finite &= std::isfinite(l[i]) && std::isfinite(r[i]);
    report.Check(finite, "Group7: SlotTape::Reset bad rate finite output");
  }

  // 7c. SlotBoost::Reset bad rates & boostMidGain/boost/tilt clamping
  {
    for (double rate : {0., -48000., kNaN, kInf})
    {
      SlotBoost boost;
      boost.Reset(rate);
      UtilityControls c;
      c.boost = 6.; c.tilt = 2.; c.extra.boostMidGain = 3.;
      std::array<double, 256> buf{};
      for (int i = 0; i < 256; ++i) buf[i] = .2 * std::sin(i * .1);
      boost.Process(buf.data(), 256, c, {});
      bool finite = true;
      for (double s : buf) finite &= std::isfinite(s);
      report.Check(finite, "Group7: SlotBoost::Reset bad rate finite output");
    }

    const auto runBoost = [](double boostDb, double tiltDb, double midDb) {
      SlotBoost boost;
      boost.Reset(48000.);
      UtilityControls c;
      c.boost = boostDb; c.tilt = tiltDb; c.extra.boostMidGain = midDb;
      std::array<double, 256> buf{};
      for (int i = 0; i < 256; ++i) buf[i] = .2 * std::sin(i * .1);
      boost.Process(buf.data(), 256, c, {});
      return buf;
    };
    const auto hi = runBoost(6., 0., 1.e6), refHi = runBoost(6., 0., 12.);
    const auto lo = runBoost(6., 0., -1.e6), refLo = runBoost(6., 0., -12.);
    const auto nanMid = runBoost(kNaN, kNaN, kNaN);
    report.Check(hi == refHi, "Group7: SlotBoost boostMidGain clamped at +12 dB");
    report.Check(lo == refLo, "Group7: SlotBoost boostMidGain clamped at -12 dB");
    bool nanFinite = true;
    for (double s : nanMid) nanFinite &= std::isfinite(s);
    report.Check(nanFinite, "Group7: SlotBoost NaN controls finite output");
  }

  // 7d. Airwindows Reverbs Core::Reset zeroing from dirty 0xFF memory & Reset(0.) safety
  {
    const auto testReverb = [&](auto tag, const auto& controls, const char* name) {
      using Core = typename decltype(tag)::type;
      auto dirtyBuf = std::make_unique<unsigned char[]>(sizeof(Core));
      auto cleanBuf = std::make_unique<unsigned char[]>(sizeof(Core));
      std::memset(dirtyBuf.get(), 0xFF, sizeof(Core));
      std::memset(cleanBuf.get(), 0x00, sizeof(Core));
      Core* dirty = new (dirtyBuf.get()) Core();
      Core* clean = new (cleanBuf.get()) Core();
      dirty->Reset(48000.);
      clean->Reset(48000.);
      dirty->Set(controls);
      clean->Set(controls);
      bool exact = true, finite = true;
      for (int i = 0; i < 256; ++i)
      {
        double dl = .15 * std::sin(i * .13), dr = .15 * std::cos(i * .11);
        double cl = dl, cr = dr;
        dirty->Tick(dl, dr);
        clean->Tick(cl, cr);
        if (!std::isfinite(dl) || !std::isfinite(dr)) finite = false;
        if (dl != cl || dr != cr) exact = false;
      }
      report.Check(finite && exact, name);

      // Also test Reset(0.) + Set + Tick stays finite
      dirty->Reset(0.);
      dirty->Set(controls);
      double zl = .1, zr = -.1;
      dirty->Tick(zl, zr);
      report.Check(std::isfinite(zl) && std::isfinite(zr), "Group7: Reverb Core::Reset(0.) finite");

      dirty->~Core();
      clean->~Core();
    };
    testReverb(std::type_identity<airwindows_reverb::Galactic3::Core>{}, std::array<double, 6>{.5, .5, .5, 1., .7, 1.},
               "Group7: Galactic3::Core::Reset from 0xFF memory matches clean");
    testReverb(std::type_identity<airwindows_reverb::kPlate140::Core>{}, std::array<double, 5>{1., .5, 1., 0., .25},
               "Group7: kPlate140::Core::Reset from 0xFF memory matches clean");
    testReverb(std::type_identity<airwindows_reverb::kGuitarHall2::Core>{}, std::array<double, 6>{.5, .5, .5, .5, .5, .5},
               "Group7: kGuitarHall2::Core::Reset from 0xFF memory matches clean");
  }

  // 7e. StereoUtility unreset Process & bad Reset rates (crashes/OOB on unpatched main)
  if (includeCrashingOnMain)
  {
    StereoUtility unreset;
    std::array<double, 64> l{}, r{};
    l.fill(.2); r.fill(-.2);
    UtilityControls c;
    unreset.Process(l.data(), r.data(), 64, c);
    report.Check(l[0] == .2 && r[0] == -.2, "Group7: StereoUtility unreset Process safe no-op");

    for (double rate : {0., -48000., kNaN, kInf})
    {
      StereoUtility util;
      util.Reset(rate);
      c.fineDelay = kNaN;
      c.extra.utilitySkew = kNaN;
      c.extra.utilityMode = kNaN;
      for (int i = 0; i < 64; ++i) { l[i] = .2 * std::sin(i * .1); r[i] = l[i]; }
      util.Process(l.data(), r.data(), 64, c);
      bool finite = true;
      for (int i = 0; i < 64; ++i) finite &= std::isfinite(l[i]) && std::isfinite(r[i]);
      report.Check(finite, "Group7: StereoUtility bad rate + NaN controls finite");
    }
  }
}
} // namespace dsp_safety_tests

inline void TestDSPSafety()
{
  dsp_safety_tests::SafetyReport report;
  dsp_safety_tests::TestGroup1SourceBankAndMatrix(report);
  dsp_safety_tests::TestGroup2DelayClampsAndSmoothing(report);
  dsp_safety_tests::TestGroup3CaptureFade(report);
  dsp_safety_tests::TestGroup4SoftLimit(report);
  dsp_safety_tests::TestGroup5ScalarFilter(report);
  dsp_safety_tests::TestGroup6OutboardDC(report);
  // Run StereoUtility unreset/negative-rate crash guards once non-crashing checks pass
  // (or always when clean), so pre-fix runs still report all non-crashing failures first.
  const bool runCrashing = (report.failures == 0);
  dsp_safety_tests::TestGroup7ResetAndBounds(report, runCrashing);
  assert(report.failures == 0);
  std::cout << "DSP safety guards: all 7 groups and valid-audio invariance checks passed\n";
}
