"""Cross-check Android save migration against the installed XP stock scripts."""
import hashlib
import sys
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'android' / 'builder'), str(ROOT / 'scripts')]
import build_android as builder
from rxdata import Entry, integer, read, string, write


class SaveMigrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = ROOT / 'System' / 'Data' / 'Scripts.rxdata'
        if not cls.path.is_file():
            raise unittest.SkipTest('Installed stock XP script fixture is unavailable')
        cls.stock = cls.path.read_bytes()
        cls.digest = hashlib.sha256(cls.stock).hexdigest()

    @classmethod
    def tearDownClass(cls):
        if cls.path.is_file():
            assert hashlib.sha256(cls.path.read_bytes()).hexdigest() == cls.digest

    def encoded_stock(self, encoding):
        entries = read(self.stock)
        sources = {i: e.source().decode('UTF-8').encode(encoding) for i, e in enumerate(entries)}
        return write(entries, sources)

    def test_real_stock_preserves_ids_and_unmodified_entries_in_all_encodings(self):
        for encoding in ('UTF-8', 'CP949', 'CP932'):
            with self.subTest(encoding=encoding):
                raw = self.encoded_stock(encoding)
                before = read(raw)
                after = read(builder.migrate_saves(raw, encoding))
                self.assertEqual([(e.id, e.name) for e in before], [(e.id, e.name) for e in after])
                changed = []
                for old, new in zip(before, after):
                    name = old.name.decode(encoding)
                    if name not in builder.SAVE_NAMES:
                        self.assertEqual(old.raw, new.raw)
                    else:
                        changed.append(name)
                        expected = old.source().decode(encoding).replace('\r\n', '\n')
                        for before_text, after_text in builder.RULES[name]:
                            expected = expected.replace(before_text, after_text)
                        self.assertEqual(expected.encode(encoding), new.source())
                self.assertCountEqual(changed, builder.SAVE_NAMES)

    def test_reviewed_converted_scripts_are_idempotent_and_modified_script_is_rejected(self):
        raw = builder.migrate_saves(self.stock, 'UTF-8')
        entries = read(raw)
        second = read(builder.migrate_saves(raw, 'UTF-8'))
        self.assertEqual([e.raw for e in entries], [e.raw for e in second])
        for index, entry in enumerate(entries):
            if entry.name.decode('UTF-8') in builder.SAVE_NAMES:
                edited = write(entries, {index: entry.source() + b'\n# user change\n'})
                with self.assertRaises(builder.BuildError):
                    builder.migrate_saves(edited, 'UTF-8')

    def test_custom_file_access_is_rejected(self):
        entries = read(self.stock)
        code = b'File.write("Save1.rxdata", "unsafe")\n'
        compressed = zlib.compress(code)
        raw_entry = b'[' + integer(3) + b'i' + integer(1337) + string(b'custom') + string(compressed)
        entries.insert(-1, Entry(1337, b'custom', compressed, raw_entry))
        raw = write(entries, {})
        with self.assertRaises(builder.BuildError):
            builder.migrate_saves(raw, 'UTF-8')

    def test_safe_custom_script_name_and_source_stay_byte_identical(self):
        for encoding, label in (('CP949', '추가'), ('CP932', '追加')):
            with self.subTest(encoding=encoding):
                entries = read(self.encoded_stock(encoding))
                name, source = label.encode(encoding), ('# ' + label + '\n').encode(encoding)
                compressed = zlib.compress(source)
                raw_entry = b'[' + integer(3) + b'i' + integer(1337) + string(name) + string(compressed)
                entries.insert(-1, Entry(1337, name, compressed, raw_entry))
                before = read(write(entries, {}))
                after = read(builder.migrate_saves(write(entries, {}), encoding))
                self.assertEqual([(e.id, e.name) for e in before], [(e.id, e.name) for e in after])
                self.assertEqual(next(e.raw for e in after if e.id == 1337), raw_entry)


if __name__ == '__main__':
    unittest.main()
