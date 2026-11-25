# src/app/tests/

## Golden images and layout rules (`tests/golden.sh`)

A visual safety net for the Settings pages and the main tabs, for when nobody can look at every page by hand.

```sh
make -C src/app golden-check            # = tests/golden.sh: render, compare, layout rules (about 6 s)
make -C src/app golden-update           # accept the fresh renders as the new golden set
tests/golden.sh --only wifi             # only the cases whose name contains "wifi"
tests/golden.sh --layout                # only the layout rules (also runs every page in pseudo-language and at text 130 %)
tests/golden.sh --determinism           # render everything twice and compare byte-wise
```

Needs `build/app-render` (`make -C src/app`). Diff percentages and diff images need ImageMagick (`magick`) and are skipped
without it; the check itself (a byte comparison of the PNGs) needs only the shell.

**What is rendered** (148 pictures, 1080p, `tests/golden/`, about 12 MB): the seven main tabs; the seven Settings category menus;
every page id of `app-render --tab settings --list-pages`; the nine pages of the first start (`--list-first-run`, names `first-<page>`) (each id is a sample state: the flows' running, failed, done and
cancelled pages, wizards, standard pages); two 60-item lists (cores list, Manage Consoles); and a look matrix of twelve
representative pages (Info, choice, error result, facts, progress, scan working, cores list, Wi-Fi networks, controller wizard,
updates confirm, disc install failed, viewer) in four looks: pseudo-language (`--pseudo`), the Largest text and hint step
(`--look text=130,hint=130`), light mode (`dark=0`), high contrast (`contrast=2`). Every render is deterministic (fixed clock and
sample values); `--determinism` renders all twice and must report 0 differing pictures. Names: `page-<category>_<page>`,
`menu-<category>`, `tab-<tab>`, `look-<look>-<category>_<page>`, `long-...`.

**Checking** compares the fresh PNG with the stored one byte by byte; a changed page is printed as
`CHANGED: <name>  <percent> % of the pixels differ`, a diff image (differing pixels in red) goes to the temp folder printed at the end
(`GOLDEN_TMP=/some/folder` chooses it). Exit code 1 for a changed, new or gone picture, 4 for a new layout finding, 0 otherwise.
After an intended change look at the changed pictures (the fresh ones are in the same temp folder), then `golden-update` and commit
`tests/golden/` with the change. A change in the shared font, theme or shell changes many pictures at once: review a few, not all.

**Layout rules** run in the same pass (`app-render --check-layout`; the Painter's optional text recorder in
`src/ui-framework/render/soft/SoftPainter.h` records every text run with its clip, and later opaque fills hide what is under a dialog).
Per picture it reports: `offscreen` (text outside the screen), `cut` (text cut by its clip), `overlap` (two visible text runs
overlapping), `elided` (text shortened with "..."). Every golden case plus every page in pseudo-language and at text 130 % is
checked. Findings that are known and accepted live in `tests/layout-known.txt` (written by `--update`); anything new fails the
check. Default-size pictures must have no `elided` finding at all.

Limits: the rules see text, not shapes (a label over an icon, a clipped button or too little contrast are not found: `ui-demo --contrast`
covers contrast), text drawn as one run with its own manual "..." is not recognised as elided, and a scrolled list that cuts a row
vertically is not reported.

## Flow tests (`make -C src/app test`)

Behaviour, not pixels: every converted Settings flow is driven by scripted key sequences against the sample services and the
test asserts the page it is on, its hints and result, and which service calls were made and how often. A case runs in its own
process (fork), so a crash or `abort()` fails that case and the rest still run.

```sh
make -C src/app test                              # build app-test and run everything (about 10 s)
make -C src/app test TARGS="wifi_ scan_"          # only the cases whose name contains one of the words
build/app-test --list                             # the case names
build/app-test --no-fork wifi_success_secured     # in this process (lldb)
TT_WALKS=500 TT_LENGTH=80 build/app-test generic_monkey   # a long random search (about 2 minutes), the default is 40 x 45 steps
```

Output per case: `PASS|FAIL|CRASH name (n checks)`; a failed check prints the file and line, the condition and the key
sequence that led there (`<open ...>` opens a flow, `[key=value]` is a sample state, `~2.5s` a wait). `KNOWN BUG` lines are bugs that
are found, reported and not fixed yet: they do not fail the run; when the bug is fixed the line turns into `NOTE ... known bug now
fixed` and the `KNOWN_BUG(...)` line is deleted. Exit code 1 when a case failed.

**Files**

| File | What |
| --- | --- |
| `Test.h`, `TestMain.cpp` | the framework: `TEST(name)`, `CHECK`, `CHECK_EQ`, `CHECK_MSG`, `KNOWN_BUG`; the fork runner |
| `Rig.h`, `Rig.cpp` | the rig: a `SettingsTab` on the `SampleLibrary` and the sample services; `keys("a,down,wait2.5,b")`, `wait(s)`, `set("system.wifi","none")`, `open(category, rowId)` (a row's A), `openPage("controllers/wizard")` (a `--list-pages` id); questions `id()`, `kind()`, `title()`, `hintLine()`, `hasHint()`, `result()`, `calls("wifi.rollback")`; `draw()` draws the tab with the built-in face (a crash shows up) |
| `Generic.h`, `Generic.cpp` | the rules every page keeps (`checkPage`: hints, closed vocabulary, Busy lists B Cancel, Result complete, `draw()` does not crash), `escapeWithB` (B leaves from anywhere), the vocabulary |
| `Flows.cpp` | the list of flows (`FlowSpec`: how it opens, which sample states change its paths, its own rules: Wi-Fi rollback once, `libraryTaskClosed` once per visit, nothing left running after the GUI closes, raw input follows the capture page) |
| `GenericTests.cpp` | `generic_every_listed_page` (every `--list-pages` id), `generic_monkey/<flow>` (seeded random walk), `generic_escape_from_start/<flow>` |
| `FirstRunTests.cpp` | the first start (Welcome, Jump Start, covers, Overview) through `Rig::openFirstRun()`, which drives the shell's `State` |
| `LibraryTests.cpp` `WifiTests.cpp` `SystemTests.cpp` `UpdatesTests.cpp` `ControllersTests.cpp` | the scripted flows |

**Add a case.** A flow change that alters a path, a hint or a service call gets a case in the file of its area:

```cpp
TEST(reboot_failure) {
    Rig r;
    r.set("system.reboot", "fails");              // a sample state, as `app-render --set` takes it
    r.open("system", "system.reboot");            // the row's A
    r.keys("a");
    CHECK_EQ(r.id(), int(kRebootFailed));         // the wizard page id (copy the flow's enum, it is the contract)
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    r.set("system.reboot", "ok");
    r.keys("a,a");
    CHECK_EQ(r.calls("reboot"), 2);               // the sample services count what was called
    CHECK(!r.active());
}
```

**Add a flow to the generic tests.** One `FlowSpec` in `Flows.cpp`: `open` (how the row opens it), the sample states the walk may
flip (`twiddles`), the keys it presses, and an optional `watch` (called before and after every key and frame) and `atEnd` (after the
GUI closed). A new page id of a category appears in `generic_every_listed_page` by itself (it comes from `pageIds()`).
A new sample state that a flow needs to be reachable goes to the sample service, not to the test.

**The rules in the generic tests** (each is a `CHECK` with the key sequence in its message): the page does not crash when drawn;
hints are not empty (a Busy page that cannot be cancelled has none) and every word is from the closed vocabulary of
`docs/components/Hints.md` (the list is in `Generic.cpp`; the Start key may name its own action; a word outside that fails unless it
is in `knownVocabularyDeviations()`); a Busy page that can be cancelled lists `B Cancel` and one that cannot lists no B; a Step page
that cannot continue gives its reason and lists no continue key; a Result has a headline, says what changed, an Error says what to do
next, and A is `Try again` exactly when it has a retry page (else `A/B Done`); at most `N = 14` B presses empty the stack, a
non-cancellable Busy page ends by itself, a Capture page that ignores B names its way out (hold Start); B on a Step page moves
to another page or leaves; after the GUI closes no page is open, no task runs, the pad is not captured, no Wi-Fi test is active.

**What it does not cover**: pixels and layout (that is `golden-check`), the real services (the sample services are fakes: the device
adapters in `src/legacy/bridge/` are not exercised), timing on the device, the toast text (no shell), the first-level rows, flows
that are not wizards (Info, Manage Consoles, the input test and dead-zone pages are only opened, drawn and left), Classic itself
(the equivalence of function with Classic is checked by reading the code, see `PARITY-settings.md`).
