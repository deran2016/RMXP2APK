# Repository Guidelines

## Project Structure & Module Organization

RMXP2APK builds experimental ARM64 Android APKs from Lunetile XP projects. `android/builder/` contains the Python CLI, GUI, and builder tests. `android/app/` contains the game activities; `android/core/`, `android/runtime/`, `android/audio/`, and `android/input/` contain the data loader, native/Ruby runtime, audio, and input integrations. `android/host/` contains host UI components and JVM tests. `scripts/` holds project migration and RXDATA/archive tools; `src/runtime/` contains shared Lunetile runtime sources. Diagnostic probes and their fixtures live alongside the relevant modules. `build/` is ignored generated output, not source.

## Build, Test, and Development Commands

Run commands from the repository root on Windows:

```powershell
pythonw android/builder/gui.py
python -X utf8 -B android/builder/build_android.py C:\MyGame C:\Output\MyGame-debug.apk --data-encoding UTF-8
python -X utf8 -B -m unittest discover -s android/builder -p 'test_*.py' -v
python -X utf8 -B -m unittest discover -s android/core -p 'test_*.py' -v
```

These launch the GUI, build a debug APK, and run the builder and core Python suites. APK builds require Python 3.12 with tkinter, JDK 21, Android SDK 36, Build Tools 36.1.0, NDK 28.2.13676358, the Ruby data checker, and an Android ARM64 Ruby build. See `README.md` for setup, encodings, RTP options, and environment variables. Module READMEs describe native builds and emulator probes.

## Coding Style & Naming

Follow the surrounding file's conventions: four-space indentation in Python and Java, two spaces in Ruby, and descriptive names. Python functions and tests use `snake_case` and `test_*.py`; Java classes use `PascalCase` and methods use `camelCase`. Keep platform-specific code in its existing module and preserve copied Lunetile source licensing. No repository-wide formatter or linter is documented.

## Testing Guidelines

Use Python's `unittest` suites for builder and core changes, adding focused regression tests for changed behavior. Host Java tests are under `android/host/test/`; native and Android probes are documented in their module READMEs. Exercise relevant emulator or device paths for runtime changes and report the environment and limits of validation. No coverage threshold is documented.

## Changes & Safety

Keep original game files and existing outputs intact; use separate staging paths and synthetic fixtures where possible. Preserve script IDs, order, untouched bytes, encodings, and save compatibility when changing migration code. Game scripts execute as code and are not sandboxed. For pull requests, describe the behavior, validation performed, compatibility limitations, and any related issue; include screenshots for visible UI changes. Git history is unavailable in this checkout, so follow the project's established commit convention when available.
