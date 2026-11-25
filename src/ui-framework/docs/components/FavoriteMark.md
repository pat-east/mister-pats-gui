# FavoriteMark

The small mark that says "this is a favorite", on cover art, in a list row and in a hint. Use it
wherever a favorite is shown. Do not reuse it for anything else (not "shared", "selected" or "new").

Layer: primitive. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext
milestone M0. The glyph (an asterisk today) and whether the star joins the glossary are open (M0).

## Anatomy

On art (a tile): a backdrop circle keeps the mark legible over any picture.

```
   +------------------------------+
   |                         (*)  |   backdrop circle 30, color: background at alpha 190
   |                              |   mark: bold "*", favorite color
   |            art               |   position: 8 from the top and right of the art body
   |                              |   the art itself is not touched or scaled
   +------------------------------+
```

In a row: bare, no backdrop, at the right.

```
   |   Item name                                                              *   |
                                                                             ^ bold, favorite color,
                                                                                right inset 26
```

In a hint: inline with the hint text, same glyph.

```
   {DPAD} Move   Hold X Add favorite  *
```

## Variants

```
OnArt    backdrop circle + mark       cover tiles, art-sized thumbs
InRow    bare mark                    list rows, result rows
InHint   inline mark                  the hold hint in the bottom bar
```

Only one glyph and one color across all three. Anything else that currently reuses a star (a "shares
mapping" star in a controller list) is removed or replaced by words.

## States

```
favorite        the mark is drawn
not favorite    nothing is drawn; there is no "off" mark and no placeholder space is reserved
                (the row's text reserves a small fixed right slot so text never jumps)
focused row     the mark does not change with focus; the row shows focus
```

```
Row, favorite, focused:
  |#| Item name                                                                *
  ######################################################################################

Row, not favorite:
  |   Item name
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Backdrop (OnArt) | circle 30, `background` at alpha 190 (`Alpha.artBackdrop`) | fixed size: it overlays fixed-size art |
| Mark position (OnArt) | 8 from top and right of the art body | |
| Mark (all) | bold `body`, `favorite` color | `favorite` follows the highlight in high contrast |
| Row slot (InRow) | right inset 26; text reserves 28 | text elides before the slot |
| Glyph | "*" for now (M0) | a vector star primitive is a later option |

The caller supplies: placement (OnArt / InRow / InHint) and the rect. The component decides color,
backdrop and size.

## Behavior

- The mark is state shown, not a control: toggling a favorite is the row's or the options panel's
  action, never the mark's.
- The mark appears in the same frame the state changes; no animation.
- The caller decides whether an item is a favorite (a title with several versions counts if any
  version is marked); the component does not know.
- A function offered as a hold shortcut ("Hold X") also exists as a plain entry in the item's options
  (for example "Favorite: On / Off"); the mark shows the result.

## Accessibility

- The mark is never the only carrier of meaning where the user must act on it: the Favorites tab and
  the item's options name the state in words ("Favorite: On"), and the hold hint names the action.
- Whether the star is a self-explanatory symbol (glossary) is an open question (M0). Until decided,
  treat it as needing a word wherever it is introduced, once per screen.
- Contrast: `favorite` on the row fill 3 : 1 (registry); on art the backdrop circle provides it. In the
  default theme `favorite` is the accent blue; no green.
- Size on art is fixed and not scaled by the text-size setting so it never covers more of the art; in
  rows and hints it follows body text.

## Largest text and high contrast

```
Largest: row text grows, the mark grows with body text, the slot reserves its width

  |#| Item name that is long and elides with an ell...                              *
  ####################################################################################

On art the circle stays 30 px: art is fixed-size and must not be covered more at Largest.

High contrast: the mark and the circle's edge use the highlight color; background alpha 190 stays.
  +------------------------------+
  |                         (*)  |     (*) = highlight-colored mark on a black circle
  +------------------------------+
```

## API

```cpp
enum class FavoritePlacement { OnArt, InRow, InHint };

int  favoriteMarkWidth(Theme&, FavoritePlacement);               // for text to reserve
void drawFavoriteMark(Painter&, Theme&, const Rect&, FavoritePlacement);
```

Stateless. The rect is the art body (OnArt), the row frame (InRow) or the hint position (InHint).

## Derived from / Used by

Derived from: the tile star badge (30 px circle plus "*"), the Games list "*", the hold-hint star and
the controller "shared" star (inventory C11 and FavoriteMark rows; the consolidation reports). Replaces
two different favorite marks and one misused one.

Used by: *none yet.*
