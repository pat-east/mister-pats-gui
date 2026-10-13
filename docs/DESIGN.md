# Interface Design System

This document defines the shared visual and interaction rules for MiSTer Pat's GUI. Use it for
new screens, settings, dialogs, empty/error/success states, and feature mockups. `GUI.md` describes
the main screens and their history; this document defines the patterns they should share.

The current implementation is the final reference for existing behavior. If an older sketch in
`GUI.md` disagrees with the implementation or this document, update the sketch when that area is
next redesigned. Keep screenshots in the README representative of the shipped interface.

## Product direction

- Make the interface feel like a console dashboard, with game artwork and clear choices rather
  than a file browser.
- Design for a television viewed from a distance and operated with a game controller.
- Prefer the simpler, familiar interaction when two choices work equally well.
- Keep every screen legible, predictable, and responsive on MiSTer's CPU-rendered framebuffer.

## Layout and sizing

- Author dimensions against the 1920 × 1080 design canvas. Scale positions, spacing, radii, and
  font sizes through `Theme::px()` and the shared `Theme` metrics; do not add a separate scale
  system for one screen.
- Follow the shared top bar, content area, and bottom controller-hint bar. Keep important content
  inside the screen safe area. Check designs at the supported framebuffer sizes, not only in a
  1080p screenshot.
- Use generous spacing and short text. Align repeated rows and columns. Elide long names where
  needed; never let text overlap a status, value, or action.
- Reuse existing surfaces, corner radii, typography, and focus treatments before introducing a
  new component style. Avoid decoration that does not explain content, state, or interaction.

## Color and status

Use the shared `Theme` colors rather than hard-coded colors in screen code.

| Purpose | Theme color | Current dark-theme value |
| --- | --- | --- |
| Main background | `background` | `#0B0D14` |
| Lower background band | `backgroundLo` | `#05070C` |
| Standard surface | `surface` | `#161A26` |
| Raised surface | `surfaceHi` | `#222839` |
| Primary action and success | `accent` | `#4C8DFF` |
| Main text | `textPrimary` | `#FFFFFF` |
| Secondary text | `textMuted` | `#9AA4C0` |
| Focus outline | `focus` | `#B9D5FF` |
| Warning and current error emphasis | `warning` | `#E06A5A` |

- Blue is the primary accent. Use it for the focused/primary action and successful completion;
  success must not introduce green.
- The design intent is orange for warnings and red for errors. The current shared theme exposes
  only `warning` (`#E06A5A`, a coral-red tone), which existing error screens also use. When the
  palette is next revised, separate these meanings with shared orange `warning` and red `error`
  theme colors; do not add local status colors in individual screens.
- **High contrast** (Settings → Accessibility) switches to a black background and lets one
  user-chosen color carry every highlight: the focus outline, progress bars, hints and status such
  as "Network connected", selected values, favorite marks, and tab underlines. The choices are
  White, Pink, Yellow (default), Cyan, Lime Green, Electric Blue, Orange and Lavender, so someone with a color-vision difference can pick the
  one they see best (`Theme::setHighContrastColor`). In high contrast no element may stay blue; use
  `accent`, `focus` or `favorite` from the theme and they follow the chosen color. The round
  checkbox outline becomes light gray instead of dark blue.
- In high contrast the search bar and every row of the Settings category and option lists also get
  a plain white 2 px border (`Theme::highContrastBorders()`), so each field is outlined even when it
  is not focused. The focus outline is drawn over it in the highlight color. With **White** as the
  highlight color the borders are left out, because they could not be told apart from the focus
  outline; the interface then looks like normal mode without them.
- Never communicate a state by color alone. Include a short label or clear icon and retain
  readable contrast in high-contrast mode.
- Keep favorite marks blue. Do not add unrelated colors to make the interface more varied.

## Typography and language

- Use the shared regular and bold fonts from `Theme`. The built-in font remains a supported
  fallback when Space Grotesk is disabled or unavailable.
- The controller hints at the bottom left have their own size setting (Settings → Accessibility →
  **Hint size**: Standard, Larger, Largest, 100 / 115 / 130 %), applied on top of the text size
  (`sizeHint()`, `hintIconSize()`). A hint line that would reach the version text steps down until
  it fits, but never below the normal small size.
- Use shared sizes (`sizeHeading` 36, `sizeTab` 27, `sizeBody` 21, `sizeSmall` 17, `sizeClock` 33,
  `sizeTitle` 48 design pixels, scaled by the text-size setting). Never hard-code a font size in
  screen code.
- Readability comes first: anything the user must read to decide or confirm (names, states,
  sizes, what will change) is **body** size or larger. Small is for secondary detail such as a
  file path or a hint under a label. Essential instructions and status are never small.
- Write all interface text in English. Use short, familiar labels and sentence case. Settings
  need a brief description that tells users what each option changes.
- Use concrete status wording such as **Missing**, **Up to date**, **Installing**, and **Complete**.
  Avoid unexplained abbreviations and technical error text in primary labels.

## Sizes and spacing reference

All values are **design pixels** on the 1920 × 1080 canvas. `Theme::px()` scales them to the real
framebuffer height (`px = value × height ÷ 1080`, at least 1 px). Fonts additionally scale with
the text-size setting through `Theme::fontPx()`. Spacing never scales with the text-size setting,
so large text must still fit these boxes; check designs at **Largest**.

### Text size setting

| Setting | Scale |
| --- | --- |
| Standard | 100 % |
| Larger | 115 % |
| Largest | 130 % |

### Font sizes (`Theme`)

| Role | Function | Size | Use |
| --- | --- | --- | --- |
| Title | `sizeTitle()` | 48 | Game detail title |
| Heading | `sizeHeading()` | 36 | Screen titles, dialog titles |
| Clock | `sizeClock()` | 33 | Top-bar time |
| Tab | `sizeTab()` | 27 | Main navigation labels |
| Body | `sizeBody()` | 21 | Everything the user reads to decide or confirm; row labels; tile labels (`sizeGameLabel()`) |
| Small | `sizeSmall()` | 17 | Secondary detail only: paths, hints under a label, counts |
| Built-in fallback | `sizeGameLabel()` / `sizeGameDetail()` | 18 / 34 | Only when Space Grotesk is unavailable |

### Global layout (`Theme`)

| Metric | Value | Notes |
| --- | --- | --- |
| Side margin (`marginX`) | 72 | Left/right of the content area |
| Vertical margin (`marginY`) | 32 | Between the bars and the content |
| Top bar height | 96 | Clock and date, tabs, network |
| Bottom bar height | 72 | Controller hints, version |
| Corner radius (`radius`) | 10 | Panels, dialogs, large surfaces |
| Gap (`gap`) | 26 | Between tiles, between Settings columns |

`GUI.md` still describes a safe area of 80 px left/right and 40 px top/bottom. The implementation
uses 72 and 32; the implementation is the reference until the two are reconciled.

### Top bar

| Element | Value |
| --- | --- |
| Tab icon size | 26 |
| Icon–label gap | 10 |
| Space between tabs | 54 with icons, 44 text only |
| Active-tab underline | 3 high, radius 2, 4 below the text line, `focus` color |
| Time above date | 16 from the top; date 6 below the time |
| Network block | 18 from the top; address 6 below the link name |

### Tiles and Home

| Element | Value |
| --- | --- |
| Tile width | 190 (artwork is square, 1 : 1) |
| Caption below the artwork | 26 for the title, plus 20 for the system line |
| Focus on cover art | Border only (3 px); the tile does not grow and has no shadow. Tiles without cover art grow 10 % |
| Row title height | 40 |
| Page header (Home, Settings) | 58 |
| Search field height | 54 (drawn 50 high), 20 below it before the content |

The caption heights are fixed. When a font size grows, check that a title and the system line
still fit the 26 + 20 reserved here, at every text-size setting.

### Box art is fixed-size

Box art has standardized sizes so the GUI never has to scale a picture while drawing. Scaling on
the CPU is the cost this avoids; each view loads a BMP that was prepared at its own size and copies
it as it is (see [BOXART.md](BOXART.md) and `Artwork.h`).

| View | Variant | Maximum size (1080p design pixels) |
| --- | --- | ---: |
| Home | `-home` | 178 × 178 |
| Games Grid | `-grid` | 245 × 245 |
| Games Boxart small | `-small` | 128 × 128 |
| Games List detail | `-detail` | 689 × 624 |
| Arcade | `-arcade` | 148 × 148 |

- **Never change the size of box art** and never make it follow the text-size setting, font size,
  focus state, or any other layout value. Tile widths, grid cells and artwork slots stay as they are
  in the tables above and in `Artwork.h`.
- Layout changes (fonts, captions, spacing) must fit around the artwork. Make the text fit the
  space; do not resize the picture to make room.
- **Focus on a cover never enlarges it.** A focused cover tile keeps its size and position; the only
  focus effect is a border (3 px, accent/focus color) around the artwork. No growth, no zoom, no
  drop shadow behind covers: each of those costs a scaled or full-tile alpha pass per frame on
  the CPU. Tiles without cover art (system icons, text placeholders) may still grow 10 %.
  Do not add new scaled or animated artwork rendering.
- Focus moves **instantly** between tiles: the border appears on the new tile and leaves the old one
  in the same frame. Do not ease or fade tile focus; a 0.4 s fade felt laggy and repainted the tiles
  on every frame of it (`Tile::focusStep`).
- A new artwork size is a format change: update `Artwork.h`, regenerate or convert the prepared
  BMPs (**Prepare box art**), and update BOXART.md and this document together.
- Mockups must draw artwork at these sizes.

### Settings screen

| Element | Value |
| --- | --- |
| Columns | Three, `gap` (26) apart; categories, options, and "What this does" |
| Column header | 34 high, 10 inner padding |
| Category row | 48–64 high (one seventh of the area), 8 between rows, radius 8, 4 px accent marker when active |
| Focus outline | 2, `focus` color |

### Dialogs and panels

| Element | Value |
| --- | --- |
| Panel padding | 28 (error dialog: 36) |
| Panel radius | `radius` (10) |
| Short dialog (update intro, checking, result) | 1120 × 760, centered, at most the screen minus 36 |
| List dialog (choose, confirm) | 1812 × 860, same limits |
| Error dialog | 620 wide, at most the screen minus 80 |
| Title to content | 16 below the title (error dialog: 20) |
| Line spacing in text blocks | line height + 9 |
| Row radius | 8 |
| Focus outline | 2, `focus` color |

### Lists inside dialogs

| Element | Value |
| --- | --- |
| Two-line list row (choose what to update) | 62 high, 6 between rows, 8 radius |
| Group heading row | 36 high |
| One-line list row (confirm step) | 46 high, in a `surface` box with 6 padding |
| Live list while checking | One text line plus 8 per row, in a `surface` box with 8 padding, no focus |
| Round checkbox | 20 in one-line rows, 24 in two-line rows, 2 outline, inset a quarter |
| Selection summary bar | 88 high, 24 inner padding |
| Scroll bar | 4 wide, `focus` color thumb on a `surfaceHi` track, 8 from the right edge |
| Tab row (filters) | Labels 36 apart, underline as in the top bar, 16 below before the list |

### Rules

- Use only these values, always through `px()` and the `Theme` font functions. A new value needs a
  reason that applies to the component, not to one screen.
- Add a value here when you introduce one that more than one screen uses.
- Text in a box must never be clipped at **Largest**; shorten or elide it instead.

## Focus and controller interaction

- Every interactive screen must work with a controller. Keyboard or pointer input may supplement
  it, but must not be required to reach or activate a control.
- Keep one clear focus target at a time. Preserve the existing raised surface, accent marker, and
  visible outline pattern used by focused rows and tiles.
- Switching to another category, tab or list puts the cursor on its first row; it does not carry
  over the position from the previous one. Returning to the same one with ← keeps its position.
- Keep navigation consistent: Up/Down move within a list; Left/Right change a value or move
  between columns when the screen already uses that pattern; A activates; B goes back or closes.
  Use shoulder buttons for tab or category changes only where the surrounding screen already does.
- Order the footer hints the way the hands move: movement first (↑/↓, then ←/→), then selecting
  or changing, then confirming (A, Start), and last the way out (B). Keep that order on every
  screen.
- Show the active controls in the global bottom-left hint bar. The hint is the canonical
  instruction for the action and must remain visible over dialogs.
- Respect the selected face-button layout in every hint. The default Xbox layout labels the
  bottom face button **A**; PlayStation labels it **✕**; Nintendo labels it **B**. Confirm is
  always assigned to the bottom face button.
- Write hints with logical button names (for example, `"A Confirm   B Back"`) and pass the full
  string through `formatButtonLabels()` from `ButtonLabels.h` when rendering. This helper applies
  the selected Xbox, PlayStation, or Nintendo labels, including shoulder-button names. Do not
  hard-code physical labels or symbols in UI hint strings. Use `buttonLabel()` or `buttonName()`
  from the same module only when formatting one standalone button label.
- Use **B Back** or a precise label such as **B Close** for the way out. Dialogs have no buttons;
  the footer hint is the only place an action is named (see Buttons).
- Make selected, disabled, and unavailable states visually distinct and explain unavailable
  actions when that helps users understand what to do next.

## Checkboxes and switches

One control, everywhere: the **round checkbox** used by the On/Off rows in Settings. Do not draw
square boxes, tick marks, or a differently colored variant for a new screen.

- **Shape:** a circle. Diameter is 20 px in a single-line row (the On/Off value slot) and 24 px in
  a two-line list row, scaled with `Theme::px()`.
- **Off:** only the dark-blue outline (`#17315F`, 2 px). Nothing inside.
- **On:** the same outline with a light-gray dot (`#D6D8DE`) inside, inset by a quarter of the
  diameter.
- The outline color does not change with focus or selection; the row shows focus. State is
  always carried by the dot, never by color alone.
- Show a checkbox only on rows that can be selected. A row that cannot be selected (already
  current, unavailable) has no checkbox and no placeholder box; do not draw an inactive checkbox.
- A switch row shows the checkbox next to the word **On** or **Off**, right-aligned in a
  fixed-width slot so the checkboxes of all rows line up. A list row (for example the update
  list) puts the checkbox at the left edge, before the name.
- In code, draw it only through the shared helper (`drawRoundCheckbox` in `SettingsScreen.cpp`;
  move it to a shared header when a second screen needs it).

## Buttons

For now, **dialogs have no buttons**. Every action is shown in the global bottom-left hint bar,
and the dialog body only says what the screen is about, what is selected, and what is missing.
Do not draw a primary, secondary, or Cancel button inside a dialog or panel.

- A dialog that cannot continue yet says why in its text (for example "No internet connection",
  "Pick at least one file to continue.") and leaves the matching action out of the footer.
- **Start** may trigger the "continue" action as a shortcut; list it in the footer as
  "Start <action>".
- Do not repeat the hint line inside the dialog.
- The large focusable action inside a screen (such as **Start game** on the Disc tab) is a
  separate component: raised gradient surface, muted outline, accent/focus outline and slight
  growth on focus, like tiles. It is not a dialog button; do not mix the two styles in one view.

This is a deliberate simplification for consistency and may be revisited later; if buttons come
back, define one size, one enabled and one disabled style here first, and use them everywhere.

## Tabs

Anything that switches between a few views or filters of the same content is a **tab row**, drawn
like the main navigation in the top bar. Do not use pills, filled chips, a background bar, a
"Show" label, or arrow glyphs beside the row.

- **Row:** labels in one line, left-aligned to the content, about 36 px apart (`Theme::px()`).
- **Active tab:** bold, `textPrimary`, with an underline in the `focus` color directly below the
  label: 3 px high, fully rounded, exactly as wide as the label, 4 px below the text line.
- **Inactive tabs:** regular weight, `textMuted`, no underline.
- Leave about 16 px of space between the underline and the content below.
- The labels are short nouns or states (**Needs update**, **All**, **Up to date**, **Changed**).
- **Controls:** the main navigation switches with LB/RB. Tabs inside a screen or dialog switch with
  ←/→ when nothing else uses that axis; name the keys in the footer hint (for example
  "←/→ Change filter"), not next to the tabs. Name what the key does to the content, using a
  precise verb ("Change filter", "Switch tab"); never a vague one such as "Show".
- Only one tab is active at a time, and there is always an active tab.

## Dialogs and confirmations

- Dim the current screen and place a centered, rounded panel over it. Preserve enough of the
  background to show where the dialog came from.
- Keep the panel and its controls inside the safe area and clear of the global footer.
- Show the next action in the footer, not as a button in the panel (see Buttons).
- Before a change, summarize what will be changed, its scope, and relevant exclusions. For
  installs, show selected items and a total size when available.
- A list that does not fit scrolls with ↑/↓, uses all the space the panel has, and shows where the
  user is: a range ("6–15 of 22") and a thin scroll bar. Never show a fixed "Showing 10 of 15" that
  leaves the rest unreachable.
- Keep long lists navigable and bounded on screen. Keep the focused row visible as the user moves.
- Progress dialogs state what is happening and whether the operation can be stopped. Do not show
  a Cancel hint if the operation cannot safely be cancelled.

## Empty, error, and success states

- **Empty:** say why there is nothing to show and give the next useful action, such as checking
  again or changing a filter.
- **Error:** use a plain-language headline, explain whether any files or settings changed, and
  offer a controller-operable next step. Keep paths, `errno`, and raw diagnostics in a secondary
  details area; never use them as the headline.
- **Success:** state what completed and what happens next. Use the blue accent and provide a clear
  way to close or continue.
- Warnings use orange; errors use red; successful states use blue. Pair each color with text.

## Mockups and implementation

- Create a mockup for every new user-facing feature. Store local planning mockups in the ignored
  `_vnext/mockups/` directory.
- Use the current README screenshots and nearby implementations as visual references. Mockups
  should show the actual planned screen states and controller hints, not a generic redesign.
- Keep colors, typography, row treatment, modal shape, and button wording consistent between the
  mockup and the implementation. Update the mockup when the implementation's visible flow changes.
- Use shared `Theme` metrics, colors, font helpers, and `formatButtonLabels()` in screen code.
  Avoid per-frame allocations, expensive redraw effects, blocking work, and repeated disk scans.
  Long operations belong in bounded background work so the GUI remains responsive.
- Review at normal viewing scale. Essential text and focus must remain clear from a couch, with
  high-contrast mode enabled, and without relying on subtle color differences.

## References

- [GUI.md](GUI.md) — screen layouts, navigation, tiles, and historical design decisions
- [PERFORMANCE.md](PERFORMANCE.md) — CPU rendering limits and measured performance work
- [README screenshots](../README.md) — current visual reference for the shipped GUI
