# Prebuilt FFmpeg (decoder-only)

Static libraries and headers of FFmpeg 8.0 for the MiSTer (ARM Cortex-A9, NEON), kept here so that
the 11 MB source archive does not have to be. They were built once with `../build-ffmpeg.sh`; a normal
`make deps` only copies them into `third_party/sysroot` (`../install-ffmpeg.sh`).

- **What is in it:** libavformat (MP4 demuxer only), libavcodec (H.264 and AAC decoders, H.264 and
  AAC parsers), libswscale and libswresample (pixel and sample conversion), libavutil. No encoders,
  no muxers, no network, no GPL or non-free code.
- **Licence:** LGPL 2.1 or later (`COPYING.LGPLv2.1`, `LICENSE.md`). It is linked statically into
  the GPL-3.0 GUI library, which is allowed; this repository and the FFmpeg source are both public.
- **Provenance:** `BUILD-INFO.txt` names the version and the SHA-256 of the source archive used
  (unmodified, from ffmpeg.org).
- **Rebuilding:** run `../build-ffmpeg.sh` (it downloads and verifies the source, needs the ARM
  cross-compiler on PATH), then commit the refreshed files. Do this only to change the version or the
  configuration.
- **Check after a rebuild:** NEON must be enabled (the script fails otherwise) and the libraries
  should stay near 4.7 MB in total.
