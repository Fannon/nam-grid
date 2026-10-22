#pragma once

template<class Save, class Load>
void TestEffectModeClap(const clap_plugin* plugin, json cleared, Save save, Load load)
{
  const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  // Verify actual host metadata for every slot, including bank boundaries. The
  // first utility ID follows the fixed input/dynamics/EQ/quality controls.
  constexpr uint32_t firstUtility = 284;
  for (int slot = 0; slot < 16; ++slot)
  {
    const auto label = std::string(slot < 4 ? "Pre " : slot < 8 ? "A " : slot < 12 ? "B " : "Post ")
                       + std::to_string(slot % 4 + 1);
    for (int p = 0; p < kUtilityControls; ++p)
    {
      clap_param_info info{};
      const uint32_t id = firstUtility + slot * kUtilityControls + p;
      assert(params->get_info(plugin, id, &info));
      const auto name = label + " " + kEffectParameters[p].name;
      assert(info.id == id && name == info.name);
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
  // Presets from the immediately preceding audition build retain their native
  // controls when the new Cosmic bank is appended. Users are comparing both.
  auto previous=cleared;
  previous["schema"]=20;
  previous["SlotEffects"][0]=static_cast<int>(SlotEffect::GalacticSpace);
  previous["Pre 1 Space Replace"]=17.; previous["Pre 1 Space Size"]=81.;
  for (int slot=0; slot<16; ++slot)
  {
    const auto label=std::string(slot<4 ? "Pre " : slot<8 ? "A " : slot<12 ? "B " : "Post ")+std::to_string(slot%4+1);
    for (int p=uCosmicTime; p<kUtilityControls; ++p) previous.erase(label+" "+kEffectParameters[p].name);
  }
  load(previous);
  const auto recalled=save().config();
  assert(recalled["SlotEffects"][0]==static_cast<int>(SlotEffect::GalacticSpace));
  assert(recalled["Pre 1 Space Replace"]==17. && recalled["Pre 1 Space Size"]==81.);
  assert(recalled["Pre 1 Cosmic Time"]==kEffectParameters[uCosmicTime].initial);
  for (int style=0; style<effect_modes::cosmicCount; ++style)
  {
    auto cosmic=cleared;
    cosmic["SlotEffects"][0]=cosmic["SlotEffects"][15]=static_cast<int>(SlotEffect::CosmicEcho);
    cosmic["Pre 1 Cosmic Style"]=style; cosmic["Post 4 Cosmic Style"]=(style+1)%effect_modes::cosmicCount;
    load(cosmic); assert(save().config()==cosmic);
  }
  // Immediately preceding presets and performance scenes use Tone for a
  // cutoff. Preserve those values while presenting High cut consistently.
  auto oldCutoffs=cleared;
  oldCutoffs["schema"]=21;
  for (const auto& [family, suffix] : std::array<std::pair<const char*,const char*>,8>{{
    {"Delay","High Cut"},{"Octaver","High Cut"},{"Freeze","High Cut"},{"Crusher","High Cut"},
    {"Ring","High Cut"},{"Shifter","High Cut"},{"Distortion","High Cut"},{"Cosmic","High Cut"}}})
  {
    const auto prefix=std::string("Pre 1 ")+family+" ";
    oldCutoffs.erase(prefix+suffix); oldCutoffs[prefix+"Tone"]=1234.;
  }
  oldCutoffs["PerformanceScenes"][0]=json{{"Pre 1 Delay Tone",2345.}};
  load(oldCutoffs);
  const auto newCutoffs=save().config();
  assert(newCutoffs["Pre 1 Delay High Cut"]==1234. && newCutoffs["Pre 1 Cosmic High Cut"]==1234.);
  assert(newCutoffs["PerformanceScenes"][0]["Pre 1 Delay High Cut"]==2345.);
  assert(!newCutoffs["PerformanceScenes"][0].contains("Pre 1 Delay Tone"));
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
  auto beforeOutboard=cleared; beforeOutboard["schema"]=23;
  beforeOutboard["Pre 1 Cosmic Warp"]=37.;
  for (int slot=0; slot<16; ++slot)
  {
    const auto label=std::string(slot<4 ? "Pre " : slot<8 ? "A " : slot<12 ? "B " : "Post ")+std::to_string(slot%4+1);
    for (int p=uStripLowCut; p<kUtilityControls; ++p) beforeOutboard.erase(label+" "+kEffectParameters[p].name);
  }
  load(beforeOutboard);
  assert(save().config()["Pre 1 Cosmic Warp"]==37.);
  assert(save().config()["Pre 1 Strip Compress"]==kEffectParameters[uStripCompress].initial);
  load(cleared);
  for (int mode=0; mode<2; ++mode)
  {
    auto gate=cleared; gate["SlotEffects"][0]=gate["SlotEffects"][15]=static_cast<int>(SlotEffect::DigitalBlack);
    gate["Pre 1 Black Mode"]=mode; gate["Post 4 Black Mode"]=1-mode;
    gate["Pre 1 Black Threshold"]=-60.; gate["Post 4 Black Mix"]=75.;
    load(gate); assert(save().config()==gate);
  }
  auto pressure=cleared;
  pressure["SlotEffects"][0]=pressure["SlotEffects"][15]=static_cast<int>(SlotEffect::Pressure);
  pressure["Pre 1 Pressure Compress"]=63.; pressure["Post 4 Pressure Mix"]=80.;
  load(pressure); assert(save().config()==pressure);
  auto beforeDynamics=cleared; beforeDynamics["schema"]=24;
  beforeDynamics["Pre 1 Console Colour Desk"]=6.;
  for (int slot=0; slot<16; ++slot)
  {
    const auto label=std::string(slot<4 ? "Pre " : slot<8 ? "A " : slot<12 ? "B " : "Post ")+std::to_string(slot%4+1);
    for (int p=uBlackThreshold; p<kUtilityControls; ++p) beforeDynamics.erase(label+" "+kEffectParameters[p].name);
  }
  load(beforeDynamics);
  assert(save().config()["Pre 1 Console Colour Desk"]==6.);
  assert(save().config()["Pre 1 Black Threshold"]==-55.);
  load(cleared);
  std::cout << "FX modes: contiguous host banks, choice labels/ranges, automation and state recall passed\n";
}
