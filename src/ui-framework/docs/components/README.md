# Component pages

One page per framework component (see [UI-FRAMEWORK.md](../UI-FRAMEWORK.md)). A page is the
orientation point for a developer, and for an AI assistant, who is about to use, change or extend a
component: what it is, what it looks like in every variant and state, what its numbers are, how it
behaves, and who uses it.

**The ASCII mockups on these pages are the specification of the look.** They are plain text so they can
be read, diffed and reviewed in a terminal, in a pull request and by an AI. Where code and a page
disagree, that is a bug in one of them: decide which one is right and fix the other in the same change.
Pictures (the gallery's rendered images, screenshots) are verification, not the source of truth.

These pages live in `src/ui-framework/docs/components/`, part of the framework. They contain no application specifics:
examples use neutral labels. Local planning mockups for application features stay in
`_vnext/mockups/` (ignored by Git, see AGENTS.md); component pages are durable and tracked.

## Rules

- **Every component has a page before it has code** (UI-FRAMEWORK.md, "The rule"). A new variant or
  state is added to the page first.
- **Every variant and every state is drawn.** At minimum: normal, focused, selected, focused and
  selected, disabled, empty, error where they exist for the component; Largest text and high contrast
  where the layout changes.
- **Numbers live in annotations, not in the drawing.** Mockups are schematic, not to scale. Sizes are
  design pixels on the 1920 × 1080 canvas and use token names (`Radius.row`, `Space.panelPad`) once the
  tokens exist.
- **Keep the drawing small and the text exact.** Width at most 110 characters. One idea per block.
- **Text in mockups is real UI text**, in English (the source language, see [I18N.md](../I18N.md)), as it would appear, including hint strings in their
  logical form (`A Confirm   B Back`).
- A page is short. If it needs more than about 200 lines, split the component.

## ASCII conventions

Use plain ASCII only (no box-drawing or emoji characters) so every tool renders it the same.

```
+------+   a frame (surface or panel)             |  vertical edge of a frame
|      |                                          -  horizontal edge
+------+

[A Label]   text exactly as shown                 ( ) round checkbox off      (o) round checkbox on
~muted~     muted text (textMuted)                |#  4 px accent marker bar at a frame's left edge
**bold**    bold text                             ##  2 px focus outline around a frame (drawn as a line of #)
==========  filled bar / progress fill            ..........  progress track / empty space
<---18--->  a measured distance in design px      ^  or  v  scroll indicator
```

Mark a drawn element with a short label on the line below or beside it, using `<-` or `^`, for
example `^ marker 4 px, accent`. Name tokens, not colors, wherever a token exists (`focus`, `accent`,
`surfaceHi`).

## Page template

````markdown
# <ComponentName>

One sentence: what it is and when to use it. One sentence: when not to.

Layer: primitive / item / container / overlay / flow / takeover. Status: proposal / agreed / implemented.

## Anatomy
<ASCII mockup of the default variant with dimensions as annotations>

## Variants
<one small mockup per variant, with the difference named>

## States
<matrix or stacked mockups: normal, focused, selected, focused+selected, disabled, ...>

## Measurements
<table of token or px values; what the caller supplies vs what the component decides>

## Behavior
<input (logical actions), focus, scrolling, hints it contributes, Back/Confirm>

## Accessibility
<contrast pairs, second cue, Largest text behavior, target size, glossary symbols used>

## Strings
<keys the component owns, English source text, translator note; no fixed length>

## Largest text, other languages and high contrast
<mockup of what changes; where it matters, the German-length variant>

## API
<short signature, value struct + draw function, who owns state>

## Derived from / Used by
<the occurrences it replaces; screens built so far>
````
