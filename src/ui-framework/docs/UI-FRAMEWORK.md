# UI Framework

Design document for the GUI's component framework (`src/ui-framework/`). It defines what the building blocks
are, how they behave, and the rule for using them.

Status: **draft, not implemented.** Nothing here changes the existing GUI: the framework is built
first and the new GUI on it is developed on a Git branch (see "Delivery"). [DESIGN.md](../../../docs/DESIGN.md) stays the reference for the visual values (colors, sizes, spacing,
focus and hint rules); this document defines the *code structure* that enforces them. Where the two
disagree, fix the disagreement in the same change.

## Why

The screens look alike but are built separately. Measured in the current code:

- The same focus frame is drawn in about ten files, each with its own radius (5, 6, 8 or 10 px) and
  thickness (2 or 3 px). The high-contrast white-border special case is copied into several of them.
- Rows, panels and dialogs are composed from `fillRoundedRect` / `strokeRoundedRect` calls inside
  each screen (about 120 `fillRoundedRect` calls outside `Canvas`).
- There is no shared modal system: `ErrorModal`, `coreUpdaterModalActive_` plus a private dialog
  enum, `infoModalActive_`, and panels in `ScanScreen`, `App` and `GameSearch`. Each routes
  Confirm/Back, hints and focus on its own.
- Multi-step flows are re-implemented per screen: `ScanScreen::Page`, `ControllersScreen::Mode::Wizard`,
  `SettingsScreen::WifiStep`, `SettingsScreen::CoreUpdaterDialog`.
- Some hint strings are built inside render code instead of coming from the screen's `hints()`.

DESIGN.md already states the rules. What is missing is code that makes the right thing the easy
thing.

## Goals

1. One way to build each kind of UI. A screen is assembled from framework components.
2. Consistent look *and* behavior (focus, Confirm/Back, hints, scrolling, empty/error/success).
3. Theme, text size, hint size, high contrast and button layout work everywhere without per-screen
   code, because only the framework touches them.
4. No cost in speed: CPU rendering, dirty-region redraw, no per-frame allocation in hot paths.
5. Adding a screen or dialog is mostly composition, not drawing.
6. **Calm by design: abstraction over stimulus overload.** Components show the state the user needs
   in the fewest words and elements, and the framework makes the quiet version the default:
   - Progress and status components show one bar and at most one short plain-language line. Counters,
     file names, percentages, ETAs, spinners and a second detail line are opt-in and justified
     (today's `ProgressPanel` shows title, status, counts, extra line and spinner at once).
   - Text changes at most about once a second; no flicker of rapidly changing values.
   - No decoration, motion or color that does not carry state (see DESIGN.md).
   - Technical detail goes into one small secondary "Details" line, never the headline.

7. **Autonomous.** The framework is a library; MiSTer Pat's GUI is an application that uses it, never
   the other way round (see "Autonomy" below).
8. **Accessible by default (premise).** The framework makes the accessible version the only easy one:
   a contrast registry tested per theme and mode, status components that require a label and an icon,
   Largest-text fit tests, one visible focus, a toast policy, components that describe themselves
   (role, label, value, state, hint). Rules and enforcement table: [ACCESSIBILITY.md](ACCESSIBILITY.md).
9. **International by default (premise).** Every user-visible text is a catalog entry with a stable key;
   English is the source language, not the only one. Layouts survive text expansion, formatting follows
   the locale, plurals follow the language, fonts cover the script, and the layout uses start/end so
   right-to-left can follow. Requirements, catalog design and tests: [I18N.md](I18N.md).

Non-goals: a general-purpose widget toolkit, a retained widget tree with layout solver, animation
framework, or pointer/touch support. Box-art rendering (`Tile`, grid art, `ImageCache`, video
preview) keeps its dedicated fast paths; the framework wraps it, it does not replace it.

## Autonomy: the framework does not know the application

**Direction of dependency.** `application → framework → platform backends`. Nothing under `src/ui-framework/`
includes or links anything from `src/app/` or `src/legacy/`. The application supplies what is specific to it
(palette, texts, data, services) through narrow interfaces; the framework never reaches for it.

**What this rules out in `src/ui-framework/`:** `Game`, `GameSystem`, `Library`, `Favorites`, `History`,
`Preferences`, `Context`, `Wifi`, updater, scraper, launcher or any other domain type; the product
name and brand colors; file paths of the application (`Paths.h`); the application's action names;
application strings (hint wording specific to a feature, "Settings → Library", …).

**What `src/ui-framework/` is allowed to know:** controller-UI concepts that hold for any 10-foot application:
focus, a logical action set, face-button layouts (Confirm is the bottom face button), safe areas,
text-size and contrast settings as *inputs*, the premises in DESIGN.md that are about interaction
rather than about this product.

### Modules

| Module | Contents | Depends on |
| --- | --- | --- |
| `ui-core` | `Geometry`, `Color`, `Image` (pixel buffer with a stable id), `Font` (FreeType: metrics and glyph rasterization), the `Painter` interface, `Clock`; optional image decode helpers (libpng, libjpeg) | libc, FreeType |
| `ui-render-soft` | CPU renderer: `Canvas` (rasterizer, blending, clipping, image blit), damage tracking | `ui-core` |
| `ui-render-gpu` | GPU renderer (textures, glyph atlas, shader-drawn rounded rects), **not planned for the device** | `ui-core`, SDL2 or GLES/EGL |
| `ui-kit` | Tokens, theme engine, primitives, items, containers, overlays, flows, hints; draws only through `Painter` | `ui-core` |
| `ui-platform-fb` | Linux framebuffer present (`Framebuffer`), evdev reader with hold gestures (`Input`) | `ui-core`, `ui-render-soft` |
| `ui-platform-host` | PNG and window present, scripted input; used by tests and the gallery | `ui-core`, a renderer |
| **Application** (`src/legacy/`) | screens, services, palette, strings, `App`, loader | all of the above |

`ui-core` and `ui-kit` are what the rule above protects. Renderers, presenters and input sources sit
behind small interfaces (`Painter`, `Display::present(regions)`, an input source that yields logical
actions), so the same components run on the device, on the Mac and in the gallery, and can run on a GPU
renderer later (see "Rendering is exchangeable").

### What the survey found in the current base

The base is already close: `Geometry`, `Color`, `Canvas`, `Font`, `Image`, `GridView`, `Tile`,
`LoadingIndicator`, `OnScreenKeyboard` and `ErrorModal` include no application header. The leaks, and
how each is closed:

| Leak today | Where | Resolution |
| --- | --- | --- |
| Application action names in the framework enum (`ToggleFavorite`, `RemoveRecent`, `CycleView`, `JumpPrev/Next`, `FaceXPress`, `Quit`) | `Input.h` `Action` | Framework `Action` is generic: directions, `Confirm`, `Back`, `Start`, `Select`, `TabPrev/Next`, and numbered secondary actions (`Tertiary`, `Quaternary`, `ShoulderPrev/Next`). The application names them (`ToggleFavorite = Tertiary`, …) and owns hold gestures that mean something to the product. |
| Theme knows color-theme names, "Silver", Nintendo/PlayStation/Xbox, high contrast + `Paths.h` font files | `Theme.h/.cpp` | Split: **theme engine** in `ui-kit` (token table, state styles, text size, hint size, high contrast, light/dark, contrast helper) and a **palette provider** interface. The application registers its palettes ("MiSTer Pat", Nintendo, …) and font files. The Silver special case becomes a palette property (`labelOnAccent`). |
| `ImageCache` depends on `ArtworkIndex`, `DebugLog` | `ImageCache.cpp` | Framework gets an `ImageSource` interface (request, poll, size policy). The cache with its box-art fast path and worker stays in the application, implementing it. Components never name a cache. |
| `nowMs()` taken from `Input.h` | `LoadingIndicator`, others | Injected `Clock`. |
| `DebugLog` used inside core pieces | `Input.cpp`, `ImageCache.cpp` | Injected logger (or no-op); the framework does not log by itself. |
| `Screen.h` pulls in the whole domain through `Context` and has game-specific hooks (`favoritePath`, `removablePath`, `selectedGame`) | `Screen.h` | Framework `Screen` has only `update`, `render`, `handle`, `hints`, `incremental`, overlay queries. The application's base class derives from it and adds the game hooks and `Context`. |
| `Tile::Content` has favorite and artwork-variant notions | `Tile.h` | `GridItem` is generic: image, label, sublabel, optional mark slot; "favorite" is the application's mark. Artwork variants and sizes are application data. |
| `ButtonLabels` encodes the three layouts | `ButtonLabels.*` | Stays framework (it is controller-UI knowledge); the selected layout is an input. |
| Texts such as "Settings → Library" in empty states | screens | Stay in the application; components take strings. |

### Consequences for the design

- Components take **plain data and callbacks**, never application objects. A `ListItem` of type
  `Switch` gets a label, a bool and a setter, not a `Preferences&`.
- Anything domain-specific is **composed in the application** from framework parts: a "game tile" is a
  `GridItem` configured by the application; the Game options panel is a `Modal` with application
  content; Controllers' pad diagram is an application component drawn with framework primitives.
- The framework's **own demo** (the component gallery, also the golden-image test) builds and runs with
  `ui-core`, `ui-kit` and `ui-platform-host` only, with no file from `src/legacy/`. If it ever needs one, the
  boundary has been broken.
- The framework carries its own tests, its own README and its own build target (`libui.a`), and could
  later move into its own repository or a submodule without changes. That is a possibility, not a plan.
- Naming: a neutral namespace (`ui::`) and directory (`src/ui-framework/`), no product name in identifiers.
  Licence stays GPL-3.0 like the rest.

## Rendering is exchangeable (CPU and GPU)

The framework draws through a small `Painter` interface, not through `Canvas`. Components call painter
primitives; a **renderer** behind the interface decides how pixels are produced. Today's `Canvas` becomes
the CPU renderer. A GPU renderer can be added later without touching a component.

Three independent axes:

| Axis | Choices | Today |
| --- | --- | --- |
| Renderer (how it is drawn) | CPU rasterizer; GPU (textures, shaders) | CPU |
| Presenter (where it appears) | Linux framebuffer (fbdev); DRM/KMS dumb buffer (CPU, page flip, vsync); DRM/KMS + GBM + EGL (GPU); a window; a PNG file | fbdev, PNG |
| Input source | evdev; window events; scripted | evdev, scripted |

The MiSTer DE10-Nano has **no GPU**: the CPU renderer is the production renderer. A GPU renderer would
serve other platforms (a PC, handhelds, boards with a GPU) and an interactive window for development.
Building one is a separate owner decision; the interface only has to keep the door open and stay honest
about it. Offering the GUI on other Linux systems (a Raspberry Pi, a PC) is described in
[PLATFORMS.md](../../../docs/PLATFORMS.md).

### The interface (sketch)

```cpp
class Painter {
public:
    virtual ~Painter() = default;
    virtual Size size() const = 0;
    virtual Capabilities caps() const = 0;            // damageTracking, cheapEffects, ...
    virtual void beginFrame(const Rect *dirty, size_t n) = 0;   // GPU ignores the list
    virtual void endFrame() = 0;
    virtual void pushClip(const Rect &) = 0;  virtual void popClip() = 0;
    virtual void drawBackdrop(const Rect &) = 0;      // the page background for a region
    virtual void fillRect(const Rect &, Color) = 0;
    virtual void fillRoundedRect(const Rect &, int radius, Color) = 0;
    virtual void strokeRoundedRect(const Rect &, int radius, int thickness, Color) = 0;
    virtual void fillGradient(const Rect &, Color top, Color bottom, int radius = 0) = 0;
    virtual void drawImage(const Image &, const Rect &dst, uint8_t alpha = 255, int radius = 0) = 0;
    virtual void drawText(FontRole, int px, Point, std::string_view, Color) = 0;
    virtual void drawIcon(IconId, const Rect &, Color) = 0;     // each renderer draws the glyphs itself
};
```

Text measurement, eliding and wrapping are not on the painter: they use the shared `Font` (FreeType) in
`ui-core`, so a layout is the same on every renderer.

### What components may and may not assume

- Components are **stateless draw functions** (as already required). They must be correct when the whole
  frame is redrawn and when only a clipped dirty region is redrawn.
- **No pixel readback and no background snapshot.** Today's screens restore "the background under a
  changed area" from a stored canvas (`restoreFrom`); that is a CPU-renderer optimization. In the
  framework a screen asks for `drawBackdrop(rect)` and then redraws the items that intersect it.
- **Damage tracking is a capability.** The CPU renderer has it: the application's repaint tracker (old
  and new cursor tile only) and `present(regions)` keep their gain. A GPU renderer reports none: the
  frame is drawn completely whenever something changed and presented on vertical sync.
- **Images are buffers with a stable id.** A GPU renderer caches a texture per id and drops it when the
  image is released; the application's `ImageCache` and the fixed-size prepared box art are unchanged.
- **Icons are semantic** (`IconId`), not paths; the CPU renderer uses its anti-aliased line routines, a
  GPU renderer its own vector drawing.
- No shadows, no scaling of art, no per-frame allocation: the same rules on every renderer.

### The look does not depend on the renderer

A GPU would make focus zoom, drop shadows and scaled art cheap. The framework does **not** use them:
the design rules that exist because of CPU cost (instant border-only focus, no shadow, fixed-size art) are
kept, because calm and consistent also hold on a fast machine. `caps().cheapEffects` may report the
possibility; no component reads it unless an owner decision says so (D29).

### Performance and testing

- The interface costs one virtual call per primitive, a few hundred per frame; the per-pixel loops stay
  inside the CPU renderer, unchanged. To be measured with the first implementation (F6).
- **Golden pixel images only for the CPU renderer.** A GPU renderer differs in anti-aliasing and is
  checked by tolerance or by eye.
- A **`RecordingPainter`** records the draw commands (a display list). It is the second implementation
  that keeps the interface honest from the first day, supports layout tests ("this row draws a marker at
  x, an outline around y"), and can print an ASCII rendering to compare with the component pages' mockups.

### Costs and risks

An abstraction that leaks CPU assumptions (mitigated by the recording painter and, if built, a GPU
renderer); small anti-aliasing differences between renderers; two code paths to maintain if a GPU
renderer is built (so build it only on a concrete need); GPU context and threading rules (uploads on the
render thread, never in the image worker); the GPU renderer and its libraries stay out of the device
build.

### UI toolkits (Qt, GTK)

A toolkit owns widgets, text and input and would replace the look; the framework draws its own look. A
toolkit could at most host a window that presents the renderer's pixels (a presenter). Not planned.

### Decisions (M0)

| # | Decision | Proposal |
| --- | --- | --- |
| D26 | The `Painter` interface in F1 | CPU renderer first; `RecordingPainter` as the second implementation for tests |
| D27 | Further renderers and presenters | A small window presenter for an interactive gallery on the Mac; a GPU renderer (SDL2 or GLES) only on a concrete need |
| D28 | Text metrics | Shared `Font` (FreeType) in `ui-core`, identical layout on every renderer |
| D29 | Look across renderers | Same look everywhere; CPU-cost-driven design rules stay |

## Method: derive, then build

The framework is not designed on paper and then imposed. Components are **derived from the
project**: from what the screens already do, and from the best existing implementation of it.

1. **Survey.** Find every place that draws or behaves the same kind of thing (focus frames, rows,
   dialogs, steps, status lines). Record file, line, measurements and behavior.
2. **Derive.** Group the occurrences. One group is one component; differences inside a group are
   either variants (a real difference in purpose) or inconsistencies (to be removed). Pick the best
   existing implementation as the reference and take its values as the standard.
3. **Specify.** Write the component page (`src/ui-framework/docs/components/`): ASCII mockups of every variant and
   state, measurements, behavior, accessibility. Differences the component removes are decided
   consciously and listed in `_vnext/vnext.md`.
4. **Build.** Implement the component in `src/ui-framework/` from its page and prove it in the gallery on the host.
   The existing screens are not touched; the new GUI uses the component when it builds that screen
   (see "Delivery").
5. **Record.** Each component page lists **Derived from** (the occurrences found) and **Used by** (the
   new-GUI screens built with it so far). A component without a derivation does not belong in the
   catalog.

The component catalog below is therefore a **hypothesis from a first read of the code**, not a
final list. The survey may merge, split, rename or drop entries; names and variants are confirmed
by it. Components that nothing in the project needs are not built.

## Inspiration: what other frameworks offer that we need

Reviewed for ideas, not for adoption: no third-party toolkit fits a 2 × Cortex-A9, no GPU, plain C++
frontend. For each family: what to take, what to leave.

| Source | Take (we need it) | Leave |
| --- | --- | --- |
| **Android TV Leanback** (`GuidedStep`, `BrowseFragment`, `ErrorFragment`, `DetailsFragment`) | The **wizard page** model: title, short description, one list of actions, one focus, optional step breadcrumb. Row-based home as a pattern (our Home). A standard **error page**. Closest match to `WizardPage`, `EmptyState`, `DetailPanel`. | Fragment lifecycle, transitions |
| **tvOS focus engine / 10-foot guidelines** (Apple, Xbox, Steam Big Picture) | **Focus memory** (returning to a list keeps the position; DESIGN.md already says so), **declared focus neighbors** for columns (Settings), safe-area and minimum text sizes for the sofa, focus is the only selection cue. | Parallax, focus zoom (banned by DESIGN.md) |
| **Kodi skin controls and dialogs** | Fixed **dialog kinds**: OK, Yes/No, Progress, Select, Text input, Busy. They map one to one onto `Modal` variants (`Info`, `Confirm`, `Progress`, `Choose`, `Input`) and show the set is complete. Skin **includes/variables**: a component defined once, referenced everywhere. | XML skin language, runtime theming by file |
| **RetroArch menu drivers** (Ozone, MaterialUI, RGUI) | One typed **menu entry model** for the whole settings tree: `{label, sublabel, value, type: action / bool / enum / path / group header}`. Replaces our rows typed by the string `"On"`, `"A"`. Same entry list can render as 3 columns or one list. | Menu drivers per skin |
| **LVGL** (embedded, CPU-rendered) | **Styles per state** (default, focused, disabled, checked) held in one place: our Tokens + a small state-to-style table. **Invalidated areas** for redraw (we already do this by hand: `painted*`, dirty rects). **Focus groups** with encoder/keypad navigation. | Object tree, event system, its memory model |
| **Dear ImGui** (immediate mode) | Ergonomics of immediate calls: `if (listItem(...)) activate()`, a layout cursor (`row`, `column`, `spacing`) instead of hand-computed y offsets, stable IDs for focus. Fits stateless draw functions + value structs. | Mouse-first interaction, its look |
| **Design systems** (Material, Carbon, Fluent) | **Design tokens** and a **type scale** (we have the start in `Theme`), a **state matrix** per component (enabled / focused / pressed / disabled / selected), documented **empty, error and loading** patterns, **contrast checks** per theme. | Elevation shadows, ripple, motion |
| **Compose / SwiftUI / Flutter** | **Slots** (a `ListItem` has a trailing slot: value, checkbox, badge, star), **state hoisting** (the component draws, the screen owns state), a **preview gallery** (`@Preview`, Storybook), **golden-image tests**. | Declarative runtime, recomposition |
| **EmulationStation / Steam / Playnite themes** | Named **view modes** over the same data (grid, detailed list, carousel), consistent metadata in one detail panel. We have this in Games; it should be one component. | Per-theme layout languages |

### Things we need that we do not have, ranked

1. **Component gallery + golden images.** A host-side program (not on the device) that draws every
   component and variant in every state (normal, Largest text, high contrast, light mode, each color
   theme) to PNG with the real `Canvas`, `Theme` and `Font`, and compares against stored images.
   `Canvas` is independent of `Framebuffer` and `tests/Makefile` already builds on the Mac with
   libpng, so this is probably cheap. It replaces "compare screenshots by eye" as the safety net for
   the whole build-out, and it is the living component catalog and the source of the mockups
   (feasibility to be confirmed in M0).
2. **Typed menu entries** for Settings (RetroArch): the largest single de-duplication of the Settings
   screen, and it removes strings-as-types.
3. **Declared focus model**: focus memory, neighbors across columns, one owner of the cursor per
   container. Today cursor and scroll live in loose members that render and input both write.
4. **Layout cursor / stack helpers** (ImGui): removes the literal y-offset arithmetic that breaks at
   Largest text (updater rows, Scan pages, Disc).
5. **State-to-style table** (LVGL, design systems): one place that says what focused, selected,
   disabled and inert-but-selectable look like.
6. **Automated contrast check** of every theme and the high-contrast colors against the surfaces they
   are drawn on (text on `surface`, `textMuted` on `surfaceHi`, accent on `background`), run in the
   test suite.
7. **Slots for list items** instead of a new row function per trailing element.
8. **A standard error page** (Leanback `ErrorFragment`): plain words, what changed, Details line, next
   step; one component serves Modal `Error` and full-screen failure.

### Deliberately not taken

Animation and transition systems, a constraint layout solver, a retained widget tree, scripting or
markup for layouts, runtime-loadable themes, complex text shaping (Arabic, Indic scripts: out of
scope for the first release, see I18N.md), pointer and touch input. Localization is **not** on this
list: it is a requirement. Each costs CPU, code or complexity that the premises
(simple, calm, fast on the device) do not pay for.

## The rule

- **New UI is built from `src/ui-framework/` components.** If a needed component does not exist, define it
  there first: add it to this document (and to DESIGN.md for its measurements), add a mockup under
  `_vnext/mockups/`, then implement it, then use it.
- **Screens do not draw primitives.** Screen code does not call `Canvas::fillRoundedRect`,
  `strokeRoundedRect` or choose radii, thicknesses, spacings or status colors. It declares
  components; the framework draws them.
- **Exceptions are explicit.** A one-off element that really does not fit a component is marked
  `// UI-EXCEPTION: <reason>` at the call site. A check lists all exceptions (see Enforcement), so
  the list stays short and visible. Two exceptions of the same kind mean a missing component.
- **Visual changes happen in the framework.** Changing a radius or a focus style is a one-place
  edit, not a search through screens.

## Architecture

Layers; each only uses the ones below it. Entries are the working hypothesis (see Method); each
gets a **Derived from / Used by** list as the survey and the build progress.

| Component | Derived from (initial leads) | Used by |
| --- | --- | --- |
| FocusFrame | `strokeRoundedRect(... theme.focus)` in Settings, Controllers, Disc, Scan, Arcade, Games, App | — |
| Checkbox | `drawRoundCheckbox` in `SettingsScreen.cpp` | — |
| Hints | `App.cpp` hint bar, `hints()` of each screen | — |
| Modal / ModalStack | `ErrorModal`, `coreUpdaterModalActive_`, `infoModalActive_`, `ScanScreen` panel | — |
| ListItem / List | Settings option rows, Controllers list, update list, Arcade rows | — |
| ResultItem / Result | scan summary, update review, verification lists | — |
| Wizard / WizardPage | `ScanScreen::Page`, `ControllersScreen::Mode::Wizard`, `WifiStep`, `CoreUpdaterDialog` | — |
| GridItem / Grid | `Tile`, `GridView`, Home rows | — |

Layering:

```
 Flows        Wizard, WizardPage
 Takeovers    Splash, CrashScreen, Screensavers
 Overlays     Modal (variants), ModalScreen, ModalStack
 Containers   Screen, Grid, List, Result
 Items        GridItem, ListItem, ResultItem
 Primitives   Surface, FocusFrame, Label, Icon, Checkbox, ProgressBar, Badge, Scrollbar, TabRow, Hints
 Tokens       Radius, Stroke, Space, TextRole, StatusColor  (all through Theme::px / Theme fonts)
```

### Tokens

Named values replace literals. Tokens are the only place a number like `px(8)` may appear.

| Token group | Members (initial) | Replaces |
| --- | --- | --- |
| `Radius` | `row` 8, `panel` 10, `chip` 5, `field` 10 | 4, 5, 6, 8, 10, 12, 16 scattered |
| `Stroke` | `focus` 2, `field` 2, `emphasis` 3 | 2 and 3 scattered |
| `Space` | `rowGap` 6, `panelPad` 28, `titleGap` 16, `lineExtra` 9, `gap` 26 | literals in screens |
| `TextRole` | `title`, `heading`, `tab`, `body`, `small`, `hint` | direct `theme.sizeX()` calls |
| `StatusColor` | `info`/`success` (accent), `warning` (orange), `error` (red), `neutral`, `disabled` | per-screen color choices |

Values start from what DESIGN.md already documents. The inventory step (below) decides the
standard where screens differ today. `error` is separated from `warning`, as DESIGN.md already
plans.

### Primitives

Stateless draw helpers. Each takes a `Rect` and a small description struct and draws exactly one
thing the same way everywhere.

- **Surface** — filled rounded rectangle in a role: `base`, `raised`, `panel`, `field`. Applies the
  theme, light/dark mode and high-contrast borders in one place.
- **FocusFrame** — the one focus outline. Handles high-contrast color, the White-highlight case,
  alpha, and radius from the item kind. Replaces every direct `theme.focus` stroke.
- **Label** — text in a `TextRole`, with elide / single-line / wrap modes. Never clips at the
  Largest text size: it elides.
- **Icon**, **Checkbox** (the round checkbox, promoted from `SettingsScreen.cpp`),
  **ProgressBar**, **Badge** (short status word with icon, never color alone), **Scrollbar**,
  **TabRow** (as specified in DESIGN.md "Tabs"), **Hints**.
- **Hints** — the bottom-bar legend. Components contribute logical hint strings; the framework
  orders them (movement, select/change, confirm, way out), runs `formatButtonLabels()`, and fits
  them. Nothing outside `Hints` calls `formatButtonLabels()` for the legend.

### Items

An item is a stateless description plus a draw routine. Focus, selection and disabled state are
inputs, not something the item stores.

**GridItem** variants:

| Variant | Use | Notes |
| --- | --- | --- |
| `Cover` | game tile with prepared box art | border-only focus, never grows, fixed art size (see DESIGN.md) |
| `Icon` | system icon / text placeholder tile | grows 10 % on focus |
| `Action` | tile that triggers something (e.g. "More") | |

**ListItem** variants:

| Variant | Content | Right side |
| --- | --- | --- |
| `Nav` | label (+ optional description) | arrow |
| `Value` | label | current value, Confirm cycles it |
| `Switch` | label | round checkbox with On / Off |
| `Choice` | checkbox at the left, name, secondary line | optional status badge |
| `Action` | label | none (runs on Confirm) |
| `Info` | label, value | none, not focusable |
| `Group` | heading row | none, not focusable |

**ResultItem** variants — one row of a status or outcome list (scan result, update review,
verification):

| Variant | Meaning | Indicator |
| --- | --- | --- |
| `Done` | completed successfully | accent icon + text |
| `Pending` / `Working` | not yet done / in progress | neutral icon, optional progress |
| `Warning` | completed with a caveat | orange icon + text |
| `Error` | failed | red icon + text; "Details" as a small secondary line |
| `Skipped` | not needed | muted |

Every variant carries a label *and* an icon; the state is never carried by color alone.

### Containers

- **Screen** — the page frame: content area inside the shared top bar and bottom bar, the safe-area
  margins, an optional title/header, the focus owner, the hint contribution, and the empty / error /
  loading states. Replaces the ad-hoc `Screen` subclasses' layout code. Keeps `update`, `render`,
  `handle`, `hints`, and the `incremental` dirty contract that `App` already relies on.
- **Grid** — rows × columns of `GridItem`, fixed cell size per variant, wrap-around and page
  rules, focus movement, scroll to keep focus visible.
- **List** — vertical `ListItem`s: focus, scroll and scrollbar, section groups, item gaps. Selection
  rules (single, multi) are List properties.
- **Result** — vertical `ResultItem`s with an optional summary bar and a next-step area. Read-only
  by default; may be focusable when items open details.

Every container owns its three non-happy states: **empty** (why, and what to do next), **error**
(plain words, what was or was not changed, small "Details" line, controller-operable next step) and
**success**, exactly as AGENTS.md requires. A screen supplies the texts; the layout is the framework's.

### Overlays

- **Modal** — a centered panel over the current screen. One structure for all variants: title,
  body (text, or a `List` / `Result`), optional footer note, and the hints it contributes. Per
  DESIGN.md dialogs have no buttons; actions live in the hint bar.

  | Variant | Purpose | Default hints |
  | --- | --- | --- |
  | `Info` | read something | `B Close` |
  | `Confirm` | approve an action | `A <Action>   B Cancel` |
  | `Error` | something failed | `A Try again   B Close` (or `A / B Dismiss`) |
  | `Progress` | work in progress | `B Cancel` (only if cancellable) |
  | `Result` | outcome | `A Done` |
  | `Choose` | pick from a list | `↑/↓ Move   A Select   B Cancel` |

  Sizes follow DESIGN.md "Dialogs and panels" (short, list, error).
- **ModalScreen** — a modal at near-full-screen size for long content (update review, details).
  Same behavior and stack as `Modal`, `Screen`-style layout inside it.
- **ModalStack** — the single owner of "which modal is on top". Routes input to the top modal first,
  draws all modals in order, contributes the top modal's hints, and makes Back close the top one.
  Replaces `Context::errorActive`, `coreUpdaterModalActive_`, `infoModalActive_` and similar flags.
  Any screen or service raises a modal through it (as `Context::showError` does today).

### Takeovers

Full-screen views that run outside the normal `Screen` frame: **Splash**, **CrashScreen** and the two
screensavers. They are components too, not exceptions: they are built from the same tokens and
primitives (`Surface`, `Label`, `ProgressBar`, logo `Image`), so theme, text size and high contrast
apply.

- **Splash** reports startup progress as a few **abstract phases**, not as a log. Calm over
  informative: the user should feel "it is starting", not watch the machinery.
  - At most three fixed, plain-language phrases, each shown for as long as its phase lasts and never
    changing faster than about once a second: for example **Starting**, **Getting your games
    ready**, **Almost there**. No file names, system names, counts, percentages, or timers; the bar
    already says how far it is.
  - The line is added only when a phase is actually long (the box-art scan or a first-run library
    build). A normal one-second start shows the logo and bar alone, exactly as today.
  - One line, `body` size, `textMuted`, under the bar; no animation, no fade between phrases, no
    hints, no second line. Phase text comes from the framework (a closed set of `StartupPhase`
    values), so a feature cannot add free text to the splash.
  - It keeps its own pacing loop, because it runs before `App`'s render path exists. Shared geometry with other screens (the Systems grid used to preload icons) lives in
  one place that both use. It needs an error path: a failing or timed-out startup task ends in the
  `Error` modal, not a frozen bar.
- **CrashScreen** uses `Panel` and `StatusColor::error`, and takes its text sizes from `TextRole`
  (it ignores the text-size setting today).
- **Screensavers** use the shared `Scrim` token.

### Flows

- **Wizard** — a step sequence presented in a `ModalScreen` or a `Screen`. Owns the step state,
  Next / Back / Cancel behavior, a step indicator ("Step 2 of 4"), a confirmation when cancelling
  would lose work, and the final `Result` page. Steps may be skipped or inserted at run time (for
  example "artwork offer" only when applicable).
- **WizardPage** — one step: a title, a body built from containers and items, the hints it
  contributes, and `canContinue` plus an optional reason ("Pick at least one file to continue.").
  A page may start background work and report progress; the Wizard shows the `Progress` variant
  while it runs.

Existing flows to be expressed this way: first-run scan (`ScanScreen::Page`), controller mapping
(`ControllersScreen::Mode::Wizard`), Wi-Fi setup (`WifiStep`), core updater (`CoreUpdaterDialog`),
Disc setup, update installer.

## Behavior contract

These are guaranteed by the framework so screens cannot get them wrong:

- **Input.** Components receive logical `Action`s. Up/Down move within a list, Left/Right change a
  value or move between columns, A activates, B goes back or closes. The top modal gets input
  first. Hold actions (hold B, hold X) stay in `App`.
- **Focus.** Exactly one focus target per visible container. A new container, tab or category puts
  focus on its first item; returning with Left keeps the position (DESIGN.md). Focus moves instantly.
- **Hints.** Components contribute logical hint strings; `Hints` orders and formats them. A modal's
  hints replace the screen's while it is on top.
- **Theme.** Every color, radius, stroke, font and spacing comes through tokens and `Theme`; high
  contrast, color themes, text size and hint size apply without screen code.
- **Text.** Labels elide; they never overflow or overlap a value, status or action, including at
  Largest.
- **Status.** `StatusColor` + label/icon; success uses the accent, never green.

## Performance

The renderer is CPU-based and redraws only what changed; the framework must not erode that.

- Components are **stateless draw functions plus small value structs**, built on the stack. No
  retained widget tree, no virtual call per pixel, no allocation per frame in `render`.
- Each container reports a **dirty rect** for a focus move, a value change or a scroll step, and the
  existing `incremental()` / `fullRedraw` contract is kept. A focus change repaints two items, not
  the screen.
- Text measurement and elision results are cached per (string, role, width) for rows that do not
  change.
- Box art keeps its fixed-size prepared BMPs and unscaled copy; `GridItem::Cover` calls the existing
  fast path. No new scaled or animated artwork.
- Lists virtualize: only visible items are built and drawn.
- No polling or storage access in components; data arrives by the screen's existing services.

## File layout

```
src/ui-framework/                              the framework; includes nothing from src/legacy/
  README.md                      what it is, how to build and run the demo
  Makefile                       host build of the library and the demo (device build follows)
  core/                          Geometry, Color, Image (buffer with stable id), Painter (the one drawing
                                 interface + Capabilities + TextMeasurer); later Clock, Action, Display, Input
  render/soft/                   the CPU renderer: Canvas, Font (FreeType), SoftPainter
  render/gpu/                    GPU renderer (only if a platform needs it; not for the device)
  kit/                           Tokens, Theme (palette, text roles, tiers, tones), Text (catalog, Text),
                                 Primitives (Icon, Surface, Label, TextBlock, FocusFrame, Checkbox,
                                 ProgressBar, Status), Components (ListItem, Hints, Shell with NavItem
                                 and tabs, EmptyState, Splash)
  platform/host/                 PNG writer (host presenter); later a window presenter, scripted input
  platform/fb/                   Linux framebuffer + evdev backend (device; not built yet)
  lang/                          the framework's own catalogs (en, ...); today the English vocabulary is
                                 registered in kit/Text.cpp
  demo/                          the UI Wireframes demo application (host runner first, device next)
  docs/                          all framework documentation: this design, accessibility, i18n, the demo,
                                 components/ (one page each with ASCII mockups)
  tests/                         framework tests (host build; not built yet)
src/legacy/                             MiSTer Pat's GUI, legacy ("Classic", removed before 0.9.0): its screens,
                                 App, Theme, and the shared services (library, preferences, launcher, ...)
src/app/                             MiSTer Pat's GUI, new ("New"): built on src/ui-framework/; its screens, App, palette,
                                 strings, UI-free helpers; uses the shared services from src/legacy/
```

In the New GUI, `App` owns the `ModalStack`; application services reach it through an
application-side `Context` that the framework does not see. The legacy GUI does not use `src/ui-framework/` at all,
apart from the drawing core, which moved into `src/ui-framework/core` (see "Delivery"). During development both GUIs are
compiled into the branch's library; 0.9.0 contains only the New GUI.

## Enforcement

- A script (`tools/check-ui.sh`, run by `make check`) fails on `fillRoundedRect`,
  `strokeRoundedRect`, direct `theme.focus` strokes and literal `px(` radii or strokes in `src/app/`,
  except lines carrying `UI-EXCEPTION`. It prints the exception count. The legacy `src/legacy/` screens are
  not checked: they go away before 0.9.0.
- **Boundary check:** `tools/check-ui.sh` also fails if any file under `src/ui-framework/` includes a path outside
  `src/ui-framework/` or names an application type, and the build links `libui.a` and the gallery on their own,
  without any file from `src/legacy/`. The dependency direction is enforced by the build, not by review.
- The component catalog (this document, with DESIGN.md measurements) is the checklist for review:
  a new user-facing feature names the components it uses, and any new component, in its vNext
  notes and mockups.
- Every new-GUI screen has a parity checklist against Classic and side-by-side screenshots (Classic
  vs New) in the state matrix.

## Delivery: framework first, then the new GUI on a Git branch

There is **no migration** of the existing screens. 0.9.0 is developed on a long-lived Git branch;
`main` stays the 0.8 line. The delivery order is:

1. **The framework** (`src/ui-framework/`) is built first, with its gallery and tests on the host. It does not touch
   the device GUI.
2. **The new GUI** is built successively in `src/app/`, on the framework, screen by screen, written new
   from the component pages.
3. **During development only**, the branch holds both GUIs and a development switch (Classic / New) for
   side-by-side comparison. At the end the legacy GUI and the switch are deleted.

The strategy, milestones, decisions and release lines are in `_vnext/vnext-0.9.0.md`; the points that
shape the framework design are these:

- **0.9.0 has only the framework GUI.** No Classic, no setting to go back, no crash fallback to another
  GUI. 0.9.0 is published when it is ready; at most one more 0.8.x is published from `main`.
- **Classic is a development reference** on the branch. It may change as convenient (for example, to
  move the drawing core into `src/ui-framework/core`) as long as it keeps building until it is removed.
- **Shared and separate.** Services and data stay (library, game database, favorites, history,
  preferences, launcher, Wi-Fi, updater, scrapers, Disc drive, `ImageCache`, evdev input,
  framebuffer); presentation is replaced. Logic that lives inside legacy screens is rewritten as
  UI-free helpers in `src/app/`.
- **Drawing core.** `Geometry`, `Color`, `Image`, `Font` move into `src/ui-framework/core`, `Canvas` into `src/ui-framework/render/soft`. The legacy
  `Theme` goes with Classic; the framework has its own theme engine and the application provides
  palettes.
- **One preferences store.** The New GUI reads and keeps the user's existing preferences (theme, text
  size, hint size, high contrast, button layout, sounds), so updating to 0.9.0 keeps their choices.
- **A strict release gate.** With no way back, 0.9.0 ships only when the parity checklists, the
  accessibility and i18n checks and the frame-time audit are complete.

### Order of work

The unit of work is **one component** (page, implementation, gallery proof), then **one screen**
(parity checklist, side-by-side screenshots). The order follows dependencies:

1. **Decisions and specification (M0).** Survey done ([UI-INVENTORY.md](../../../docs/UI-INVENTORY.md)), component
   pages written ([components/OVERVIEW.md](components/OVERVIEW.md)); the open decisions are settled and
   the current palettes audited for contrast.
2. **Framework (F1 to F6).** Module, tokens, host backend, contrast registry; primitives; hints,
   toast and text; items and containers; overlays, flows and the screen frame; takeovers and the
   device backend.
3. **New GUI (A0 to A5).** Shell and switch; Settings; browse screens; game flows; setup flows; parity
   review, then removal of the legacy GUI.

Per new-GUI screen: the component pages say what it is built from, the parity checklist says what it
must do, side-by-side screenshots (dark, light, high contrast, 100 / 130 % text) show it, and the
component's **Used by** entry is updated.

## Open questions

- Standard row radius: 8 (most common today, 68 uses) is the proposal; confirm in the inventory.
- Should `Choose` modals and Settings option lists share one `List` implementation? Proposal: yes.
- Does the Disc "Start game" action become a `ListItem::Action` variant or its own `Button`
  primitive? DESIGN.md currently treats it as a separate component.
- Where does the text-entry flow (on-screen keyboard) fit: a `Modal` variant (`Input`) or a
  `WizardPage` body? Proposal: a modal variant.
