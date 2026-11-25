#pragma once

#include <functional>

class Framebuffer;
class Theme;
class Icons;
class ImageCache;

// Startup screen: first lets storage settle, then runs the startup task and prepares the Systems
// icons before the main UI appears. It stays visible until both tasks have completed.
namespace Splash {

// Waits for `initialWaitMs` (the first 25% of progress), then loads every system icon into the
// shared cache. `startupTask` runs after the wait while the splash remains visible.
void show(Framebuffer &framebuffer, Theme &theme, int initialWaitMs, const Icons &icons,
          ImageCache &images, const std::function<void()> &startupTask);

} // namespace Splash
