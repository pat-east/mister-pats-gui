# TextField

A single-line text field, filled by the on-screen [Keyboard](Keyboard.md). Use it for search,
names, passwords and codes. Do not use it for choosing among options (see
[ListItem](ListItem.md), kind `Value`) or for long text.

Layer: item. Status: **implemented in the wireframes.** Values marked (M0) wait for the owner's decision.

## Anatomy

```
 +-----------------------------------------------------------------------------------+  ^
 |  <16>  Search games...                                                            |  | 54 (drawn 50)
 +-----------------------------------------------------------------------------------+  v
   ^ placeholder, body, Muted                     radius 10 (Radius.field)
```

- Text starts 16 px from the left edge, vertically centered, elided at the right edge.
- The field holds only its text. **Hints never sit inside the field** (no "A EDIT" label): they
  belong in the bottom bar.
- A label or title is the screen's business (a `PageHeader` or a caption above), not drawn here.

## Variants

```
Plain      text is shown as typed
Masked     each character shown as "*" (password, key); a Show/Hide toggle reveals it
Search     Plain, in a SearchHeader: placeholder "Search games...", B clears an existing query
```

## States

```
empty, idle            placeholder, Muted                       no outline (rest border: none)
 | Search games...                                                                       |

focused (not editing)  placeholder or text, outline 2 px focus
 ##########################################################################################
 | Search games...                                                                       |

editing (keyboard up)  text + a static underscore cursor, outline 2 px focus
 | abc_                                                                                  |

masked                 asterisks, cursor
 | *****_                                                                                |

disabled               text Disabled tier, no outline, not focusable
 | ~Read only value~                                                                     |

invalid                outline in Warning/Error tone, Status line below the field (icon + word)
 | abc                                                                                   |
 (x) This name is already used.
```

The cursor is a static `_`, no blinking. In high contrast every field also shows the 2 px white border
when idle (Roomy rule, M0), and the focus outline in the highlight color when focused.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Height | 54, drawn 50 (2 above/below) | 20 below before the content |
| Radius | `Radius.field` 10 | |
| Fill | `surface` inside a panel (which is `surfaceHi`); `surfaceHi` on the page background (M0) | the field must stand out from what it sits on |
| Outline | 2 px `focus` when focused or editing | **not `accent`** (the search field used accent) |
| High-contrast border | 2 px white when idle, unless the highlight is White | Roomy rule |
| Text inset | 16 left and right | |
| Placeholder | body, `Muted` | sentence case |
| Value | body, `Primary`, elided | masked: "*" per character |
| Cursor | static `_`, `Primary` | appended to the value while editing |

The caller supplies: value, placeholder, masked, shown, state (focused, editing, disabled), optional
invalid Status. The component decides geometry, fills, outline, masking, elision.

## Behavior

- **Focus the field** with Up from the top of the content (search) or by moving to it.
- **Confirm** opens the Keyboard (editing); **Start** submits; **Back** discards an unsaved
  edit and restores the old value, or clears an existing query on a search field (it passes through
  when empty).
- While editing, input goes to the Keyboard; the field repaints on each change (one row).
- Hints contributed (bottom bar, not in the field): focused `A Edit   B Clear`; editing see
  `Keyboard`.
- A masked field wipes its buffer on delete and on close.

## Accessibility

- The outline is the same `focus` token as every other target and meets 3 : 1; focus is visible
  when editing and when merely focused (rule 1.4).
- Placeholder and value meet 4.5 : 1 against the fill in all themes (contrast registry).
- An invalid state is shown by icon + word + tone below the field, not by an outline color alone
  (rule 1.2).
- Target height 54 px; text elides at Largest, the field does not grow.
- Plain instructions: the placeholder says what to type; the hint bar says which key does what.

## Largest text and high contrast

```
Largest: field height stays 54; long text elides at the left of the cursor's end

 | A long query that is cut with an ellipsis at the end...                              |
```

High contrast: idle field gets the white 2 px border; focused/editing shows the highlight color
outline over it. With a White highlight the white border is omitted.

## API

```cpp
enum class FieldState { Idle, Focused, Editing, Disabled };

struct TextFieldSpec {
    std::string_view value, placeholder;
    bool masked = false, shown = false;
    FieldState state = FieldState::Idle;
    const Status *invalid = nullptr;             // optional: icon + word below the field
};
int  textFieldHeight(Theme &);                                   // 54
void drawTextField(Painter &, Theme &, const Rect &, const TextFieldSpec &);
```

The value buffer is owned by the screen or `Modal` (the `Keyboard` edits it); the field is drawn from
a string view.

## Derived from / Used by

Derived from: the Games search field, the Settings account and Wi-Fi fields. Replaces the accent
focus stroke, the hard-coded white border, the "A EDIT / B CLEAR" text inside the field, and the
copied cursor code. Survey: inventory C14.

Used by: [PageHeader](PageHeader.md) (the search field), [TextEntryDialog](TextEntryDialog.md), the demo Input tab.

## Implementation

`drawTextField` and `TextFieldSpec` in src/ui-framework/kit/Input.cpp: placeholder, value, masked (one `*` per character), focus outline, text cursor while editing, high-contrast border. Shown in the Input tab.
