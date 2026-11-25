# Badge

A status shown as an icon and a word, colored by a tone. Use it to tell the state of an item, a step
or a connection. Do not use a color, a dot or an icon alone, and do not invent status words: the
vocabulary below is closed.

Layer: primitive. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext M0.

## Anatomy

```
 [v] Up to date            [^] Update available        [!] Low space        [x] Failed
  ^   ^                     ^                           ^                    ^
  |   word: body, bold      icon: up arrow, accent      icon: !, orange      icon: x, red
  icon: check, 16, accent
  <8> gap between icon and word
```

The icon and the word are always both present. The tone only colors them. In ASCII `[v]` stands for the
check icon, `[^]` for the up arrow, `[!]` for the exclamation mark and `[x]` for the cross.

## Variants

```
inline   [v] Up to date                       default: icon + word on the row, no fill

pill     ( [v] Up to date )                   fill in a faint tone surface, radius half the height;
                                              only inside list rows that show several statuses (M0)
```

## States

Five tones. The Working look is the Neutral tone with a trailing ellipsis and no spinner.

| Tone | Icon | Color | Words |
| --- | --- | --- | --- |
| Done | check | `accent` | Up to date, Installed, Ready, Saved, Connected, Verified |
| Action | up arrow | `accent` | Update available, Not installed, Changed, Restart to finish |
| Warning | exclamation | `warning` (orange) | Unverified, Incompatible, Missing (required parts only), Low space |
| Error | cross | `error` (red) | Failed, Cannot be checked, Could not be read |
| Neutral | dot | `textMuted` | Not set, Not checked, Offline, Not available |
| Neutral, working | dot | `textMuted` | Checking..., Downloading..., Installing..., Scanning..., Waiting... |

```
[v] Ready        [^] Update available      [!] Unverified        [x] Failed       . Offline
Done             Action                    Warning               Error            Neutral

. Checking...    <- Neutral, working: muted, ellipsis in the word, no motion
```

Done and Action share the accent color and are told apart by the icon and the word, so color is never
the cue. Success is the accent, never green.

## In lists

```
|#| (o)  File name of the item                                   [^] Update available    12.4 MB
|#|      ~path or second line~
|   (o)  Another item                                             [v] Up to date            3.1 MB
|        Third item                                               [x] Cannot be checked
|        Fourth item                                              . Not checked
```

The Badge is the right-hand status slot of a `ListItem` Choice row and of a `ResultItem`.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Icon | 16 | follows the text size at Largest (M0) |
| Gap icon to word | 8 | |
| Word | body, bold | never small for a status the user must read |
| Pill height | 32, radius 16 | pill variant only (M0) |
| Right inset in a row | 16 | |

The caller supplies: tone and word (from the closed vocabulary). The component decides: icon, color,
weight, layout, elision of the word (never of the icon).

## Behavior

- No input, no motion. Working statuses show an ellipsis, not a spinner.
- A status changes only when the underlying state changes, never faster than about once a second.
- An item that cannot be selected shows a Badge and no checkbox; an up-to-date item says so in words.
- Words are chosen from the closed set; a new word is added to this page first.

## Accessibility

- Never color alone: every Badge is icon + word. The word is a real word, readable alone.
- Contrast: word at least 4.5 : 1 and icon at least 3 : 1 against the row surface for every tone,
  in every color theme, dark and light mode and high contrast (contrast registry pairs `accent`,
  `warning`, `error`, `textMuted` on `surface` and `surfaceHi`).
- In high contrast the accent tones take the highlight color; `warning` and `error` keep their hue
  and gain the icon as the second cue.
- Words are plain and short; technical detail goes into a small Details line elsewhere.
- Icons used (check, up arrow, exclamation, cross, dot) belong to the self-explanatory glossary, and
  the word is still shown.

## Largest text and high contrast

```
Largest (130 %): the word elides, the icon stays whole.

  [^] Update avail...                [x] Cannot be che...
```

High contrast: Done and Action in the highlight color, Warning orange and Error red with a white
icon outline, Neutral in the high-contrast muted color.

## API

```cpp
enum class Tone { Neutral, Done, Action, Warning, Error };
struct Status { Tone tone; std::string_view word; };      // word from the closed vocabulary
void drawStatus(Painter&, Theme&, const Rect& slot, const Status&, bool pill = false);
int  statusWidth(Theme&, const Status&);                   // for right-aligned slots
```

## Derived from / Used by

Derived from: the "Shown / Hidden" pill, the updater state words (bold, colored), the Disc drive
status table, Scan and Wi-Fi status lines, the update phase rows, the favorite and preview status
strings (inventory C13, status vocabulary report).

Notes for the New GUI: merge "Missing" and "Not installed" (updater) into Not installed; merge
"Modified" and "Changed" into Changed; "Unavailable" into Cannot be checked; the Disc drive states
map Neutral (no drive, no disc), Neutral working (checking), Done (readable), Warning (unsupported),
Error (unreadable); the top bar disk warning "(!)" becomes Warning "Low space"; the updater success
result becomes Done (it is muted today).

Used by: *none yet.*
