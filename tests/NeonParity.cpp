#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include "FinalGate.h"

#define NAM_AI_GATE_SCALAR 1
#define NAM_NOISE_PRINT_SCALAR 1
#define ai_gate ai_gate_scalar
#define noise_print noise_print_scalar
#include "AIGate.h"
#include "NoisePrint.h"
#undef ai_gate
#undef noise_print
#undef NAM_AI_GATE_SCALAR
#undef NAM_NOISE_PRINT_SCALAR

namespace neon_parity_scalar
{
struct AIGateParams
{
  double reduction = 40., sensitivity = 0., mix = 100., attackProtection = 50.;
  double gateThreshold = -100., gateRelease = 40.;
};

struct AIGateResult
{
  std::vector<double> left, right;
  double reduction = 0., cleanPeak = 0.;
};

AIGateResult RunAIGate(double rate, const std::vector<double>& inL, const std::vector<double>* inR,
                       const std::vector<int>& blocks, const AIGateParams& p)
{
  ai_gate_scalar::Processor proc;
  proc.Reset(rate);
  ai_gate_scalar::Controls c{p.reduction, p.sensitivity, p.mix, p.attackProtection, p.gateThreshold, p.gateRelease};
  AIGateResult res;
  res.left = inL;
  if (inR) res.right = *inR;
  size_t pos = 0;
  for (int n : blocks)
  {
    const int count = static_cast<int>(std::min<size_t>(n, res.left.size() - pos));
    if (count <= 0) break;
    proc.Process(res.left.data() + pos, inR ? res.right.data() + pos : nullptr, count, c);
    pos += count;
  }
  res.reduction = proc.Reduction();
  res.cleanPeak = proc.CleanPeak();
  return res;
}

struct NoisePrintParams
{
  bool learn = false;
  double sensitivity = 1.5, reduction = 24., attack = 2., release = 120., mix = 100.;
  double gateThreshold = -100., gateRelease = 40., protectAttack = 100.;
  int trackingMode = 0;
};

struct NoisePrintStep
{
  NoisePrintParams params;
  std::vector<int> blocks;
  bool restoreInvalid = false;
};

struct NoisePrintResult
{
  std::vector<double> left, right;
  std::array<double, 96> profilePower{};
  bool profileValid = false;
  double reduction = 0., cleanPeak = 0., trackingScale = 1.;
};

NoisePrintResult RunNoisePrint(double rate, const std::vector<double>& inL, const std::vector<double>* inR,
                               const std::vector<NoisePrintStep>& steps)
{
  noise_print_scalar::Processor proc;
  proc.Reset(rate);
  NoisePrintResult res;
  res.left = inL;
  if (inR) res.right = *inR;
  size_t pos = 0;
  for (const auto& step : steps)
  {
    if (step.restoreInvalid)
      proc.Restore(noise_print_scalar::Profile{});
    noise_print_scalar::Controls c{
      step.params.learn, step.params.sensitivity, step.params.reduction, step.params.attack, step.params.release,
      step.params.mix, step.params.gateThreshold, step.params.gateRelease, step.params.protectAttack,
      step.params.trackingMode};
    for (int n : step.blocks)
    {
      const int count = static_cast<int>(std::min<size_t>(n, res.left.size() - pos));
      if (count <= 0) break;
      proc.Process(res.left.data() + pos, inR ? res.right.data() + pos : nullptr, count, c);
      pos += count;
    }
  }
  const auto& prof = proc.GetProfile();
  res.profilePower = prof.power;
  res.profileValid = prof.valid;
  res.reduction = proc.Reduction();
  res.cleanPeak = proc.CleanPeak();
  res.trackingScale = proc.TrackingScale();
  return res;
}
} // namespace neon_parity_scalar
