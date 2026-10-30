#pragma once
#ifdef __APPLE__
  #include <clap/clap.h>
  #include <filesystem>

// Hosts the CLAP editor in an on-screen Cocoa window that ignores the real pointer, sends
// synthetic mouse events straight to the editor view, and reads frames from the editor's
// own Metal layer, never the desktop, so no screen-recording permission is needed.
struct MacEditor;
MacEditor* OpenMacEditor(const clap_plugin* plugin, const clap_plugin_gui* gui, int width, int height);
// Runs the main run loop briefly so the editor's display timer draws and presents frames.
void PumpMacEditor();
void MacEditorMouse(MacEditor* editor, int x, int y, bool click);
// Writes the last presented frame as a 1x PNG; `top` keeps only editor rows from there down.
void CaptureMacEditor(MacEditor* editor, const std::filesystem::path& file, int top = 0);
void CloseMacEditor(MacEditor* editor, const clap_plugin* plugin, const clap_plugin_gui* gui);
#endif
