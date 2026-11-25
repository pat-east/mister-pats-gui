# TextEntryDialog

The one way to type text with the controller: a raised panel over the dimmed screen with a title, the
text field and the on-screen keyboard. Use it for every text entry (search, names, passwords). Do not
build a keyboard flow inside a tab. It uses [TextField](TextField.md) and [Keyboard](Keyboard.md).

Layer: overlay. Status: **implemented** (`src/ui-framework/kit/TextEntryDialog.h`, shown on the Input and Grids tabs).

## Anatomy

```
  (screen dimmed by the dialog scrim)
        +--------------------------------------------------------------+
        |  **Search**                                                  |
        |  +--------------------------------------------------------+  |
        |  | zel_                                                   |  |  field (editing)
        |  +--------------------------------------------------------+  |
        |  +--------------------------------------------------------+  |
        |  |  ! @ # $ % ^ & * ( ) - =               7 8 9          |  |
        |  |  q w e r t y u i o p [ ]               4 5 6          |  |
        |  |  a s d f g h j k l ; ' \               1 2 3          |  |
        |  |  Shift ` z x c v b n m , . /           . 0 -          |  |
        |  |  Symbols  Space  Delete  Clear  Done                   |  |
        |  +--------------------------------------------------------+  |
        +--------------------------------------------------------------+
 + Choose key   X Delete   Y Space   A Type   Start Done   B Back              (bottom bar)
```

One fixed size (1200 design pixels wide, as high as its content) and position (centered on the screen).
The panel is `surfaceHi` (Raised) with `Radius.panel`; the keyboard keeps its own `surface` panel.

## States and results

| Result | When | Value |
| --- | --- | --- |
| None | moving over the keys; typing when `live` is off; a key past `maxLength` | unchanged |
| Changed | a key changed the text and `live` is on | `value()` is the new text |
| Submitted | the Done key or Start | `value()` is the final text (trimmed with `trim`) |
| Cancelled | B | `value()` is the starting value again |

- Live (`live = true`): the owner applies each Changed (a search that filters while typing).
- On submit (`live = false`): the owner reads the value on Submitted only (a name, a password).
- `maxLength`: a key that would exceed it does nothing (default 64, Classic's limit; 0 = unlimited). `trim`: leading and trailing
  whitespace is removed on Submitted.
- A masked field (`masked`) shows one `*` per character and gets the keyboard's Show / Hide key, which reveals the text.

## Behavior

- The dialog takes all input while open; the tabs are locked and the screen behind does not react.
- D-pad moves over the keys, A presses, X deletes, Y inserts a space, Start finishes, B cancels: exactly the inputs
  Classic's keyboard takes (`Action::Up/Down/Left/Right/Confirm/FaceXPress/CycleView`, plus Start and Back handled by
  the caller). Nothing else is needed, so `TextEntryEvent` has no more entries.
- The app maps its events with one helper, `src/app/TextEntryInput.h` (`mapTextEntryEvent`); every tab uses it.
- Hints: D-pad Choose key, A Type, X Delete, Y Space, Start Done, B Back (Classic's settings keyboard wording). The
  bottom bar sorts hints by key rank, so the order is D-pad, X, Y, A, Start, B (Classic printed A before X).
  `pressHint`, `doneHint` and `cancelHint` replace the verbs; the search tabs use Classic's search wording
  (A Enter, Start Search, B Close).
- The dialog draws its own scrim. The owner redraws the bottom bar above it with `hints()` as the help.

## Accessibility

- The hint bar names every key (D-pad icon needs no word); the field says what it is for by the dialog
  title and the placeholder.
- The focused key has fill, outline and a brighter label (Keyboard); the field shows the cursor.
- Text size: the title and the field keep fixed heights and elide; keys use design sizes.
- High contrast: the panel and the field get the white border, the focused key the highlight outline.

## API

```cpp
enum class TextEntryEvent { Up, Down, Left, Right, Press, Delete, Space, Done, Cancel };
enum class TextEntryResult { None, Changed, Submitted, Cancelled };
struct TextEntrySpec {
    Text title, placeholder;
    std::string value;
    bool masked = false, live = true;
    size_t maxLength = 64;       // 0 = unlimited
    bool trim = false;
    Text pressHint, doneHint, cancelHint;   // replace "Type", "Done", "Back" in the hints
};
class TextEntryDialog {
public:
    explicit TextEntryDialog(const TextEntrySpec&);
    const std::string &value() const;
    TextEntryResult handle(TextEntryEvent);
    std::vector<Hint> hints() const;
    void draw(Painter&, const Theme&, const Rect &canvas) const;   // scrim + panel
    Rect panelRect(const Theme&, const Rect &canvas) const;
};
```

A value type like `Modal`: the owner keeps one, replaces it when opening and drops the open flag when
`handle` returns Submitted or Cancelled.

## Derived from / Used by

Derived from: Classic's search keyboard (`GameSearch`) and the Settings text entries (account, Wi-Fi), each with its own
panel, hints and result handling; the keyboard itself is a port of `OnScreenKeyboard` (see Keyboard).

Used by: the demo's Input tab and the Grids tab search.
