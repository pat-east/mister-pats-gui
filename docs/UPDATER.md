# Core and MRA Updates — Specification

## Update responsibilities

The updater work is split into two independent features and release scopes:

1. **Core updates — version 0.8.3:** install selected Arcade and Console core RBF files, together
   with Arcade MRA files. MRA updates are part of the Core Updates feature and this document
   specifies the combined scope.
2. **System updates — release version TBD:** update MiSTer system files using the official system
   update mechanism. They require a separate specification and process-safety analysis; they are
   not implemented by Core and MRA Updates.

Core and MRA updates must not invoke or depend on the System Updater. Conversely, the System
Updater must not be treated as the source or mechanism for Core and MRA updates.

## Goal

MiSTer Pat's GUI can identify official Arcade and Console cores and Arcade MRAs in the MiSTer
Distribution, check their status, and install selected or all missing/outdated files. The user can
review what will change before anything is written and must explicitly start the update.

This specification covers **core RBF files and Arcade MRA files**. Linux, MiSTer firmware, the
menu core, BIOS files, ROMs, scripts, and other distribution files are outside its scope. BIOS
files and ROMs must not be sourced, downloaded, bundled, hosted, distributed, or managed by this
feature.

## Source and scope

The only update source for this feature is the official
[MiSTer-devel/Distribution_MiSTer](https://github.com/MiSTer-devel/Distribution_MiSTer)
repository. The entry point is its published `db.json.zip` manifest. The manifest is the source
of relative installation paths, download URLs, file sizes, hashes, tags, and replacement
information.

Select only manifest files that match either of these scopes:

1. **Arcade and Console cores:** have the `.rbf` extension and the official
   `arcadecores` or `consolecores` tag.
2. **Arcade MRAs:** are `.mra` files directly under `_Arcade/`, excluding nested directories
   such as `_Arcade/cores/`.

Classification uses manifest tags, not guessed paths or filenames. This is necessary because
Arcade and Console cores can live in different locations. A core carrying additional tags is
included if it matches either target tag. MRA classification uses the manifest path and extension.

The selection excludes, in particular, `essential` files such as the menu core and MiSTer
firmware, `computer-cores`, `other-cores`, `service-cores`, files outside the Arcade MRA path,
and ROM/BIOS files. The selection comes from the official manifest; alternative databases from
Update All and custom Downloader databases are not included.

Official documentation:

- [Distribution_MiSTer: filter tags](https://github.com/MiSTer-devel/Distribution_MiSTer#tags-that-you-may-use-with-download-filters-feature)
- [Downloader_MiSTer: filter behavior](https://github.com/MiSTer-devel/Downloader_MiSTer/blob/main/docs/download-filters.md)
- [Downloader_MiSTer](https://github.com/MiSTer-devel/Downloader_MiSTer)

## How this relates to the official update scripts

`Scripts/update.sh` launches the official MiSTer Downloader. A normal run follows the existing
`downloader.ini` and can update many more files than the core RBFs and MRAs selected here.
Depending on its configuration, it can also update Linux, remove files, and reboot.

`update_all.sh` from [Update All](https://github.com/theypsilon/Update_All_MiSTer) invokes the
same Downloader with additional databases and tools. It is therefore not a narrowly scoped,
core/MRA-only invocation.

Downloader filters are persistent configuration and affect the files managed by Downloader. A
temporary filter in the existing configuration is not a safe approach for this feature: files
excluded by a changed filter may be removed from the Downloader-managed set. Core and MRA Updates
must change neither `downloader.ini` nor `Scripts/.config/downloader/downloader.json`.

## Ownership and state

`Scripts/.config/downloader/downloader.json` belongs exclusively to the official Downloader. It
writes this file during its own update run. Our feature does not read, write, or imitate this
internal format.

Core and MRA Updates keep their own state under a GUI-owned configuration path, for example
`/media/fat/Scripts/.config/mister-pat-gui/core-updater/`. This state is a cache/run-status file, not
the authoritative record of which files are installed. Each check compares the manifest with the
actual filesystem so that manual changes, deletions, and corrupted files are detected as well.

Still to test on a MiSTer: how a core installed by the GUI updater is handled by a later
`Scripts/update.sh` run. The official Downloader must not make unintended deletions, and no
persistent duplicates or conflicting versions may result. The GUI updater keeps no shared
inventory, so this is a known limitation of 0.8.3 rather than a release blocker; the test remains
open.

## Checking and status model

When the view opens or the user manually refreshes it:

1. Fetch the manifest over HTTPS from the Distribution.
2. Verify that the response is a valid archive and contains a parseable manifest.
3. Filter core RBFs by manifest tags and Arcade MRAs by manifest path and extension.
4. Check each selected manifest path in the active MiSTer filesystem.
5. Find the installed file and compare its hash with the manifest hash.
6. Read `tangle` as a core identity token (the Distribution currently emits values such as
   `<core_name>_core` for date-suffixed RBFs). Do not treat it as an old relative filename. The
   mapping from that identity to installed historical paths still needs validation before cleanup.

The view shows at least these states:

- **Up to date:** the expected file exists at its target path and its hash matches the manifest.
- **Missing:** the file is absent and no installed predecessor is identified by the manifest.
- **Update available:** reserved until manifest replacement metadata can be interpreted safely.
  The current implementation does not infer this state from `tangle` data.
- **Modified/invalid:** a file at the expected path has a different hash or is incomplete.
- **Unavailable/error:** manifest, network, storage, or file checking failed; show the error and
  do not install anything.

The hash algorithm must be taken from the official manifest format and explicitly validated by
the parser. An expected hash with an unknown length or unsupported format results in an error
state; file size alone never authorizes installation.

## User flow

The entry is **Settings → Library → Core and MRA updates**. Selecting it opens a dialog over
Settings; there are no buttons in the dialogs, the actions are in the footer hints (see
[DESIGN.md](DESIGN.md)). The separate Arcade readiness overview is named **Arcade game status**.
The steps are:

1. **Start dialog:** explains the scope and checks that a network is available. Offline it says so
   and points to Settings → System → Connect to Wi-Fi; **A** starts the check when online.
2. **Checking:** a progress bar and a live list of the files being compared. **B** stops it.
3. **Choose what to update:** tabs for Needs update, All, Up to date and Changed; a list grouped by
   Arcade cores, Console cores and Arcade MRAs with round checkboxes; the number of files and the
   download size. **A** selects, **X** selects everything that needs an update, **Start** continues.
4. **Confirm installation:** every selected file with its path and size, scrolling; **A** installs.
5. **Installing:** per-file and overall progress. **Result:** what was installed or what failed.

A missing network connection or manifest must not block the rest of the GUI. Checking is read-only;
files are installed only after an explicit user action.

## Installation flow

For each selected core or MRA file:

1. Validate the target path and filename against the manifest. Paths must be relative,
   normalized, and contained within the MiSTer system storage. Reject absolute paths, `..`,
   symlinks escaping the target tree, and path collisions.
2. Download the file to a temporary file on the same volume as its final target.
3. Verify the complete file size and manifest hash. On failure, remove the temporary file and
   leave the existing target file untouched.
4. Install the verified file at its target path using an atomic replacement if the filesystem
   supports it for that target. Verify this on MiSTer's exFAT storage before implementation; do
   not assume atomicity.
5. Verify the installed content again.
6. Do not remove predecessor files. `tangle` identifies a core family; it does not name an old
   relative path. The mapping from a core identity to installed historical paths has not been
   validated sufficiently to authorize deletion. Do not generally clean up old `.rbf` files.
7. Update the GUI-owned state only after installation has been confirmed.

A failed file stops further changes to that file, but need not stop the entire run. The final
report must clearly show partial success. A retry must skip verified files and allow safe recovery.

## Process model

Core and MRA updates run in the GUI process. They do not require the separate system-update
service or binary (`mister-pat-sys-update`), because this feature only installs individually
selected and verified core RBF and Arcade MRA files; it does not replace system files, invoke
`update.sh`/`update_all.sh`, or reboot MiSTer.

Only one core/MRA update run may be active at a time. A GUI exit or crash may stop an in-progress run;
this is safe only because each file is downloaded and verified before replacing its target.
Temporary files must be recoverable or cleaned up on the next run, and status must be recomputed
from the manifest and filesystem when the GUI opens again. The GUI must remain responsive while an
update is running. Power-loss and filesystem-error behavior must be verified on real hardware.

Linux image, kernel, menu core, firmware, scripts, ROMs, and other files remain outside the
selected scope. A Core and MRA Updates run must not reboot MiSTer.

## Safety and data rules

- Use only HTTPS sources listed in the official MiSTer Distribution manifest.
- Treat manifest data as untrusted input: validate schema, paths, URLs, sizes, tags, and hashes.
- Never use manifest URLs, shell commands, or target paths without validation.
- Do not source, download, bundle, host, distribute, or manage ROMs, game content, or BIOS files.
- Do not delete user files outside explicitly identified official core or MRA targets.
- Leave an existing target file untouched after a download or hash failure.
- Check for sufficient free space before downloading temporary files.
- Do not allow simultaneous GUI Updater and official Downloader runs while both could modify the
  same targets. How to reliably detect this conflict must be verified on hardware.

## Error cases

At minimum, handle and clearly report these cases without uncontrolled file changes:

- no network, HTTP error, or TLS certificate error;
- corrupt, unexpected, or unsupported manifest;
- missing or invalid URL, size, hash, tag, or replacement instruction;
- download interruption or hash/size mismatch;
- insufficient space, read-only storage, or filesystem error;
- target path outside system storage or unexpected symlink;
- GUI restart or crash during a core/MRA update;
- an official Downloader run or second GUI update run is already active;
- a core or MRA file was changed, deleted, or manually replaced;
- download succeeds but predecessor cleanup fails.

The result list ("Choose what to update") groups files under Arcade cores, Console cores and
Arcade MRAs. Each row has a readable name (the date and extension are dropped from core names), the
file path, its state (Up to date, Not installed, Changed, Cannot be checked) and its download
size. Only files that can be installed have a round checkbox; Up to date and Cannot be checked
rows have none, because they cannot be selected. Tabs at the top (Needs update, All, Up to date, Changed) filter by state with ←/→; the active tab has the same colored underline as the main navigation. The
bottom bar shows how many files are selected, how many cores and MRAs they are and the download
size, or "Pick at least one file to continue." when nothing is selected. There are no buttons in
the dialogs; the actions are in the footer hint.

Controls: **A** selects or unselects the focused file, **X** selects every file that needs an
update, **Start** opens the review (only once something is selected), and **B** closes. There is
no separate type filter: the groups already separate the kinds of file.

The confirmation step ("Confirm installation") lists every selected file with its path and size.
The list uses all the room in the panel and scrolls with ↑/↓; a range such as "6–15 of 22" and a
scroll bar show the position. **A** installs, **B** goes back to the selection.

### Saved result (15 minutes, survives a restart)

A completed check is saved so that reopening the updater is fast, also after the MiSTer was
restarted. Two small files in the GUI's own `cache/` directory (`core-manifest.zip`, the official
list as downloaded, and `core-check.tsv`, the time of the check plus size, modification time and
MD5 of each installed file that was hashed) are written once per completed check, never while it
runs and not at all when nothing changed.

- For 15 minutes after a check, the next check uses the saved list and does not download it.
- Even then, every installed file is compared by size and modification time with the saved record
  and hashed again if either differs, so a file changed in the meantime is never reported with an
  old answer. Files that did not change are not read from the SD card again.
- After 15 minutes, or when the clock says the saved time lies in the future (the MiSTer has no
  battery clock), the result is not used: the list is downloaded again and every file is hashed.
- The list shows "Checked N minutes ago". **Y Check again** ignores the saved result and asks the
  official source again.
- A damaged or unreadable cache file is ignored and replaced; it never causes an error screen.

While a check runs, the dialog shows the progress bar and a list of the files being compared,
newest at the bottom, each with its result (Up to date, Missing, Modified, Unavailable). The list
follows the check on its own and keeps the last 40 files in memory.

Before a check starts, the first dialog confirms that a network is available (an interface with
an IPv4 address and a default route; no traffic is sent). Offline, it says so and points to Settings → System → Connect to Wi-Fi; the footer then offers
only **B Back**, so the check cannot be started. "Try again" after a failed check returns to this
dialog. Being connected does not guarantee the download works; that is still reported on the
failure screen.

A failed check is shown on the "Update check failed" screen in plain words (what went wrong and
what to try), states that nothing was changed, and keeps the technical reason as a small
"Details" line. Raw error strings such as file paths or `errno` text are never the headline.

## Status on hardware (2026-10-13)

The check, the saved result and the installation were run on a MiSTer. Five Arcade cores
(`ChampionBaseball`, `PolePosition`, `SNK6502`, `Tank`, `Vastar`), nine Arcade MRAs and one Console
core (`Marty`) were installed; the MD5 and size of each file matched the official manifest, no
temporary `.part` files were left behind, and every installed MRA names a core that is present.
The updater keeps no shared state under `Scripts/.config`; its only files are the two cache files in
the GUI's own `cache/` directory, which can be deleted at any time.

Not yet verified: a run of the official Downloader (`Scripts/update.sh`) after a GUI update, and
removal of older core versions. Older versions stay next to the new ones (for example
`Vastar_20260901.rbf` beside `Vastar_20261011.rbf`) because the replacement data in the manifest is
not understood well enough to delete files. This is recorded as a known limitation of the release
rather than a blocker, because the updater neither writes the Downloader's inventory nor deletes
anything.

## Acceptance criteria

- Only manifest RBFs tagged `arcadecores` or `consolecores`, and `.mra`
  files directly under `_Arcade/`, appear in Core and MRA Updates.
- Console and Arcade files are identified through manifest tags, not hard-coded filenames.
- Check reports Missing, Modified/invalid, and Up to date based on the actual filesystem and
  verified hashes. `Update available` stays unused until replacement metadata is verified.
- Download and hash verification complete before replacing the active file.
- A failed download/hash check never overwrites or deletes an existing core or MRA file.
- Existing files are not deleted as predecessor cleanup. Replacement cleanup remains disabled
  until the official metadata semantics are verified.
- Core and MRA updates change neither `downloader.ini` nor `downloader.json`, and do not update
  files outside the selected core and MRA scope.
- Core and MRA updates run without the separate system-update binary and do not reboot MiSTer.
- After GUI exit, restart, or crash, a core/MRA update can be safely retried; status is recomputed
  from the manifest and filesystem.
- No automatic reboot and no Linux image changes occur during a core/MRA run.
- Coexistence with a normal official Downloader run is verified on MiSTer and causes no unintended
  deletions or persistent duplicate core versions.
- Network, storage, path, integrity, and process errors are reported; the GUI render loop remains
  responsive.

## Open questions

1. **Manifest format and file paths:** document the `db.json.zip` schema, hash algorithm, URL
   rules, tag location, and how core identity (`tangle`) maps to installed historical paths.
2. **Filesystem atomicity:** verify that temporary-file-plus-rename is atomic and sufficiently
   safe against power loss on MiSTer's exFAT partition. Define a backup/recovery strategy if not.
3. **Shared ownership:** on hardware, test a GUI core/MRA update followed by `Scripts/update.sh`,
   including upgrades and removals, without changing `downloader.json`.
4. **Run conflicts:** detect an official Downloader run or define another reliable locking
   strategy.
5. **Console file scope:** confirm against the current Distribution that the tags cover all
   desired Console RBFs and exclude unwanted menu/utility cores.
6. **Removing old versions:** verify how `tangle` core identities map to manually installed
   versions and deviations from the manifest before enabling cleanup.
