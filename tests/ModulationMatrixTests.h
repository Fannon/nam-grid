#pragma once
#include "ModulationMatrix.h"
#include <set>

void TestModulationMatrix()
{
  using namespace grid_performance;
  using namespace grid_modulation;
  using parameter_layout::slotControls;
  using parameter_layout::standardControls;
  // The field map covers every slot control the audio snapshot reads, exactly
  // once, so host refresh and matrix writes agree. Full Quality is prepared off audio.
  {
    FixedNAMGrid::Controls c;
    std::array<int, slotControls> visits{};
    VisitSlotControls(c, [&](int control, auto&) { assert(control >= 0 && control < slotControls); ++visits[control]; });
    for (int control = 0; control < slotControls; ++control)
      assert(visits[control] == (control == parameter_layout::kSlotFullQuality ? 0 : 1));
    // Writes land in the visited field and convert like IParam::Bool()/Int().
    for (int control = 0; control < slotControls; ++control)
    {
      if (control == parameter_layout::kSlotFullQuality) continue;
      FixedNAMGrid::Controls written;
      WriteControl(written, control, 3.75);
      const double back = ReadControl(written, control);
      assert(back == 3.75 || back == 3. || back == 1.);
      int changed = 0;
      VisitSlotControls(written, [&](int other, const auto& field) {
        if (other != control && static_cast<double>(field) != ReadControl(FixedNAMGrid::Controls{}, other)) ++changed;
      });
      assert(changed == 0);
    }
    assert(FieldValue<bool>(.49) == false && FieldValue<bool>(.5) == true && FieldValue<int>(2.9) == 2);
  }
  // Only continuous controls are destinations.
  assert(Modulatable(parameter_layout::kSlotIn) && Modulatable(standardControls + uDelayMix));
  assert(!Modulatable(parameter_layout::kSlotEnabled) && !Modulatable(parameter_layout::kSlotFullQuality));
  assert(!Modulatable(standardControls + uTempoSync) && !Modulatable(standardControls + uTempoDivision));
  assert(!Modulatable(standardControls + uDistStyle) && !Modulatable(standardControls + uEQMode));
  assert(!Modulatable(standardControls + uFreezeHold) && !Modulatable(slotControls));
  assert(ValidDestination(gridSlot, GridSplit) && !ValidDestination(gridSlot, GridDestinations) && !ValidDestination(17, 0));
  for (int i = 0; i < Count; ++i)
    assert(StateOnly(i) == ((i >= ModRow && i < RowParam(rows, 0) && (i - ModRow) % RowFields != RowDepth)
                            || i == LFO1Shape || i == Env1Input || i == LFO1Retrigger || i == Seq1Retrigger
                            || i == LFO2Shape || i == Env2Input || i == LFO2Retrigger || i == Seq2Retrigger || i == MixLaw));

  // Released source integers and macro identities must not move when banks append.
  static_assert(SourceLFO1 == 1 && SourceEnv1 == 2 && SourceSeq1 == 3);
  static_assert(SourceMacro1 == 4 && SourceMacro2 == 5 && SourceMacro3 == 6 && SourceMacro4 == 7);
  static_assert(SourceLFO2 == 8 && SourceEnv2 == 9 && SourceSeq2 == 10);
  static_assert(SourceParam(LFO1Shape, 1) == LFO2Shape && SourceParam(Seq1Step8, 1) == Seq2Step8);
  assert(std::set<std::string>(sourceKeys.begin(), sourceKeys.end()).size() == SourceCount);
  {
    std::set<int> sources;
    std::string previous;
    for (int source : sourceMenuOrder)
    {
      assert(sources.insert(source).second);
      const auto label = SourceLabel(source, "Test");
      assert(previous <= label);
      previous = label;
      assert(sourceShortNames[source] && std::strlen(sourceShortNames[source]) > 0);
    }
    assert(sources.size() == SourceCount);
    assert(SourceLabel(SourceMacro1, "Test") == "Macro 1: Test");
    assert(SourceLabel(SourceMacro2, "Macro 2") == "Macro 2");
    assert(std::string(sourceShortNames[SourceLFO2]) == "L2");
    assert(std::string(sourceShortNames[SourceEnv2]) == "E2");
    assert(std::string(sourceShortNames[SourceSeq2]) == "S2");
  }

  // Only dynamic sources that actually contribute through a nonzero-depth row
  // need audio-rate control quanta. A dynamic Via is a dependency too.
  {
    Modulation sources;
    auto v = Defaults();
    assert(!sources.Active(v) && !sources.DynamicAssignments(v));
    v[RowParam(0, RowSource)] = SourceMacro1; v[RowParam(0, RowDepth)] = 50.;
    assert(sources.Active(v) && !sources.DynamicAssignments(v));
    v[RowParam(0, RowVia)] = SourceMacro2;
    assert(!sources.DynamicAssignments(v));
    for (int source : sourceTabs)
    {
      v[RowParam(0, RowVia)] = source;
      assert(sources.DynamicAssignments(v));
      v[RowParam(0, RowDepth)] = 0.;
      assert(!sources.DynamicAssignments(v));
      v[RowParam(0, RowDepth)] = 50.;
      v[RowParam(0, RowSource)] = source; v[RowParam(0, RowVia)] = SourceNone;
      assert(sources.DynamicAssignments(v));
      v[RowParam(0, RowSource)] = SourceMacro1;
    }
  }

  // Only destinations that depend on a changing source bound the independent
  // input prefix. Via dependencies count; macros, zero depth and invalid rows do not.
  {
    auto v = Defaults();
    assert(Matrix::IndependentInputPrefix(v) == 4);
    v[RowParam(0, RowSource)] = SourceMacro1; v[RowParam(0, RowDepth)] = 50.;
    v[RowParam(0, RowSlot)] = 0; v[RowParam(0, RowDestination)] = parameter_layout::kSlotIn;
    assert(Matrix::IndependentInputPrefix(v) == 4);
    v[RowParam(0, RowVia)] = SourceEnv2;
    assert(Matrix::IndependentInputPrefix(v) == 0);
    for (int slot = 0; slot <= gridSlot; ++slot)
    {
      v[RowParam(0, RowSlot)] = slot;
      v[RowParam(0, RowDestination)] = slot == gridSlot ? int(GridBlend) : int(parameter_layout::kSlotIn);
      assert(Matrix::IndependentInputPrefix(v) == std::min(4, slot));
    }
    v[RowParam(0, RowSlot)] = 0; v[RowParam(0, RowDepth)] = 0.;
    assert(Matrix::IndependentInputPrefix(v) == 4);
    v[RowParam(0, RowDepth)] = 50.; v[RowParam(0, RowDestination)] = parameter_layout::kSlotEnabled;
    assert(Matrix::IndependentInputPrefix(v) == 4);
    v[RowParam(1, RowSource)] = SourceSeq2; v[RowParam(1, RowDepth)] = 10.;
    v[RowParam(1, RowSlot)] = 2; v[RowParam(1, RowDestination)] = parameter_layout::kSlotTrim;
    assert(Matrix::IndependentInputPrefix(v) == 2);
  }

  // Control timing scales within host-rate families without losing buffer alignment.
  // A countdown also preserves the schedule across arbitrary callback boundaries.
  {
    const std::array<double, 7> rates{1000., 44100., 48000., 88200., 96000., 176400., 192000.};
    const std::array<int, 7> periods{1, 16, 16, 32, 32, 64, 64};
    const auto v = Defaults();
    std::array<double, 64> silence{};
    for (size_t i = 0; i < rates.size(); ++i)
    {
      Modulation sources; sources.Reset(rates[i]);
      const int q = periods[i];
      assert(sources.Quantum() == q);
      sources.Advance(silence.data(), nullptr, q - 1, v, 120.);
      assert(sources.Quantum() == 1 && sources.Read().phase == 0.);
      sources.Advance(silence.data(), nullptr, 1, v, 120.);
      assert(sources.Quantum() == q && sources.Read().phase > 0.);
      sources.Reset(rates[i]);
      assert(sources.Quantum() == q && !sources.Read().running && sources.Value(SourceLFO1, v) == 0.);
    }
    // Exact 2x/4x host-rate multiples publish the same physical LFO/envelope
    // state at each control tick, rather than publishing more often.
    auto shaped = Defaults(); shaped[LFO1Rate] = 7.3; shaped[LFO2Rate] = 3.7;
    shaped[Env1Attack] = shaped[Env2Attack] = 5.;
    Modulation base, high; base.Reset(48000.);
    std::array<double, 64> input{}; input.fill(.2);
    for (const auto [baseRate, rate] : {std::pair{44100., 88200.}, std::pair{44100., 176400.},
                                       std::pair{48000., 96000.}, std::pair{48000., 192000.}})
    {
      base.Reset(baseRate); high.Reset(rate);
      for (int tick = 0; tick < 331; ++tick)
      {
        base.Advance(input.data(), nullptr, base.Quantum(), shaped, 120.);
        high.Advance(input.data(), nullptr, high.Quantum(), shaped, 120.);
        for (int source : sourceTabs)
          assert(std::abs(base.Value(source, shaped) - high.Value(source, shaped)) < 1.e-10);
      }
    }
  }

  // Macro-only whole callbacks preserve unused source histories for a later
  // dynamic assignment, including MIDI/onset retriggers and odd block lengths.
  {
    for (double rate : {44100., 48000., 96000., 192000.})
    for (int retrigger : {RetriggerMidi, RetriggerOnset})
    {
      auto v = Defaults();
      v[LFO1Retrigger] = v[LFO2Retrigger] = v[Seq1Retrigger] = v[Seq2Retrigger] = retrigger;
      v[LFO1Rate] = 10.; v[LFO2Rate] = 7.; v[LFO1Shape] = v[LFO2Shape] = 5.;
      std::array<double, 257> signal{}, second{};
      for (int i = 0; i < 257; ++i) { signal[i] = i > 53 ? .3 : 0.; second[i] = i > 91 ? .7 : 0.; }
      Modulation whole, chunks; whole.Reset(rate); chunks.Reset(rate);
      const int notes[] = {0, 15, 16, 127, 256};
      for (int block = 0; block < 20; ++block)
      {
        whole.Advance(signal.data(), nullptr, 257, v, 120., notes, 5, second.data());
        for (int offset = 0; offset < 257;)
        {
          const int count = std::min(chunks.Quantum(), 257 - offset);
          std::vector<int> local;
          for (int note : notes) if (note >= offset && note < offset + count) local.push_back(note - offset);
          chunks.Advance(signal.data() + offset, nullptr, count, v, 120., local.data(), static_cast<int>(local.size()), second.data() + offset);
          offset += count;
        }
        assert(whole.Quantum() == chunks.Quantum());
        for (int source : sourceTabs) assert(whole.Value(source, v) == chunks.Value(source, v));
      }
    }
  }

  // Different clocks, shapes, follower signals and step patterns remain independent.
  {
    auto v = Defaults();
    v[LFO1Rate] = 1.; v[LFO2Rate] = 2.; v[LFO1Shape] = 2.; v[LFO2Shape] = 3.;
    v[Seq1Sync] = v[Seq2Sync] = 0.; v[Seq1Rate] = 10.; v[Seq2Rate] = 20.;
    v[Seq1Step2] = 25.; v[Seq2Step3] = 90.;
    v[Env1Sensitivity] = v[Env2Sensitivity] = 0.;
    std::array<double, 16> loud{}, quiet{}; loud.fill(.5);
    Modulation sources; sources.Reset(48000.);
    for (int i = 0; i < 330; ++i)
      sources.Advance(loud.data(), nullptr, 16, v, 120., nullptr, 0, quiet.data());
    const auto monitor = sources.Read();
    assert(std::abs(monitor.phase - .11) < 1.e-6 && std::abs(monitor.second.phase - .22) < 1.e-6);
    assert(std::abs(sources.Value(SourceLFO1, v) + .78) < 1.e-9);
    assert(std::abs(sources.Value(SourceLFO2, v) - .56) < 1.e-9);
    assert(sources.Value(SourceEnv1, v) > .49 && sources.Value(SourceEnv2, v) == 0.);
    assert(monitor.step == 1 && monitor.second.step == 2);
    assert(sources.Value(SourceSeq1, v) == -.5 && std::abs(sources.Value(SourceSeq2, v) - .8) < 1.e-12);
    for (int source : sourceTabs)
    {
      double live = 0.;
      assert(LiveValue(source, v, monitor, live));
      assert(std::abs(live - sources.Value(source, v)) < 1.e-6);
    }
    assert(Bipolar(SourceLFO2) && Bipolar(SourceSeq2) && !Bipolar(SourceEnv2));
    assert(Scale(SourceLFO2, 0.) == .5);
    v[RowParam(0, RowSource)] = SourceLFO2; v[RowParam(0, RowVia)] = SourceEnv1;
    v[RowParam(0, RowSlot)] = gridSlot; v[RowParam(0, RowDestination)] = GridBlend; v[RowParam(0, RowDepth)] = 50.;
    Matrix::Targets targets;
    assert(Matrix::Collect(v, sources, targets) == 1);
    assert(std::abs(targets[0].offset - .14) < 1.e-4);
    const auto ring = DestinationRing(v, monitor, gridSlot, GridBlend);
    assert(ring.low == -.5 && ring.high == .5 && ring.showLive && std::abs(ring.live - targets[0].offset) < 1.e-6);
    // MIDI restarts bank 2 only, while bank 1 keeps its original phase.
    v[LFO2Retrigger] = v[Seq2Retrigger] = RetriggerMidi;
    const int note = 0;
    sources.Advance(loud.data(), nullptr, 16, v, 120., &note, 1, quiet.data());
    assert(sources.Read().phase > .1f && sources.Read().second.phase < .001f && sources.Read().second.step == 0);
    sources.Idle();
    assert(!sources.Read().running && !sources.Read().second.running);
  }

  // Collection: inactive and invalid rows are skipped; shared destinations add; Via scales.
  {
    Modulation sources; sources.Reset(48000.);
    auto v = Defaults();
    v[Macro1] = 100.; v[Macro2] = 50.; v[Macro3] = 0.;
    const auto set = [&](int row, int source, int slot, int control, double depth, int via = SourceNone) {
      v[RowParam(row, RowSource)] = source; v[RowParam(row, RowSlot)] = slot; v[RowParam(row, RowDestination)] = control;
      v[RowParam(row, RowDepth)] = depth; v[RowParam(row, RowVia)] = via;
    };
    set(0, SourceMacro1, 4, standardControls + uDelayMix, 20.);
    set(1, SourceMacro2, 4, standardControls + uDelayMix, 40.);
    set(2, SourceMacro1, 4, parameter_layout::kSlotEnabled, 50.); // Not modulatable.
    set(3, SourceMacro1, 9, standardControls + uReverbMix, 0.);   // Depth 0: inactive.
    set(4, SourceMacro1, gridSlot, GridLaneB, -30., SourceMacro2);
    set(5, SourceMacro1, 2, parameter_layout::kSlotIn, 30., SourceMacro3);
    assert(sources.Active(v));
    Matrix::Targets targets;
    const int count = Matrix::Collect(v, sources, targets);
    assert(count == 3);
    assert(targets[0].slot == 4 && targets[0].control == standardControls + uDelayMix && std::abs(targets[0].offset - .4) < 1.e-12);
    assert(targets[1].slot == gridSlot && std::abs(targets[1].offset + .15) < 1.e-12);
    assert(targets[2].slot == 2 && targets[2].offset == 0.);
    // Restore returns only modulated fields to their host values.
    std::array<FixedNAMGrid::Controls, 16> base{}, animated{};
    base[4].utility.delay[4] = 33.; animated[4].utility.delay[4] = 90.; animated[4].utility.delay[0] = 123.;
    Matrix::Restore(animated, base, targets, count);
    assert(animated[4].utility.delay[4] == 33. && animated[4].utility.delay[0] == 123.);
    // A bipolar Via centres: the LFO at zero halves the row.
    assert(Scale(SourceLFO, 0.) == .5 && Scale(SourceMacro1, .25) == .25);
    // The ring spans the reach of every row on a destination.
    Modulation::Monitor monitor;
    const auto ring = DestinationRing(v, monitor, 4, standardControls + uDelayMix);
    assert(ring.rows == 2 && ring.source == SourceMacro1 && ring.low == 0. && std::abs(ring.high - .6) < 1.e-12);
    assert(ring.showLive && std::abs(ring.live - .4) < 1.e-12);
    set(6, SourceLFO, 4, standardControls + uDelayMix, 10.);
    const auto bipolar = DestinationRing(v, monitor, 4, standardControls + uDelayMix);
    assert(std::abs(bipolar.low + .1) < 1.e-12 && std::abs(bipolar.high - .7) < 1.e-12 && !bipolar.showLive);
  }
  // LFO shapes and the held Random value.
  {
    assert(std::abs(LFOWave(0, .25) - 1.) < 1.e-12 && std::abs(LFOWave(1, .25) - 1.) < 1.e-12);
    assert(std::abs(LFOWave(1, .75) + 1.) < 1.e-12 && std::abs(LFOWave(1, 0.)) < 1.e-12);
    assert(LFOWave(2, 0.) == -1. && LFOWave(3, 0.) == 1. && LFOWave(4, .2) == 1. && LFOWave(4, .7) == -1.);
    Modulation sources; sources.Reset(48000.);
    auto v = Defaults();
    v[LFOShape] = 5.; v[LFORate] = 10.; v[RowParam(0, RowSource)] = SourceLFO; v[RowParam(0, RowDepth)] = 50.;
    std::array<double, 64> silence{};
    std::set<double> held;
    for (int block = 0; block < 1500; ++block)
    {
      sources.Advance(silence.data(), nullptr, 64, v, 120.);
      const double value = sources.Value(SourceLFO, v);
      assert(value >= -1. && value <= 1.);
      held.insert(value);
    }
    // Two seconds at 10 Hz: about twenty held values, constant within each cycle.
    assert(held.size() >= 18 && held.size() <= 22);
    assert(sources.Read().running);
    sources.Idle();
    assert(!sources.Read().running);
  }
  // Follower shaping: the defaults keep the plain level times Sensitivity.
  {
    auto v = Defaults();
    v[EnvSensitivity] = 6.;
    const double gain = std::pow(10., 6. * .05);
    assert(Modulation::ShapeEnvelope(.1, v) == std::clamp(.1 * gain, 0., 1.) && Modulation::ShapeEnvelope(.9, v) == 1.);
    v[EnvThreshold] = -20.; // .1 linear
    assert(Modulation::ShapeEnvelope(.09, v) == 0. && std::abs(Modulation::ShapeEnvelope(.2, v) - .1 * gain) < 1.e-12);
    v[EnvThreshold] = kThresholdOff; v[EnvCurve] = 50.; // exponent 1/2
    assert(std::abs(Modulation::ShapeEnvelope(.125, v) - std::sqrt(.125 * gain)) < 1.e-12);
    v[EnvCurve] = -50.;
    assert(std::abs(Modulation::ShapeEnvelope(.125, v) - std::pow(.125 * gain, 2.)) < 1.e-12);
  }
  // Retrigger: onsets and MIDI notes restart the LFO and sequencer; one shot holds its end.
  {
    const double rate = 48000.;
    std::vector<double> quiet(4800, 0.), pluck(4800, 0.);
    for (size_t i = 0; i < pluck.size(); ++i) pluck[i] = .5 * std::exp(-static_cast<double>(i) / 2000.) * std::sin(i * .05);
    auto v = Defaults();
    v[LFORate] = 1.; v[LFOShape] = 2.; v[StepRate] = 15.; v[StepSync] = 0.;
    const auto run = [&](Modulation& m, const std::vector<double>& signal, std::vector<int> midi = {}) {
      for (size_t at = 0; at < signal.size(); at += 16)
      {
        std::vector<int> notes;
        for (const int frame : midi)
          if (frame >= static_cast<int>(at) && frame < static_cast<int>(at) + 16) notes.push_back(frame - static_cast<int>(at));
        m.Advance(signal.data() + at, nullptr, 16, v, 120., notes.data(), static_cast<int>(notes.size()));
      }
    };
    // Free running ignores onsets: a 1 Hz saw is 0.1 into its cycle after 0.1 s.
    Modulation free; free.Reset(rate);
    run(free, pluck);
    assert(std::abs(free.Read().phase - .1) < .01);
    v[LFORetrigger] = RetriggerOnset; v[StepRetrigger] = RetriggerOnset;
    Modulation onset; onset.Reset(rate);
    run(onset, quiet); run(onset, quiet); // 0.2 s in, then a plucked note restarts both.
    run(onset, pluck);
    assert(onset.Read().phase < .101 && onset.Read().phase > .09 && onset.Read().step == 1); // 0.1 s after the attack.
    // Below the threshold nothing restarts.
    v[EnvThreshold] = -3.;
    Modulation gated; gated.Reset(rate);
    run(gated, quiet); run(gated, quiet); run(gated, pluck);
    assert(gated.Read().phase > .29);
    v[EnvThreshold] = kThresholdOff;
    v[LFORetrigger] = RetriggerMidi; v[StepRetrigger] = RetriggerMidi;
    Modulation midi; midi.Reset(rate);
    run(midi, quiet, {800, 1600}); // note-ons at 17 and 33 ms: the last one counts
    assert(std::abs(midi.Read().phase - (4800 - 1600) / rate) < .001 && midi.Read().step == 1);
    // One shot: a 1 Hz Saw down from the note, then held at its end.
    v[LFOOneShot] = 1.; v[LFOShape] = 3.;
    Modulation shot; shot.Reset(rate);
    run(shot, quiet);
    assert(shot.Value(SourceLFO, v) == -1.); // Idle until the first trigger.
    run(shot, quiet, {0});
    assert(std::abs(shot.Value(SourceLFO, v) - (1. - 2. * .1)) < .01);
    for (int i = 0; i < 10; ++i) run(shot, quiet);
    assert(shot.Value(SourceLFO, v) == -1.);
  }
  // The follower can listen after any slot: the tap copies that slot's output. Lane A's
  // level applies after its slots, so a tap in lane A hears the signal before it.
  {
    FixedNAMGrid grid; grid.ResetStopped(48000., 64);
    std::array<FixedNAMGrid::Controls, 16> controls{};
    Routing routing; routing.gainA = -20.;
    for (int slot : {5, 13})
    {
      std::array<double, 64> l{}, r{};
      grid.SetTap(slot);
      l.fill(.5); r.fill(.25);
      grid.Process(l.data(), 64, controls, 0., r.data(), routing);
      assert(grid.TapRight() != nullptr && std::abs(l[63] - .05) < 1.e-9);
      const double expected = slot == 13 ? .05 : .5;
      assert(std::abs(grid.TapLeft()[63] - expected) < 1.e-9 && std::abs(grid.TapRight()[63] - expected * .5) < 1.e-9);
    }
  }
  // Destination lists use each module's own knob labels, in editor order.
  {
    const auto delay = Destinations(SlotEffect::DigitalDelay, false, false, false, false);
    assert(!delay.empty() && std::is_sorted(delay.begin(), delay.end(), [](const auto& a, const auto& b) { return a.position < b.position; }));
    const auto has = [](const auto& list, int control) {
      return std::any_of(list.begin(), list.end(), [&](const auto& d) { return d.control == control; });
    };
    assert(has(delay, standardControls + uDelayMix) && has(delay, standardControls + uDelayTime));
    assert(!has(delay, standardControls + uTempoSync) && !has(delay, standardControls + uDelayPingPong));
    assert(!has(Destinations(SlotEffect::DigitalDelay, false, true, false, false), standardControls + uDelayTime));
    const auto nam = Destinations(SlotEffect::NAM, true, false, false, false);
    assert(nam.size() == 7 && nam.front().control == parameter_layout::kSlotIn && has(nam, parameter_layout::kSlotTrim));
    assert(Destinations(SlotEffect::NAM, false, false, false, false).empty());
    const auto cuts = Destinations(SlotEffect::ParametricEQ, true, false, false, false);
    const auto shelf = Destinations(SlotEffect::ParametricEQ, true, false, false, false, true);
    assert(cuts.size() == 7 && shelf.size() == 8);
    assert(cuts[3].control == parameter_layout::kSlotEQHighPass && cuts[3].position == 4);
    assert(cuts[5].control == parameter_layout::kSlotEQLowPass && cuts[5].position == 6);
    assert(std::string(shelf[4].label) == "Low shelf" && shelf[4].position == 4);
    const auto ir = Destinations(SlotEffect::CabinetIR, true, false, false, false);
    assert(ir.front().control == parameter_layout::standardControls + uIRInput);
    assert(ir.size() == 6);
    assert(ir[4].control == parameter_layout::kSlotIRLevel && ir.back().control == parameter_layout::standardControls + uIRMix);
    for (int e = 0; e <= static_cast<int>(SlotEffect::Last); ++e)
      if (IsValidSlotEffect(static_cast<SlotEffect>(e)))
        for (const auto& d : Destinations(static_cast<SlotEffect>(e), true, false, false, false))
          assert(Modulatable(d.control) && d.label && *d.label);
    assert(std::string(ControlName(standardControls + uDelayMix)) == "Delay Mix");
  }
  std::cout << "Modulation matrix: field map, destinations, 0.1 conversion, collection, rings, LFO shapes, follower shaping, retrigger and tap passed\n";
}
