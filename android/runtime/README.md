# Experimental Android runtime

This is an incomplete Android port of Lunetile XP. It is not a verified replacement for `Lunetile.dll`, and the diagnostic APKs are not game releases.

- `build-ruby-arm64.sh` checks the pinned Ruby 4.0.7 archive SHA-256, builds a PIC ARM64 static VM for Android API 26 with NDK 28, and generates the encoding tables. It refuses an existing build directory. Run it with MSYS2 bash, an installed Ruby 3.1+ with ERB, `make`, and an NDK path without spaces.
- `game_jni.cpp` joins the data core, Ruby script loader, Bitmap/Graphics, input, audio and Android Surface. Ruby is initialized once per process. Stop and pause reach the VM at `Graphics` frame boundaries; a script that never yields a frame can prevent timely shutdown.
- `lunetile/` contains the MIT-licensed Lunetile graphics Ruby sources and autotile metadata. Android's CPU renderer uses their legacy command path. It supports rectangular clips, viewport tone/color, normal/add/subtract sprite blending, rotation, sprite tone/color, bush opacity and transition masks. These paths need further parity, performance and full-game validation.
- `BitmapCodec` uses Android BitmapFactory and Canvas. Font appearance and metrics are Android-specific; Windows GDI parity is not established. MIDI playback, looping and pitch are subject to Android MediaPlayer behavior.
- `probe/` contains independently named diagnostics. `build-probe.ps1` produces a Ruby/Bitmap/Graphics probe, while `build-game-probe.ps1` packages explicitly supplied fixture assets. These do not mark the engine ready or change the original project.

## Recorded checks

On Android 16/API 36 x86_64 emulator `emulator-5562` with ARM64 translation, Ruby setup and evaluation, PNG decode, Korean text drawing, viewport tone and transition mask pixels, and Surface presentation passed the isolated JNI checks. The Ruby archive SHA-256 was `a60e649149434ce9ace4f6b17b57d6c271eee5e931e58073337a392f4f27af0c`.

The XP validation project was copied into a separate diagnostic APK. Entry 89, `RGSSXP stock boot test`, was removed from the copied script archive because it writes test map files at startup; the original was not changed, and all other script entries retained their IDs, order and bytes. The original Scripts SHA-256 was `d3afc04ff450a25e35ef243654d69814ab4d10347fb393f7a1a7574790fb2fc0`; the diagnostic archive SHA-256 was `2de0b5ebe5913ceb4eb200421dc08f1aec1f318471601fd049d482e5fd5af13d`.

With original MIDI assets, the diagnostic reached the title, selected New Game, displayed a map and dialogue, executed the Continue choice, applied viewport tone and entered the battle screen. This is not evidence of battle completion, save/restart, physical-device compatibility, audio quality, or broad script compatibility. A separate earlier graphics-only test inserted a silent WAV fixture; its result is not evidence of MIDI playback.
