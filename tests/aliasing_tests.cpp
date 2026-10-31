#include "SpectralAnalysis.h"
#include "SlotOperations.h"
#include "ModulationMatrix.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <chrono>

namespace
{
using E = SlotEffect;
using Controls = FixedNAMGrid::Controls;
constexpr int count = 8192, block = 128;
void Require(bool condition, const std::string& message)
{
  if (!condition) throw std::runtime_error(message);
}
enum class Interpretation { Linear, Harmonic, Dynamic, Creative, Model };
Interpretation Classify(E effect)
{
  switch (effect)
  {
    case E::CabinetIR: case E::ToneEQ: case E::CutEQ: case E::ParametricEQ:
    case E::Boost: case E::Utility: case E::SignalMonitor: case E::Wah: case E::GraphicEQ: return Interpretation::Linear;
    case E::Distortion: case E::PedalDrive: case E::TubeBoost: case E::Tape:
    case E::ConsoleColour: case E::ConsoleEQ: case E::OutboardChannel:
    case E::Limiter: return Interpretation::Harmonic;
    case E::DynamicEQ: return Interpretation::Dynamic;
    case E::TransientShaper: case E::Multiband: return Interpretation::Dynamic;
    case E::NAM: return Interpretation::Model;
    case E::Bitcrusher: case E::RingMod: case E::Octaver: case E::Doubler:
    case E::FrequencyShifter: case E::Freeze: case E::Stutter: case E::LoFiTape:
    case E::PitchShift: case E::ShimmerReverb: return Interpretation::Creative;
    case E::NoiseGate: case E::NoisePrint: case E::SmoothComp: case E::PunchComp:
    case E::DigitalBlack: case E::ButterComp: case E::PedalComp: case E::SynthFilter: case E::AutoSwell:
    case E::DigitalDelay: case E::AnalogDelay: case E::TapeDelay: case E::MultiHeadDelay:
    case E::ReverseDelay: case E::DualDelay: case E::DiffusedDelay:
    case E::RoomReverb: case E::PlateReverb: case E::CloudReverb: case E::HallReverb:
    case E::SpringReverb: case E::AirPlate: case E::AirHall: case E::GalacticSpace:
    case E::CosmicEcho: case E::Chorus: case E::JunoChorus: case E::Flanger:
    case E::Phaser: case E::Tremolo: case E::HarmonicTremolo: case E::Vibrato: case E::Rotary: case E::UniVibe:
      return Interpretation::Dynamic;
  }
  throw std::runtime_error("New module needs a spectral interpretation");
}
const char* Label(Interpretation type)
{
  switch (type)
  {
    case Interpretation::Linear: return "linear-control";
    case Interpretation::Harmonic: return "harmonic-candidate";
    case Interpretation::Dynamic: return "dynamic-diagnostic";
    case Interpretation::Creative: return "intentional-spectrum";
    case Interpretation::Model: return "capture-dependent";
  }
  return "unknown";
}
Controls Defaults(E effect)
{
  Controls c;
  const auto values = SlotDefaultValues(effect);
  grid_modulation::VisitSlotControls(c, [&](int index, auto& value) {
    value = grid_modulation::FieldValue<std::remove_reference_t<decltype(value)>>(values[index]);
  });
  return c;
}
void Stress(Controls& c, E effect)
{
  auto& u = c.utility;
  // Deliberate, documented stress cases, not all-controls-at-maximum presets.
  if (effect == E::Distortion) { u.distortion[1] = 36.; u.distortion[2] = 12000.; }
  if (effect == E::PedalDrive) { u.pedal[1] = 36.; u.pedal[2] = 12000.; }
  if (effect == E::TubeBoost) u.tube[0] = 24.;
  if (effect == E::Tape) u.tape[0] = 12.;
  if (effect == E::ConsoleColour) u.colour[0] = 18.;
  if (effect == E::OutboardChannel) u.strip[4] = 100.;
  // Exercise nonlinear paths that neutral settings would bypass. In particular,
  // Console EQ is an exact dry return when flat, and the limiter's input can
  // drive substantial peak limiting even when its optional Clip control is off.
  if (effect == E::ConsoleEQ) { u.consoleEQ[0] = 6.; u.consoleEQ[1] = 3.; u.consoleEQ[2] = 6.; }
  if (effect == E::Limiter) { u.extra.limiterInput = 12.; u.limiterClip = 100.; }
  if (effect == E::LoFiTape) u.extra.lofiDrive = 18.;
  if (effect == E::Rotary) u.rotary[4] = 18.;
  if (effect == E::SpringReverb) u.spring[1] = 12.;
  if (effect == E::AnalogDelay || effect == E::TapeDelay || effect == E::MultiHeadDelay)
  { u.delay[3] = 100.; u.delay[4] = 100.; u.delay[0] = 50.; }
}
struct Variant { std::string name; Controls controls; };
std::vector<Variant> Variants(E effect)
{
  const auto defaults = Defaults(effect);
  std::vector<Variant> variants{{"insertion", defaults}};
  // Change one selector at a time; this covers every registered algorithm,
  // including secondary selectors, without an unbounded Cartesian product.
  for (int control = 0; control < kUtilityControls; ++control)
    if (UsesUtility(effect, control))
      for (const auto& mode : AlgorithmIdentities(control))
      {
        auto c = defaults;
        grid_modulation::WriteControl(c, 17 + control, mode.value);
        variants.push_back({std::string(kEffectParameters[control].name) + ":" + mode.key, c});
      }
  return variants;
}
struct Rendered { std::array<std::vector<double>, 2> previous, audio; int latency; double seconds; };
Rendered Render(E effect, const Controls& c, double rate, int bin, double amplitude,
                const std::filesystem::path& capture, bool tiny = false, int resolution = 1, int candidateFactor = 0, bool adaa = true)
{
  const int analysisCount = count * resolution;
  auto p = std::make_unique<NAMStage::Prepared>();
  p->effect = effect;
  if (effect == E::NAM)
    p->model = std::make_unique<ResamplingNAM>(nam::get_dsp(capture), rate);
  if (effect == E::CabinetIR)
  {
    AdaptiveIR::IRData data{{1.f, -.25f, .125f}, rate};
    p->ir = std::make_unique<AdaptiveIR>(data, rate, block, static_cast<AdaptiveIR::Mode>(c.utility.irLength));
  }
  // Publish resets the actual stereo slot processors, including resampling.
  auto* prepared = p.get();
  NAMStage stage;
  stage.Publish(std::move(p));
  stage.ResetStopped(rate, block);
  if (candidateFactor > 0)
  {
    const auto configure = [&](NAMStage::Prepared* state) {
      if (effect == E::TubeBoost) state->tube.Reset(rate, candidateFactor);
      if (effect == E::Tape) state->tape.Reset(rate, candidateFactor);
      if (effect == E::PedalDrive) state->pedal.Reset(rate, adaa);
      if (effect == E::ConsoleColour) state->outboard.Reset(rate, effect, candidateFactor);
    };
    configure(prepared);
    if (prepared->right) configure(prepared->right.get());
  }
  Rendered result{{std::vector<double>(analysisCount), std::vector<double>(analysisCount)},
                  {std::vector<double>(analysisCount), std::vector<double>(analysisCount)}, stage.Latency(), 0.};
  // Half a second of warm-up plus a full preceding analysis window. Slow tails
  // and modulation can remain nonstationary; those are reported diagnostically.
  const int warm = static_cast<int>(rate * .5), end = warm + 2 * analysisCount;
  std::array<double, block> left{}, right{};
  int index = 0;
  for (int position = 0; position < end;)
  {
    const int size = std::min(end - position, tiny ? std::array<int, 4>{1, 3, 64, 127}[index++ % 4] : block);
    for (int i = 0; i < size; ++i)
    {
      const double phase = 2. * spectral_test::pi * bin * double(position + i) / analysisCount;
      left[i] = amplitude * std::sin(phase);
      right[i] = amplitude * .71 * std::sin(phase + .37);
    }
    const auto begin = std::chrono::steady_clock::now();
    stage.Process(left.data(), size, c.enabled, c.inputDB, c.trimDB, c.dynamics, c.eq, c.utility, right.data());
    result.seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    for (int i = 0; i < size; ++i)
    {
      Require(std::isfinite(left[i]) && std::isfinite(right[i]), "Nonfinite output: " + std::string(SlotEffectName(effect)));
      const int offset = position + i - warm;
      if (offset >= 0)
      {
        auto& audio = offset < analysisCount ? result.previous : result.audio;
        const int sample = offset % analysisCount;
        audio[0][sample] = left[i]; audio[1][sample] = right[i];
      }
    }
    position += size;
  }
  result.seconds /= end / rate; // Processing time / rendered audio duration.
  return result;
}
void TestAnalyzer()
{
  constexpr int bin = 1093, alias = count - 5 * bin;
  std::vector<double> clean(count), harmonic(count), folded(count);
  for (int i = 0; i < count; ++i)
  {
    const double phase = 2. * spectral_test::pi * bin * i / count;
    clean[i] = .5 * std::sin(phase);
    harmonic[i] = clean[i] + .1 * std::sin(3. * phase);
    folded[i] = harmonic[i] + .03 * std::sin(5. * phase);
  }
  const auto a = spectral_test::Measure(clean, bin), b = spectral_test::Measure(harmonic, bin), d = spectral_test::Measure(folded, bin);
  Require(a.ResidualDB() < -180. && b.ResidualDB() < -180., "Analyzer misclassifies legitimate harmonics");
  Require(std::abs(d.foldedRMS - .03 / std::sqrt(2.)) < 1.e-10, "Analyzer misses known folded fifth");
  Require(std::abs(spectral_test::ToneAmplitude(folded, alias) - .03) < 1.e-10, "Independent DFT disagrees");
  Require(std::abs(d.rms - std::sqrt((.25 + .01 + .0009) / 2.)) < 1.e-10, "FFT power normalization");
  Require(spectral_test::FoldBin(5 * bin, count) == alias, "Fold calculation");
  Require(spectral_test::Measure(harmonic, bin, 8192).foldedRMS < 1.e-10, "Colliding aliases must be excluded");
  std::vector<double> high(4 * count);
  for (int i = 0; i < 4 * count; ++i)
  {
    const double phase = 2. * spectral_test::pi * bin * i / (4 * count);
    high[i] = .5 * std::sin(phase) + .03 * std::sin(5. * phase);
  }
  const auto highMeasured = spectral_test::Measure(high, bin, 63, count);
  Require(highMeasured.residualRMS < 1.e-10, "Reference counts ultrasonic harmonics as baseband residual");
  for (int i = 0; i < 4 * count; ++i)
    high[i] += .02 * std::sin(2. * spectral_test::pi * alias * i / (4 * count));
  Require(std::abs(spectral_test::Measure(high, bin, 63, count).residualRMS - .02 / std::sqrt(2.)) < 1.e-10,
          "Reference misses baseband contamination");
  for (int i = 0; i < 4 * count; ++i)
    high[i] -= .02 * std::sin(2. * spectral_test::pi * alias * i / (4 * count));
  const auto bins = spectral_test::FoldedBins(count, bin);
  Require(spectral_test::BandRMS(high, bins) < 1.e-10, "High-rate reference mislabels ultrasonic fifth");
  Require(std::abs(spectral_test::BandRMS(folded, bins) - d.foldedRMS) < 1.e-12, "Band power disagrees");
  // The reusable processor comparison retains the pedal's existing >40 dB
  // folded-fifth rejection requirement, without involving a particular voice.
  grid_pedal::Oversampler4x os; os.Reset();
  std::vector<double> native(count), filtered(2 * count);
  for (int i = 0; i < 2 * count; ++i)
  {
    const double x = 4. * std::sin(2. * spectral_test::pi * bin * i / count);
    if (i >= count) native[i - count] = std::tanh(x);
    filtered[i] = os.Tick(x, [](double value) { return std::tanh(value); });
  }
  Require(spectral_test::SuppressionDB(native, std::span<const double>(filtered).subspan(count), alias) > 40.,
          "Pedal oversampler folded-fifth rejection regressed");
}
void TestDistortionQuality()
{
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
    for (double frequency : {6500., 10000.})
    for (int style = 0; style < effect_modes::distortionCount; ++style)
    {
      // Compare the same physical high-frequency tones at each host rate; a
      // fixed normalized bin would rise beyond audibility at 176.4/192 kHz.
      const int bin = static_cast<int>(std::round(frequency * count / rate)) | 1;
      // Rectification creates DC. Warm the 5 Hz blocker for a fixed duration,
      // then discard another complete window before measuring the final one.
      const int samples = (static_cast<int>(std::ceil(rate * .5 / count)) + 2) * count;
      std::array<std::vector<double>, 2> audio{std::vector<double>(samples), std::vector<double>(samples)};
      for (int mode = 0; mode < 2; ++mode)
      {
        SlotDistortion effect;
        if (mode == 0) effect.Reset(rate, 1, false); else effect.Reset(rate);
        UtilityControls c; c.distortion = {double(style), 36., 12000., 0., 100., -6.};
        for (int i = 0; i < samples; ++i) audio[mode][i] = .8 * std::sin(2. * spectral_test::pi * bin * i / count);
        effect.Process(audio[mode].data(), samples, c);
      }
      const auto original = spectral_test::Measure(std::span<const double>(audio[0]).last(count), bin);
      const auto improved = spectral_test::Measure(std::span<const double>(audio[1]).last(count), bin);
      const double reduction = spectral_test::DB(original.residualRMS) - spectral_test::DB(improved.residualRMS);
      const double minimum = rate < 88200. ? 20. : rate < 176400. ? 12. : 6.;
      Require(reduction > minimum, "Distortion alias suppression regressed: " + std::string(effect_modes::distortionNames[style])
                                  + " at " + std::to_string(rate) + ": " + std::to_string(reduction) + " dB");
    }
  // The generalized filters remain causal and preserve gain for every factor.
  const auto checkFilter = []<int Factor>() {
    grid_oversampling::FIR<Factor> filter; filter.Reset();
    std::array<double, 256> impulse{};
    for (int i = 0; i < impulse.size(); ++i) impulse[i] = filter.Tick(i == 0 ? 1. : 0., [](double x) { return x; });
    Require(filter.latency == 0 && impulse[0] != 0., "Oversampling introduced fixed lookahead");
    Require(std::max_element(impulse.begin(), impulse.end()) - impulse.begin() <= 8, "Oversampling impulse peaks late");
    double gain = 0.; for (double value : impulse) gain += value;
    Require(std::abs(gain - 1.) < 1.e-5, "Oversampling DC gain changed");
    for (double fraction : {.5, .6, .8, 1.})
      Require(std::abs(filter.Response(fraction, Factor)) < 2.e-4, "Oversampling stopband regressed");
  };
  checkFilter.template operator()<2>(); checkFilter.template operator()<4>(); checkFilter.template operator()<8>();
}
void StudyDistortion(const std::filesystem::path& report)
{
  std::ofstream csv(report);
  Require(csv.good(), "Distortion study needs --report file.csv");
  csv << "style,rate,frequency_hz,drive_db,factor,adaa,residual_db_relative,residual_rms_dbfs,cpu_percent\n";
  for (int style = 0; style < effect_modes::distortionCount; ++style)
    for (double rate : {48000., 96000.})
      for (double frequency : {1500., 6500., 10000.})
        for (double drive : {9., 36.})
          for (int factor : {1, 2, 4, 8})
            for (bool adaa : {false, true})
            {
              SlotDistortion effect; effect.Reset(rate, factor, adaa);
              UtilityControls c; c.distortion = {double(style), drive, 12000., 0., 100., -6.};
              const int bin = static_cast<int>(std::round(frequency * count / rate)) | 1;
              const int samples = (static_cast<int>(std::ceil(rate * .5 / count)) + 2) * count;
              std::vector<double> audio(samples);
              for (int i = 0; i < audio.size(); ++i)
                audio[i] = .8 * std::sin(2. * spectral_test::pi * bin * i / count);
              const auto begin = std::chrono::steady_clock::now();
              effect.Process(audio.data(), static_cast<int>(audio.size()), c);
              const double cpu = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count()
                                 / (audio.size() / rate) * 100.;
              const auto measured = spectral_test::Measure(std::span<const double>(audio).last(count), bin);
              csv << effect_modes::distortionNames[style] << ',' << rate << ',' << bin * rate / count << ',' << drive << ','
                  << factor << ',' << adaa << ',' << measured.ResidualDB() << ',' << spectral_test::DB(measured.residualRMS)
                  << ',' << cpu << '\n';
            }
  Require(csv.good(), "Could not write distortion study");
  std::cout << "Distortion oversampling/ADAA study complete\n";
}

void TestFollowupQuality(const std::filesystem::path& capture)
{
  for (auto effect : {E::TubeBoost, E::Tape, E::ConsoleColour, E::PedalDrive})
    for (double rate : {44100., 48000., 88200., 96000.})
      for (double frequency : {6500., 10000.})
      {
        auto c = Defaults(effect); Stress(c, effect);
        if (effect == E::PedalDrive) c.utility.pedal[0] = 1.;
        const int bin = static_cast<int>(std::round(frequency * count / rate)) | 1;
        const auto native = Render(effect, c, rate, bin, .8, capture, false, 1, 1, false);
        const auto selected = Render(effect, c, rate, bin, .8, capture);
        Require(selected.latency == 0, "Drive treatment added reported latency");
        for (int channel = 0; channel < 2; ++channel)
        {
          const auto before = spectral_test::Measure(native.audio[channel], bin);
          const auto after = spectral_test::Measure(selected.audio[channel], bin);
          const double reduction = spectral_test::DB(before.residualRMS) - spectral_test::DB(after.residualRMS);
          const double minimum = effect == E::TubeBoost ? (rate < 88200. ? 18. : 8.)
                               : effect == E::PedalDrive ? 5. : rate < 88200. ? 8. : -.1;
          Require(reduction > minimum, "Drive quality regressed: " + std::string(SlotEffectName(effect))
                  + " " + std::to_string(rate) + ": " + std::to_string(reduction) + " dB");
        }
      }
}

void StudyVoicing(const std::filesystem::path& report, const std::filesystem::path& capture)
{
  std::ofstream csv(report); Require(csv.good(), "Cannot open voicing study");
  csv << "module,variant,rate,frequency,channel,gain_change_db\n";
  for (auto effect : {E::TubeBoost, E::Tape, E::ConsoleColour, E::PedalDrive})
  {
    auto variants = Variants(effect);
    if (effect == E::PedalDrive) { variants.resize(1); variants[0].controls.utility.pedal[0] = 1.; }
    for (const auto& v : variants)
      for (double rate : {48000., 96000.})
        for (double f : {20., 40., 80., 100., 250., 1000., 5000., 10000., 18000.})
        {
          const int bin = static_cast<int>(std::round(f * count / rate)) | 1;
          const auto a = Render(effect, v.controls, rate, bin, .01, capture, false, 1, 1, false);
          const auto b = Render(effect, v.controls, rate, bin, .01, capture);
          for (int channel = 0; channel < 2; ++channel)
          {
            const auto x = spectral_test::Measure(a.audio[channel], bin), y = spectral_test::Measure(b.audio[channel], bin);
            csv << std::quoted(SlotEffectName(effect)) << ',' << std::quoted(v.name) << ',' << rate << ',' << bin * rate / count
                << ',' << channel << ',' << spectral_test::DB(y.fundamental / std::max(1.e-15, x.fundamental)) << '\n';
          }
        }
  }
}

void StudyDrives(const std::filesystem::path& report, const std::filesystem::path& capture)
{
  std::ofstream csv(report); Require(csv.good(), "Cannot open candidate study");
  csv << "module,variant,profile,rate,frequency,factor,adaa,channel,residual_dbfs,folded_dbfs,fundamental,cpu_percent\n";
  csv << std::setprecision(12);
  for (auto effect : {E::TubeBoost, E::Tape, E::ConsoleColour, E::PedalDrive})
  {
    auto variants = Variants(effect);
    if (effect == E::PedalDrive)
    {
      variants.resize(1); variants[0].controls.utility.pedal[0] = 1.; variants[0].name = "Rodent";
    }
    for (const auto& variant : variants)
      for (double rate : {48000., 96000.})
        for (double frequency : {1500., 6500., 10000.})
          // Compare only the alternatives the shipped code offers: Tube Boost picks
          // 1x/2x/4x by host rate, Tape and Console Colour run native or 2x, and the
          // Rodent keeps its 4x with the local treatment on or off.
          for (const auto& [factor, adaa] : effect == E::PedalDrive
                                              ? std::vector<std::pair<int, bool>>{{4, false}, {4, true}}
                                            : effect == E::TubeBoost
                                              ? std::vector<std::pair<int, bool>>{{1, false}, {2, false}, {4, false}}
                                              : std::vector<std::pair<int, bool>>{{1, false}, {2, false}})
            for (int profile = 0; profile < 2; ++profile)
            {
              auto c = variant.controls; if (profile) Stress(c, effect);
              const int bin = static_cast<int>(std::round(frequency * count / rate)) | 1;
              const auto audio = Render(effect, c, rate, bin, profile ? .8 : .2, capture, false, 1, factor, adaa);
              for (int channel = 0; channel < 2; ++channel)
              {
                const auto m = spectral_test::Measure(audio.audio[channel], bin);
                csv << std::quoted(SlotEffectName(effect)) << ',' << std::quoted(variant.name) << ',' << profile << ','
                    << rate << ',' << bin * rate / count << ',' << factor << ',' << adaa << ',' << channel << ','
                    << spectral_test::DB(m.residualRMS) << ',' << spectral_test::DB(m.foldedRMS) << ','
                    << m.fundamental << ',' << 100. * audio.seconds << '\n';
              }
            }
    std::cout << "Candidate study: " << SlotEffectName(effect) << " complete\n" << std::flush;
  }
  csv.flush(); Require(csv.good(), "Candidate study write failed");
}

} // namespace
int main(int argc, char** argv)
try
{
  if (argc < 2) throw std::runtime_error("Usage: nam-aliasing-tests <source-root> [--full] [--report file.csv] [--module id] [--capture file.nam] [--reference-factor 2|4] [--control name value]");
  bool study = false, driveStudy = false, voicing = false; bool full = false; int only = -1, referenceFactor = 1;
  std::filesystem::path report;
  std::vector<std::pair<int, double>> overrides;
  std::string overrideDescription;
  std::filesystem::path capture = std::filesystem::path(argv[1]) / "NeuralAmpModeler/resources/defaults/riffsmith-clean.nam";
  for (int i = 2; i < argc; ++i)
  {
    const std::string option = argv[i];
    if (option == "--voicing-study") voicing = true;
    else if (option == "--drive-study") driveStudy = true;
    else if (option == "--distortion-study") study = true;
    else if (option == "--full") full = true;
    else if (option == "--report" && i + 1 < argc) report = argv[++i];
    else if (option == "--module" && i + 1 < argc) only = std::stoi(argv[++i]);
    else if (option == "--reference-factor" && i + 1 < argc) referenceFactor = std::stoi(argv[++i]);
    else if (option == "--capture" && i + 1 < argc) capture = argv[++i];
    else if (option == "--control" && i + 2 < argc)
    {
      const std::string name = argv[++i];
      const double value = std::stod(argv[++i]);
      int control = -1;
      for (int p = 0; p < kUtilityControls; ++p) if (name == kEffectParameters[p].name) control = p;
      Require(control >= 0, "Unknown effect control: " + name);
      const auto& d = kEffectParameters[control];
      Require(std::isfinite(value) && value >= d.minimum && value <= d.maximum, "Control value out of range: " + name);
      Require(EffectChoices(control).empty() && !IsAlgorithmMode(control), "Use registered selector variants instead: " + name);
      overrides.emplace_back(control, value);
      overrideDescription += "; " + name + "=" + std::to_string(value);
    }
    else throw std::runtime_error("Invalid spectral test argument: " + option);
  }
  Require(only == -1 || IsValidSlotEffect(static_cast<E>(only)), "Invalid module ID");
  Require(referenceFactor == 1 || referenceFactor == 2 || referenceFactor == 4, "Reference factor must be 1, 2 or 4");
  if (!overrides.empty())
  {
    Require(only >= 0, "--control requires --module so measurements target the intended processor");
    for (const auto& [control, value] : overrides)
      Require(UsesUtility(static_cast<E>(only), control), "Control is not used by the selected module: " + std::string(kEffectParameters[control].name));
  }
  nam::activations::Activation::enable_fast_tanh();
  TestAnalyzer();
  if (voicing) { StudyVoicing(report, capture); return 0; }
  if (driveStudy) { StudyDrives(report, capture); return 0; }
  if (study) { StudyDistortion(report); return 0; }
  TestDistortionQuality();
  TestFollowupQuality(capture);
  std::ofstream csv;
  if (!report.empty())
  {
    csv.open(report); Require(csv.good(), "Cannot open spectral report");
    csv << "module_id,module,variant,profile,interpretation,rate,frequency_hz,amplitude,channel,latency_samples,output_rms_dbfs,residual_db_relative,folded_db_relative,folded_rms_dbfs,folded_change_db,cpu_percent,reference_factor,reference_folded_rms_dbfs,reference_reduction_db,reference_residual_rms_dbfs,reference_residual_reduction_db,assessment\n";
    csv << std::setprecision(10);
  }
  int rows = 0, modules = 0;
  for (int id = 0; id <= static_cast<int>(E::Last); ++id)
  {
    if (only >= 0 && only != id) continue;
    const auto effect = static_cast<E>(id);
    const auto type = Classify(effect);
    auto variants = Variants(effect);
    if (!full) variants.resize(1);
    for (auto& variant : variants) variant.name += overrideDescription;
    double worst = -300.;
    for (const auto& variant : variants)
      for (int profile = 0; profile < (full ? 2 : 1); ++profile)
        for (double rate : full ? std::vector<double>{44100., 48000., 88200., 96000.} : std::vector<double>{48000.})
          for (double frequency : full ? std::vector<double>{1500., 6500., 10000.} : std::vector<double>{6500.})
          {
            auto controls = variant.controls;
            if (profile) Stress(controls, effect);
            for (const auto& [control, value] : overrides) grid_modulation::WriteControl(controls, 17 + control, value);
            const double amplitude = profile ? .8 : .2;
            // Odd bins are coprime with the power-of-two FFT length.
            const int bin = static_cast<int>(std::round(frequency * count / rate)) | 1;
            const auto rendered = Render(effect, controls, rate, bin, amplitude, capture);
            if (effect != E::NAM) Require(rendered.latency == 0, "Built-in effect added reported latency");
            // A separately rendered higher-rate processor is a spectral
            // reference, not a proposed plugin processing mode. Stateful and
            // trained algorithms can change with rate; interpret accordingly.
            Rendered reference;
            if (referenceFactor > 1)
              reference = Render(effect, controls, rate * referenceFactor, bin, amplitude, capture, false, referenceFactor);
            for (int channel = 0; channel < 2; ++channel)
            {
              const auto measured = spectral_test::Measure(rendered.audio[channel], bin);
              const auto previous = spectral_test::Measure(rendered.previous[channel], bin);
              if (type == Interpretation::Linear)
                Require(measured.residualRMS < amplitude * 1.e-7, "Linear control has unexpected spectral energy: " + std::string(SlotEffectName(effect)));
              const double change = spectral_test::DB(measured.foldedRMS) - spectral_test::DB(previous.foldedRMS);
              // Triage, not a psychoacoustic pass/fail threshold. No baseline
              // fixture blesses today's aliases. Dynamic/creative energy is not
              // automatically called aliasing, even when it hits a folded bin.
              const bool triageCandidate = measured.FoldedDB() > -60. && spectral_test::DB(measured.foldedRMS) > -80.;
              const char* assessment = type == Interpretation::Linear ? "linear-control"
                : type != Interpretation::Harmonic ? "diagnostic-only"
                : !triageCandidate ? "below-triage-level"
                : std::abs(change) > 1. ? "check-settling-or-modulation" : "investigate-aliasing";
              worst = std::max(worst, measured.FoldedDB());
              const auto referenceMeasured = referenceFactor > 1
                ? spectral_test::Measure(reference.audio[channel], bin, 63, count) : spectral_test::Measurement{};
              const double referenceFolded = referenceMeasured.foldedRMS;
              if (csv.is_open())
                csv << id << ',' << std::quoted(SlotEffectName(effect)) << ',' << std::quoted(variant.name) << ','
                    << (profile ? "driven" : "insertion") << ',' << Label(type) << ',' << rate << ',' << bin * rate / count << ','
                    << amplitude << ',' << channel << ',' << rendered.latency << ',' << spectral_test::DB(measured.rms) << ','
                    << measured.ResidualDB() << ',' << measured.FoldedDB() << ',' << spectral_test::DB(measured.foldedRMS) << ','
                    << change << ',' << rendered.seconds * 100. << ',' << referenceFactor << ','
                    << (referenceFactor > 1 ? std::to_string(spectral_test::DB(referenceFolded)) : std::string{}) << ','
                    << (referenceFactor > 1 ? std::to_string(spectral_test::DB(measured.foldedRMS) - spectral_test::DB(referenceFolded)) : std::string{})
                    << ',' << (referenceFactor > 1 ? std::to_string(spectral_test::DB(referenceMeasured.residualRMS)) : std::string{})
                    << ',' << (referenceFactor > 1 ? std::to_string(spectral_test::DB(measured.residualRMS) - spectral_test::DB(referenceMeasured.residualRMS)) : std::string{})
                    << ',' << assessment << '\n';
              ++rows;
            }
          }
    std::cout << id << ' ' << SlotEffectName(effect) << ": " << Label(type) << ", worst folded-bin energy " << worst << " dB relative\n" << std::flush;
    ++modules;
  }
  if (csv.is_open()) { csv.flush(); Require(csv.good(), "Failed to write spectral report"); }
  Require(modules == (only < 0 ? static_cast<int>(E::Last) + 1 : 1), "Incomplete module coverage");
  // Verify the spectral renders are independent of callback partitioning.
  for (auto effect : {E::Distortion, E::PedalDrive, E::TubeBoost, E::Tape, E::ConsoleColour})
  {
    const auto c = Defaults(effect);
    const auto a = Render(effect, c, 48000., 1093, .2, capture);
    const auto b = Render(effect, c, 48000., 1093, .2, capture, true);
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < count; ++i)
        Require(std::abs(a.audio[ch][i] - b.audio[ch][i]) < 1.e-12, "Spectral render depends on callback partition");
  }
  std::cout << "Spectral checks passed: " << modules << " modules, " << rows << " channel measurements. Dynamic/creative results are diagnostic.\n";
  return 0;
}
catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
