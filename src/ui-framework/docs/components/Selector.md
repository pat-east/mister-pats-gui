# Selector

One value out of a short list, stepped with Left and Right: a compact field with a chevron on each side and the
value centered between them. Use it on a page where a list would be too much for a handful of choices (for
example a system to prepare). In a row of a settings list a cycling `Value` row does the same job.

Layer: item. Status: **implemented** (`kit/Selector.h`, `Selector.cpp`).

## Anatomy

```
 System                                  small caption above, drawn by the caller (muted)
 +-----------------------------------------+
 |  <            All systems             >  |    56 high, 520 wide, radius 10, chevrons 22, pad 16
 +-----------------------------------------+
```

## States

```
focused    focus outline, primary text and chevrons
resting    muted text and chevrons
no step    a chevron that cannot step (the end of a list that does not wrap) is drawn faint
```

High contrast adds the white border like every field.

## Behavior

The caller steps the value on Left and Right (wrapping or not) and names the keys in the help
("Left/Right Choose ..."). The Selector draws only.

## Accessibility

The chevrons are a second cue next to the value; the keys are named in the help bar; text is `Primary` or `Muted`
tier, so the contrast report covers it; the focus outline is the framework's.

## API

```cpp
struct SelectorSpec { Text value; bool focused = true; bool canStepLeft = true; bool canStepRight = true; };
int selectorHeight(const Theme &); int selectorWidth(const Theme &);
void drawSelector(Painter &, const Theme &, const Rect &r, const SelectorSpec &);
```

Used by: the application's Settings pages (for example "Prepare box art"). Derived from Classic's "<  All systems  >"
picker.
