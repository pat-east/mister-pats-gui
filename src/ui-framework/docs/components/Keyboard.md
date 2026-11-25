# Keyboard

The on-screen keyboard that edits a [TextField](TextField.md) with the controller. Use it whenever text must be typed.
Do not use it for choosing among options (see [ListItem](ListItem.md)).

Layer: item. Status: **implemented** (`ui::Keyboard` in `src/ui-framework/kit/Input.h`). It is a port of Classic's
`OnScreenKeyboard` (`src/legacy/OnScreenKeyboard.cpp`): the keys, the pages, the navigation and the text rules are the same;
only the drawing uses the framework's tokens.

## Layout (Classic `keys()`)

A 16-unit-wide grid with five rows. The keyboard is columns 0-12, the number pad is columns 13-15 (rows 0-3). Keys are
rounded rectangles; wide keys span 2 or 3 units.

```
Letters page (units 0..12, then the pad at 13..15)
  !  @  #  $  %  ^  &  *  (  )  -  =         7 8 9
  q  w  e  r  t  y  u  i  o  p  [  ]         4 5 6
  a  s  d  f  g  h  j  k  l  ;  '  \         1 2 3
 Shift ` z  x  c  v  b  n  m  ,  .  /         . 0 -
 [Symbols ][  Space   ][ Delete ][ Clear ][  Done   ]        (Done is 3 units wide)

Masked field: the Done key shrinks to 2 units and [ Show ] (2 units) sits before it.

Symbols page (the Symbols key reads "ABC")
  ~  !  @  #  $  %  ^  &  *  (  )  _
  [  ]  {  }  |  \  <  >  ?  :  ;  =
  `  "  '  .  ,  /  ?  \  ~  +  -  _
 Shift z  x  c  v  b  n  m  ,  .  /          (ten keys; the backtick moved to row 2)
```

- There is no digit row: digits come from the number pad, which is the same on both pages.
- Key widths: Shift 1, Symbols 2, Space 3, Delete 2, Clear 2, Show/Hide 2, Done 2 or 3, every other key 1.
- Sizes (Classic `bounds()`): unit at most 70 px (smaller when the area is narrow), gap 4, key height 46, all
  through `Theme::px`; the grid is centered. The panel around it has padding 12.
- Labels: characters use the body size, action keys the small size, all bold. Key words come from the catalog
  (`keyboard.shift`, `keyboard.symbols`, `keyboard.letters`, `keyboard.space`, `keyboard.delete`, `keyboard.clear`,
  `keyboard.show`, `keyboard.hide`, `keyboard.done`) and elide when a translation is long.

## Behavior (Classic `moveCursor()`, `handle()`)

- **Focus.** The cursor starts on the first key (`!`). Pressing the Symbols key moves it back to the first key.
- **Left / Right** move to the nearest key (by key center) in the same row that lies in that direction. In rows 0-3
  this crosses from the keyboard into the number pad and back. Where no key lies further, the focus stays where it is: Left and Right stop at the ends of a row and do not jump to the
  opposite end (owner decision 2026-11-24; Classic wrapped). Up and Down still wrap within their block.
- **Up / Down** move to the key whose center is nearest in the row above or below, staying in the block: the pad and
  the keyboard never swap. The keyboard wraps between row 0 and row 4, the pad between its rows 0 and 3.
- **A** presses the focused key:
  - a character is appended (upper-cased when Shift is on and it is a letter); nothing happens when the text is at
    `maxLength` (64 in Classic);
  - **Shift** toggles upper case and stays on until pressed again (it does not reset after a letter); it has no effect
    on symbols and digits;
  - **Symbols / ABC** switches the page;
  - **Space** appends a space (limit applies); **Delete** removes the last character; **Clear** empties the text;
  - **Show / Hide** (masked fields only) reveals or hides the text; the key reads "Hide" while revealed;
  - **Done** submits.
- **X** deletes the last character, **Y** appends a space, from anywhere on the keyboard. **Start** and **B** are the
  dialog's (Done and Cancel). Directions repeat while held by the input layer, as everywhere.
- **Text rules.** Text is append-only at the end: there is no caret movement and no insertion. `setValue` cuts to
  `maxLength`. A masked text is overwritten with `'\0'` as it shrinks and when cleared or destroyed.
- Result: `None`, `Changed` (the text changed), `Submitted` (Done key).

## States

A key has focus, toggle and normal states; focus is never color alone.

```
normal        surfaceHi fill, label Muted
focused       accent fill (alpha 100) + focus outline + label Primary
toggled on    accent fill (alpha 55) + label Primary: Shift while upper case, ABC on the Symbols page,
              Hide while the text is revealed (the label also changes for the last two)
```

High contrast: the same, with the highlight outline on the focused key and the white border on the panel.

## Accessibility

- A visible focus outline plus the fill; the label brightens on focus (rules 1.4, 1.1).
- Characters are body size; the keys are 46 design pixels high. At 115 and 130 % text the labels grow inside the
  fixed keys and elide (action words) rather than overflow.
- Fully controller-operable with a closed, predictable movement (Up/Down wrap within the block, Left/Right stop at the row ends); the hint bar names every shortcut.
- Contrast: label and fill colors are palette tokens, checked by the contrast registry.

## API

```cpp
enum class KeyResult { None, Changed, Submitted };
class Keyboard {
public:
    explicit Keyboard(size_t maxLength = 64);
    void setValue(const std::string &);  const std::string &value() const;   // setValue cuts to maxLength
    void setMaxLength(size_t);
    void setMasked(bool);  bool masked() const;   // setMasked(true) adds the Show key; masked() = hidden right now
    void clear();                                 // wipes the text
    void move(int dx, int dy);                    // D-pad
    KeyResult press();                            // A
    KeyResult backspace();                        // X
    KeyResult space();                            // Y
    int  height(const Theme &) const;
    void draw(Painter &, const Theme &, const Rect &area) const;
};
```

Apps use it through [TextEntryDialog](TextEntryDialog.md), which owns one.

## Differences from Classic

- The text field is the framework's [TextField](TextField.md) (a caret bar after the text instead of a trailing `_`;
  an empty field shows the placeholder without a caret).
- Colors: the keys are `surfaceHi` on a `surface` panel instead of `surface` alpha 180 on alpha 235; the focused key
  gets the framework's focus outline in addition to Classic's fill.
- State is not kept between openings: the dialog is created per entry, so the focus starts on the first key and Shift
  and the Symbols page start off (Classic's long-lived keyboard objects kept them).
- Hints: see TextEntryDialog.

## Derived from / Used by

Derived from: `OnScreenKeyboard::keys`, `moveCursor`, `handle`, `clear`, `setMasked`, `bounds`, `render`, and its callers
`GameSearch`, `SettingsScreen::handleAccountEntry` and `handleWifiFlow`.

Used by: [TextEntryDialog](TextEntryDialog.md) (the keyboard); the demo Input tab.
