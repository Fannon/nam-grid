#pragma once
#include "NAMStage.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"

enum class SlotEditMode
{
  Copy,
  Move,
  Swap
};
struct SlotConfig
{
  SlotEffect effect = SlotEffect::NAM;
  std::string path;
  // NAM in/trim/on, gate x3, compressor x3, Tone EQ x3, Cut EQ x3, IR level, Full quality.
  std::array<double, 17> values{0., 0., 1., -55., 120., 60., -18., 120., 0., 0., 0., 0., 0., 20000., 0., -18., 1.};
};
struct PreparedSlot
{
  SlotConfig config;
  std::unique_ptr<NAMStage::Prepared> dsp;
};

// Control thread only. Prepare every affected resource before changing any slot.
inline PreparedSlot PrepareSlot(SlotConfig config, double rate, int block)
{
  if (config.effect < SlotEffect::NAM || config.effect > SlotEffect::CabinetIR)
    throw std::runtime_error("Invalid slot effect");
  auto p = std::make_unique<NAMStage::Prepared>();
  p->effect = config.effect;
  if (!config.path.empty() && config.effect == SlotEffect::NAM)
  {
    auto model = nam::get_dsp(std::filesystem::u8path(config.path));
    if (model->NumInputChannels() != 1 || model->NumOutputChannels() != 1)
      throw std::runtime_error("NAM slots require a mono input and output model");
    p->model = std::make_unique<ResamplingNAM>(std::move(model), rate);
    if (auto* slimmable = p->model->GetSlimmableModel())
      slimmable->SetSlimmableSize(config.values[16]); // Core explicitly forbids this on audio.
  }
  else if (!config.path.empty() && config.effect == SlotEffect::CabinetIR)
  {
    dsp::ImpulseResponse::IRData data;
    const auto status = dsp::wav::Load(config.path.c_str(), data.mRawAudio, data.mRawAudioSampleRate);
    if (status != dsp::wav::LoadReturnCode::SUCCESS)
      throw std::runtime_error(dsp::wav::GetMsgForLoadReturnCode(status));
    if (data.mRawAudio.empty() || !std::isfinite(data.mRawAudioSampleRate) || data.mRawAudioSampleRate <= 0.
        || !std::all_of(data.mRawAudio.begin(), data.mRawAudio.end(), [](float x) { return std::isfinite(x); }))
      throw std::runtime_error("Invalid or empty impulse response");
    p->ir = std::make_unique<dsp::ImpulseResponse>(data, rate);
  }
  else if (!config.path.empty())
    throw std::runtime_error("This effect does not use a file");
  p->Reset(rate, block);
  if (p->latency > 16384)
    throw std::runtime_error("Model conversion delay exceeds the grid's supported range");
  return {std::move(config), std::move(p)};
}

struct PreparedSlotEdit
{
  PreparedSlot destination;
  std::unique_ptr<PreparedSlot> source;
};
inline PreparedSlotEdit PrepareSlotEdit(const SlotConfig& source, const SlotConfig& destination, SlotEditMode mode,
                                        double rate, int block)
{
  if (mode < SlotEditMode::Copy || mode > SlotEditMode::Swap)
    throw std::runtime_error("Invalid slot operation");
  PreparedSlotEdit result{PrepareSlot(source, rate, block), nullptr};
  if (mode != SlotEditMode::Copy)
    result.source =
      std::make_unique<PreparedSlot>(PrepareSlot(mode == SlotEditMode::Swap ? destination : SlotConfig{}, rate, block));
  return result;
}
