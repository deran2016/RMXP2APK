"""Make a separate user-input validation copy; never modify the source project."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'build' / 'RGSSXP-validation-playable-20261018'
sys.path.insert(0, str(ROOT / 'scripts'))
from rxdata import read, write


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(directory):
    return {p.relative_to(directory).as_posix(): digest(p) for p in directory.rglob('*') if p.is_file()}


def map_tool():
    schema = json.loads((ROOT / 'android/core/lunetile/rpg_schema.json').read_text(encoding='utf-8'))
    lines = ['module RPG; end']
    for name, fields in schema.items():
        lines.extend(['class ' + name + '; end', name + '.class_eval { attr_accessor ' + ','.join(':' + f for f in fields) + ' }'])
    lines.append('''
%w[Table Color Tone Rect].each do |name|
  value_class = Class.new do
    def self._load(payload); item = allocate; item.instance_variable_set(:@bytes, payload); item; end
    def _dump(_depth); @bytes; end
  end
  Object.const_set(name, value_class)
end
raw = File.binread(ARGV.fetch(0))
before = Marshal.load(raw)
map = Marshal.load(raw)
event = map.events.fetch(1)
raise 'Unexpected event fixture' unless event.name == 'RGSSXP validation story' && event.x == 4 && event.y == 4 && event.pages.size == 1 && event.pages[0].trigger == 3
raise 'Unexpected existing graphic' unless event.pages[0].graphic.character_name.empty?
event.x, event.y, event.pages[0].trigger = 9, 8, 0
event.pages[0].graphic.character_name = '001-Fighter01'
result = Marshal.dump(map)
check = Marshal.load(result)
check_event = check.events.fetch(1)
check_event.x, check_event.y, check_event.pages[0].trigger = 4, 4, 3
check_event.pages[0].graphic.character_name = ''
raise 'Unexpected map change' unless Marshal.dump(check) == Marshal.dump(before)
File.binwrite(ARGV.fetch(1), result)
''')
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--ruby', type=Path, required=True)
    args = parser.parse_args()
    build = (ROOT / 'build').resolve()
    output = args.output.resolve()
    if not output.is_relative_to(build) or output == build or output.exists():
        parser.error('Output must be a new directory inside the workspace build directory')
    if not args.ruby.is_file():
        parser.error('Lunetile Ruby executable is required')
    before = inventory(SOURCE)
    scripts = read((SOURCE / 'Data/Scripts.rxdata').read_bytes())
    matches = [i for i, entry in enumerate(scripts) if entry.id == 0 and entry.name == b'RGSSXP stock boot test']
    expected = (ROOT / 'tests/runtime_stock_scenario.rb').read_bytes()
    if len(matches) != 1 or scripts[matches[0]].source() != expected:
        parser.error('Unexpected test hook; fixture generation stopped')
    for name in ('Data', 'Graphics', 'Audio'):
        shutil.copytree(SOURCE / name, output / name)
    for name in ('Game.exe', 'Game.ini', 'RGSS104E.dll'):
        shutil.copy2(SOURCE / name, output / name)
    del scripts[matches[0]]
    (output / 'Data/Scripts.rxdata').write_bytes(write(scripts, {}))
    helper = output / 'fixture-map-helper.rb'
    helper.write_text(map_tool(), encoding='utf-8')
    try:
        result = subprocess.run([str(args.ruby), '--disable-gems', str(helper), str(SOURCE / 'Data/Map001.rxdata'), str(output / 'Data/Map001.rxdata')], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)
    finally:
        helper.unlink(missing_ok=True)
    if result.returncode:
        raise RuntimeError('Fixture map preparation failed: ' + result.stderr)
    if before != inventory(SOURCE):
        raise RuntimeError('Source project changed while the fixture was created')
    source_audio = inventory(SOURCE / 'Audio')
    if source_audio != inventory(output / 'Audio'):
        raise RuntimeError('Audio copy mismatch')
    info = {'source': str(SOURCE), 'source_scripts_sha256': before['Data/Scripts.rxdata'],
            'source_map_sha256': before['Data/Map001.rxdata'], 'removed_test_hook_sha256': hashlib.sha256(expected).hexdigest(),
            'output_scripts_sha256': digest(output / 'Data/Scripts.rxdata'), 'output_map_sha256': digest(output / 'Data/Map001.rxdata'),
            'audio_files': len(source_audio), 'audio_identical': True,
            'map_change': {'event_id': 1, 'old_position': [4, 4], 'new_position': [9, 8], 'old_trigger': 3, 'new_trigger': 0, 'character': '001-Fighter01'}}
    (output / 'fixture-manifest.json').write_text(json.dumps(info, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(info, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
