import json
import unittest
from pathlib import Path

from embed_core import bundle


class CoreBundleTests(unittest.TestCase):
    def test_bundle_includes_every_schema_class_and_constructor(self):
        root = Path(__file__).resolve().parent
        values, data, text, saves, scripts, project, integer_map = bundle(root)
        schema = json.loads((root / "lunetile" / "rpg_schema.json").read_text(encoding="utf-8"))
        for name in schema:
            self.assertIn(f'RGSSXP.define_data_class("{name}",', data)
        self.assertIn("class RPG::Map", data)
        self.assertIn("class Color", values)
        self.assertIn("def self.normalize_data", text)
        self.assertIn('RGSSXP::Saves', saves)
        self.assertIn('def self.run_scripts', scripts)
        self.assertIn('def self.read_file', project)
        self.assertIn('def self.restore_integer_maps', integer_map)


if __name__ == "__main__":
    unittest.main()
