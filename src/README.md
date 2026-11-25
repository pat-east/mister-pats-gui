# Source layout

All sources live in three trees below `src/`.

```
src/
  legacy/            the Classic GUI and its device code (flat, one class per file), incl. Loader.c
    bridge/          adapters that connect the new GUI to the Classic objects
      settings/      one adapter per Settings category
  ui-framework/      the UI framework: core, render/soft, kit, platform/host, demo, docs, assets, tools
  app/               the new GUI on the framework: tabs, settings/, host/ (PNG runner), docs, PARITY-*.md
```

## Layering rules

- **`src/ui-framework/` knows nothing.** It includes only itself and the standard/system libraries; never
  `app/` or anything from `legacy/`. Its host build (`make -C src/ui-framework`) has the include root `src/` and
  cannot even see the Classic headers.
- **`src/app/` includes only the framework and itself** (`"ui-framework/..."`, `"app/..."`), plus the standard
  library. It reaches the device and the Classic code only through the interfaces it defines (`app/Library.h`,
  `app/settings/*Service.h`, `app/Look.h`, ...). Its host build (`make -C src/app run`) renders every tab from
  sample data without a device.
- **`src/legacy/bridge/` implements the app's interfaces on top of the Classic objects**
  (`AppLibraryAdapter`, `LookBridge`, the settings adapters, and the hosting screens `PatsGuiScreen` and
  `WireframesScreen`). It is the only place that includes both Classic and app/framework headers.
- **`src/legacy/` disappears with the 0.9.0 cleanup**, when the new GUI replaces the Classic GUI. What the new GUI
  still needs from it (adapters, device services) is moved or rewritten behind the app's interfaces then; the bridge
  goes with the Classic code it adapts.

## Include convention

The ARM build compiles with `-Isrc -Isrc/legacy`; the host builds of the framework and the app use `-I..` (that is
`src/`).

| Header of | Written as | Resolves to |
| --- | --- | --- |
| framework | `"ui-framework/kit/List.h"` | `src/ui-framework/kit/List.h` |
| app | `"app/Tab.h"`, `"app/settings/SettingsPage.h"` | `src/app/...` |
| Classic | `"Canvas.h"` (bare name) | `src/legacy/Canvas.h` |
| bridge | `"bridge/PatsGuiScreen.h"`, `"bridge/settings/AdapterEnv.h"` | `src/legacy/bridge/...` |
| tests (`tests/`) | `"../src/legacy/Alphabet.h"` | `src/legacy/Alphabet.h` |

## Builds

| Target | What it builds |
| --- | --- |
| `make` (repository root) | the ARM library and loader: `src/legacy`, `src/legacy/bridge`, `src/ui-framework` (without `demo/main.cpp` and `platform/host`) and `src/app` (without `host/`); objects in `build/obj/release/<tree>/...` |
| `make -C src/ui-framework [run]` | the host library and the UI Wireframes demo (`build/ui-demo`), PNGs in `src/ui-framework/build/out` |
| `make -C src/app [run]` | the host runner of the new GUI (`build/app-render`), PNGs in `src/app/build/out` |
| `make -C tests` | host-side checks of Classic code |

Documents: `src/ui-framework/docs/` (framework design, components, accessibility, i18n), `src/app/README.md` (the
new GUI, its rules and the library interface), `src/app/PARITY-*.md` (the comparison with the Classic GUI),
`src/app/settings/PORTING.md` (the Settings port).
