#include "ClapTestHost.h"
#include "../NeuralAmpModeler/SlotDefaults.h"
#include "../NeuralAmpModeler/RigPreset.h"
#include <iomanip>

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
  if (argc < 3 || argc > 4 || (argc == 4 && std::string(argv[3]) != "cosmic" && std::string(argv[3]) != "heads"))
  {
    std::cerr << "Usage: nam-fx-audition <plugin> <output-folder> [cosmic|heads]\n";
    return 1;
  }
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
    Stream bytes; bytes.string("###NAMGrid-v1###"); bytes.string(config.dump());
    clap_istream streamIn{&bytes,Stream::read}; assert(state->load(plugin,&streamIn));
  };
  constexpr int rate=48000, count=rate*8, block=128;
  clap_input_events empty{nullptr,[](const clap_input_events*)->uint32_t{return 0;},
    [](const clap_input_events*,uint32_t)->const clap_event_header*{return nullptr;}};
  clap_output_events sent{nullptr,[](const clap_output_events*,const clap_event_header*){return true;}};
  const auto render=[&](const json& config,bool impulse) {
    load(config); assert(plugin->activate(plugin,rate,1,block)); assert(plugin->start_processing(plugin));
    Audio audio{std::vector<double>(count),std::vector<double>(count)};
    std::array<double,block> inL{},inR{},outL{},outR{};
    double* inputs[]{inL.data(),inR.data()},*outputs[]{outL.data(),outR.data()};
    clap_audio_buffer input{nullptr,inputs,2,0,0},output{nullptr,outputs,2,0,0};
    clap_process process{}; process.audio_inputs=&input; process.audio_outputs=&output;
    process.audio_inputs_count=process.audio_outputs_count=1; process.in_events=&empty; process.out_events=&sent;
    double seconds=0.;
    for(int pos=0;pos<count;pos+=block)
    {
      process.frames_count=std::min(block,count-pos);
      for(uint32_t i=0;i<process.frames_count;++i)
      {
        const int sample=pos+i; double x=impulse && sample==0 ? 1. : 0.;
        if(!impulse)
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
        inL[i]=inR[i]=x;
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
    auto config=base; config["CaptureFolders"]=json::array(); config["CaptureFolder"]="";
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
    for(const auto* key:{"PresetDirectory","CaptureFolder","CaptureFolders","Tuner Reference","Tuner Mute"}) rig.erase(key);
    rig_preset::Write(destination/(label+".ngrid"),rig);
    auto [mixed,cpu]=render(config,false); WriteWave(destination/(label+"-pluck.wav"),mixed,rate);
    double peak=0.; for(const auto& channel:mixed) for(double value:channel) peak=std::max(peak,std::abs(value));
    for(int p:{uDelayMix,uReverbMix,uAirPlateMix,uAirHallMix,uSpaceMix,uCosmicMix})
      config[std::string("Pre 1 ")+kEffectParameters[p].name]=100.;
    auto [wet,unused]=render(config,true); WriteWave(destination/(label+"-wet-impulse.wav"),wet,rate);
    double energy=0.; for(const auto& channel:wet) for(double value:channel) energy+=value*value;
    csv<<std::quoted(name)<<','<<cpu/8.*100.<<','<<energy<<','<<EstimatedRT60(wet,rate)<<','<<peak<<'\n';
    std::cout<<name<<": peak="<<peak<<" wet energy="<<energy<<" estimated RT60="<<EstimatedRT60(wet,rate)<<" CPU="<<cpu/8.*100.<<"%\n";
  }
  load(base); plugin->destroy(plugin); entry->deinit();
#ifdef _WIN32
  FreeLibrary(library);
#else
  dlclose(library);
#endif
}
