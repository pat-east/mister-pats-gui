# Disc tab: parity with the Classic Disc screen

Legacy: `src/legacy/DiscScreen.*` and its services. New: `src/app/DiscTab.cpp` (data via `Library`, adapter in
`src/legacy/bridge/AppLibraryAdapter.*`). Status: done / different (intentional, reason) / shell (needs a shell or
Settings change, not the tab).

| Legacy behavior | New tab | Status |
| --- | --- | --- |
| Screen title | the shared `PageHeader` with the title "Disc" above every state (D16) | different (Classic draws none) |
| No drive: "No optical drive detected", connect hint; hint bar "No optical drive detected" | Notice page with why and next step | done |
| No disc: "Please insert disc", PlayStation/Saturn text; hint "Insert disc" | Notice page, same meaning | done |
| Reading: heading "Reading disc…", button "Waiting for supported disc" | Notice "Reading the disc" (no dead button) | different: nothing to press while reading |
| Unsupported: "No supported game detected" (warning) | Notice, warning icon, next step | done |
| Unreadable: "Disc could not be read" + technical error line | Notice, error icon, "Details:" line | done |
| Readable heading: disc title, else "Sega Saturn disc detected" / "PlayStation disc detected" / "Supported disc detected" | Title, else the same three headings (they read "PlayStation disc / Sega Saturn disc / Disc" until this was fixed) | done |
| Metadata: System, Year, Region, Disc ID, Players, Co-op | Same rows; empty ones hidden; Rating and Drive model added | done (superset) |
| Overview, cut to the space available | "Overview" label + text with ellipsis | done |
| `disc.error` line in warning color on any page | Warning status line above the button | done |
| Box art, "Boxart loading…" / "No Boxart found" | Art panel with placeholder text "Loading artwork…" / "No artwork found" | done |
| 4 screenshot tiles, focus frame, Down/Up/Left/Right | Same; A opens the framework viewer | done |
| Screenshot overlay: Left/Right browse, A/B close, "n / m" | `Modal::viewer` (same keys, counter) | done |
| Start button labels: "Start game" / "Physical Disc package is updating" / "Rechecking disc and core…" / "Install matching FPGA core" / "Configure Physical Disc package" | The same words (`disc.button.*`; they were paraphrases until this was fixed); blocker kind from `DiscStatus::blocker` | done |
| A while not startable: toasts "Wait for the Physical Disc package update to finish.", "Install the matching PlayStation or Saturn core from the Downloader.", "Install the scoped Physical Disc package in Settings.", "Checking disc and core before launch…", and, when the core is ready but the MGL path is empty, "The Physical Disc MGL is unavailable." | A opens an info dialog ("Why not?" hint) with the same words, including the MGL case (`DiscStatus::mglMissing`, which the adapter reads from the core status) | different: toasts vanish by themselves (accessibility 2.4), a dialog stays until read |
| Start: toast "Rechecking disc and Physical Disc core…", re-check disc and core, 15 s timeout, then the toasts "Disc/core recheck timed out. Press Start game to try again.", "Physical Disc package update started. Press Start game when it finishes.", "Disc changed during the check. Press Start game again.", "Disc or Physical Disc core is no longer ready. Check the status and retry."; launch error dialog | `startDisc()` / `stepDiscLaunch()` in the adapter: the same sequence and the same texts word for word (the earlier texts were paraphrases, and the "Rechecking" toast and the MGL case were missing) | done (adapter; `t7b_discseq`: 19 same, 0 differ, texts compared) |
| Leaving the tab cancels a pending launch | Tab calls `cancelDiscLaunch()` on tab switch or Back while launching | done (new Library call) |
| Y reload artwork: clears cached images, reloads; toast; needs a readable, known disc | Y only offered for a readable known disc; progress shown by the art placeholder | different: state instead of a toast |
| Disc change resets cursor and overlay | Same (generation) | done |
| X (no Classic equivalent) | X "Check again" = `rescanDisc()` in every state | added |
| Hint bar lines | Hints declared per state; A only when it can act | done |
| Drive info lines (device, model, status, Main, MGLs, cores, project URL) are in Settings > Disc drive, not on the tab | Drive model shown on the tab; the rest is in Settings > System > Disc drive (`SystemSettings`) | done (Settings) |
| Install/update Disc cores (confirm, progress, cancel, recovery) are in Settings | Disc tab only names the way (dialog text); the flow is Settings > System (`SystemSettings`) | done (Settings) |
| Disc tab is hidden unless a drive is present and the scoped package is installed (`visibleTabs`); leaving it when the drive disappears | `Library::discTabAvailable()` (adapter: Classic's exact rule `drive != NoDrive && alternateMainInstalled && scopedMainConfigured && hasAnyCompatibleMgl()`, read once a second); the shell hides the tab (`State::hidden[]`) and returns to Home when it was active | done (shell, adapter) |
| Top bar / notices for discs | None besides the tab | done |

Counts: 25 rows; 21 done, 3 different, 1 added.

Verified against Classic code on the host (2026-11-23): the real `DiscScreen` against the adapter and `DiscTab` with scripted drive and core states:
`t7_disc` (every combination of media, system, core, MGL and installer state: heading, button state, enabled), `t7b_discseq` (the start sequence
step by step: texts, pending state, launch, cancel on leaving).

Intentional differences: no dead button while reading; toasts replaced by a dialog or visible state;
unsupported/unreadable/reading use a notice page instead of the half-empty two-column page.

Open: nothing in this file. The viewer's scrim dims the bottom bar; the shell redraws the tab's hints above it (`drawOverlays`).
