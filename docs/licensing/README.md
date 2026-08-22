# Licensing

This package is MIT. Codec dependencies are not.

FFmpeg 7.1.1 is linked, LGPL-2.1-or-later, configured with `--disable-gpl --disable-nonfree` and
no external codec libraries. It is linked **dynamically** on both platforms — per-ABI `.so` files
on Android, embedded XCFrameworks on iOS — so the LGPL components stay replaceable.

Still outstanding before a public release:

- `dependencies/manifest.lock.json` records the source URL, version, SHA-256, license, configure
  flags and the exact component allowlist; both build scripts verify the checksum before extracting.
- Each artifact must ship a generated `media-capabilities.json` and `third-party-notices.json`,
  and CI must fail when binary symbols, configure flags and the manifest disagree.
- The iOS LGPL packaging design (app-embedded dynamic XCFrameworks preferred) needs legal sign-off
  before any iOS artifact ships.

This file is a checklist, not legal advice.
