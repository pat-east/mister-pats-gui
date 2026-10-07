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

When scraping, the application downloads matched PNGs and saves them as JPEGs in the
`media` directory for each system: a full-size file `<Title>.jpg` and a smaller
`<Title>-sm.jpg` for the Grid view. Existing images are skipped by default. System
files and BIOS entries are not treated as games. Unmatched titles are written to
`/media/fat/mister-pat/scrape-misses.txt`.

## Results and device verification

An **index match** means only that the server lists a matching image name. The analysis
did not download or decode the image files. A scrape with the updated application has
since been completed on the MiSTer. The audit input was dated September 30, 2026, so the
table above describes that snapshot and does not report the later device run's
fetched/skipped/missing totals. For titles with
multiple regional versions, the current index often prefers a US cover even when the
ROM is from another region.

The local audit tool is at [tools/scrape_misses_audit.cpp](tools/scrape_misses_audit.cpp).
It reads a `scrape-misses.txt` file from the MiSTer and writes its detailed results to a
local report directory, which is excluded from version control.
