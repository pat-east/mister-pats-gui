# ScreenScraper.fr: video previews

The List view and the Game options panel can play a short preview video of the selected game,
fetched from [ScreenScraper.fr](https://screenscraper.fr). ([#4](https://github.com/pat-east/mister-pats-gui/issues/4))
The API notes behind it are in `_vnext/SCREENSCRAPER-FR.md`, the measurements in
`_vnext/SCREENSCRAPER-POC.md`.

## What the user sees

- The game's box art is shown. After the **Preview delay** (default 5 s) the GUI looks for the
  video; the box art is replaced by the video, unscaled, in the same area. It plays three times,
  then the box art returns.
- A status line under the picture says what is happening: `Video plays in: N s`,
  `Scraping game info ...`, `Downloading video ...`, `Video found: verifying...`, or why nothing
  plays (`No video found`, `Video found: too large to show (...)`, `Enter your login in Settings`,
  `No connection, trying again soon`, `No video preview for this system`).
- Moving to another game cancels whatever is running. The screensaver and error dialogs stop it.
- Arcade and computer systems are not scraped (ScreenScraper identifies them differently); the
  status line says so.

## Settings (Library, section "ScreenScraper.fr")

| Setting | Values | Default |
| --- | --- | --- |
| Video previews | On / Off | On |
| Account | status of the login (read only) | |
| User name, Password | own ScreenScraper account; the password is shown as `********` | empty |
| Video sound | Off, 100%, 75%, 50%, 25%, 10% | Off |
| Default region | EU / US / JP | EU |
| Preview delay | 5 to 30 s in steps of 5 | 5 s |
| Video storage limit | 256 MB to 8 GB, Unlimited | 256 MB |

An account is free at screenscraper.fr. Without one the feature stays off and says so.

## How it works

1. **Gate.** Only systems with a known ScreenScraper system id are scraped. The account is checked
   once (`ssuserInfos`) and again every 30 s while it is unavailable.
2. **Cache first.** Results are stored per system as TSV files under
   `/media/fat/mister-pat/screenscraper/` (`video`, `novideo`, `nogame`); a stored "no video" is
   shown at once and never asked again. Videos are `<gameid>-<type>.mp4`. When the storage limit is
   reached the oldest videos are deleted first.
3. **Scrape.** `jeuRecherche` with the title and system id, exact normalised title match, no
   hashing. It prefers `video-normalized` (smaller) over `video`.
4. **Download and verify.** Size and CRC32 from the API, plus an MP4 header check. A file that fails
   is discarded.
5. **Play.** FFmpeg (minimal static build, LGPL: MP4, H.264, AAC) decodes on a low-priority thread;
   frames are copied to the screen at 30 Hz and never scaled, so a video that does not fit is
   refused with a reason. Sound goes to `aplay` as raw PCM at the chosen volume. MP3 audio is not
   supported and plays silent.

## Request rules

One request at a time, at least 1.3 s apart, no automatic retries. HTTP 429 pauses for 60 s,
401/423 for 10 min, 430/431/426 and a rejected login for the rest of the session. Credentials are
passed to curl on stdin, never on the command line or in the log. Nothing is sent without a CA
bundle.

## Files

| File | Content |
| --- | --- |
| `/media/fat/mister-pat/screenscraper-fr-settings.ini` | the logins (see below) |
| `/media/fat/mister-pat/screenscraper/*.tsv` | per-system result cache |
| `/media/fat/mister-pat/screenscraper/*.mp4` | downloaded videos |

The settings file is not part of the settings backup. The card is FAT-like, so file permissions do
not protect it: anyone with SSH or FTP access can read it. Use a password you use nowhere else.

## Developer login

ScreenScraper requires every request to carry the software's developer login (`devid`,
`devpassword`) besides the user's own. It belongs to the software, not to the user, and a user
cannot get one without applying for it. So the release build carries it:

- The repository never contains it. The maintainer keeps it in the git-ignored
  `_build/screenscraper.env` (keys `SCREENSCRAPER_DEVID`, `SCREENSCRAPER_DEVPASSWORD`).
- `make` runs `tools/embed-dev-login.py`, which writes the two values into the binary scrambled
  (`build/<flavor>/ScreenScraperDevLogin.cpp`), so `strings` does not show them. Nothing else from
  the env file is read.
- At run time they are unscrambled into ordinary variables in memory and sent with each request.
  They are never logged and never written to the card.
- Anyone willing to spend the time can still recover them from the binary. That is accepted: there
  is no way to give software a secret and hide it from the person running it.
- A build without the env file has no login (the Account row says `Developer login missing`). It
  still works when `SCREENSCRAPER_DEVID` and `SCREENSCRAPER_DEVPASSWORD` are entered by hand in the
  settings file; a login built into the binary takes precedence over those.
- ScreenScraper's team has been asked whether this is acceptable; no answer yet.

## Not built yet

Clear the cache, delete downloaded videos, choose the video location (`fat`, `usbN`), Arcade
support, MP3 audio.
