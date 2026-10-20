#pragma once
#ifdef _WIN32
  #include <windows.h>
  #include <cassert>
  #include <filesystem>
  #include <fstream>
  #include <utility>
  #include <vector>
  #include <GL/gl.h>
  #pragma comment(lib, "opengl32.lib")

// Optional visual-review artifact from the plugin's own OpenGL surface, never the desktop.
inline void CaptureEditor(HWND editor, const std::filesystem::path& file)
{
  // Flush a complete frame before reading pixels, including labels outside the last dirty region.
  InvalidateRect(editor, nullptr, FALSE);
  UpdateWindow(editor);
  RECT rect{};
  GetClientRect(editor, &rect);
  const int width = rect.right, height = rect.bottom;
  auto dc = GetDC(editor);
  auto previousDC = wglGetCurrentDC();
  auto previousContext = wglGetCurrentContext();
  auto context = wglCreateContext(dc);
  assert(context && wglMakeCurrent(dc, context));
  std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
  glReadBuffer(GL_FRONT);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  assert(glGetError() == GL_NO_ERROR);
  wglMakeCurrent(previousDC, previousContext);
  wglDeleteContext(context);
  ReleaseDC(editor, dc);
  for (size_t i = 0; i < pixels.size(); i += 4)
    std::swap(pixels[i], pixels[i + 2]);
  BITMAPFILEHEADER header{};
  header.bfType = 0x4d42;
  header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
  BITMAPINFOHEADER info{};
  info.biSize = sizeof(info);
  info.biWidth = width;
  info.biHeight = height;
  info.biPlanes = 1;
  info.biBitCount = 32;
  info.biSizeImage = static_cast<DWORD>(pixels.size());
  std::ofstream out(file, std::ios::binary);
  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(&info), sizeof(info));
  out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
  assert(out.good());
}
#endif
