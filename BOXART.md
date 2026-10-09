# Box Art Scraping

## Status

This analysis is based on **1,538 entries** from the earlier MiSTer file
`scrape-misses.txt`, dated September 30, 2026. Each ROM revision counts as a separate
entry. It analyzes the missing matches from that run; it is not a success rate for the
entire game library.

| Stage | Matches in the Libretro box art index | Excluded system files | Still unmatched |
| --- | ---: | ---: | ---: |
| Round 1 | 93 | 51 | 1,394 |
| Round 10 | 493 | 51 | 994 |
| Round 11 | 521 | 51 | 966 |
| Round 12 | 530 | 51 | 957 |
| Round 13 | **541** | **51** | **946** |

The last three rounds added **48 matches**. Of the total, 451 matches are for Game Boy
Advance and four are for Arcade. The largest remaining groups are Game Boy Advance
(376), Arcade (238), and NES (82).

## How the scraper works

For each supported system, the scraper downloads the `Named_Boxarts` directory index
from `thumbnails.libretro.com` and stores it on the MiSTer at
`/media/fat/mister-pat/scrape-index/`. It matches local game titles against the image
names listed in that index. Arcade titles are first checked for an exact match,
including their revision.

Among other things, the matching rules account for regional titles, trailing articles
such as `Legend of Zelda, The`, unambiguous subtitles, GBA Video episodes, Famicom
Mini volume numbers, and variations in series names. The last three rounds added
handling for omitted `Disney's` in GBA titles, long subtitles that can be matched
unambiguously, and series names between a main title and subtitle. Ambiguous matches
such as `Pirates of the Caribbean` are deliberately left unresolved.

In 0.4.0, **Settings → Prepare box art** also prepares existing local covers. A matched
download is decoded once and stored beside the game as `<Title>.jpg`, `<Title>-sm.jpg`,
and five view-sized BMPs. Existing JPEG or PNG covers can be converted to the same BMPs
without downloading them again. Each BMP keeps the cover's aspect ratio within these
1080p bounds:

| View | Filename suffix | Maximum size |
| --- | --- | ---: |
| Home | `-home.bmp` | 178×178 px |
| Games Grid | `-grid.bmp` | 245×245 px |
| Games Boxart small | `-small.bmp` | 128×128 px |
| Games List detail | `-detail.bmp` | 689×624 px |
| Arcade | `-arcade.bmp` | 148×148 px |

The GUI prefers the matching BMP and falls back to an existing JPEG or PNG if that BMP is
absent. The uncompressed variants can use up to about 1.7 MB per cover; keeping the JPEGs
supports older installations and covers without prepared variants. Background images remain
JPEG/PNG. Arcade still fetches covers only, without background screenshots.

The scraper processes one game per frame. Stopping it leaves completed artwork in place;
another run fills gaps. System files and BIOS entries are excluded. Unmatched titles are
written to `/media/fat/mister-pat/scrape-misses.txt`.

Its ETA is an approximate completion **clock time**: elapsed time since the run began,
multiplied by `(total games - processed games) / processed games`. It includes download,
conversion, index preparation and frame pacing. It can move when later systems require
different amounts of work. The displayed time uses the MiSTer's system time zone.

## Results and device verification

An **index match** means only that the server lists a matching image name. The analysis
did not download or decode the image files. A 0.4.0 preparation run over 11,502 entries
has since completed on the MiSTer. Its fetched/prepared/skipped/missing totals were not
recorded here, so the table above still describes only the older audit snapshot. The
revised ETA was deployed after that run and has not been checked against a later full run.
For titles with
multiple regional versions, the current index often prefers a US cover even when the
ROM is from another region.

The local audit tool is at [tools/scrape_misses_audit.cpp](tools/scrape_misses_audit.cpp).
It reads a `scrape-misses.txt` file from the MiSTer and writes its detailed results to a
local report directory, which is excluded from version control.
