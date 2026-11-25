# Accessibility Guide

Accessibility is a premise **defined by the UI framework** ([UI-FRAMEWORK.md](UI-FRAMEWORK.md)), not
by any application. The framework carries this guide as part of itself (it lives in `src/ui-framework/docs/`), and an application built on the framework inherits accessibility by using it:
components are accessible by construction, the framework's tests check it, and anything an
application builds outside the framework is a listed exception that has to meet the same rules. The
application's own audit (what fails today) lives with the application, in
[UI-INVENTORY.md](../../../docs/UI-INVENTORY.md).

This is not a feature added at the end. The framework makes the accessible version the default, so a
screen cannot easily get it wrong.

The framework is built for controller-operated interfaces on a television, viewed from a distance.
The guide follows the four classic principles (perceivable, operable, understandable, robust) and
states each as a few rules that can be checked. Numbers come from WCAG 2.x level AA where a standard
exists, and from the framework's design tokens where it does not.

## Who this is for

People who see less or differently (low vision, color-vision differences, glare or a dim room, a
distant screen), who use the controller differently (one hand, limited strength or speed, tremor),
who read or think differently (dyslexia, attention, cognitive load, a second language), and everyone
in a situation that behaves like that: a bright room, a tired evening, a visiting guest. Designing for
the first group improves the product for all of them.

## Core features

### 1. Perceivable

**1.1 Contrast is a property of the palette, not of a screen.**
- Text against its actual background: **4.5 : 1**; large text (design size ≥ 24 px, or bold ≥ 19 px)
  **3 : 1**.
- Non-text parts that carry meaning (focus outline, checkbox outline, progress fill and track, row
  and field borders, selected markers, icons): **3 : 1** against what they sit on.
- Measured against the real surface in every combination: every color theme × dark/light × high
  contrast × each high-contrast highlight color, including gradients and any alpha applied to text.
  Muted text and alpha "tiers" must still pass; a muted style that fails is not allowed, however calm
  it looks.
- Disabled and unavailable states may be lower but must stay readable (≥ 3 : 1) and explained.

**1.2 Never color alone.** Every status, selection and focus has a second cue: a word, an icon, a
shape, a position (checkbox dot, marker bar, "Selected", "Missing", "Hidden"). Success is the accent,
warning is orange, error is red, and each also carries its label and icon. Red/green pairs are not
used.

**1.3 Text is large enough and scales.** Body text (everything read to decide or confirm) is at least
the `body` role (a token, 21 px on a 1080p design canvas); small is for secondary detail only. The three text-size
steps (100 / 115 / 130 %) and the hint-size steps work everywhere, and **nothing clips, overlaps or
leaves its box at Largest**: text elides or wraps, boxes grow within their presets, panels have no
fixed heights that text can outgrow.

**1.4 Focus and selection are unmistakable, and they are different things.** The user must be able to
read two facts at a glance, from the sofa: *where am I* (focus) and *what is chosen* (selected, on,
current).
- **Focus:** exactly one focus target is visible at all times. The cue is strong and does not depend on
  hue: a 2 px outline in the `focus` token plus a marker bar (3 : 1 or better), the same treatment on
  every kind of target (tile, row, button, field, key, tab). In high contrast it takes the chosen
  highlight color and rows also get borders. Focus moves in the same frame as the input and is never
  hidden by an overlay, a scroll or a dimmed layer.
- **Selection:** a chosen, on or current element carries its own cue (checkbox dot, "Selected", the
  active tab underline) that stays visible when the element is not focused, and is not mistaken for
  focus. An element that is both focused and selected shows both.
- **Target size:** a focus target is at least about 44 px high on the design canvas; small targets are
  grouped into larger rows.
- Emphasis is sufficient, not loud: strong enough to find without searching, without flashing, glow
  or size changes (see 1.6).

**1.5 Color-vision and glare choices.** High contrast offers several highlight colors so each person
can pick the one they see best (existing). The default theme is blue-accented; no theme introduces
green for success.

**1.6 Motion and flashing are optional.** Nothing essential is conveyed by motion. No element flashes
more than three times per second. Focus moves instantly; fades are short and decorative only. A
"reduce motion" choice is added if any longer animation is ever introduced.

**1.7 Sound is never the only carrier.** Every sound cue has a visible counterpart, and sounds can be
turned off (existing). Nothing important is announced by sound alone.

**1.8 Text is legible, not just large.** Readability is more than size:
- A clear sans-serif face with distinct letterforms, regular and bold only; no light weights, no
  decorative or condensed faces, no italics for content.
- Generous line spacing in text blocks (one shared step), short lines (wrap at roughly 60 characters,
  never full-width paragraphs on a wide screen), left-aligned, sentence case. No all-caps for anything
  longer than a short label, no centered paragraphs.
- Text sits on a flat surface that meets the contrast rule; never directly on busy artwork or
  gradients that change the contrast under the letters. No text baked into images.
- Numbers, units and states are written the same way everywhere (one size format, one date format).
- Letters, line breaks and truncation are predictable: elide with an ellipsis, never cut mid-glyph,
  never overlap neighbours.

### 2. Operable

**2.1 Everything works with the controller alone**, in logical actions, with the user's own
face-button layout. No function needs a keyboard, pointer or a button combination not shown in the
hint bar.

**2.2 No hold-only gestures.** A function reached only by holding a button for a while must also be
reachable by a normal press through a menu or options entry. Holds are shortcuts, never the only way,
and they show progress.

**2.3 No dead ends, no lost focus.** Back always leaves the current context; every list item is
reachable (no rows beyond the visible area without scrolling); focus returns to where it was when a
dialog closes; focus is never trapped in a flow without a cancel.

**2.4 No time pressure.** Nothing important disappears by itself. Toasts carry only non-essential,
repeatable information and stay long enough to read (duration scales with length, never shorter than
the 3.5 s default for a full sentence). Anything the user must read, decide or act on stays until
dismissed. Timeouts that must exist (an idle countdown, a search that gives up) are named in the
text where the user reads them, can be restarted, and never lose work silently.

**2.5 Predictable and forgiving.** The same button does the same thing everywhere (A confirms, B
goes back, Start is the shortcut to continue). Destructive or hard-to-reverse actions ask first and say
what will change; repeated or accidental presses do no harm; long work can be cancelled.

**2.6 Efficient.** Long lists support jumping (letters, pages); a list shows where the user is
("12–21 of 340" and a scroll bar); the hint bar lists exactly what the current state offers.

### 3. Understandable

**3.1 Plain language, in every language** (see [I18N.md](I18N.md)). Short sentences, familiar words, sentence case, one term per concept ("Back",
"Close", "Cancel" each with one meaning). No jargon in a headline; technical detail goes into one small
"Details" line.

**3.2 Say where I am and what happens next.** Every screen and dialog has a title; flows show their
step ("Step 2 of 4") when they have steps; hints name the next action; an empty screen says why it is
empty and what to do; progress says what is happening in one calm line.

**3.3 Errors are explained, not announced.** Plain words for what went wrong, whether anything was
changed, what to try, and a controller-operable next step. Never a raw error string, path or errno as
the headline.

**3.4 Calm, consistent, reduced.** Fewer elements, fewer words, no flicker of changing numbers, one way
to do each thing. Consistency is itself an accessibility feature: what is learned once works
everywhere. This is why the framework exists.

**3.5 Settings explain themselves.** Each setting has a short description of what it changes, in the
same place on every screen.

**3.6 Enough words, where they are needed.** Words are an accessibility feature, and so is leaving them
out when they add nothing.
- *Where needed:* a symbol whose meaning is not obvious to a first-time user has a text, and no
  function, status or setting is carried by such a symbol alone. Every setting has a short description of
  what it changes. Every status has a word. Every empty, loading and error state says what is going on
  and what to do. A button hint names the action, not only the key.
- *Self-explanatory symbols need no text.* Things people already know from the controller and from
  everyday interfaces stand for themselves: the D-pad cross, the face and shoulder buttons (drawn in the
  user's layout), a checkbox, scroll arrows and the scroll bar, a magnifier for search, a clock, a
  network symbol. The framework keeps this as a small, closed **glossary of self-explanatory symbols**; a
  symbol outside it gets a text next to it. The glossary stays short on purpose. When in doubt, a
  first-time user decides, not the developer.
- *Only where needed:* when position, grouping or a title already says it, no extra sentence. One short
  sentence beats a paragraph; an instruction is said once (not in the body and again in a footer); the
  headline carries the point and detail goes one level deeper. Do not label the obvious (no "D-pad" next
  to the D-pad symbol, no "Checkbox" next to a checkbox).
- *Test:* "Would a first-time user, reading from the sofa, miss this word?" If yes, add it; if no, remove
  it. Where the two rules collide, keep the screen calm and move the extra text into a "Details" line or
  the option's description.

### 4. Robust

**4.1 One source of truth for style.** Contrast, size and focus rules live in tokens and components
(`src/ui-framework/`), so a theme, a text size or a high-contrast change applies everywhere with no per-screen
work. Screens that bypass the framework are listed exceptions.

**4.2 The UI is data first.** Each component knows its role, label, value, state and hint (for
example `Switch "Dark mode", On, focused`). That costs nothing at draw time and keeps the door open
for spoken output or other assistive output later without redesigning screens (see open questions).

**4.3 Tested automatically.** Contrast, Largest-text fit, focus visibility and status-with-label are
checked by tests, not by memory (see "How the framework enforces this").

### 5. Usability and experience

Accessible design and good usability are the same work seen from two sides: what is hard for one
person is tiring for everyone. These rules keep the *experience* of using the interface part of the
guide, not a separate taste question.

**5.1 Immediate, honest feedback.** Every input produces a visible response in the same frame (focus
move) or within about 100 ms. Work that takes longer shows a calm busy state and can be cancelled.
Input is never silently swallowed: a disabled action says why or is not offered; a successful action is
confirmed in one short line.

**5.2 Responsiveness is accessibility.** Lag and stutter hurt most the people who are slower, more
careful or less precise. No expensive work in the render or input path, stable frame times, no
layout that jumps when data arrives (reserve the space, then fill it).

**5.3 Recognition over recall.** The state is visible, not remembered: the hint bar lists exactly what
the current state offers, labels name what the action does, defaults are shown as defaults.

**5.4 One pattern, learned once.** The same component behaves the same everywhere: same focus, same
Back, same list scrolling, same dialog shape. This is the main reason the framework exists.

**5.5 Short paths for frequent tasks.** Few steps, sensible defaults, remembered positions, shortcuts
as an *addition* to a plain path, never instead of it. Fewer settings with safe defaults beat many
options.

**5.6 Steady and tidy.** Alignment, spacing and timing are regular; nothing flickers, jumps or moves
without a reason; the screen has one clear thing to do.

**5.7 Premises in conflict.** When "calm and reduced" and "complete information" pull apart, keep the
screen calm and move detail one level deeper (a "Details" line, an options panel), never remove
information the user needs to decide.

## How the framework enforces this

| Guide rule | Enforcement in `src/ui-framework/` |
| --- | --- |
| 1.1 Contrast | The theme engine ships a **contrast registry**: every pair the components draw (`text` on `surface`, `textMuted` on `surfaceHi`, `focus` on `background`, `accent` on `surface`, `warning`/`error` on `surfaceHi`, checkbox outline, progress track vs fill, …) with its required minimum (4.5, 3 for large or non-text). A test takes a palette provider and fails for any theme × dark/light × high-contrast color × pair that falls short. The application's palettes are tested in the application's tests with the framework's checker. Named alpha tiers are part of the registry, so a "muted" tier cannot silently fail. |
| 1.2 Not color alone | `Badge`, `ResultItem`, `StatusLine` and `Switch` take label and icon as required parameters; the tone only colors them. There is no API for a color-only status. |
| 1.8 / 3.6 Legible, enough words | `Icon` takes a text label unless the symbol is in the framework's closed glossary of self-explanatory symbols (D-pad, face and shoulder buttons, checkbox, scroll arrows, search, clock, network); `Badge`, `Switch` and status components always carry text; every settings entry requires a description; all text blocks go through the one wrapper (shared line step, measure limit, sentence case); the gallery flags icon-only elements outside the glossary, text over imagery without a surface, and abbreviations not in the glossary of terms. |
| 1.3 Size and Largest text | Components measure and elide or wrap; presets, not fixed heights. The gallery renders every component and state at 100 / 115 / 130 % and a test fails on clipped or overlapping text. |
| 1.4 Focus and selection | One `FocusFrame` and one `SelectionMark` primitive, one owner of the cursor per container. Invariant tests: after any scripted input there is exactly one visible focus; a focused-and-selected item renders both cues; a selected, unfocused item still renders its cue; every focus target meets the minimum height. The gallery renders every component in focused / selected / both / neither. |
| 5.1 Feedback | Host test harness counts frames between a scripted input and the repaint of the affected area (must be the same frame for focus moves); components that can be inert take a `reason` string. |
| 5.2 / 5.6 Steady | Components reserve their space (no layout shift on data arrival); the gallery renders loading, empty and filled states at equal size. |
| 1.6 Motion | No animation primitive in the framework beyond the existing instant focus and the short, optional fade; the check script lists any use. |
| 2.2 / 2.3 | `ListItem` actions and the hint model make hold-only functions visible in review; a container test walks the whole list and confirms every item is focusable and visible by scrolling. |
| 2.4 Time | `Toast` has a duration policy (length-based, minimum) and a flag for essential content that turns it into a `Modal` instead. |
| 3.x Language | The hint vocabulary and status words are a closed set in the framework; the modal, empty-state and error-page components have one fixed structure (what happened, what changed, what to try, next step). |
| 4.2 Data first | Every component exposes `{role, label, value, state, hint}` through one small interface (`Describable`), used by the gallery today and available for output later. |

## Process rules

- Every new component and screen is checked against this guide before it is considered done; the
  feature's mockups show Largest text and high contrast where layout could change.
- A change that lowers a contrast ratio, removes a second cue, or adds a hold-only action needs an
  owner decision, recorded in vNext and DESIGN.md.
- When two options are equally good, choose the one that is easier to perceive, operate and
  understand.
- Accessibility defects found in testing are fixed in the milestone that touches that screen, not
  deferred to a cleanup.

## Open questions (owner decisions)

1. **Spoken output.** The target device has audio but no screen reader. Is optional speech output
   (text-to-speech or prerecorded phrases for the closed hint and status vocabulary) a goal? The
   `Describable` interface keeps it possible; the cost and the audio path must be checked before
   promising it.
2. **Reduce motion setting.** Only if longer animations are introduced; not needed today.
3. **Replacement for hold gestures.** Proposed rule for applications: every function offered as a hold
   shortcut also exists as a plain entry in the item's options; the framework's `ListItem` and hint
   model make a hold-only function visible in review.
4. **Toast policy.** Decided and implemented in `Toast::duration()`: 3.5 s for a short line, +1 s per additional 40 characters,
   essential content never a toast.
5. **Alternative font option.** Is a further, more legible-for-dyslexia font choice wanted next to the
   default and the built-in fallback?
6. **Contrast registry strictness for themes.** Some saturated light themes may fail 4.5 : 1 today; the
   audit decides which themes are adjusted and which are marked "decorative only".

## Deliberate exception: hold to favorite and to remove from recents (owner decision, 2026-11-23)

Rule 2.2 says a hold must never be the only way to do something. The owner decided that the new GUI
behaves like the Classic GUI here: hold X for two seconds toggles a favorite, hold B for two seconds removes
an entry from the recently played list. These two have no short-press path. What makes it acceptable: the
hold is named in the help ("Hold X Favorite") and a filling bar shows its progress (`HoldProgress`), removal
asks for confirmation, and adding a favorite is undone by the same hold. Revisit if users with limited hand
function report trouble.
