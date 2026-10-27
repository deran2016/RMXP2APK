"""Build a disposable stock-XP diagnostic without marking the runtime ready."""
import hashlib
import argparse
import json
import shutil
import tempfile
import zipfile
from pathlib import Path
import build_android as builder

root = builder.ROOT
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--fixture', type=Path, default=root / 'build/android-game-stock-diagnostic/assets/game')
parser.add_argument('--output', type=Path, default=root / 'build/android-builder-diagnostic.apk')
parser.add_argument('--preserve-scripts', action='store_true', help='Use the fixture scripts instead of replacing them with original stock scripts')
args = parser.parse_args()
fixture = args.fixture.resolve()
stock = root / 'System/Data/Scripts.rxdata'
output = args.output.resolve()
if not fixture.is_dir() or not stock.is_file():
    raise SystemExit('A local stock diagnostic fixture and original stock scripts are required')
if output.exists():
    raise SystemExit(f'Refusing to overwrite {output}')
manifest_path = output.with_suffix('.sources.json')
if manifest_path.exists():
    raise SystemExit(f'Refusing to overwrite {manifest_path}')
def source_hashes():
    hashes = {}
    for name in ('runtime', 'app', 'audio', 'core', 'host', 'input'):
        source_root = root / 'android' / name
        for path in sorted(source_root.rglob('*')):
            relative = path.relative_to(source_root)
            if any(part in ('build', '.gradle', '.cxx', '__pycache__') for part in relative.parts):
                continue
            if path.is_file() and path.suffix != '.pyc':
                hashes[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    return hashes
before = source_hashes()
digest = hashlib.sha256(stock.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='android-builder-probe-', dir=root / 'build') as temp:
    stage = Path(temp)
    source = stage / 'game'
    runtime = stage / 'runtime'
    shutil.copytree(fixture, source)
    if not args.preserve_scripts:
        shutil.copy2(stock, source / 'Data/Scripts.rxdata')
    shutil.copytree(root / 'android/runtime', runtime, ignore=shutil.ignore_patterns('.gradle', 'build'))
    builder.build(source, output, data_encoding='UTF-8', runtime=runtime, app_id=builder.application_id(fixture))
if hashlib.sha256(stock.read_bytes()).hexdigest() != digest:
    raise SystemExit('Original stock scripts unexpectedly changed')
after = source_hashes()
if before != after:
    raise SystemExit(f'Source changed during build; diagnostic APK is not traceable: {output}')
with zipfile.ZipFile(output) as apk:
    native_hash = hashlib.sha256(apk.read('lib/arm64-v8a/liblunetile_game.so')).hexdigest()
with manifest_path.open('x', encoding='utf-8') as stream:
    json.dump({'source_sha256': before, 'apk_sha256': hashlib.sha256(output.read_bytes()).hexdigest(), 'native_sha256': native_hash}, stream, indent=2)
print('Diagnostic APK only, not runtime certification:', output)
print('SHA-256:', hashlib.sha256(output.read_bytes()).hexdigest())
print('Native SHA-256:', native_hash)
print('Source manifest:', manifest_path)
