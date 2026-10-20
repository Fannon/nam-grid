#pragma once
#include "SlotOperations.h"

void TestSlotFeatures()
{
  const auto directory = std::filesystem::current_path() / "slot-fixtures";
  std::filesystem::create_directories(directory);
  const auto modelFile = directory / "packed-a2.nam", irFile = directory / "cabinet.wav";
  std::ofstream(modelFile) << fixture::packed().dump();
  {
    std::ofstream wav(irFile, std::ios::binary);
    auto number = [&](uint32_t value, int bytes) {
      for (int b = 0; b < bytes; ++b)
        wav.put(static_cast<char>((value >> (8 * b)) & 255));
    };
    wav.write("RIFF", 4);
    number(42, 4);
    wav.write("WAVEfmt ", 8);
    number(16, 4);
    number(1, 2);
    number(1, 2);
    number(48000, 4);
    number(96000, 4);
    number(2, 2);
    number(16, 2);
    wav.write("data", 4);
    number(6, 4);
    number(16384, 2);
    number(8192, 2);
    number(61440, 2);
  }
  for (double rate : {44100., 48000., 96000.})
  {
    for (bool cuts : {false, true})
    {
      // Flat EQ is unity; parameter ramps give identical samples under any block partition.
      SlotEQ single, blocked;
      single.Reset(rate);
      blocked.Reset(rate);
      EQControls c;
      std::array<double, 512> x{}, y{};
      allocations = deletions = 0;
      for (int b = 0; b < 160; ++b)
      {
        if (b == 40)
        {
          c.bass = 8.;
          c.mid = -7.;
          c.treble = 6.;
          c.highPass = 120.;
          c.lowPass = 5000.;
          c.presence = -4.;
        }
        if (b == 100)
          c = {};
        const int n = std::array<int, 6>{1, 3, 64, 127, 256, 512}[b % 6];
        for (int i = 0; i < n; ++i)
          x[i] = y[i] = .2 * std::sin((b * 512 + i) * .04);
        tracking = true;
        blocked.Process(y.data(), n, cuts, c);
        for (int i = 0; i < n; ++i)
          single.Process(&x[i], 1, cuts, c);
        tracking = false;
        for (int i = 0; i < n; ++i)
        {
          assert(std::isfinite(y[i]) && std::abs(y[i]) < 10. && x[i] == y[i]);
          if (b < 40)
            assert(std::abs(y[i] - .2 * std::sin((b * 512 + i) * .04)) < 1.e-9);
        }
      }
      assert(allocations == 0 && deletions == 0);
      // Measure the frequency response: each tone control boosts its intended band.
      const auto magnitude = [&](EQControls settings, double hz) {
        SlotEQ eq;
        eq.Reset(rate);
        double energy = 0.;
        for (int i = 0; i < static_cast<int>(rate); ++i)
        {
          double sample = std::sin(6.283185307179586 * hz * i / rate);
          eq.Process(&sample, 1, cuts, settings);
          if (i >= rate / 2)
            energy += sample * sample;
        }
        return std::sqrt(energy / (static_cast<int>(rate) - static_cast<int>(rate / 2)) * 2.);
      };
      if (cuts)
      {
        c = {};
        c.highPass = 200.;
        assert(magnitude(c, 20.) < .11);
        assert(std::abs(magnitude(c, 200.) - std::sqrt(.5)) < .002);
        c = {};
        c.lowPass = 2000.;
        assert(magnitude(c, 10000.) < .25);
        assert(std::abs(magnitude(c, 2000.) - std::sqrt(.5)) < .002);
        c = {};
        c.presence = 6.;
        assert(magnitude(c, 10000.) > 1.7);
      }
      else
      {
        c = {};
        c.bass = 6.;
        assert(magnitude(c, 30.) > 1.9);
        c = {};
        c.mid = 6.;
        assert(magnitude(c, 800.) > 1.95);
        c = {};
        c.treble = 6.;
        assert(magnitude(c, 10000.) > 1.9);
      }
    }
    // Per-NAM tone EQ follows inference, precedes output trim, and adds no allocation or delay.
    NAMStage toneStage;
    toneStage.Publish(prepare(fixture::model(3, 123), rate));
    toneStage.ResetStopped(rate, 512);
    auto referenceModel = prepare(fixture::model(3, 123), rate);
    SlotEQ referenceEQ, referenceCuts;
    referenceCuts.Reset(rate);
    referenceCuts.Process(nullptr, 0, true, {});
    referenceEQ.Reset(rate);
    referenceEQ.Process(nullptr, 0, false, {});
    EQControls tone;
    std::array<double, 512> actual{}, referenceInput{}, referenceOutput{};
    for (int block = 0; block < 80; ++block)
    {
      if (block == 8)
      {
        tone.bass = 6.;
        tone.mid = -4.;
        tone.treble = 3.;
        tone.highPass = 90.;
        tone.lowPass = 6000.;
      }
      if (block == 45)
        tone = {};
      const int n = std::array<int, 6>{1, 3, 64, 127, 256, 512}[block % 6];
      for (int i = 0; i < n; ++i)
      {
        actual[i] = .1 * std::sin((block * 512 + i) * .04);
        referenceInput[i] = actual[i] * std::pow(10., 6. * .05);
      }
      double* in = referenceInput.data();
      double* out = referenceOutput.data();
      allocations = deletions = 0;
      tracking = true;
      Eigen::internal::set_is_malloc_allowed(false);
      toneStage.Process(actual.data(), n, true, 6., -3., false, 0., 0, 1., {}, tone);
      referenceModel->model->process(&in, &out, n);
      referenceEQ.Process(out, n, false, tone);
      referenceCuts.Process(out, n, true, tone);
      Eigen::internal::set_is_malloc_allowed(true);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      assert(toneStage.Latency() == referenceModel->latency);
      for (int i = 0; i < n; ++i)
        assert(std::abs(actual[i] - out[i] * std::pow(10., -3. * .05)) < 1.e-8);
    }
    if (rate == 48000.)
    {
      tone.bass = 12.;
      for (int b = 0; b < 3; ++b)
      {
        actual.fill(.1);
        toneStage.Process(actual.data(), 512, false, 6., -3., false, 0., 0, 1., {}, tone);
        if (b > 0)
          for (double x : actual)
            assert(x == .1);
      }
    }
    for (const auto effect : {SlotEffect::ParametricEQ, SlotEffect::Boost})
    {
      SlotConfig config;
      config.effect = effect;
      config.values[17] = 1000.;
      config.values[18] = 6.;
      config.values[19] = 1.;
      config.values[20] = 6.;
      auto copied = PrepareSlotEdit(config, SlotConfig{}, SlotEditMode::Copy, rate, 512);
      assert(copied.destination.config.values == config.values);
      NAMStage stage;
      stage.Publish(std::move(copied.destination.dsp));
      stage.ResetStopped(rate, 512);
      UtilityControls settings{1000., 6., 1., 6.};
      EQControls cuts;
      std::array<double, 512> signal{};
      double energyIn = 0., energyOut = 0.;
      int position = 0, block = 0;
      while (position < static_cast<int>(rate))
      {
        const int n =
          std::min(std::array<int, 6>{1, 3, 64, 127, 256, 512}[block++ % 6], static_cast<int>(rate) - position);
        for (int i = 0; i < n; ++i)
        {
          signal[i] = .05 * std::sin(6.283185307179586 * 1000. * (position + i) / rate);
          if (position + i >= rate / 2)
            energyIn += signal[i] * signal[i];
        }
        allocations = deletions = 0;
        tracking = true;
        Eigen::internal::set_is_malloc_allowed(false);
        stage.Process(signal.data(), n, true, 0., 0., false, 0., 0, 1., {}, cuts, settings);
        Eigen::internal::set_is_malloc_allowed(true);
        tracking = false;
        assert(allocations == 0 && deletions == 0 && stage.Latency() == 0);
        for (int i = 0; i < n; ++i)
        {
          assert(std::isfinite(signal[i]));
          if (position + i >= rate / 2)
            energyOut += signal[i] * signal[i];
        }
        position += n;
      }
      assert(std::abs(std::sqrt(energyOut / energyIn) - std::pow(10., 6. * .05)) < .002);
      for (int b = 0; b < 3; ++b)
      {
        signal.fill(.05);
        stage.Process(signal.data(), 512, false, 0., 0., false, 0., 0, 1., {}, cuts, settings);
        if (b > 0)
          for (double x : signal)
            assert(x == .05);
      }
    }
    SlotConfig cab;
    cab.effect = SlotEffect::CabinetIR;
    cab.path = irFile.string();
    cab.values[15] = 0.;
    NAMStage cabinet;
    cabinet.Publish(std::move(PrepareSlot(cab, rate, 512).dsp));
    cabinet.ResetStopped(rate, 512);
    dsp::ImpulseResponse reference(irFile.string().c_str(), rate);
    std::array<double, 512> buffer{}, ref{};
    double* refPointer = ref.data();
    reference.Process(&refPointer, 1, 512);
    EQControls controls;
    controls.irLevel = 0.;
    for (int block = 0; block < 80; ++block)
    {
      const int n = std::array<int, 6>{1, 3, 64, 127, 256, 512}[block % 6];
      for (int i = 0; i < n; ++i)
        buffer[i] = ref[i] = block == 0 && i == 0 ? 1. : 0.;
      allocations = deletions = 0;
      tracking = true;
      Eigen::internal::set_is_malloc_allowed(false);
      cabinet.Process(buffer.data(), n, true, 12., 12., false, 0., 0, 1., {}, controls);
      auto* expected = reference.Process(&refPointer, 1, n);
      Eigen::internal::set_is_malloc_allowed(true);
      tracking = false;
      assert(cabinet.Latency() == 0 && allocations == 0 && deletions == 0);
      for (int i = 0; i < n; ++i)
        assert(std::abs(buffer[i] - expected[0][i] * std::pow(10., 18. * .05)) < 1.e-8);
    }
    cabinet.ResetStopped(rate, 1);
    double sample = 1.;
    tracking = true;
    cabinet.Process(&sample, 1, true, 0., 0., false, 0., 0, 1., {}, controls);
    tracking = false;
    assert(std::isfinite(sample) && allocations == 0 && deletions == 0);
    // Full/Lite are both prepared outside audio and matched to the direct A2 child model.
    for (double quality : {0., 1.})
    {
      SlotConfig model;
      model.path = modelFile.string();
      model.values[16] = quality;
      auto packed = PrepareSlot(model, rate, 512);
      NAMStage stage, direct;
      assert(packed.dsp->Info().slimmable);
      stage.Publish(std::move(packed.dsp));
      stage.ResetStopped(rate, 512);
      direct.Publish(prepare(fixture::model(quality == 0. ? 3 : 8, quality == 0. ? 123 : 456), rate));
      direct.ResetStopped(rate, 512);
      for (int b = 0; b < 80; ++b)
      {
        const int n = std::array<int, 6>{1, 3, 64, 127, 256, 512}[b % 6];
        for (int i = 0; i < n; ++i)
          buffer[i] = ref[i] = .1 * std::sin((b * 512 + i) * .03);
        allocations = deletions = 0;
        tracking = true;
        Eigen::internal::set_is_malloc_allowed(false);
        stage.Process(buffer.data(), n, true, 0., 0., false, 0., 0, quality);
        direct.Process(ref.data(), n, true, 0., 0., false, 0., 0, 1.);
        Eigen::internal::set_is_malloc_allowed(true);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        for (int i = 0; i < n; ++i)
          assert(std::abs(buffer[i] - ref[i]) < 1.e-8);
      }
    }
  }
  // Real model/IR resources and every setting survive transactional copy/move/swap preparation.
  SlotConfig source, destination;
  source.path = modelFile.string();
  source.values[0] = 3.;
  source.values[1] = -6.;
  source.values[16] = 0.;
  destination.effect = SlotEffect::CabinetIR;
  destination.path = irFile.string();
  destination.values[15] = -9.;
  for (const auto mode : {SlotEditMode::Copy, SlotEditMode::Move, SlotEditMode::Swap})
  {
    auto plan = PrepareSlotEdit(source, destination, mode, 48000., 512);
    assert(plan.destination.config.path == source.path && plan.destination.config.values == source.values);
    assert(plan.destination.dsp->model && plan.destination.dsp->Info().slimmable);
    if (mode == SlotEditMode::Copy)
      assert(!plan.source);
    else if (mode == SlotEditMode::Move)
      assert(plan.source->config.path.empty() && !plan.source->dsp->Info().loaded);
    else
      assert(plan.source->dsp->ir && plan.source->config.values == destination.values);
  }
  auto missing = destination;
  missing.path = (directory / "missing.wav").string();
  bool rejected = false;
  try
  {
    auto ignored = PrepareSlotEdit(source, missing, SlotEditMode::Swap, 48000., 512);
  }
  catch (const std::exception&)
  {
    rejected = true;
  }
  assert(rejected && source.path == modelFile.string() && destination.path == irFile.string());
  // Meter taps see slot I/O peaks and gain reduction without changing the audio.
  NAMStage empty;
  empty.ResetStopped(48000., 512);
  double impulse = 1.5;
  empty.Process(&impulse, 1, true, 0., 0., false, 0., 0, 1.);
  auto meter = empty.TakeMeter();
  assert(meter.input == 1.5f && meter.output == 1.5f && meter.reduction == 0.f);
  assert(empty.TakeMeter().input == 0.f);
  NAMStage compressor;
  SlotConfig comp;
  comp.effect = SlotEffect::SmoothComp;
  compressor.Publish(std::move(PrepareSlot(comp, 48000., 512).dsp));
  compressor.ResetStopped(48000., 512);
  std::array<double, 512> loud;
  for (int i = 0; i < 100; ++i)
  {
    loud.fill(.5);
    compressor.Process(loud.data(), 512, true, 0., 0., false, 0., 0, 1.);
  }
  meter = compressor.TakeMeter();
  assert(meter.input == .5f && meter.output <= .5f && meter.reduction > 7.f && meter.reduction < 8.f);
  std::cout << "EQ responses/automation, IR reference/reset, A2 Full/Lite, slot transactions and meter taps passed\n";
}

void TestRealCaptureQuality(const char* path)
{
  for (double quality : {0., 1.})
  {
    SlotConfig config;
    config.path = path;
    config.values[16] = quality;
    auto prepared = PrepareSlot(config, 48000., 512);
    assert(prepared.dsp->Info().slimmable);
    NAMStage stage;
    stage.Publish(std::move(prepared.dsp));
    stage.ResetStopped(48000., 512);
    std::array<double, 512> audio{};
    double energy = 0.;
    allocations = deletions = 0;
    for (int b = 0; b < 120; ++b)
    {
      const int n = std::array<int, 6>{1, 3, 64, 127, 256, 512}[b % 6];
      for (int i = 0; i < n; ++i)
        audio[i] = .05 * std::sin((b * 512 + i) * .03);
      tracking = true;
      Eigen::internal::set_is_malloc_allowed(false);
      stage.Process(audio.data(), n, true, 0., 0., false, 0., 0, 1.);
      Eigen::internal::set_is_malloc_allowed(true);
      tracking = false;
      for (int i = 0; i < n; ++i)
      {
        assert(std::isfinite(audio[i]));
        energy += audio[i] * audio[i];
      }
    }
    assert(energy > 1.e-12 && allocations == 0 && deletions == 0);
    std::cout << "Real capture " << (quality == 0. ? "Lite" : "Full")
              << ": finite nonzero output, no audio allocations\n";
  }
}
