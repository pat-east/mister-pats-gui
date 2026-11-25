# Pat's UI Framework

A controller-first UI framework for 10-foot interfaces: components, a CPU renderer, a main-screen layout,
and a demo application that shows every component. It includes nothing from the application or the Classic GUI (`../app`, `../legacy`);
headers are named `"ui-framework/kit/List.h"` (include root `src/`). Documentation is in [docs/](docs/UI-FRAMEWORK.md);
the three source trees are described in [../README.md](../README.md).

## Build and run the demo (host)

```sh
make -C src/ui-framework          # builds src/ui-framework/build/ui-demo (needs FreeType and libpng via pkg-config)
make -C src/ui-framework run      # renders the splash and every tab to src/ui-framework/build/out/*.png
src/ui-framework/build/ui-demo --out DIR --height 720 --text 130 --pseudo --warning
```

- `--text 100|115|130`: the text-size setting.
- `--pseudo`: the pseudo-locale (accented, about 40 % longer, in markers) to expose literals and clipping.
- `--warning`: shows the nav items in their warning state.
- Fonts: on the Mac, Arial; set `UI_FONT_REGULAR` and `UI_FONT_BOLD` to use other files.

## Layout

| Directory | Contents |
| --- | --- |
| `core/` | geometry, color, image buffer, the `Painter` interface, the SVG renderer |
| `render/soft/` | the CPU renderer (Canvas, Font, SoftPainter) |
| `kit/` | tokens, theme, text catalog, primitives, icons, components, grid, the shell |
| `platform/host/` | PNG writer |
| `demo/` | the UI Wireframes demo |
| `docs/` | design, accessibility, i18n, demo, component pages |

## Status

Started 2026-11-22. Built so far: the core, the CPU renderer, the text catalog with the pseudo-locale,
primitives (icon, surface, label, text block, focus frame, checkbox, progress bar, status), list items, hints,
the shell (tabs, nav items, help, footer), the splash and an empty state; the demo renders the splash and
the tabs Overview, Text and Lists (the others are placeholders). Not yet: dialogs, grids, flows, input
components and the framework's own device presenter (framebuffer, evdev).

On the MiSTer the demo runs inside the Classic GUI: **Settings, Developer, UI-Framework Wireframes**
(`src/legacy/bridge/WireframesScreen.cpp` hosts it; the library build compiles `src/ui-framework/` in).
