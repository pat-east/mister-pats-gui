# NavItem

A small item in the top-left or top-right region of the `Shell` (and, as `FooterText`, a static text in the
bottom-right region): a clock with its date, disk usage, a network state, a version. Use it for
information about the system or the session that is always visible. Do not use it for content or for the
help (that is `Hints`).

Layer: item. Status: **proposal.** Values marked (M0) wait for the owner's decision.

## Anatomy

One item has a primary line and an optional secondary line, an optional icon, and a tone.

```
Nav left:                                   Nav right:
  10:42            <- primary, Clock role,       Disk 62 %      <- primary, body, Neutral
  Tue 22 Nov       <- secondary, small, Muted    62 of 120 GB   <- secondary, small, Muted
  ^ top at y + 16                                ^ right-aligned, top at y + 18

  (i) Ethernet     <- icon + word, tone Done     (i) Offline    <- icon + word, tone Neutral
      192.168.1.20                                   No network
```

## Variants

```
Text item        10:42 / Tue 22 Nov                 primary + secondary text, no icon
Status item      (i) Ethernet / 192.168.1.20        icon + word, tone: Neutral, Done, Warning
Gauge item       Disk 62 %  [=======...]            text with a thin bar; bar only when it helps
List item        Disk usage / fat:  7% of 128GB     a title and one row per thing, columns lined up (see Rows)
                                  usb0: 80% of 512GB
FooterText       Version 1.4.2                      one static line in the footer region
                 Update available: 1.5.0            (Action tone, accent, still static text)
```

- **Status items** always carry a word and an icon; the tone only colors them.
- **Warning** appears only when it needs the user's attention, with the reason in words
  (for example `(!) Low space`); an item in a calm state stays quiet.
- **FooterText** is static: a version, a build, a license note. It is right-aligned in the bottom bar,
  items separated by the standard gap.

## Rows

An item that lists several things (one row per disk) gives `rows` instead of `secondary`. The three columns of a row
are lined up over all rows: the label left (`fat:`, `usb0:`), the value right (`7%`, `80%`, so the digits end in one
line), the note left (`of 128GB`). The title shares the item's left edge. A row in the Warning tone is coloured and
must say so in its note too (`of 64GB (!)`), never colour alone. Rows that do not fit under the title (the item may
use the empty margin under the bar) end in `...`; the item is as wide as its widest row.

## States

```
normal           text in Primary / Muted tiers on the bar
warning          icon + word in the Warning tone ("Low space"), still the same size and position
unavailable      the item is absent (a missing capability is never a greyed-out item)
too narrow       the item with the lowest priority is dropped first; an item is never clipped
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Primary line | `Clock` role (33) for time, `body` otherwise | `TextRole` |
| Secondary line | `small`, `Muted` tier, 6 below the primary | |
| Item spacing | 40 between items (M0) | right region, from the right edge inwards |
| Icon | 20, gap 10 to the text | glossary icons only (clock, network) |
| Update rate | at most once per second | the clock once per minute; no flicker |
| Footer text | `small`, `Muted` tier, 26 between items | tone Action uses `accent` |

The caller supplies: text, optional secondary text, optional icon id, tone, priority. The component
decides: sizes, positions, dropping, colors, high contrast.

## Behavior

- Display-only in the first version; no focus, no input.
- Values come from the application through a provider interface (a clock, system information); the
  framework never reads the system itself.
- A new value redraws only the item's rectangle. Time and date follow the locale (12/24 hour, order).
- Priority decides what drops first on a narrow screen; the clock and the footer version have the
  highest priority.
- Open (D45): may an item become actionable (for example to open a network chooser)? Proposal: not in
  the first version; the shell stays calm and its focus model simple.

## Accessibility

- 4.5 : 1 for the text on the bar in every theme and mode; the warning tone adds a word and an icon.
- Time and date are plain text with the locale's format; no symbol-only status.
- Sizes follow the text-size setting; the secondary line elides rather than overlaps.

## Strings

| Key | English source | Note |
| --- | --- | --- |
| `nav.disk.low` | `Low space` | warning word, short |
| `nav.network.connected` | `Connected` | status word |
| `nav.network.offline` | `Offline` | status word |
| `footer.version` | `Version {version}` | `{version}` is data |

The text of a nav item is supplied by the application; the demo's items are generic.

## Largest text, other languages and high contrast

```
Largest: the secondary line elides first
   Disk 62 %                 (i) Ethernet
   62 of 120 ...             192.168.1....

German: longer words, same rule
   Speicher 62 %             (i) Verbunden
```

## API

```cpp
enum class NavTone { Neutral, Done, Warning, Action };
struct NavItemSpec {
    Text primary, secondary;      // secondary may be empty
    IconId icon = IconId::None;   // glossary icons only
    NavTone tone = NavTone::Neutral;
    int priority = 0;             // higher stays longer on a narrow screen
};
struct FooterTextSpec { Text text; NavTone tone = NavTone::Neutral; int priority = 0; };
```

## Derived from / Used by

Derived from: the top bar's clock and date block, the network block and the disk-space block
(`TopBar.cpp`), the version and update label in the bottom bar (`App.cpp`).

Used by: *none yet.*
