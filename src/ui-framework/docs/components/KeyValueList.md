# KeyValueList

Label/value rows describing one object: version, storage, status, metadata. Use it for read-only
facts. Do not use it for settings the user changes (see [ListItem](ListItem.md)) or for rows that
compare many objects (see `Table`).

Layer: container. Status: **implemented in the wireframes.** Values marked (M0) wait for the owner's decision.

## Anatomy

```
 +-----------------------------------------------------------------------------------+
 |  **System**                                                                        |  heading
 |  ---------------------------------------------------------------------------------  |
 |  Version               1.4.2                                                        |  row
 |  Storage free          7.9 / 120 GB                                                 |
 |                                                                                    |  spacer
 |  **Network**                                                                       |
 |  ---------------------------------------------------------------------------------  |
 |  Connection            (v) Connected                                                |  value + Status
 |  Address               192.168.0.12                                                 |
 +-----------------------------------------------------------------------------------+
    <---- label column ---->  16  <---- value column ---->
    min(widest label, 38 %)
```

- Labels are `Muted`, values `Primary`. Both left-aligned in their columns.
- A value may carry a **Status** (icon + word, see `ResultItem` vocabulary) instead of plain text.

## Variants

One entry type, three kinds, encoded one way:

```
Row      label + value (value may be a Status)
Heading  bold body, Primary, hairline below; not part of any cursor
Spacer   one blank step (6 px), no text
```

This replaces three encodings (`Row.heading`, `Fact{label,""}`, `ChangelogLine.heading`).

## States

```
fits            all rows visible, no scroll bar
scrolls         taller than the area: scroll bar + position text "a-b of n"
empty value     the value shows "Not available" in Muted, never an empty gap
value Status    icon + word + tone (Done, Warning, Error, Neutral...)
```

```
 scrolls:
 |  Version               1.4.2                                                    |  ^
 |  Storage free          7.9 / 120 GB                                              |  |  4 px bar
 |  ...                                                                             |  v
                                                                         3-9 of 21
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Label column | `min(widest label, 38 %)` of the width | the Disc dialog's fixed 330 px goes away |
| Column gap | 16 | |
| Row step | line height + 9 (M0) | was +3 (info), +38 (Disc), +9 elsewhere |
| Heading | bold body, `Primary`, step + 8, hairline 1 px (alpha low) | |
| Spacer | 6 | |
| Label color | `Muted` (not alpha 170) | contrast registry |
| Value | body, `Primary`, elided | Status: bold body in the tone color |
| Inner padding | 28 inside a `Modal`, 24 inside a panel | from the container |

The caller supplies: entries (kind, label, value or Status). The component decides columns,
spacing, elision and scrolling.

## Behavior

- Read-only. No cursor; Up/Down scroll when the content is taller than the area.
- Content that is taller than the area scrolls with the shared indicator; it is never silently
  truncated (rule 1.3).
- Values elide with an ellipsis; labels elide only if longer than their column.
- Hints contributed: `UP/DOWN Scroll` when scrollable; the owner adds `B Close`.
- No per-frame allocation: row values are string views.

## Accessibility

- Label and value are separate columns, not one run of text; `Muted` labels still meet 4.5 : 1
  (rule 1.1).
- A Status value always shows icon and word (rule 1.2).
- Line step +9 and sentence-case headings keep long lists legible (rule 1.8); no all-caps labels.
- At Largest text the label column stays at most 38 %; values wrap by eliding, rows grow.
- Position is shown when scrolling (rule 2.6).

## Largest text and high contrast

```
Largest: the label column hits its 38 % limit and elides before the value

 |  Storage free for the...   7.9 / 120 GB                                         |
 |  Connection                (v) Connected                                         |
```

High contrast: heading hairline and scroll thumb use the highlight color; labels and values keep
their tiers (`Muted` or brighter).

## API

```cpp
enum class KvKind { Row, Heading, Spacer };

struct KvEntry {
    KvKind kind;
    std::string_view label, value;
    const Status *status = nullptr;      // optional: replaces value with icon + word
};
struct KvConfig { Rect area; const KvEntry *entries; int count; };

int  kvContentHeight(Theme &, const KvConfig &);
void drawKeyValueList(Painter &, Theme &, const KvConfig &, int scroll);
```

State is one integer (`scroll`), owned by the screen or the `Modal`.

## Derived from / Used by

Derived from: the system-information dialog and the Settings facts column, the Disc drive status
table (`line(label, value, color)`), the Disc metadata grid, the Controllers detail pane.
Survey: inventory C10b.

Used by: *none yet.*

## Implementation

`drawKeyValueList` in src/ui-framework/kit/Data.cpp: Row, Heading (hairline) and Spacer entries, status values. Shown in the Data tab.
The overload with `KeyValueView` scrolls (`first`, the scroll bar when the list is taller than the area; `scrolls`/`visible`/`total` come back), grows the row with the text size, aligns plain values with status words when the list has any, and `minLabelPercent` gives two pages one value column.
