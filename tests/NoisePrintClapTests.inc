// Included inside the real CLAP harness after its shared host/state/audio setup.
auto learnedNoiseState = cleared;
learnedNoiseState["SlotEffects"][0] = 64;
learnedNoiseState["Output"] = 0.;
load(learnedNoiseState);
clap_param_info learnInfo{};
for (uint32_t i = 0; i < params->count(plugin); ++i)
{
  assert(params->get_info(plugin, i, &learnInfo));
  if (std::strcmp(learnInfo.name, "Pre 1 Noise Learn") == 0)
    break;
}
assert(std::strcmp(learnInfo.name, "Pre 1 Noise Learn") == 0);
auto setNoiseLearn = [&](bool enabled) {
  clap_event_param_value event{};
  event.header.size = sizeof(event);
  event.header.type = CLAP_EVENT_PARAM_VALUE;
  event.param_id = learnInfo.id;
  event.note_id = event.port_index = event.channel = event.key = -1;
  char booleanText[32]{};
  assert(params->value_to_text(plugin, learnInfo.id, enabled ? 1. : 0., booleanText, sizeof(booleanText)));
  assert(params->text_to_value(plugin, learnInfo.id, booleanText, &event.value));
  assert(event.value == (enabled ? 1. : 0.));
  clap_input_events update{&event, [](const clap_input_events*) -> uint32_t { return 1; },
                           [](const clap_input_events* events, uint32_t) -> const clap_event_header* {
                             return &static_cast<clap_event_param_value*>(events->ctx)->header;
                           }};
  params->flush(plugin, &update, &sent);
};
assert(plugin->activate(plugin, 48000., 1, 512));
assert(plugin->start_processing(plugin));
uint32_t noiseSeed = 17;
int noisePosition = 0;
auto renderNoise = [&](int frames) {
  for (int offset = 0; offset < frames;)
  {
    const int n = std::min(512, frames - offset);
    process.frames_count = n;
    for (int i = 0; i < n; ++i)
    {
      noiseSeed = noiseSeed * 1664525u + 1013904223u;
      input[0][i] = input[1][i] = .006 * (double(noiseSeed) / 4294967296. * 2. - 1.)
                                  + .006 * std::sin(6.283185307179586 * 60. * (noisePosition + i) / 48000.);
    }
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    for (int i = 0; i < n; ++i)
      assert(std::isfinite(output[0][i]) && std::isfinite(output[1][i]));
    noisePosition += n;
    offset += n;
  }
};
setNoiseLearn(true);
double actualLearn = 0.;
assert(params->get_value(plugin, learnInfo.id, &actualLearn));
assert(actualLearn == 1.);
renderNoise(96000);
assert(save().config()["Pre 1 Noise Learn"] == 0.); // Never recall recording mode.
assert(save().config()["NoisePrintProfiles"][0].is_null());
setNoiseLearn(false);
assert(params->get_value(plugin, learnInfo.id, &actualLearn) && actualLearn == 0.);
renderNoise(48000);
learnedNoiseState = save().config();
assert(learnedNoiseState["NoisePrintProfiles"][0]["version"] == 1);
assert(learnedNoiseState["NoisePrintProfiles"][0]["power"].size() == noise_print::bands);
std::ofstream(fixtures / "noise-print-profile.json") << learnedNoiseState["NoisePrintProfiles"][0].dump(2);
const auto* noiseLatency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
assert(noiseLatency->get(plugin) == 0);
plugin->stop_processing(plugin);
plugin->deactivate(plugin);
clap_param_info gateInfo{};
for (uint32_t i = 0; i < params->count(plugin); ++i)
{
  assert(params->get_info(plugin, i, &gateInfo));
  if (std::strcmp(gateInfo.name, "Pre 1 Noise Gate Threshold") == 0)
    break;
}
assert(std::strcmp(gateInfo.name, "Pre 1 Noise Gate Threshold") == 0);
assert(gateInfo.flags& CLAP_PARAM_IS_AUTOMATABLE);
double gateValue = 0.;
char gateText[64]{};
assert(params->get_value(plugin, gateInfo.id, &gateValue));
assert(params->value_to_text(plugin, gateInfo.id, gateValue, gateText, sizeof(gateText)));
assert(std::strcmp(gateText, "Off") == 0);
clap_event_param_value gateEvent{};
gateEvent.header.size = sizeof(gateEvent);
gateEvent.header.type = CLAP_EVENT_PARAM_VALUE;
gateEvent.param_id = gateInfo.id;
gateEvent.note_id = gateEvent.port_index = gateEvent.channel = gateEvent.key = -1;
assert(params->text_to_value(plugin, gateInfo.id, "-42.0", &gateEvent.value));
clap_input_events gateEvents{&gateEvent, [](const clap_input_events*) -> uint32_t { return 1; },
                             [](const clap_input_events* events, uint32_t) -> const clap_event_header* {
                               return &static_cast<clap_event_param_value*>(events->ctx)->header;
                             }};
params->flush(plugin, &gateEvents, &sent);
auto automatedGateState = save().config();
assert(std::abs(automatedGateState["Pre 1 Noise Gate Threshold"].get<double>() + 42.) < 1.e-9);
automatedGateState["Pre 1 Noise Gate Threshold"] = -100.;
assert(automatedGateState == learnedNoiseState);
learnedNoiseState = save().config();
load(base);
load(learnedNoiseState);
assert(save().config() == learnedNoiseState);
assert(plugin->activate(plugin, 96000., 1, 512));
assert(plugin->start_processing(plugin));
renderNoise(512);
assert(save().config()["NoisePrintProfiles"] == learnedNoiseState["NoisePrintProfiles"]);
// Verify the actual plugin control reaches DSP, including the final mix path.
assert(params->text_to_value(plugin, gateInfo.id, "-20", &gateEvent.value));
params->flush(plugin, &gateEvents, &sent);
renderNoise(96000);
for (int i = 0; i < static_cast<int>(process.frames_count); ++i)
  assert(std::abs(output[0][i]) < 1.e-10 && std::abs(output[1][i]) < 1.e-10);
assert(params->text_to_value(plugin, gateInfo.id, "Off", &gateEvent.value));
params->flush(plugin, &gateEvents, &sent);
renderNoise(512);
double reopenedEnergy = 0.;
for (int i = 0; i < 512; ++i)
  reopenedEnergy += output[0][i] * output[0][i];
assert(reopenedEnergy > 1.e-10 && noiseLatency->get(plugin) == 0);
plugin->stop_processing(plugin);
plugin->deactivate(plugin);
load(learnedNoiseState);
for (const json& invalidProfile : {json(123), json{{"version", 1}, {"power", {1, 2}}},
                                   json{{"version", 2}, {"power", std::vector<double>(noise_print::bands, .1)}},
                                   json{{"version", 1}, {"power", std::vector<double>(noise_print::bands, -1.)}}})
{
  auto invalid = learnedNoiseState;
  invalid["NoisePrintProfiles"][0] = invalidProfile;
  Stream bytes;
  bytes.string("###NAMGrid-v1###");
  bytes.string(invalid.dump());
  assert(!loadBytes(bytes));
  assert(save().config() == learnedNoiseState);
}
// Check sparse IDs and the pre-experiment named-parameter migration explicitly.
auto missingPrintBank = learnedNoiseState;
missingPrintBank.erase("NoisePrintProfiles");
Stream missingPrintBytes;
missingPrintBytes.string("###NAMGrid-v1###");
missingPrintBytes.string(missingPrintBank.dump());
assert(!loadBytes(missingPrintBytes));
assert(save().config() == learnedNoiseState);

auto invalidGap = learnedNoiseState;
invalidGap["SlotEffects"][0] = 63;
Stream invalidGapBytes;
invalidGapBytes.string("###NAMGrid-v1###");
invalidGapBytes.string(invalidGap.dump());
assert(!loadBytes(invalidGapBytes));
assert(save().config() == learnedNoiseState);
auto schema9 = cleared;
schema9["schema"] = 9;
schema9.erase("NoisePrintProfiles");
for (int slot = 0; slot < 16; ++slot)
{
  const auto label = std::string(slot < 4    ? "Pre "
                                 : slot < 8  ? "A "
                                 : slot < 12 ? "B "
                                             : "Post ")
                     + std::to_string(slot % 4 + 1);
  for (int p = uNoiseLearn; p < kUtilityControls; ++p)
    schema9.erase(label + " " + kEffectParameters[p].name);
}
load(schema9);
assert(save().config() == cleared);
auto schema1001 = learnedNoiseState;
schema1001["schema"] = 1001;
auto migrated1001 = learnedNoiseState;
for (int slot = 0; slot < 16; ++slot)
{
  const auto key = std::string(slot < 4    ? "Pre "
                               : slot < 8  ? "A "
                               : slot < 12 ? "B "
                                           : "Post ")
                   + std::to_string(slot % 4 + 1) + " Noise Gate Threshold";
  schema1001.erase(key);
  migrated1001[key] = -100.;
}
load(schema1001);
assert(save().config() == migrated1001);

// The previously released main schemas get bypassed/untrained Noise Print defaults.
for (int schema : {11, 12, 13})
{
  auto prior = cleared;
  prior["schema"] = schema;
  prior.erase("NoisePrintProfiles");
  for (int slot = 0; slot < 16; ++slot)
  {
    const auto label = std::string(slot < 4    ? "Pre "
                                   : slot < 8  ? "A "
                                   : slot < 12 ? "B "
                                               : "Post ")
                       + std::to_string(slot % 4 + 1);
    for (int p = uNoiseLearn; p < kUtilityControls; ++p)
      prior.erase(label + " " + kEffectParameters[p].name);
  }
  load(prior);
  assert(save().config() == cleared);
}
// A genuine experiment had no roadmap controls, performance controls or presets.
auto oldExperiment = learnedNoiseState;
oldExperiment["schema"] = 1002;
oldExperiment.erase("PerformanceScenes");
oldExperiment.erase("PresetName");
oldExperiment.erase("PresetDirectory");
for (const auto& descriptor : grid_performance::parameters)
  oldExperiment.erase(descriptor.name);
for (int slot = 0; slot < 16; ++slot)
{
  const auto label = std::string(slot < 4    ? "Pre "
                                 : slot < 8  ? "A "
                                 : slot < 12 ? "B "
                                             : "Post ")
                     + std::to_string(slot % 4 + 1);
  for (int p = uUtilityDelay; p < uNoiseLearn; ++p)
    oldExperiment.erase(label + " " + kEffectParameters[p].name);
}
load(oldExperiment);
assert(save().config() == learnedNoiseState);
