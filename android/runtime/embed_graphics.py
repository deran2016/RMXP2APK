"""Embed the pinned Lunetile Ruby graphics layer for the Android runtime."""
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parent / 'lunetile'
quarters = json.loads((root / 'autotile_quarters.json').read_text(encoding='utf-8'))['quarters']
parts = [
    (root / 'timing.rb').read_text(encoding='utf-8'),
    (root / 'graphics.rb').read_text(encoding='utf-8'),
    'module RGSSXP; AUTOTILE_QUARTERS = ' + json.dumps(quarters) + '; end',
    (root / 'tilemap.rb').read_text(encoding='utf-8'),
    (root / 'rpg_graphics.rb').read_text(encoding='utf-8'),
    '''module RGSSXP
  def self.__fast_sprite_emit?; false; end
  def self.scene_commands; __legacy_scene_commands; end
end
class Tilemap
  def layers; legacy_layers; end
end
''',
]
source = '\n'.join(parts)
if ')RGSSANDROID"' in source:
    raise ValueError('Ruby source conflicts with C++ raw-string delimiter')
Path(sys.argv[1]).write_text('static const char graphics_support[] = R"RGSSANDROID(# encoding: UTF-8\n' + source + ')RGSSANDROID";\n', encoding='utf-8', newline='\n')
