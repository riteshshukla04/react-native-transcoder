# Licensing

This package is MIT. Codec dependencies are not.

FFmpeg 7.1.1 is linked, LGPL-2.1-or-later, configured with `--disable-gpl --disable-nonfree`. It is
linked **dynamically** on both platforms — per-ABI `.so` files on Android, embedded XCFrameworks on
iOS — so the LGPL components stay replaceable.

Three external encoder libraries are built from pinned source and linked **statically into those
shared FFmpeg libraries**. Nothing new ships as its own binary; `libavcodec` simply grows.

| Library | Version | License | Provides |
|---|---|---|---|
| LAME | 3.100 | LGPL-2.0-or-later | MP3 encode |
| Opus | 1.5.2 | BSD-3-Clause | Opus encode |
| libvorbis | 1.3.7 | BSD-3-Clause | Vorbis encode |
| libogg | 1.3.5 | BSD-3-Clause | libvorbis dependency |

LAME is the only one of the four with a copyleft license, and it carries the same obligation FFmpeg
already does: because it is absorbed into `libavcodec`, that whole shared library — not just the
FFmpeg part of it — must stay replaceable by a user who wants to swap in their own LAME. The
existing dynamic-linking posture is what satisfies this, so it is now load-bearing for two
dependencies rather than one. `scripts/build-codec-libs.sh` pins every source URL, version and
SHA-256 and records the two patches applied, so the exact library can be rebuilt and relinked.

No `--enable-gpl` flag is needed for any of these: LAME is LGPL, and Opus and Vorbis are BSD.

Still outstanding before a public release:

- `dependencies/manifest.lock.json` records the source URL, version, SHA-256, license, configure
  flags, patches and the exact component allowlist for all five dependencies; both build scripts
  verify every checksum before extracting.
- Each artifact must ship a generated `media-capabilities.json` and `third-party-notices.json`,
  and CI must fail when binary symbols, configure flags and the manifest disagree. The notices file
  must now carry four upstream license texts, not just FFmpeg's.
- The iOS LGPL packaging design (app-embedded dynamic XCFrameworks preferred) needs legal sign-off
  before any iOS artifact ships, now covering statically absorbed LAME as well as FFmpeg.

This file is a checklist, not legal advice.
