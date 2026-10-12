#!/usr/bin/env python3
"""Rewrites the screenshot gallery in README.md from the pictures in screenshots/.

The gallery lives between the two marker comments in the README:

    <!-- screenshots:start -->
    <!-- screenshots:end -->

Everything between them is generated here; edit GALLERY below instead of the README. A picture
that does not exist yet is left out, with a warning. tools/screenshot.sh runs this after
rendering the standard set; it can also be run on its own.

Usage: update-readme-screenshots.py [README.md]

Running the script refreshes the standard screenshots from the MiSTer and then rewrites the
gallery. It always replaces the PNGs, even when they already exist.
"""

import os
import subprocess
import sys

START = "<!-- screenshots:start -->"
END = "<!-- screenshots:end -->"

# ("pair", [(file name without .png, title), ...]) becomes a two-column table,
# ("text", "...") a paragraph.
GALLERY = [
    ("pair", [("home", "Home"), ("favorites", "Favorites")]),
    ("pair", [("systems", "Systems"), ("arcade", "Arcade")]),
    ("single", ("disc", "Disc")),
    ("text", "Every system's games in the presentation you prefer — Y cycles through them. "
             "Here, SNES in Grid, Boxart small and List:"),
    ("pair", [("snes-grid", "Grid"), ("snes-small", "Boxart small")]),
    ("pair", [("snes-list", "List"), ("select-version", "Choose version")]),
    ("single", ("games", "Games")),
    ("text", "Settings, including Interface, Accessibility, System, the Changelog, the update page and controller input test:"),
    ("pair", [("settings", "Settings"), ("settings-interface", "Interface settings")]),
    ("pair", [("settings-accessibility", "Accessibility settings"),
               ("settings-system", "System settings")]),
    ("pair", [("settings-updates", "Updates"), ("controller-test", "Controller input test")]),
]

ALT = {
    "disc": "Disc tab",
    "snes-grid": "SNES, grid view",
    "snes-small": "SNES, small box art view",
    "snes-list": "SNES, list view",
    "select-version": "Choose version for Adventures of Batman & Robin on Game Gear",
}


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    readme = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "README.md")

    # This entry point is also the refresh command: always render a fresh set first so an
    # existing PNG can never cause stale README images to be kept. screenshot.sh invokes us
    # again after rendering; the environment marker makes that second call gallery-only.
    if os.environ.get("MISTER_SCREENSHOTS_GALLERY_ONLY") != "1":
        env = os.environ.copy()
        env["MISTER_SCREENSHOTS_GALLERY_ONLY"] = "1"
        subprocess.run([os.path.join(root, "tools", "screenshot.sh"), "set"],
                       cwd=root, env=env, check=True)

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
        if kind == "single":
            name, title = content
            if have(name):
                blocks.append("\n".join((f"| {title} |", "| --- |",
                                          f"| ![{ALT.get(name, title)}](screenshots/{name}.png) |")))
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


if __name__ == "__main__":
    main()
