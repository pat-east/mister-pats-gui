#pragma once

class Framebuffer;
class Theme;
class Icons;
class ImageCache;

// Startup screen: first lets storage settle, then prepares the Systems icons before the main UI
// appears. Its progress reflects both phases and it stays visible until icon loading completes.
namespace Splash {

// Waits for `initialWaitMs` (the first 25% of progress), then loads every system icon into the
// shared cache. The splash remains visible until that work is complete.
void show(Framebuffer &framebuffer, Theme &theme, int initialWaitMs, const Icons &icons,
          ImageCache &images);

} // namespace Splash
