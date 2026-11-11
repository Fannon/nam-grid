#pragma once

void TestNoisePrintRefinements()
{
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    noise_print::Processor clean, contaminated, stereo;
    clean.Reset(rate); contaminated.Reset(rate); stereo.Reset(rate);
    noise_print::Controls c;
    c.learn = true;
    c.protectAttack = 100.;
    uint32_t seed = 20261125;
    for (int i = 0; i < static_cast<int>(2. * rate); ++i)
    {
      seed = seed * 1664525u + 1013904223u;
      const double t = i / rate;
      const double bed = .002 * (double(seed) / 4294967296. * 2. - 1.) + .0005 * std::sin(6.283185307179586 * 50. * t);
      const double age = std::max(0., t - .85);
      const double pluck = t >= .85 ? .03 * std::exp(-age / .065) * std::sin(6.283185307179586 * 196. * age) : 0.;
      double a = bed, b = bed + pluck, l = b, r = -b;
      allocations = deletions = 0; tracking = true;
      clean.Process(&a, nullptr, 1, c);
      contaminated.Process(&b, nullptr, 1, c);
      stereo.Process(&l, &r, 1, c);
      tracking = false;
      assert(!allocations && !deletions);
      assert(a == bed && b == bed + pluck && l == b && r == -b);
    }
    c.learn = false;
    double a = 0., b = 0., l = 0., r = 0.;
    allocations = deletions = 0; tracking = true;
    clean.Process(&a, nullptr, 1, c);
    contaminated.Process(&b, nullptr, 1, c);
    stereo.Process(&l, &r, 1, c);
    tracking = false;
    assert(!allocations && !deletions);
    double reference = 0., captured = 0.;
    for (int k = 0; k < noise_print::bands; ++k)
    {
      reference += clean.GetProfile().power[k];
      captured += contaminated.GetProfile().power[k];
    }
    assert(clean.GetProfile().valid && contaminated.GetProfile().valid);
    assert(captured < 2. * reference); // A brief accidental pluck must not double the noise estimate.
    assert(stereo.GetProfile() == contaminated.GetProfile());
    // If outlier rejection leaves only near-silence, preserve a useful print.
    const auto prior = clean.GetProfile();
    clean.Reset(rate);
    c.learn = true;
    for (int i = 0; i < static_cast<int>(2. * rate); ++i)
    {
      double sample = i == static_cast<int>(.5 * rate) ? .0001 : 0.;
      clean.Process(&sample, nullptr, 1, c);
    }
    c.learn = false;
    a = 0.;
    clean.Process(&a, nullptr, 1, c);
    assert(clean.GetProfile() == prior);
  }
  // Long captures coalesce bounded buckets without changing time weighting.
  noise_print::RobustCapture capture;
  capture.Reset(1000.);
  std::array<std::array<double, noise_print::bands>, 2> split{};
  split[0].fill(.25); split[1].fill(-.25);
  std::array<double, noise_print::bands> fallback{};
  for (int i = 0; i < 13000; ++i)
  {
    for (int k = 0; k < noise_print::bands; ++k)
      capture.Accumulate(k, split[0][k] * split[0][k]);
    capture.Advance();
  }
  for (double power : capture.Finish(fallback))
    assert(power == .0625);

  // Target the opening error on repeated modest plucks. Both processors learn
  // exactly the same noise; protection changes opening, not the learned print.
  noise_print::Processor legacy, assisted;
  legacy.Reset(48000.); assisted.Reset(48000.);
  noise_print::Controls old, current;
  old.protectAttack = 0.; // The original opening response.
  current.protectAttack = 100.;
  uint32_t seed = 20261126;
  double oldError = 0., newError = 0.;
  for (int i = 0; i < 7 * 48000; ++i)
  {
    const double t = i / 48000., age = std::fmod(t, .5);
    double clean = 0.;
    if (t >= 2. && t < 6.)
      for (int h = 1; h <= 8; ++h)
        clean += .02 * std::min(age / .001, 1.) * std::exp(-age / .035)
                 * std::sin(6.283185307179586 * 110. * h * age) / std::pow(double(h), 1.5);
    seed = seed * 1664525u + 1013904223u;
    const double noise = .002 * (double(seed) / 4294967296. * 2. - 1.) + .002 * std::sin(6.283185307179586 * 60. * t);
    double a = clean + noise, b = a;
    old.learn = current.learn = t < 1.;
    legacy.Process(&a, nullptr, 1, old);
    assisted.Process(&b, nullptr, 1, current);
    if (t >= 2. && t < 6. && age < .020)
    {
      oldError += (a - clean) * (a - clean);
      newError += (b - clean) * (b - clean);
    }
  }
  std::cout << "Smart attack protection: opening error " << oldError << " -> " << newError << '\n';
  assert(newError < .85 * oldError);
  assert(legacy.GetProfile() == assisted.GetProfile());

  noise_print::Processor whole, partitioned;
  whole.Reset(48000.); partitioned.Reset(48000.);
  whole.Restore(legacy.GetProfile()); partitioned.Restore(legacy.GetProfile());
  noise_print::Controls c;
  c.protectAttack = 100.; c.trackingMode = 1;
  std::array<double, 128> a{}, b{}, l{}, r{};
  for (int block = 0; block < 1500; ++block)
  {
    for (int i = 0; i < 128; ++i)
    {
      seed = seed * 1664525u + 1013904223u;
      const int position = block * 128 + i;
      a[i] = b[i] = (position % 12000 < 1200 ? .03 : .002) * (double(seed) / 4294967296. * 2. - 1.);
      l[i] = r[i] = -a[i];
    }
    allocations = deletions = 0; tracking = true;
    whole.Process(a.data(), l.data(), 128, c);
    for (int offset = 0; offset < 128; offset += 37)
      partitioned.Process(b.data() + offset, r.data() + offset, std::min(37, 128 - offset), c);
    tracking = false;
    assert(!allocations && !deletions);
    for (int i = 0; i < 128; ++i)
      assert(std::isfinite(a[i]) && a[i] == b[i] && l[i] == r[i] && a[i] == -l[i]);
  }
  assert(noise_print::Controls{}.protectAttack == 100.);
  assert(SlotDefaultValues(SlotEffect::NoisePrint)[17 + uNoiseProtectAttack] == 100.);
}
