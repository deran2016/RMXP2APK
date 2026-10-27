# Android build preflight (incomplete)

`python -X utf8 -B android/builder/build_android.py PROJECT OUTPUT.apk --data-encoding UTF-8|CP949|CP932` inspects a copied-project build candidate; it does not modify the original or overwrite an existing output. Use `--ini-encoding` for a non-ASCII BOM-less Game.ini, `--rtp-dir` for an explicitly referenced single RTP, and `--ruby` to choose the Lunetile Ruby 4.0.7 x86 data checker.

Each project gets a stable Android application ID derived from its normalized absolute Windows path; the builder prints it. Different project paths get different app IDs. Moving or renaming a project changes the default ID: pass the printed original ID as `--application-id org.example.game` on subsequent builds to keep the same Android app and its existing saves. Updating an installed APK also requires the same debug signing key; a different machine usually has a different key. Supplying the same explicit ID for different projects intentionally makes them the same Android app, so use it only for versions of the same game.

The checker reads XP data and static resource fields, common event and movement commands, validates encodings, rejects links, unsafe paths and known Windows-only APIs, and reports missing resources with their data locations. Case mismatches are rejected until Android's actual case-insensitive resource resolver exists. Resources assembled dynamically by Ruby or through commands not enumerated by the checker cannot be proven present by this inspection. Archive-only and multiple-RTP projects are not yet supported.

The reviewed stock save scripts are migrated inside the staging copy only; an exact previously converted copy is preserved. Script IDs, names, order, and unmodified entry bytes are retained. Unknown save scripts and other unreviewed direct file access are rejected with their script and location. UTF-8, CP949 and CP932 are compared against the reviewed stock text after strict decoding and written in the selected encoding.

The first Android host currently uses XP's 640×480 logical viewport. Projects explicitly requesting a different `[Lunetile] Width` or `Height` are rejected rather than silently resized.

An extension match and MIDI type 0/1 header check do not establish that the Android runtime will decode or play every file. WMA and MP3 are rejected. MIDI support was checked with a synthetic type 0 fixture; actual playback quality requires device validation.

This is an experimental beta. The builder checks that required runtime sources and build tools exist, but their presence does not certify compatibility with an arbitrary project. `diagnostic_build.py` exercises the same staging and Gradle path using an isolated project copy and names its output as a diagnostic. The Gradle wrapper is pinned to 8.13; this Windows setup used JDK 21, SDK 36, NDK 28.2 and a local SDK aapt2 override.
