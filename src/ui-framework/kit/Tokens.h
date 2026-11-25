#pragma once

// Named values every component is built from (see docs/components/Tokens.md). All lengths are design
// pixels on the 1920 x 1080 canvas; components convert them with Theme::px().

namespace ui {

namespace Radius { constexpr int chip = 5, row = 8, panel = 10; }
namespace Stroke { constexpr int hairline = 1, focus = 2, emphasis = 3; }
namespace Space {
constexpr int pad = 28, titleGap = 16, margin = 36, gap = 26, inset = 18, insetRight = 16;
constexpr int marker = 4, lineExtra = 9, rowGapRoomy = 8, rowGapDense = 6, rowGapTable = 4;
}
namespace Alpha { constexpr int scrimDialog = 210, scrimNested = 120, scrimViewer = 230, surfaceFaint = 110; }

enum class TextRole { Title, Heading, Clock, Tab, Body, Small, Hint };
enum class TextTier { Primary, Muted, Faint, Disabled };
enum class Tone { Neutral, Done, Action, Warning, Error };

}  // namespace ui
