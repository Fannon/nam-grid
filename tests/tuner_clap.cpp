#include "ClapTestHost.h"
#include <thread>
#include "WinEditorCapture.h"

int main(int argc, char** argv)
{
  assert(argc == 2 || argc == 3);
#ifdef _WIN32
  auto library = LoadLibraryW(std::filesystem::path(argv[1]).c_str()); assert(library);
  auto* entry = reinterpret_cast<const clap_plugin_entry*>(GetProcAddress(library, "clap_entry"));
#else
  auto library = dlopen(argv[1], RTLD_NOW); assert(library);
  auto* entry = reinterpret_cast<const clap_plugin_entry*>(dlsym(library, "clap_entry"));
#endif
  assert(entry && entry->init(argv[1]));
  auto* factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  Host host; auto* plugin = factory->create_plugin(factory, &host.host, factory->get_plugin_descriptor(factory, 0)->id);
  assert(plugin && plugin->init(plugin));
  auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  auto* state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
  const auto save = [&] { Stream data; clap_ostream stream{&data, Stream::write}; assert(state->save(plugin, &stream)); return data; };
  const auto load = [&](json config) { Stream data; data.string("###nam-grid###"); data.string(config.dump()); clap_istream stream{&data, Stream::read}; assert(state->load(plugin, &stream)); };
  const auto parameter = [&](const char* name) {
    clap_param_info info{};
    for (uint32_t i = 0; i < params->count(plugin); ++i)
    { assert(params->get_info(plugin, i, &info)); if (std::strcmp(name, info.name) == 0) return info; }
    assert(false); return info;
  };
  const auto tuner = parameter("Tuner"), mute = parameter("Tuner Mute");
  assert((tuner.flags & CLAP_PARAM_IS_AUTOMATABLE) && (tuner.flags & CLAP_PARAM_IS_STEPPED));
  const auto base = save().config(); assert(base["Tuner"] == 0.);
  std::vector<uint16_t> tunerEvents;
  clap_output_events sent{&tunerEvents, [](const clap_output_events* list, const clap_event_header* event) {
    auto& events = *static_cast<std::vector<uint16_t>*>(list->ctx);
    if (event->space_id != CLAP_CORE_EVENT_SPACE_ID) return true;
    if (event->type == CLAP_EVENT_PARAM_VALUE || event->type == CLAP_EVENT_PARAM_GESTURE_BEGIN || event->type == CLAP_EVENT_PARAM_GESTURE_END)
      events.push_back(event->type);
    return true;
  }};
  const auto automate = [&](clap_id id, double value) {
    clap_event_param_value change{}; change.header.size = sizeof(change); change.header.type = CLAP_EVENT_PARAM_VALUE;
    change.param_id = id; change.value = value; change.note_id = change.port_index = change.channel = change.key = -1;
    clap_input_events updates{&change, [](const clap_input_events*)->uint32_t { return 1; },
      [](const clap_input_events* list, uint32_t)->const clap_event_header* { return &static_cast<clap_event_param_value*>(list->ctx)->header; }};
    params->flush(plugin, &updates, &sent);
  };
  clap_input_events empty{nullptr, [](const clap_input_events*)->uint32_t{return 0;}, [](const clap_input_events*,uint32_t)->const clap_event_header*{return nullptr;}};
  const auto pumpFor = [&](int milliseconds) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    do { pump(); if (host.callback) { host.callback = false; plugin->on_main_thread(plugin); } std::this_thread::sleep_for(std::chrono::milliseconds(1)); } while (std::chrono::steady_clock::now() < until);
  };
  std::array<double,512> inL{}, inR{}, outL{}, outR{};
  for (int i=0;i<512;++i) inL[i]=inR[i]=.1*std::sin(i*.05);
  double* ins[]{inL.data(),inR.data()}, *outs[]{outL.data(),outR.data()};
  clap_audio_buffer input{nullptr,ins,2,0,0}, output{nullptr,outs,2,0,0};
  clap_process process{}; process.frames_count=512; process.audio_inputs=&input; process.audio_outputs=&output;
  process.audio_inputs_count=process.audio_outputs_count=1; process.in_events=&empty; process.out_events=&sent;
  assert(plugin->activate(plugin,48000.,1,512)); assert(plugin->start_processing(plugin));
  const auto render = [&] { assert(plugin->process(plugin,&process)!=CLAP_PROCESS_ERROR); };
  const auto isMuted = [&] { for (int i=300;i<512;++i) assert(outL[i]==0. && outR[i]==0.); };
  render(); assert(std::abs(outL[400])>.001);
  automate(tuner.id,1.); render(); isMuted();
  const auto active = save().config(); assert(active["Tuner"]==1.);
  automate(tuner.id,0.); render(); assert(std::abs(outL[400])>.001);
  plugin->stop_processing(plugin); plugin->deactivate(plugin);
  load(active); // Host project recall restores the activation parameter.
  assert(plugin->activate(plugin,48000.,1,512)); assert(plugin->start_processing(plugin));
  render(); isMuted();
  automate(mute.id,0.); render(); assert(std::abs(outL[400])>.001);
  automate(mute.id,1.); render(); isMuted();
#ifdef _WIN32
  auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
  assert(gui->create(plugin,CLAP_WINDOW_API_WIN32,false));
  auto parent = CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"nam-grid tuner test",WS_POPUP,0,0,1000,720,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
  assert(parent); clap_window window{};window.api=CLAP_WINDOW_API_WIN32;window.win32=parent; assert(gui->set_parent(plugin,&window));
  SetWindowPos(parent,HWND_TOP,0,0,1000,720,SWP_NOACTIVATE|SWP_SHOWWINDOW);
  auto editor=GetWindow(parent,GW_CHILD);assert(editor);pumpFor(100);
  for(int block=0;block<170;++block)
  {
    for(int i=0;i<512;++i) inL[i]=inR[i]=.1*std::sin(6.283185307179586*110.*(block*512+i)/48000.);
    render();pumpFor(11);
  }
  if(argc==3) CaptureEditor(editor,std::filesystem::path(argv[2]));
  tunerEvents.clear();
  SendMessage(editor,WM_KEYDOWN,VK_ESCAPE,static_cast<LPARAM>(MapVirtualKey(VK_ESCAPE,MAPVK_VK_TO_VSC))<<16);pumpFor(50);
  params->flush(plugin,&empty,&sent);
  assert(save().config()["Tuner"]==0.);render();assert(std::abs(outL[400])>.001);
  assert(tunerEvents.size()==3 && tunerEvents[0]==CLAP_EVENT_PARAM_GESTURE_BEGIN
    && tunerEvents[1]==CLAP_EVENT_PARAM_VALUE && tunerEvents[2]==CLAP_EVENT_PARAM_GESTURE_END);
  automate(tuner.id,1.);pumpFor(50);
  tunerEvents.clear();
  SendMessage(editor,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(936,500));SendMessage(editor,WM_LBUTTONUP,0,MAKELPARAM(936,500));pumpFor(50);
  params->flush(plugin,&empty,&sent);
  assert(save().config()["Tuner"]==0.);
  assert(tunerEvents.size()==3 && tunerEvents.front()==CLAP_EVENT_PARAM_GESTURE_BEGIN && tunerEvents.back()==CLAP_EVENT_PARAM_GESTURE_END);
  automate(tuner.id,1.);pumpFor(50);render();isMuted();
  gui->destroy(plugin);DestroyWindow(parent);
  assert(save().config()["Tuner"]==1.);render();isMuted();
#endif
  automate(tuner.id,0.);render();assert(std::abs(outL[400])>.001);
  plugin->stop_processing(plugin);plugin->deactivate(plugin);plugin->destroy(plugin);entry->deinit();
#ifdef _WIN32
  FreeLibrary(library);
#else
  dlclose(library);
#endif
  std::cout << "Tuner parameter, migration, mute/project recall, native gestures and headless automation passed\n";
}
