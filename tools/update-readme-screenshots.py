#!/usr/bin/env python3
"""Rewrites the screenshot gallery in README.md from the pictures in screenshots/.

The gallery lives between the two marker comments in the README:

    <!-- screenshots:start -->
    <!-- screenshots:end -->

Everything between them is generated here; edit GALLERY below instead of the README. A picture
that does not exist yet is left out, with a warning. tools/screenshot.sh runs this after
rendering the standard set; it can also be run on its own.

Usage: update-readme-screenshots.py [README.md]
"""

import os
import sys

START = "<!-- screenshots:start -->"
END = "<!-- screenshots:end -->"

# ("pair", [(file name without .png, title), ...]) becomes a two-column table,
# ("text", "...") a paragraph.
GALLERY = [
    ("pair", [("home", "Home"), ("favorites", "Favorites")]),
    ("pair", [("systems", "Systems"), ("arcade", "Arcade")]),
    ("text", "Every system's games in the presentation you prefer — Y cycles through them. "
             "Here, SNES in Grid, Boxart small and List:"),
    ("pair", [("snes-grid", "Grid"), ("snes-small", "Boxart small")]),
    ("pair", [("snes-list", "List"), ("games", "Games")]),
    ("text", "Settings, with the update page and the controller input test:"),
    ("pair", [("settings", "Settings"), ("settings-interface", "Interface settings")]),
    ("pair", [("settings-updates", "Updates"), ("controller-test", "Controller input test")]),
]

ALT = {
    "snes-grid": "SNES, grid view",
    "snes-small": "SNES, small box art view",
    "snes-list": "SNES, list view",
}


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    readme = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "README.md")

    with open(readme, encoding="utf-8") as handle:
        text = handle.read()
    start, end = text.find(START), text.find(END)
    if start < 0 or end < start:
        raise SystemExit(f"{readme}: missing the {START} / {END} markers")

    def have(name):
        if os.path.exists(os.path.join(root, "screenshots", name + ".png")):
            return True
        print(f"  warning: screenshots/{name}.png does not exist, left out", file=sys.stderr)
        return False

    blocks = []
    for kind, content in GALLERY:
        if kind == "text":
            blocks.append(content)
            continue
        cells = [(name, title) for name, title in content if have(name)]
        if not cells:
            continue
        lines = ["| " + " | ".join(title for _, title in cells) + " |",
                 "| " + " | ".join("---" for _ in cells) + " |",
                 "| " + " | ".join(f"![{ALT.get(name, title)}](screenshots/{name}.png)"
                                    for name, title in cells) + " |"]
        blocks.append("\n".join(lines))

    new = text[:start + len(START)] + "\n\n" + "\n\n".join(blocks) + "\n\n" + text[end:]
    if new != text:
        with open(readme, "w", encoding="utf-8") as handle:
            handle.write(new)
    print("  README.md gallery " + ("updated" if new != text else "already current"))


main()
