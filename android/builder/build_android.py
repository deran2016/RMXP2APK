"""Validate an XP project and build a debug APK without changing the project."""
import argparse
import configparser
import json
import hashlib
import ntpath
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from migrate_project import MigrationError, RULES, inspect_data, read_project_ini
from rxdata import read, write

RUNTIME = ROOT / 'android' / 'runtime'
REQUIRED_DATA = ('Actors', 'Animations', 'Armors', 'Classes', 'CommonEvents',
                 'Enemies', 'Items', 'MapInfos', 'Skills', 'States', 'System',
                 'Tilesets', 'Troops', 'Weapons')
UNSUPPORTED = re.compile(r'\b(?:Win32API|Win32OLE|Fiddle|DL|Lunetile::(?:Browser|Network|UIInput))\b')
SAVE_NAMES = ('Window_SaveFile', 'Scene_Title', 'Scene_File', 'Scene_Save', 'Scene_Load')
CONVERTED_SAVE_HASHES = {
    'Window_SaveFile': 'aed8994a461d33e1cf6c7a594284fb06f83cd6fb7850e1e77cb2a28730f53db8',
    'Scene_Title': '2db2db66b385950a61addec4e8183db9207878ecaded6a5c222ae399934afa4e',
    'Scene_File': 'cac5ef655f6e5bd5e67a820115a2ab15efa473dd5995513ba3c4c678952a57e3',
    'Scene_Save': '9ff32742cb8ea0ce5acabacf25b81a5762a5e57d03d8dcca094362dbafc009b1',
    'Scene_Load': '406c9c25390459bd3bc8c6e40537c878d7110b4819eda6deff47666d19822493',
}
UNREVIEWED_SAVE = re.compile(r'\b(?:Marshal|File|FileTest)\s*(?:\.|::)|\bRGSSXP::Saves\b|Save\d+\.rxdata', re.I)


class BuildError(ValueError):
    pass


def application_id(source, explicit=None):
    if explicit is not None:
        if len(explicit) > 255 or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+', explicit):
            raise BuildError('Invalid Android application ID: use dot-separated segments starting with letters')
        return explicit
    normalized = ntpath.normcase(ntpath.normpath(str(source.resolve())))
    return 'org.lunetile.game.p' + hashlib.sha256(normalized.encode('utf-8')).hexdigest()[:16]


def project_file(project, value):
    # Game.ini uses Windows path separators even on non-Windows test hosts.
    parts = value.replace('\\', '/').split('/')
    if not parts or any(part in ('', '.', '..') or ':' in part for part in parts):
        raise BuildError(f'Invalid Game.ini Scripts path: {value}')
    path = project.joinpath(*parts)
    if not path.is_file():
        raise BuildError(f'Missing scripts: {path}')
    return path


def validate_tree(directory):
    for path in directory.rglob('*'):
        if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
            raise BuildError(f'Project links are not supported: {path}')


def migrate_saves(raw, encoding):
    entries = read(raw)
    stock = json.loads((ROOT / 'src/runtime/stock_scripts.json').read_text(encoding='utf-8'))['scripts']
    known = {item['name']: item['source_sha256'] for item in stock if item['name'] in SAVE_NAMES}
    replacements, seen = {}, set()
    for index, entry in enumerate(entries):
        try:
            name = entry.name.decode(encoding)
            text = entry.source().decode(encoding)
        except UnicodeDecodeError as error:
            raise BuildError(f'Script {index}: invalid {encoding} at byte {error.start}') from error
        if name in SAVE_NAMES:
            digest = hashlib.sha256(text.encode('utf-8')).hexdigest()
            if name in seen or digest not in (known.get(name), CONVERTED_SAVE_HASHES.get(name)):
                raise BuildError(f'Script {index} ({name}), line 1: unreviewed save script; cannot safely migrate')
            seen.add(name)
            if digest == CONVERTED_SAVE_HASHES.get(name):
                continue
            text = text.replace('\r\n', '\n')
            for before, after in RULES[name]:
                if text.count(before) != 1:
                    raise BuildError(f'Script {index} ({name}): expected save migration rule exactly once')
                text = text.replace(before, after)
            replacements[index] = text.encode(encoding)
        else:
            match = UNREVIEWED_SAVE.search(text)
            if match:
                raise BuildError(f'Script {index} ({name}), line {text.count(chr(10), 0, match.start()) + 1}: unreviewed file/save access')
    missing = set(SAVE_NAMES) - seen
    if missing:
        raise BuildError('Missing reviewed stock save scripts: ' + ', '.join(sorted(missing)))
    return write(entries, replacements)


def inspect_project(source, ini_encoding, data_encoding, rtp_dir):
    if not source.is_dir():
        raise BuildError(f'Project directory does not exist: {source}')
    validate_tree(source)
    if not (source / 'Game.ini').is_file():
        raise BuildError('Missing Game.ini')
    ini, _ = read_project_ini(source / 'Game.ini', ini_encoding)
    sections = [s for s in ini.sections() if s.casefold() == 'game']
    if len(sections) != 1:
        raise BuildError('Game.ini must have exactly one [Game] section')
    section = sections[0]
    settings = [s for s in ini.sections() if s.casefold() == 'lunetile']
    if len(settings) > 1:
        raise BuildError('Game.ini has ambiguous [Lunetile] sections')
    if settings:
        try:
            width = ini.getint(settings[0], 'Width', fallback=640)
            height = ini.getint(settings[0], 'Height', fallback=480)
        except ValueError as error:
            raise BuildError('Game.ini [Lunetile] Width/Height must be integers') from error
        if (width, height) != (640, 480):
            raise BuildError(f'Android currently supports only 640x480; Game.ini requests {width}x{height}')
    scripts = project_file(source, ini.get(section, 'Scripts', fallback='Data\\Scripts.rxdata'))
    if not scripts.is_relative_to(source / 'Data'):
        raise BuildError('Scripts must be inside the Data directory')
    if (source / 'Game.rgssad').exists():
        raise BuildError('Game.rgssad is not supported by this builder; use an unpacked project')
    for name in REQUIRED_DATA:
        if not (source / 'Data' / (name + '.rxdata')).is_file():
            raise BuildError(f'Missing data: Data/{name}.rxdata')
    if not list((source / 'Data').glob('Map[0-9][0-9][0-9].rxdata')):
        raise BuildError('Missing map data: Data/MapNNN.rxdata')
    if data_encoding is None:
        # Non-ASCII Marshal bytes require a known encoding; names alone are not enough to infer it.
        raise BuildError('Project data requires explicit --data-encoding UTF-8, CP949 or CP932')
    for index, entry in enumerate(read(scripts.read_bytes())):
        try:
            name = entry.name.decode(data_encoding)
            code = entry.source().decode(data_encoding)
        except UnicodeDecodeError as error:
            raise BuildError(f'Script {index}: invalid {data_encoding} at byte {error.start}') from error
        match = UNSUPPORTED.search(code)
        if match:
            line = code.count('\n', 0, match.start()) + 1
            raise BuildError(f'Script {index} ({name}), line {line}: unsupported {match.group()}')
    migrate_saves(scripts.read_bytes(), data_encoding)
    rtps = [ini.get(section, f'RTP{i}', fallback='').strip() for i in range(1, 4)]
    rtps = [name for name in rtps if name]
    if len(rtps) > 1:
        raise BuildError('Multiple RTPs need separate resource resolution and are not supported')
    if rtps and not rtp_dir:
        raise BuildError(f'RTP {rtps[0]} is required: provide --rtp-dir with its Graphics and Audio directories')
    if rtp_dir:
        if not rtp_dir.is_dir() or not (rtp_dir / 'Graphics').is_dir() or not (rtp_dir / 'Audio').is_dir():
            raise BuildError(f'RTP directory must contain Graphics and Audio: {rtp_dir}')
        validate_tree(rtp_dir)
    return scripts.relative_to(source)


def validate_data(source, encoding, stage, ruby):
    if not ruby.is_file():
        raise BuildError(f'Data inspection requires a Lunetile Ruby checker: {ruby}; use --ruby to select rgssxp-ruby.exe')
    version = subprocess.run([str(ruby), '--disable-gems', '-e', 'abort "Ruby 4.0.7 x86 required" unless RUBY_VERSION == "4.0.7" && [0].pack("J").bytesize == 4'], capture_output=True, text=True)
    if version.returncode:
        raise BuildError(f'Ruby checker version is incompatible: {version.stderr.strip()}')
    report = stage / 'report'
    (report / 'migration').mkdir(parents=True)
    try:
        inspect_data(source, None, ruby, stage, report, encoding)
    except MigrationError as error:
        raise BuildError(str(error)) from error


def validate_runtime(runtime):
    required = ('gradlew.bat', 'settings.gradle', 'CMakeLists.txt', 'app/build.gradle',
                'app/src/main/AndroidManifest.xml', 'gradle/wrapper/gradle-wrapper.jar',
                'game_jni.cpp', 'graphics.cpp', 'bitmap.cpp', 'raster.cpp', 'surface.cpp')
    missing = [path for path in required if not (runtime / path).is_file()]
    if missing:
        raise BuildError('Android runtime is not complete; missing: ' + ', '.join(missing))


def validate_resources(source, encoding, stage, ruby, rtp_dir):
    schema = json.loads((ROOT / 'src/runtime/rpg_schema.json').read_text(encoding='utf-8'))
    declarations = ['module RPG; end']
    for name, fields in schema.items():
        declarations.extend(['class ' + name + '; end', name + '.class_eval { attr_accessor ' + ','.join(':' + f for f in fields) + ' }'])
    checker = stage / 'resource-check.rb'
    setup = "module RGSSXP; DATA_ENCODING = " + repr(encoding) + "; end\n"
    checker.write_text('\n'.join(declarations) + '\n' + setup + (ROOT / 'src/runtime/text.rb').read_text(encoding='utf-8') + '\n' + (ROOT / 'android/builder/resource_references.rb').read_text(encoding='utf-8'), encoding='utf-8')
    result = subprocess.run([str(ruby), '--disable-gems', str(checker), str(source / 'Data')], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if result.returncode:
        raise BuildError('Resource inspection failed: ' + result.stderr.strip())
    for line in result.stdout.splitlines():
        group, encoded, location = line.split('\t', 2)
        name = bytes.fromhex(encoded).decode('utf-8')
        parts = name.replace('\\', '/').split('/')
        if any(part in ('', '.', '..') or ':' in part for part in parts):
            raise BuildError(f'{location}: invalid resource path {name!r}')
        prefixes = [source] + ([rtp_dir] if rtp_dir else [])
        extensions = ('.png', '.jpg', '.jpeg', '.bmp') if group.startswith('Graphics/') else ('.ogg', '.wav', '.mp3', '.mid', '.midi', '.wma')
        def exists(base):
            node = base
            for part in group.split('/') + parts[:-1]:
                if not node.is_dir():
                    return False
                matches = [child for child in node.iterdir() if child.name.casefold() == part.casefold()]
                if len(matches) > 1:
                    raise BuildError(f'Ambiguous resource directory: {node / part}')
                if not matches:
                    return False
                if matches[0].name != part:
                    raise BuildError(f'{location}: Android path case mismatch: {node / part} vs {matches[0]}')
                node = matches[0]
            if not node.is_dir():
                return False
            target = parts[-1].casefold()
            matches = [child for child in node.iterdir() if child.is_file() and (child.name.casefold() == target or child.name.casefold() in [target + ext for ext in extensions])]
            if len(matches) > 1:
                raise BuildError(f'{location}: ambiguous resource {group}/{name}')
            if matches and matches[0].name not in [parts[-1]] + [parts[-1] + ext for ext in extensions]:
                raise BuildError(f'{location}: Android path case mismatch: {group}/{name} vs {matches[0].name}')
            if matches and group.startswith('Audio/') and matches[0].suffix.lower() not in ('.wav', '.ogg', '.mid', '.midi'):
                raise BuildError(f'{location}: unsupported Android audio format: {matches[0]}')
            if matches and group.startswith('Audio/') and matches[0].suffix.lower() in ('.mid', '.midi'):
                with matches[0].open('rb') as stream:
                    header = stream.read(14)
                if len(header) != 14 or header[:8] != b'MThd\x00\x00\x00\x06' or header[8:10] not in (b'\x00\x00', b'\x00\x01'):
                    raise BuildError(f'{location}: only MIDI SMF type 0 or 1 is supported: {matches[0]}')
            return bool(matches)
        if not any(exists(base) for base in prefixes):
            raise BuildError(f'{location}: missing resource {group}/{name}')


def build(source, output, ini_encoding=None, data_encoding=None, rtp_dir=None, runtime=RUNTIME, ruby=None, app_id=None):
    source, output, runtime = source.resolve(), output.resolve(), runtime.resolve()
    app_id = application_id(source, app_id)
    rtp_dir = rtp_dir.resolve() if rtp_dir else None
    ruby = (ruby or ROOT / 'build' / 'runtime-native' / 'bin' / 'Release' / 'rgssxp-ruby.exe').resolve()
    if not output.parent.is_dir():
        raise BuildError('Output parent directory must exist')
    if output.suffix.lower() != '.apk':
        raise BuildError('Output filename must end in .apk')
    if output.exists():
        raise BuildError(f'Output already exists: {output}')
    if output.is_relative_to(source):
        raise BuildError('Output must be outside the original project')
    script_path = inspect_project(source, ini_encoding, data_encoding, rtp_dir)
    with tempfile.TemporaryDirectory(prefix='.android-build-', dir=output.parent) as temp:
        stage = Path(temp)
        validate_data(source, data_encoding, stage, ruby)
        validate_resources(source, data_encoding, stage, ruby, rtp_dir)
        validate_runtime(runtime)
        project = stage / 'runtime'
        shutil.copytree(runtime, project, ignore=shutil.ignore_patterns('.gradle', 'build'))
        for folder in ('app', 'audio', 'core', 'host', 'input'):
            shutil.copytree(ROOT / 'android' / folder, stage / folder, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        game = project / 'app' / 'src' / 'main' / 'assets' / 'game'
        if game.exists():
            raise BuildError('Runtime template already contains game assets')
        game.mkdir(parents=True)
        if rtp_dir:
            for folder in ('Graphics', 'Audio'):
                shutil.copytree(rtp_dir / folder, game / folder)
        for folder in ('Data', 'Graphics', 'Audio'):
            if (source / folder).is_dir():
                shutil.copytree(source / folder, game / folder, dirs_exist_ok=True)
        (game / script_path).write_bytes(migrate_saves((source / script_path).read_bytes(), data_encoding))
        shutil.copy2(source / 'Game.ini', game / 'Game.ini')
        (game / 'android-build.json').write_text(json.dumps({'data_encoding': data_encoding, 'ini_encoding': ini_encoding}), encoding='utf-8')
        env = os.environ.copy()
        env.setdefault('LUNETILE_ANDROID_RUBY_ROOT', str(ROOT / 'build' / 'android-ruby-arm64-pic'))
        print('Android application ID:', app_id, flush=True)
        print('Keep this ID and pass --application-id when moving the project to retain the same Android app.', flush=True)
        sdk_candidates = [env.get('ANDROID_SDK_ROOT'), env.get('ANDROID_HOME'), str(Path(env.get('LOCALAPPDATA', '')) / 'Android' / 'Sdk')]
        sdk = next((Path(path) for path in sdk_candidates if path and (Path(path) / 'platforms').is_dir()), None)
        if sdk is None:
            raise BuildError('Android SDK not found; set ANDROID_SDK_ROOT')
        env['ANDROID_SDK_ROOT'] = env['ANDROID_HOME'] = str(sdk)
        java_home = env.get('JAVA_HOME')
        java = str(Path(java_home) / 'bin' / 'java.exe') if java_home else shutil.which('java')
        if not java or not Path(java).is_file():
            raise BuildError('JDK not found; set JAVA_HOME')
        scratch = ROOT / 'build' / 'android-gradle-tmp'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='g-', dir=scratch) as short_temp:
            env['TMP'] = env['TEMP'] = short_temp
            wrapper = project / 'gradle' / 'wrapper' / 'gradle-wrapper.jar'
            command = [java, '-Djava.io.tmpdir=' + short_temp, '-classpath', str(wrapper), 'org.gradle.wrapper.GradleWrapperMain', '--no-daemon',
                       '-Dorg.gradle.jvmargs=-Xmx2g -Dfile.encoding=UTF-8 -Djava.io.tmpdir=' + short_temp]
            aapt = sdk / 'build-tools' / '36.1.0' / 'aapt2.exe'
            if aapt.is_file():
                command.append('-Pandroid.aapt2FromMavenOverride=' + str(aapt))
            command.append('-PlunetileApplicationId=' + app_id)
            result = subprocess.run(command + ['assembleDebug'], cwd=project, env=env)
        if result.returncode:
            raise BuildError(f'Gradle failed with exit code {result.returncode}')
        apk = project / 'app' / 'build' / 'outputs' / 'apk' / 'debug' / 'app-debug.apk'
        if not apk.is_file() or apk.stat().st_size == 0:
            raise BuildError('Gradle did not produce app-debug.apk')
        # A hard link makes the final publish atomic and fails if a competing output exists.
        os.link(apk, output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--ini-encoding', choices=['UTF-8', 'CP949', 'CP932'])
    parser.add_argument('--data-encoding', choices=['UTF-8', 'CP949', 'CP932'])
    parser.add_argument('--rtp-dir', type=Path)
    parser.add_argument('--ruby', type=Path, help='Lunetile Ruby 4.0.7 x86 executable used to inspect project data')
    parser.add_argument('--application-id', help='Stable Android package ID; reuse it when moving a project to keep its app and saves')
    args = parser.parse_args()
    print('Experimental Android XP build: compatibility is limited to the documented test scope.', flush=True)
    try:
        result = build(args.project, args.output, args.ini_encoding, args.data_encoding, args.rtp_dir, ruby=args.ruby, app_id=args.application_id)
    except (BuildError, MigrationError, OSError, ValueError, configparser.Error) as error:
        parser.exit(1, f'Android build stopped: {error}\n')
    print(f'Android debug APK: {result}')


if __name__ == '__main__':
    main()
