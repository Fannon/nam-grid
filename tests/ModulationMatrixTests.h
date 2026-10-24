#pragma once
#include "ModulationMatrix.h"
#include <set>
#include <tuple>

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
    assert(StateOnly(i) == (i >= ModRow && (i - ModRow) % RowFields != RowDepth));

  // Fixed 0.1 targets become rows with the same physical movement.
  {
    std::array<SlotEffect, gridSlot> effects{};
    effects[6] = SlotEffect::AirHall;
    auto v = Defaults();
    v[LFOSlot] = 5.; v[LFOTarget] = 2.; v[LFODepth] = 60.;
    v[EnvSlot] = 6.; v[EnvTarget] = 5.; v[EnvDepth] = -30.;
    v[Macro2Slot] = 3.; v[Macro2Target] = 7.; v[Macro2Depth] = 20.;
    v[StepTarget] = 0.; v[StepDepth] = 50.; // No target: stays off.
    assert(ConvertLegacy(v, effects, true));
    assert(v[LFOTarget] == 0. && v[EnvTarget] == 0. && v[Macro2Target] == 0.);
    assert(v[RowParam(0, RowSource)] == SourceLFO && v[RowParam(0, RowSlot)] == 5.);
    assert(v[RowParam(0, RowDestination)] == standardControls + uFilterCutoff);
    // Six octaves across 40..12000 Hz: 60% of six octaves is 3.6 octaves of an 8.23-octave range.
    assert(std::abs(v[RowParam(0, RowDepth)] - 60. * 6. / std::log2(300.)) < 1.e-12);
    assert(v[RowParam(1, RowSource)] == SourceEnvelope && v[RowParam(1, RowDestination)] == standardControls + uAirHallMix);
    assert(v[RowParam(1, RowDepth)] == -30.);
    assert(v[RowParam(2, RowSource)] == SourceMacro2 && v[RowParam(2, RowSlot)] == gridSlot && v[RowParam(2, RowDestination)] == GridBlend);
    assert(v[RowParam(3, RowSource)] == SourceNone);
    // Balance, trim and ring frequency scales.
    for (const auto& [target, control, scale] : {std::tuple{3, standardControls + uUtilityBalance, .5},
                                                 std::tuple{6, static_cast<int>(parameter_layout::kSlotTrim), .3},
                                                 std::tuple{8, standardControls + uRingFrequency, 4. / std::log2(5000.)}})
    {
      const auto row = LegacyRow(target, 1, SlotEffect::Utility);
      assert(row.slot == 1 && row.control == control && std::abs(row.scale - scale) < 1.e-12);
    }
    // A live conversion keeps the user's rows and takes the next free one.
    auto live = Defaults();
    live[RowParam(0, RowSource)] = SourceMacro3; live[RowParam(0, RowDepth)] = 10.;
    live[Macro1Target] = 1.; live[Macro1Depth] = 40.; live[Macro1Slot] = 2.;
    assert(ConvertLegacy(live, effects, false));
    assert(live[RowParam(0, RowSource)] == SourceMacro3 && live[RowParam(1, RowSource)] == SourceMacro1);
    assert(live[RowParam(1, RowDestination)] == standardControls + uWahPosition && live[RowParam(1, RowDepth)] == 40.);
    assert(!ConvertLegacy(live, effects, false));
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
    assert(Destinations(SlotEffect::CabinetIR, true, false, false, false).front().control == parameter_layout::kSlotIRLevel);
    for (int e = 0; e <= static_cast<int>(SlotEffect::Last); ++e)
      if (IsValidSlotEffect(static_cast<SlotEffect>(e)))
        for (const auto& d : Destinations(static_cast<SlotEffect>(e), true, false, false, false))
          assert(Modulatable(d.control) && d.label && *d.label);
    assert(std::string(ControlName(standardControls + uDelayMix)) == "Delay Mix");
  }
  std::cout << "Modulation matrix: field map, destinations, 0.1 conversion, collection, rings and LFO shapes passed\n";
}
