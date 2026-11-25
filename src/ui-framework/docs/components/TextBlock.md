# TextBlock

A wrapped paragraph, and a small rich-text subset for notes. Use it for every text of more than one
line: help, explanations, empty-state reasons, release notes. Do not use it for single lines (see
`Label`) and do not hand-split a sentence over several strings.

Layer: primitive. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext M0.

## Anatomy

```
 +------------------------------------------------------------+
 | Your list could not be saved. Nothing was changed, so your |   body, Muted, left-aligned
 | games are still the same as before. Check that the card    |   line step = line height + 9
 | is not full or locked, then try again.                     |
 +------------------------------------------------------------+
   <- the block is limited to about 60 characters per line; the rest of a wide area stays empty ->
```

Greedy word wrap. A word longer than a line is elided with an ellipsis instead of overflowing.

## Variants

```
plain         one paragraph or several, separated by one blank gap
maxLines      Your list could not be saved. Nothing was changed, so your
              games are still the same as before. Check that the card is...    <- ellipsis on the
                                                                                 last line
rich          a small markdown subset (below)
```

Rich-text subset, nothing more:

```
# Heading one        body bold, Primary          (a title inside the text)
## Heading two       small bold, Muted
**bold** text        bold run inside a line
`code`               backticks are removed, text drawn as normal
- bullet item        bullet, indent 24, wraps with a hanging indent
                     (ASCII shows "-"; the UI draws a round dot)
blank line           paragraph gap
```

```
What is new                                  <- # heading
  - Faster start on slow cards               <- bullet, hanging indent
  - The **Hold** hint now names its action   <- bold run
  - Fixed a crash when the list was empty
Known limits                                 <- ## heading
```

## States

```
normal          wrapped to the area, Muted or Primary tier
Largest         fewer words per line, more lines; box grows, line step stays +9
taller than     the owner shows a Scrollbar and scrolls by lines; the block never silently truncates
the area
empty           draws nothing; the owner shows an EmptyState instead
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Line step | line height + 9 | one value everywhere (replaces +0, +3, +4, +6, +8, +10) |
| Measure | about 60 characters (`Space.measure`) (M0) | the block width is limited to this, left-aligned |
| Paragraph gap | 12 (M0) | between paragraphs |
| Bullet indent | 24 | hanging indent for wrapped lines |
| Roles | body for text read to decide; small for reading text that is secondary (notes) | |
| Alignment | left; never justified; centered only for a short single-paragraph empty state | |
| Case | sentence case | |

The caller supplies: text (plain or rich), rect, role, tier, `maxLines` (0 = no limit). The component
decides: wrap, step, measure, truncation, rich-text runs.

## Behavior

- One wrapper for the whole framework; word wrap, never glyph wrap.
- Returns its height (`measureParagraph`) so containers can size themselves before drawing.
- Scrolling is the container's job: the block draws from a first-line offset (`firstLine`) and reports
  its total line count to a `Scrollbar`.
- No input, no state, no allocation per frame (a reusable line buffer owned by the caller).
- An instruction appears once: not in the body and again in a footer.

## Accessibility

- Short lines (about 60 characters), generous line step, left-aligned, sentence case: the legibility
  rules of the guide (1.8).
- Body text meets 4.5 : 1 on its surface; never directly on artwork.
- Plain words: one short sentence beats a paragraph; detail goes one level deeper (3.6).
- At Largest the block wraps instead of overflowing; nothing is cut off silently; if it does not fit it
  scrolls with a visible position.
- Bold carries emphasis; color is never the only emphasis. Backticks never change the font (no
  monospace, no italics).

## Largest text and high contrast

```
Largest (130 %):
 Your list could not be saved.
 Nothing was changed, so your
 games are still the same as
 before. Check that the card
 is not full or locked, then
 try again.
```

High contrast: text in the high-contrast text color on a flat black surface; no change in layout.

## API

```cpp
struct TextBlock {
    std::string_view text;       // plain, or the rich subset when rich == true
    bool rich = false;
    TextRole role = TextRole::Body;
    TextTier tier = TextTier::Muted;
    int maxLines = 0;            // 0 = unlimited; ellipsis on the last allowed line
    int firstLine = 0;           // for scrolling
};
int measureParagraph(Theme&, const Rect& area, const TextBlock&, int* lineCount = nullptr);
int drawParagraph(Painter&, Theme&, const Rect& area, const TextBlock&);   // returns height drawn
```

## Derived from / Used by

Derived from (inventory TextBlock and RichText rows, 7 wrappers): `ErrorModal::wrap`, `GamesScreen`
`wrapText` and its inline copy, `DiscScreen::wrapText`, `SettingsScreen::wrapHelp`, the changelog
parser (strips `**` and backticks, glyph breaks, step +4) and the release-notes lambda (keeps bold,
breaks mid-word, step +3), Scan `drawParagraph` (hand-split lines, step +6). Line steps found: +0, +3,
+4, +6, +8, +9, +10.

Used by: *none yet.*
