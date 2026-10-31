#include "IsolatedUserHome.h"
#include "ClapTestHost.h"
#include "../NeuralAmpModeler/SlotDefaults.h"
#include "../NeuralAmpModeler/ModulePresets.h"
#include "../NeuralAmpModeler/RigPreset.h"
#include <iomanip>
#include <cstdlib>

// Offline audition/export tool; never opens an audio device or plays sound.
using Audio = std::array<std::vector<double>, 2>;
static void WriteWave(const std::filesystem::path& path, const Audio& audio, int rate)
{
  std::ofstream out(path, std::ios::binary);
  const auto word = [&](uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) out.put(static_cast<char>((v >> (8*i)) & 255));
  };
  const uint32_t size = static_cast<uint32_t>(audio[0].size()*8);
  out.write("RIFF",4); word(size+36,4); out.write("WAVEfmt ",8); word(16,4);
  word(3,2); word(2,2); word(rate,4); word(rate*8,4); word(8,2); word(32,2);
  out.write("data",4); word(size,4);
  for (size_t i = 0; i < audio[0].size(); ++i)
    for (int ch = 0; ch < 2; ++ch)
    { const float x = static_cast<float>(audio[ch][i]); uint32_t bits; std::memcpy(&bits,&x,4); word(bits,4); }
  assert(out.good());
}
static double EstimatedRT60(const Audio& audio, double rate)
{
  std::vector<double> decay(audio[0].size()); double total = 0.;
  for (size_t i = decay.size(); i-- > 0;)
    decay[i] = total += audio[0][i]*audio[0][i]+audio[1][i]*audio[1][i];
  if (total < 1.e-20) return 0.;
  double sx=0., sy=0., sxx=0., sxy=0., n=0.;
  for (size_t i=0; i<decay.size(); ++i)
  {
    const double y=10.*std::log10(std::max(1.e-30,decay[i]/total));
    if (y < -5. && y > -25.) { const double x=i/rate; sx+=x; sy+=y; sxx+=x*x; sxy+=x*y; ++n; }
  }
  const double denominator=n*sxx-sx*sx;
  if (n < 16 || denominator <= 0.) return 0.;
  const double slope=(n*sxy-sx*sy)/denominator;
  return slope < 0. ? -60./slope : 0.;
}
int main(int argc,char** argv)
{
  IsolatedUserHome userHome("nam-grid-fx-audition"); // Never the user's settings or library.
  if (argc < 3 || argc > 4 || (argc == 4 && std::string(argv[3]) != "cosmic" && std::string(argv[3]) != "heads" && std::string(argv[3]) != "factory"))
  {
    std::cerr << "Usage: nam-fx-audition <plugin> <output-folder> [cosmic|heads|factory]\n";
    return 1;
  }
  const bool factoryOnly=argc==4 && std::string(argv[3])=="factory";
  const bool cosmicOnly=argc==4 && std::string(argv[3])=="cosmic";
  const bool headsOnly=argc==4 && std::string(argv[3])=="heads";
  const std::filesystem::path destination=argv[2]; std::filesystem::create_directories(destination);
#ifdef _WIN32
  auto library=LoadLibraryW(std::filesystem::path(argv[1]).c_str()); assert(library);
  auto* entry=reinterpret_cast<const clap_plugin_entry*>(GetProcAddress(library,"clap_entry"));
#else
  auto library=dlopen(argv[1],RTLD_NOW); assert(library);
  auto* entry=reinterpret_cast<const clap_plugin_entry*>(dlsym(library,"clap_entry"));
#endif
  assert(entry && entry->init(argv[1]));
  auto* factory=static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  Host host; auto* plugin=factory->create_plugin(factory,&host.host,factory->get_plugin_descriptor(factory,0)->id);
  assert(plugin && plugin->init(plugin));
  auto* state=static_cast<const clap_plugin_state*>(plugin->get_extension(plugin,CLAP_EXT_STATE));
  Stream initial; clap_ostream streamOut{&initial,Stream::write}; assert(state->save(plugin,&streamOut));
  const auto base=initial.config();
  auto load=[&](const json& config) {
    Stream bytes; bytes.string("###nam-grid###"); bytes.string(config.dump());
    clap_istream streamIn{&bytes,Stream::read}; assert(state->load(plugin,&streamIn));
  };
  // Optional tempo for synced factory recipes; always send explicit host tempo.
  double bpm=120.;
  if (const char* value=std::getenv("NAM_AUDITION_BPM"))
  {
    char* end=nullptr; bpm=std::strtod(value,&end);
    if (end==value || *end!='\0' || !std::isfinite(bpm) || bpm<30. || bpm>300.)
    { std::cerr<<"NAM_AUDITION_BPM must be a number from 30 to 300\n"; return 1; }
  }
  constexpr int rate=48000, block=128;
  clap_input_events empty{nullptr,[](const clap_input_events*)->uint32_t{return 0;},
    [](const clap_input_events*,uint32_t)->const clap_event_header*{return nullptr;}};
  clap_output_events sent{nullptr,[](const clap_output_events*,const clap_event_header*){return true;}};
  // Optional real-guitar input: raw little-endian float32 mono at 48 kHz, followed by a 4 s tail.
  const auto render=[&](const json& config,bool impulse,double inputScale=1.,const std::vector<float>* source=nullptr,bool wetImpulse=false) {
    load(config); assert(plugin->activate(plugin,rate,1,block)); assert(plugin->start_processing(plugin));
    // Long wet responses start after one second of silence, with a gentler
    // impulse so nonlinear reverb excitation is not driven at full scale.
    const int settle=wetImpulse ? rate : 0;
    const int count=source ? static_cast<int>(source->size())+rate*4 : settle+rate*(wetImpulse ? 60 : 8);
    Audio audio{std::vector<double>(count),std::vector<double>(count)};
    std::array<double,block> inL{},inR{},outL{},outR{};
    double* inputs[]{inL.data(),inR.data()},*outputs[]{outL.data(),outR.data()};
    clap_audio_buffer input{nullptr,inputs,2,0,0},output{nullptr,outputs,2,0,0};
    clap_process process{}; process.audio_inputs=&input; process.audio_outputs=&output;
    process.audio_inputs_count=process.audio_outputs_count=1; process.in_events=&empty; process.out_events=&sent;
    clap_event_transport transport{};
    transport.header.size=sizeof(transport); transport.header.type=CLAP_EVENT_TRANSPORT;
    transport.header.space_id=CLAP_CORE_EVENT_SPACE_ID; transport.flags=CLAP_TRANSPORT_HAS_TEMPO;
    transport.tempo=bpm; process.transport=&transport;
    double seconds=0.;
    for(int pos=0;pos<count;pos+=block)
    {
      process.frames_count=std::min(block,count-pos);
      for(uint32_t i=0;i<process.frames_count;++i)
      {
        const int sample=pos+i; double x=impulse && sample==settle ? (wetImpulse ? .1 : 1.) : 0.;
        if(source) x=sample<static_cast<int>(source->size()) ? (*source)[sample] : 0.;
        else if(!impulse)
          for(int note=0;note<9;++note)
          {
            const double age=double(sample)/rate-(note/3)*1.4-(note%3)*.16;
            if(age<0.) continue;
            constexpr double notes[]{110.,164.813778,220.};
            const double frequency=notes[note%3]*(note/3==1 ? 1.122462048 : 1.);
            for(int partial=1;partial<=6;++partial)
              x+=.045/partial*std::exp(-age*(2.+partial*.45))*(1.-std::exp(-age*800.))
                *std::sin(6.283185307179586*frequency*partial*age);
          }
        inL[i]=inR[i]=inputScale*x;
      }
      const auto begin=std::chrono::steady_clock::now();
      assert(plugin->process(plugin,&process)!=CLAP_PROCESS_ERROR);
      seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
      for(uint32_t i=0;i<process.frames_count;++i)
      { assert(std::isfinite(outL[i]) && std::isfinite(outR[i])); audio[0][pos+i]=outL[i]; audio[1][pos+i]=outR[i]; }
    }
    plugin->stop_processing(plugin); plugin->deactivate(plugin);
    return std::make_pair(std::move(audio),seconds);
  };
  if (factoryOnly)
  {
    // Optional real-guitar audition: NAM_AUDITION_GUITAR names raw little-endian float32
    // mono audio at 48 kHz; NAM_AUDITION_CAPTURE adds a .nam amp in Pre 4. Recipes that
    // usually follow an amp render in Post 1, the others in Pre 1 before it.
    std::vector<float> guitar;
    if (const char* path=std::getenv("NAM_AUDITION_GUITAR"))
    {
      std::ifstream in(path,std::ios::binary); assert(in);
      in.seekg(0,std::ios::end); guitar.resize(static_cast<size_t>(in.tellg())/sizeof(float)); in.seekg(0);
      in.read(reinterpret_cast<char*>(guitar.data()),static_cast<std::streamsize>(guitar.size()*sizeof(float)));
      assert(in && !guitar.empty());
    }
    const char* capture=std::getenv("NAM_AUDITION_CAPTURE");
    const char* onlyModule=std::getenv("NAM_AUDITION_MODULE");
    // Optional exact recipe name for focused tempo comparisons.
    const char* onlyPreset=std::getenv("NAM_AUDITION_PRESET");
    int renderedPresets=0;
    if (onlyModule)
    {
      bool found=false;
      for (int id=0; id<=static_cast<int>(SlotEffect::Last); ++id)
        if (std::strcmp(onlyModule,SlotEffectName(static_cast<SlotEffect>(id)))==0
            && !module_presets::For(static_cast<SlotEffect>(id)).empty()) found=true;
      if (!found) { std::cerr<<"No factory presets for module: "<<onlyModule<<'\n'; return 1; }
    }
    const auto followsAmp=[](SlotEffect e) {
      switch (e)
      {
        case SlotEffect::Chorus: case SlotEffect::JunoChorus: case SlotEffect::Flanger: case SlotEffect::Tremolo: case SlotEffect::HarmonicTremolo:
        case SlotEffect::Vibrato: case SlotEffect::Rotary: case SlotEffect::Doubler: case SlotEffect::Stutter:
        case SlotEffect::Freeze: case SlotEffect::LoFiTape: case SlotEffect::Limiter: case SlotEffect::ButterComp:
        case SlotEffect::OutboardChannel: case SlotEffect::ConsoleColour: case SlotEffect::ConsoleEQ:
        case SlotEffect::Tape: case SlotEffect::Bitcrusher: case SlotEffect::RingMod: case SlotEffect::FrequencyShifter:
          return true;
        default: return IsTrailEffect(e);
      }
    };
    auto chain=base;
    if (capture) { chain["SlotEffects"][3]=static_cast<int>(SlotEffect::NAM); chain["SlotPaths"][3]=capture; }
    if (!guitar.empty())
      WriteWave(destination/"reference-guitar.wav",render(chain,false,1.,&guitar).first,rate);
    std::ofstream csv(destination/"factory-measurements.csv");
    csv << "label,module,preset,placement,input_gain_db,cpu_percent_128_frames_48khz,mixed_peak,tail_rms_dbfs\n";
    // Full recipes at their intended mix/output; no hidden normalization. A
    // deterministic pluck is a level/stability screen, not a listening verdict.
    for (int id=0; id<=static_cast<int>(SlotEffect::Last); ++id)
    {
      const auto effect=static_cast<SlotEffect>(id);
      if (onlyModule && std::strcmp(onlyModule,SlotEffectName(effect))!=0) continue;
      int index=0;
      for (const auto& preset : module_presets::For(effect))
      {
        const auto values=module_presets::Apply(effect,preset,SlotDefaultValues(effect));
        const auto place=[&](json config,int slot) {
          config["SlotEffects"][slot]=id;
          for (int c=0; c<static_cast<int>(values.size()); ++c)
            config[GridSlotName(slot)+" "+module_presets::ControlName(c)]=values[c];
          return config;
        };
        const auto config=place(base,0);
        const auto label=std::to_string(id)+"-"+std::to_string(index++);
        if (onlyPreset && std::strcmp(onlyPreset,preset.name)!=0) continue;
        ++renderedPresets;
        auto rig=config; rig["PresetName"]=std::string(SlotEffectName(effect))+" - "+preset.name;
        rig_preset::Write(destination/(label+".ngrid"),rig);
        // Keep the mixed response for comparing echo levels to dry, and
        // export an isolated, longer wet response for decay measurements.
        WriteWave(destination/(label+"-impulse.wav"),render(config,true).first,rate);
        int wetMix=-1;
        if (IsDelay(effect)) wetMix=uDelayMix;
        else switch (effect)
        {
          case SlotEffect::SpringReverb: wetMix=uSpringMix; break;
          case SlotEffect::ShimmerReverb: wetMix=uShimmerMix; break;
          case SlotEffect::AirPlate: wetMix=uAirPlateMix; break;
          case SlotEffect::AirHall: wetMix=uAirHallMix; break;
          case SlotEffect::GalacticSpace: wetMix=uSpaceMix; break;
          case SlotEffect::CosmicEcho: wetMix=uCosmicMix; break;
          default: if (IsReverb(effect)) wetMix=uReverbMix; break;
        }
        if (wetMix>=0)
        {
          auto wet=config;
          wet[std::string("Pre 1 ")+kEffectParameters[wetMix].name]=100.;
          WriteWave(destination/(label+"-wet-impulse.wav"),render(wet,true,1.,nullptr,true).first,rate);
        }
        const bool post=followsAmp(effect);
        if (!guitar.empty())
          WriteWave(destination/(label+"-guitar.wav"),render(place(chain,post ? 12 : 0),false,1.,&guitar).first,rate);
        for (double inputScale : {1.,4.})
        {
          auto [audio,cpu]=render(config,false,inputScale);
          const auto suffix=inputScale==1. ? "" : "-hot";
          WriteWave(destination/(label+suffix+".wav"),audio,rate);
          double peak=0.,tail=0.;
          for (const auto& channel:audio)
            for (size_t i=0; i<channel.size(); ++i)
            {
              peak=std::max(peak,std::abs(channel[i]));
              if (i>=channel.size()-rate/2) tail+=channel[i]*channel[i];
            }
          assert(peak>0. && peak<8.); // Catch silence, runaway feedback and huge gain errors.
          csv<<label<<','<<std::quoted(SlotEffectName(effect))<<','<<std::quoted(preset.name)<<','<<(post ? "post" : "pre")<<','
             <<20.*std::log10(inputScale)<<','<<cpu/8.*100.<<','<<peak<<','<<10.*std::log10(std::max(1.e-30,tail/rate))<<'\n';
        }
      }
    }
    if (!renderedPresets) { std::cerr<<"No matching factory presets\n"; return 1; }
  }
  else
  {
    std::ofstream csv(destination/"measurements.csv");
    csv<<"module,cpu_percent_128_frames_48khz,wet_impulse_energy,estimated_rt60_s,mixed_peak\n";
    std::vector<std::pair<SlotEffect,int>> candidates;
    if (!cosmicOnly && !headsOnly)
      for (auto effect : {SlotEffect::RoomReverb,SlotEffect::PlateReverb,SlotEffect::CloudReverb,SlotEffect::HallReverb,
                         SlotEffect::MultiHeadDelay,SlotEffect::DiffusedDelay,SlotEffect::AirPlate,SlotEffect::AirHall,SlotEffect::GalacticSpace})
        candidates.emplace_back(effect,-1);
    if (!headsOnly)
      for (int style=0; style<effect_modes::cosmicCount; ++style) candidates.emplace_back(SlotEffect::CosmicEcho,style);
    if (headsOnly)
      for (int heads=0; heads<effect_modes::tapeHeadNames.size(); ++heads) candidates.emplace_back(SlotEffect::MultiHeadDelay,heads);
    for (const auto& [effect,mode] : candidates)
    {
      auto config=base;
      config["SlotEffects"][0]=static_cast<int>(effect);
      const auto defaults=SlotDefaultValues(effect);
      for(int p=0;p<kUtilityControls;++p) config[std::string("Pre 1 ")+kEffectParameters[p].name]=defaults[17+p];
      std::string name=SlotEffectName(effect);
      if(effect==SlotEffect::CosmicEcho)
      {
        config["Pre 1 Cosmic Style"]=mode;
        name+=std::string(" - ")+effect_modes::cosmicNames[mode];
      }
      if (effect==SlotEffect::MultiHeadDelay && mode>=0)
      {
        config["Pre 1 Tape Heads"]=mode;
        name+=std::string(" - ")+effect_modes::tapeHeadNames[mode];
      }
      const auto label=std::to_string(static_cast<int>(effect))+"-"+name;
      auto rig=config; rig["PresetName"]=name;
      for(const auto* key:{"PresetDirectory","Tuner Reference","Tuner Mute","Tuner"}) rig.erase(key);
      rig_preset::Write(destination/(label+".ngrid"),rig);
      auto [mixed,cpu]=render(config,false); WriteWave(destination/(label+"-pluck.wav"),mixed,rate);
      double peak=0.; for(const auto& channel:mixed) for(double value:channel) peak=std::max(peak,std::abs(value));
      for(int p:{uDelayMix,uReverbMix,uAirPlateMix,uAirHallMix,uSpaceMix,uCosmicMix})
        config[std::string("Pre 1 ")+kEffectParameters[p].name]=100.;
      auto [wet,unused]=render(config,true); WriteWave(destination/(label+"-wet-impulse.wav"),wet,rate);
      double energy=0.; for(const auto& channel:wet) for(double value:channel) energy+=value*value;
      csv<<std::quoted(name)<<','<<cpu/(mixed[0].size()/double(rate))*100.<<','<<energy<<','<<EstimatedRT60(wet,rate)<<','<<peak<<'\n';
      std::cout<<name<<": peak="<<peak<<" wet energy="<<energy<<" estimated RT60="<<EstimatedRT60(wet,rate)<<" CPU="<<cpu/(mixed[0].size()/double(rate))*100.<<"%\n";
    }
  }
  load(base); plugin->destroy(plugin); entry->deinit();
#ifdef _WIN32
  FreeLibrary(library);
#else
  dlclose(library);
#endif
}
