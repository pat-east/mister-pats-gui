# v0.4.6 — versioned GUI release trial

This release introduces the stable GUI loader and a versioned GUI library. The installer
verifies release metadata and SHA-256 hashes before installing. Settings can check GitHub
for newer versions and install a new GUI library without replacing the bootstrap binaries.

The GUI selects the newest installed library on startup. An optional
`mister-pats-gui.so` symlink pins a specific installed version. Settings shows both the
running version and the startup selection.

Update checks compare with that startup selection. A pinned older version can therefore fetch
v0.4.6 even if a higher library is also stored on the MiSTer.

When automatic update checks are enabled, Settings waits for an Internet connection before
showing "Checking GitHub". Network errors during Wi-Fi startup are retried quietly.

This release is intended to exercise the new packaging and update path before v0.5.0.
For installation and recovery instructions, see `docs/INSTALL.md` at the v0.4.6 tag.
