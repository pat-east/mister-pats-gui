#pragma once

class Framebuffer;
class Theme;

// A one-time, fixed-length screen shown before the real interface starts, so the app spends
// a few idle seconds on screen instead of straight into stat()-ing the game drives. This is a
// diagnostic: if it turns out to fix boxart that goes missing on a fresh boot, the real fix is
// a proper settle wait around library/artwork loading, not this fake bar staying forever.
namespace Splash {

// Blocks for `durationMs`, painting the embedded logo and a progress bar that always finishes
// in that time regardless of what else is or isn't ready yet.
void show(Framebuffer &framebuffer, Theme &theme, int durationMs);

} // namespace Splash
