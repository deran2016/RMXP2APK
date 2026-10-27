import tempfile
import shutil
import unittest
import zlib
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import build_android as builder
from rxdata import Entry, read, write


class AndroidBuilderTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=builder.ROOT / 'build')
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        self.migration_patch = patch.object(builder, 'migrate_saves', side_effect=lambda raw, encoding: raw)
        self.migration_patch.start()
        self.addCleanup(self.migration_patch.stop)
        data = self.source / 'Data'
        data.mkdir(parents=True)
        (self.source / 'Game.ini').write_text('[Game]\nScripts=Data\\Scripts.rxdata\n', encoding='ascii')
        for name in builder.REQUIRED_DATA:
            (data / f'{name}.rxdata').write_bytes(b'fixture')
        (data / 'Map001.rxdata').write_bytes(b'fixture')
        self.set_script('puts 1')

    def tearDown(self):
        self.temp.cleanup()

    def set_script(self, source):
        entry = Entry(1, b'Main', zlib.compress(source.encode()), b'')
        (self.source / 'Data' / 'Scripts.rxdata').write_bytes(write([entry], {0: source.encode()}))

    def test_project_inspection_and_unsupported_script(self):
        self.assertEqual(builder.inspect_project(self.source, None, 'UTF-8', None), Path('Data/Scripts.rxdata'))
        self.set_script('x = Win32API.new("user32", "x", [], "i")')
        with self.assertRaisesRegex(builder.BuildError, r'Main.*line 1.*Win32API'):
            builder.inspect_project(self.source, None, 'UTF-8', None)

    def test_missing_data_and_rtp_fail_closed(self):
        (self.source / 'Data' / 'Actors.rxdata').unlink()
        with self.assertRaisesRegex(builder.BuildError, 'Actors'):
            builder.inspect_project(self.source, None, 'UTF-8', None)
        (self.source / 'Data' / 'Actors.rxdata').write_bytes(b'fixture')
        (self.source / 'Game.ini').write_text('[Game]\nRTP1=Standard\n', encoding='ascii')
        with self.assertRaisesRegex(builder.BuildError, '--rtp-dir'):
            builder.inspect_project(self.source, None, 'UTF-8', None)

    def test_output_and_runtime_guard_preserve_source(self):
        output = self.root / 'result.apk'
        output.write_bytes(b'existing')
        with self.assertRaisesRegex(builder.BuildError, 'already exists'):
            builder.build(self.source, output, data_encoding='UTF-8')
        self.assertEqual(output.read_bytes(), b'existing')
        before = (self.source / 'Data' / 'Scripts.rxdata').read_bytes()
        output.unlink()
        with patch.object(builder, 'validate_data'), patch.object(builder, 'validate_resources'):
            with self.assertRaisesRegex(builder.BuildError, 'runtime is not complete'):
                builder.build(self.source, output, data_encoding='UTF-8', runtime=self.root / 'missing')
        self.assertFalse(output.exists())
        self.assertEqual(before, (self.source / 'Data' / 'Scripts.rxdata').read_bytes())

    def test_script_path_escape_rejected(self):
        (self.source / 'Game.ini').write_text('[Game]\nScripts=..\\escape.rxdata\n', encoding='ascii')
        with self.assertRaisesRegex(builder.BuildError, 'Invalid Game.ini Scripts path'):
            builder.inspect_project(self.source, None, 'UTF-8', None)

    def test_nondefault_lunetile_resolution_is_rejected(self):
        (self.source / 'Game.ini').write_text('[Game]\nScripts=Data\\Scripts.rxdata\n[Lunetile]\nWidth=1280\nHeight=720\n', encoding='ascii')
        with self.assertRaisesRegex(builder.BuildError, 'only 640x480.*1280x720'):
            builder.inspect_project(self.source, None, 'UTF-8', None)

    def test_application_ids_are_stable_distinct_and_validated(self):
        first = builder.application_id(self.source)
        self.assertEqual(first, builder.application_id(Path(str(self.source).upper())))
        self.assertNotEqual(first, builder.application_id(self.root / 'other-project'))
        self.assertEqual('com.example.game', builder.application_id(self.source, 'com.example.game'))
        for value in ('', 'single', '1bad.game', 'bad.-name', 'bad.name;cmd', 'bad..name', 'a.' + 'x' * 254):
            with self.subTest(value=value), self.assertRaisesRegex(builder.BuildError, 'Invalid Android application ID'):
                builder.application_id(self.source, value)

    def test_script_encoding_is_explicit(self):
        source = 'puts "한글"'.encode('cp949')
        entry = Entry(1, '메인'.encode('cp949'), zlib.compress(source), b'')
        (self.source / 'Data' / 'Scripts.rxdata').write_bytes(write([entry], {0: source}))
        builder.inspect_project(self.source, None, 'CP949', None)
        with self.assertRaisesRegex(builder.BuildError, 'invalid UTF-8'):
            builder.inspect_project(self.source, None, 'UTF-8', None)

    def test_stock_save_migration_preserves_other_entries_and_rejects_changes(self):
        self.migration_patch.stop()
        source = builder.ROOT / 'System' / 'Data' / 'Scripts.rxdata'
        if not source.is_file():
            self.skipTest('Original stock XP scripts fixture is unavailable')
        before = read(source.read_bytes())
        after = read(builder.migrate_saves(source.read_bytes(), 'UTF-8'))
        self.assertEqual([(entry.id, entry.name) for entry in before], [(entry.id, entry.name) for entry in after])
        for old, new in zip(before, after):
            if old.name.decode('utf-8') not in builder.SAVE_NAMES:
                self.assertEqual(old.raw, new.raw)
        index = next(i for i, entry in enumerate(before) if entry.name == b'Scene_Save')
        edited = write(before, {index: before[index].source() + b'\n# user edit\n'})
        with self.assertRaisesRegex(builder.BuildError, r'Scene_Save.*unreviewed save script'):
            builder.migrate_saves(edited, 'UTF-8')

    def test_save_migration_retains_selected_script_encoding(self):
        import hashlib
        self.migration_patch.stop()
        for encoding, comment in [('CP949', '한글'), ('CP932', '日本語')]:
            entries, replacements, hashes = [], {}, []
            for index, name in enumerate(builder.SAVE_NAMES):
                source = ('\n'.join(before for before, _ in builder.RULES[name]) + '\n# ' + comment).encode(encoding)
                entries.append(Entry(index, name.encode('ascii'), zlib.compress(source), b''))
                replacements[index] = source
                hashes.append({'name': name, 'source_sha256': hashlib.sha256(source.decode(encoding).encode('utf-8')).hexdigest()})
            raw = write(entries, replacements)
            with patch.object(builder.json, 'loads', return_value={'scripts': hashes}):
                migrated = read(builder.migrate_saves(raw, encoding))
            for entry in migrated:
                self.assertIn(comment, entry.source().decode(encoding))

    def test_real_stock_reencoded_cp949_and_cp932(self):
        self.migration_patch.stop()
        source = builder.ROOT / 'System' / 'Data' / 'Scripts.rxdata'
        if not source.is_file():
            self.skipTest('Original stock XP scripts fixture is unavailable')
        original = read(source.read_bytes())
        for encoding in ('CP949', 'CP932'):
            entries = [Entry(e.id, e.name.decode('utf-8').encode(encoding), e.compressed, e.raw) for e in original]
            raw = write(entries, {i: e.source().decode('utf-8').encode(encoding) for i, e in enumerate(original)})
            before, after = read(raw), read(builder.migrate_saves(raw, encoding))
            self.assertEqual([(e.id, e.name) for e in before], [(e.id, e.name) for e in after])
            for old, new in zip(before, after):
                new.source().decode(encoding)
                if old.name.decode(encoding) not in builder.SAVE_NAMES:
                    self.assertEqual(old.raw, new.raw)

    def test_known_converted_save_scripts_are_not_changed_again(self):
        self.migration_patch.stop()
        source = builder.ROOT / 'System' / 'Data' / 'Scripts.rxdata'
        if not source.is_file():
            self.skipTest('Original stock XP scripts fixture is unavailable')
        converted = builder.migrate_saves(source.read_bytes(), 'UTF-8')
        self.assertEqual(converted, builder.migrate_saves(converted, 'UTF-8'))

    def test_high_refresh_converts_reviewed_scripts_and_rejects_edits(self):
        import hashlib
        from high_refresh import RULES as timing_rules
        names = ['Game_Character 2', 'Main']
        sources = ['\n'.join(before for before, _ in timing_rules[names[0]]), '# Main\n']
        entries = [Entry(i, name.encode(), zlib.compress(text.encode()), b'') for i, (name, text) in enumerate(zip(names, sources))]
        raw = write(entries, {i: text.encode() for i, text in enumerate(sources)})
        stock = {'scripts': [{'name': name, 'source_sha256': hashlib.sha256(text.encode()).hexdigest()} for name, text in zip(names, sources)]}
        with patch.object(builder.json, 'loads', return_value=stock):
            converted = read(builder.migrate_high_refresh(raw, 'UTF-8'))
            self.assertEqual([(e.id, e.name) for e in entries], [(e.id, e.name) for e in converted])
            self.assertIn('Timing.step_distance', converted[0].source().decode())
            self.assertIn('def self.import_save', converted[1].source().decode())
            modified = write(read(raw), {0: (sources[0] + '# changed').encode()})
            with self.assertRaisesRegex(builder.BuildError, 'reviewed stock scripts'):
                builder.migrate_high_refresh(modified, 'UTF-8')

    def test_resource_references_report_the_location(self):
        line = 'Graphics/Characters\t' + '한글'.encode().hex() + '\tActors.rxdata[1].character_name\n'
        result = SimpleNamespace(returncode=0, stdout=line, stderr='')
        with patch.object(builder.subprocess, 'run', return_value=result):
            with self.assertRaisesRegex(builder.BuildError, 'Actors.rxdata.*missing resource Graphics/Characters/한글'):
                builder.validate_resources(self.source, 'UTF-8', self.root, Path('ruby'), None)
            character = self.source / 'Graphics' / 'Characters'
            character.mkdir(parents=True)
            (character / '한글.png').write_bytes(b'fixture')
            builder.validate_resources(self.source, 'UTF-8', self.root, Path('ruby'), None)

    def test_real_ruby_detects_missing_project_resource(self):
        fixture = builder.ROOT / 'build' / 'RGSSXP-validation-playable-20261018' / 'Data'
        ruby = builder.ROOT / 'build' / 'runtime-native' / 'bin' / 'Release' / 'rgssxp-ruby.exe'
        if not fixture.is_dir() or not ruby.is_file():
            self.skipTest('Local XP validation fixture and Lunetile Ruby are not present')
        project = self.root / 'stock'
        shutil.copytree(fixture, project / 'Data')
        with self.assertRaisesRegex(builder.BuildError, r'\.rxdata.*missing resource (Graphics|Audio)/'):
            builder.validate_resources(project, 'UTF-8', self.root, ruby, None)

    def test_resource_case_extension_priority_and_escape(self):
        reference = lambda name: SimpleNamespace(returncode=0, stdout='Graphics/Characters\t' + name.encode().hex() + '\tMap001.rxdata.event\n', stderr='')
        project_assets = self.source / 'Graphics' / 'Characters'
        project_assets.mkdir(parents=True)
        (project_assets / 'Hero.PNG').write_bytes(b'project')
        rtp = self.root / 'rtp'
        rtp_assets = rtp / 'Graphics' / 'Characters'
        rtp_assets.mkdir(parents=True)
        (rtp_assets / 'hero.png').write_bytes(b'rtp')
        (rtp_assets / 'hero.jpg').write_bytes(b'rtp-ambiguous')
        with patch.object(builder.subprocess, 'run', return_value=reference('hero')):
            with self.assertRaisesRegex(builder.BuildError, 'Android path case mismatch'):
                builder.validate_resources(self.source, 'UTF-8', self.root, Path('ruby'), rtp)
            (project_assets / 'Hero.PNG').rename(project_assets / 'rename.tmp')
            (project_assets / 'rename.tmp').rename(project_assets / 'hero.png')
            builder.validate_resources(self.source, 'UTF-8', self.root, Path('ruby'), rtp)
        with patch.object(builder.subprocess, 'run', return_value=reference('../secret')):
            with self.assertRaisesRegex(builder.BuildError, 'Map001.rxdata.event: invalid resource path'):
                builder.validate_resources(self.source, 'UTF-8', self.root, Path('ruby'), rtp)

    def test_unsupported_audio_format_is_rejected(self):
        audio = self.source / 'Audio' / 'BGM'
        audio.mkdir(parents=True)
        (audio / 'theme.mp3').write_bytes(b'fixture')
        result = SimpleNamespace(returncode=0, stdout='Audio/BGM\t' + b'theme'.hex() + '\tSystem.rxdata.title_bgm\n', stderr='')
        with patch.object(builder.subprocess, 'run', return_value=result):
            with self.assertRaisesRegex(builder.BuildError, 'System.rxdata.*unsupported Android audio format'):
                builder.validate_resources(self.source, 'UTF-8', self.root, Path('ruby'), None)

    def test_project_link_is_rejected(self):
        try:
            (self.source / 'linked').symlink_to(self.root, target_is_directory=True)
        except OSError:
            self.skipTest('Windows symlink creation is unavailable')
        with self.assertRaisesRegex(builder.BuildError, 'Project links are not supported'):
            builder.inspect_project(self.source, None, 'UTF-8', None)


if __name__ == '__main__':
    unittest.main()
