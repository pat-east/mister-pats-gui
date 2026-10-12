# Game version groups

The **Organize game-versions** setting groups regional, language and revision ROMs under a
shared title. It is enabled by default. A group with multiple paths opens a controller-operated
version chooser; after a successful launch, the selected path is used for later starts. The
setting can be changed without rebuilding the game database. Arcade alternatives are grouped
from MiSTer's `_alternatives` paths; other systems use trailing filename tags.

## Rules for future changes

The current filename handling is deliberately simple. It removes trailing parenthetical tags
from the visible title and uses them as variant labels. Known trailing ROM dump tags such as
`[!]` are removed from the title; tags that identify a revision can also contribute to the
variant label. Grouping is limited to one system and uses a normalized title key. It does not
use fuzzy title matching.

When extending the parser, preserve the original ROM paths and treat ambiguous suffixes
conservatively. In particular:

- **Disc sets are not interchangeable versions.** PlayStation names such as `Gran Turismo 2
  (D1)` and `(D2)` need to remain separate disc parts or use a dedicated disc-set model. Classify
  them before broad suffix grouping so they do not become choices in one version group.
- **Dump and release tags have system-specific meanings.** Atari Lynx `[a1]`/`[o1]` tags can
  describe dumps; GBA tags such as `(Venom)` can identify a dump group. Neither should be treated
  as a game version without system-specific evidence. A hack or alternate build may still be a
  legitimate separate choice.
- **Arcade's alternatives folders are curated, but broad.** They can contain hacks or related
  titles as well as regional revisions. Keep those entries visibly distinguishable, and do not
  infer additional groups from a similar title alone.
- **Only trailing metadata is removable.** Parentheses in the middle of a title are part of the
  title. Unknown trailing tags should remain available as variant details rather than being
  silently discarded.

Useful regression examples when changing the rules include Game Gear's dated *Adventures of
Batman & Robin* betas, NES region/revision combinations such as `(J) (V1.0) [!]`, Atari Lynx dump
tags, and Arcade alternatives whose filenames do not exactly match their group title. Check both
the visible title and the chooser labels, and verify that unrelated titles remain separate.

Grouping should continue to happen while preparing the library index, not during drawing or
each button press. Views, search, favorites, Home and console lists should all consume the same
grouped entries. See [GUI.md](GUI.md) for the interface design and [ARCADE.md](ARCADE.md) for
Arcade library behavior.
