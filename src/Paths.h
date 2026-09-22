#pragma once

// Everything the GUI owns on the SD card lives under one directory: the binary, the console
// icons, the fonts and the small state files it writes. Keeping the root in a single place
// means relocating an installation is a one-line change rather than a hunt through headers.
//
// A macro rather than a constant because the defaults built from it are themselves
// `constexpr const char *`, and adjacent string literals concatenate at compile time —
// which a `std::string` root could not do.
#define MISTER_PAT_ROOT "/media/fat/mister-pat"
