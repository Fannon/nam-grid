// macOS editor hosting and capture for the README gallery; see MacEditorCapture.h.
#include "MacEditorCapture.h"
#import <Cocoa/Cocoa.h>
#import <ImageIO/ImageIO.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <objc/runtime.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

// The editor's renderer composes each frame offscreen and copies it whole into a layer
// drawable, so the most recently presented drawable holds the complete current frame.
static id<CAMetalDrawable> sLastDrawable;

@implementation CAMetalLayer (NAMGridGalleryCapture)
- (id<CAMetalDrawable>)namGridGalleryNextDrawable
{
  id<CAMetalDrawable> drawable = [self namGridGalleryNextDrawable]; // The original, after the exchange.
  if (drawable)
    sLastDrawable = drawable;
  return drawable;
}
@end

struct MacEditor
{
  NSWindow* window = nil;
  NSView* view = nil;
  int width = 0, height = 0;
};

MacEditor* OpenMacEditor(const clap_plugin* plugin, const clap_plugin_gui* gui, int width, int height)
{
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
  [NSApp finishLaunching];
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    method_exchangeImplementations(class_getInstanceMethod([CAMetalLayer class], @selector(nextDrawable)),
                                   class_getInstanceMethod([CAMetalLayer class], @selector(namGridGalleryNextDrawable)));
  });
  auto* editor = new MacEditor;
  editor->width = width;
  editor->height = height;
  editor->window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height)
                                               styleMask:NSWindowStyleMaskBorderless
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
  editor->window.releasedWhenClosed = NO;
  editor->window.ignoresMouseEvents = YES; // Only synthetic events reach the editor; no hover from the real pointer.
  if (!gui->create(plugin, CLAP_WINDOW_API_COCOA, false))
    throw std::runtime_error("The plugin did not create a Cocoa editor");
  clap_window parent{};
  parent.api = CLAP_WINDOW_API_COCOA;
  parent.cocoa = (__bridge void*)editor->window.contentView;
  if (!gui->set_parent(plugin, &parent))
    throw std::runtime_error("The plugin editor did not attach to the gallery window");
  gui->show(plugin);
  [editor->window orderFrontRegardless]; // The display link only draws for an on-screen window.
  editor->view = editor->window.contentView.subviews.firstObject;
  if (![editor->view.layer isKindOfClass:[CAMetalLayer class]])
    throw std::runtime_error("The editor does not render into a Metal layer");
  // Readable drawables; set before the first frame so the layer's pool is created that way.
  ((CAMetalLayer*)editor->view.layer).framebufferOnly = NO;
  return editor;
}

void PumpMacEditor()
{
  @autoreleasepool
  {
    while (NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES])
      [NSApp sendEvent:event];
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:.002]];
  }
}

void MacEditorMouse(MacEditor* editor, int x, int y, bool click)
{
  @autoreleasepool
  {
    // The editor view is flipped: (x, y) are top-left editor coordinates, as on Windows.
    const NSPoint location = [editor->view convertPoint:NSMakePoint(x, y) toView:nil];
    const auto event = [&](NSEventType type) {
      return [NSEvent mouseEventWithType:type location:location modifierFlags:0
                               timestamp:[NSProcessInfo processInfo].systemUptime
                            windowNumber:editor->window.windowNumber context:nil eventNumber:0 clickCount:1 pressure:1];
    };
    if (click)
    {
      [editor->view mouseDown:event(NSEventTypeLeftMouseDown)];
      [editor->view mouseUp:event(NSEventTypeLeftMouseUp)];
    }
    else
      [editor->view mouseMoved:event(NSEventTypeMouseMoved)];
  }
}

void CaptureMacEditor(MacEditor* editor, const std::filesystem::path& file, int top)
{
  @autoreleasepool
  {
    // Frames are only drawn while the run loop is pumped; let the GPU finish the last one.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    id<CAMetalDrawable> drawable = sLastDrawable;
    if (!drawable)
      throw std::runtime_error("The editor has not presented a frame");
    id<MTLTexture> texture = drawable.texture;
    if (texture.framebufferOnly || texture.pixelFormat != MTLPixelFormatBGRA8Unorm)
      throw std::runtime_error("The editor frame is not a readable BGRA texture");
    const NSUInteger pixelsWide = texture.width, pixelsHigh = texture.height;
    const NSUInteger scale = pixelsWide / static_cast<NSUInteger>(editor->width);
    if (scale < 1 || pixelsWide != scale * editor->width || pixelsHigh != scale * editor->height)
      throw std::runtime_error("Unexpected editor frame size");
    const NSUInteger rowBytes = pixelsWide * 4;
    id<MTLBuffer> buffer = [texture.device newBufferWithLength:rowBytes * pixelsHigh options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue = [texture.device newCommandQueue];
    id<MTLCommandBuffer> commands = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
    [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(pixelsWide, pixelsHigh, 1) toBuffer:buffer destinationOffset:0
      destinationBytesPerRow:rowBytes destinationBytesPerImage:rowBytes * pixelsHigh];
    [blit endEncoding];
    [commands commit];
    [commands waitUntilCompleted];

    // Average each scale x scale block, so Retina frames match the 1x Windows gallery.
    top = std::clamp(top, 0, editor->height - 1);
    const int width = editor->width, height = editor->height - top;
    const auto* source = static_cast<const unsigned char*>(buffer.contents);
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
        for (int channel = 0; channel < 4; ++channel)
        {
          unsigned sum = 0;
          for (NSUInteger dy = 0; dy < scale; ++dy)
            for (NSUInteger dx = 0; dx < scale; ++dx)
              sum += source[((y + top) * scale + dy) * rowBytes + (x * scale + dx) * 4 + channel];
          pixels[(static_cast<size_t>(y) * width + x) * 4 + channel] =
            static_cast<unsigned char>((sum + scale * scale / 2) / (scale * scale));
        }

    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context = CGBitmapContextCreate(pixels.data(), width, height, 8, width * 4, space,
                                                 kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little);
    CGImageRef image = CGBitmapContextCreateImage(context);
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:file.string().c_str()]];
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
    const bool written = destination && (CGImageDestinationAddImage(destination, image, nullptr), CGImageDestinationFinalize(destination));
    if (destination) CFRelease(destination);
    CGImageRelease(image);
    CGContextRelease(context);
    CGColorSpaceRelease(space);
    if (!written)
      throw std::runtime_error("Could not write " + file.string());
  }
}

void CloseMacEditor(MacEditor* editor, const clap_plugin* plugin, const clap_plugin_gui* gui)
{
  sLastDrawable = nil;
  gui->destroy(plugin);
  [editor->window close];
  delete editor;
}
