#pragma once
#include "../NeuralAmpModeler/ParameterLayout.h"

template<class Save, class Load>
void TestEffectModeClap(const clap_plugin* plugin, json cleared, Save save, Load load)
{
  const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  // Verify actual host metadata for every slot, including bank boundaries. The
  // first utility ID follows the fixed input/dynamics/EQ/quality controls.

  for (int slot = 0; slot < 16; ++slot)
  {
    const auto label = std::string(slot < 4 ? "Pre " : slot < 8 ? "A " : slot < 12 ? "B " : "Post ")
                       + std::to_string(slot % 4 + 1);
    for (int p = 0; p < kUtilityControls; ++p)
    {
      clap_param_info info{};
      const uint32_t id = SlotUtilityParam(slot, p);
      assert(params->get_info(plugin, id, &info));
      const auto name = label + " " + kEffectParameters[p].name;
      assert(info.id == id && name == info.name);
      assert(bool(info.flags & CLAP_PARAM_IS_AUTOMATABLE) == !IsAlgorithmMode(p));
      const auto choices = EffectChoices(p);
      if (choices.empty()) continue;
      assert((info.flags & CLAP_PARAM_IS_ENUM) && (info.flags & CLAP_PARAM_IS_STEPPED));
      assert(info.min_value == 0. && info.max_value == choices.size() - 1);
      assert(info.default_value == kEffectParameters[p].initial);
      for (int choice = 0; choice < choices.size(); ++choice)
      {
        char text[128]{}; double value = -1.;
        assert(params->value_to_text(plugin, id, choice, text, sizeof(text)));
        assert(std::strcmp(text, choices[choice]) == 0);
        assert(params->text_to_value(plugin, id, text, &value) && value == choice);
        if (slot != 0 && slot != 15) continue;
        if (IsAlgorithmMode(p))
        {
          // Exercise boundary-bank recall alongside the full host integration.
          // parameter-contract covers every mode in all banks in one load per
          // mode, avoiding hundreds of redundant whole-rig preparations here.
          auto configured = save().config();
          configured[name] = choice;
          load(configured);
          assert(save().config()[name] == choice);
          continue;
        }
        clap_event_param_value event{};
        event.header.size = sizeof(event); event.header.type = CLAP_EVENT_PARAM_VALUE;
        event.param_id = id; event.value = choice;
        event.note_id = event.port_index = event.channel = event.key = -1;
        clap_input_events events{&event, [](const clap_input_events*) -> uint32_t { return 1; },
          [](const clap_input_events* e, uint32_t) -> const clap_event_header* {
            return &static_cast<clap_event_param_value*>(e->ctx)->header;
          }};
        params->flush(plugin, &events, &sent);
        assert(params->get_value(plugin, id, &value) && value == choice);
        assert(save().config()[name] == choice);
      }
    }
  }
  // Six-style state recall uses actual integer values, independent of host
  // normalization. Include both new modes at opposite ends of the slot bank.
  auto configured = cleared;
  configured["SlotEffects"][0] = configured["SlotEffects"][15] = static_cast<int>(SlotEffect::Distortion);
  configured["Pre 1 Distortion Style"] = static_cast<int>(effect_modes::DistortionStyle::Diode);
  configured["Post 4 Distortion Style"] = static_cast<int>(effect_modes::DistortionStyle::Rectify);
  load(configured); assert(save().config() == configured);
  for (int style=0; style<effect_modes::cosmicCount; ++style)
  {
    auto cosmic=cleared;
    cosmic["SlotEffects"][0]=cosmic["SlotEffects"][15]=static_cast<int>(SlotEffect::CosmicEcho);
    cosmic["Pre 1 Cosmic Style"]=style; cosmic["Post 4 Cosmic Style"]=(style+1)%effect_modes::cosmicCount;
    load(cosmic); assert(save().config()==cosmic);
  }
  for (int pattern=0; pattern<effect_modes::tapeHeadNames.size(); ++pattern)
  {
    auto heads=cleared; heads["SlotEffects"][0]=heads["SlotEffects"][15]=static_cast<int>(SlotEffect::MultiHeadDelay);
    heads["Pre 1 Tape Heads"]=heads["Post 4 Tape Heads"]=pattern;
    load(heads); assert(save().config()==heads);
  }
  load(cleared);
  // New outboard modules participate in full state recall, including the
  // header Desk choice and independent values at opposite bank boundaries.
  for (int desk=0; desk<8; ++desk)
  {
    auto colour=cleared;
    colour["SlotEffects"][0]=colour["SlotEffects"][15]=static_cast<int>(SlotEffect::ConsoleColour);
    colour["Pre 1 Console Colour Desk"]=desk; colour["Post 4 Console Colour Desk"]=7-desk;
    colour["Pre 1 Console Colour Input"]=3.; colour["Post 4 Console Colour Output"]=-2.;
    load(colour); assert(save().config()==colour);
  }
  for (auto effect : {SlotEffect::OutboardChannel,SlotEffect::ConsoleEQ,SlotEffect::ButterComp})
  {
    auto outboard=cleared; outboard["SlotEffects"][0]=outboard["SlotEffects"][15]=static_cast<int>(effect);
    load(outboard); assert(save().config()==outboard);
  }
  for (int mode=0; mode<2; ++mode)
  {
    auto gate=cleared; gate["SlotEffects"][0]=gate["SlotEffects"][15]=static_cast<int>(SlotEffect::DigitalBlack);
    gate["Pre 1 Black Mode"]=mode; gate["Post 4 Black Mode"]=1-mode;
    gate["Pre 1 Black Threshold"]=-60.; gate["Post 4 Black Mix"]=75.;
    load(gate); assert(save().config()==gate);
  }
  std::cout << "FX modes: stable host IDs, state-only algorithms, musical automation and recall passed\n";
}
