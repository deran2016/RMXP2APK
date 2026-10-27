"""Exercise both save versions with the local Ruby checker when available."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUBY = os.environ.get('LUNETILE_TEST_RUBY') or shutil.which('ruby')


@unittest.skipUnless(RUBY, 'Ruby with StringIO unavailable; set LUNETILE_TEST_RUBY')
class SaveTimingTests(unittest.TestCase):
    def test_legacy_import_high_save_and_downgrade_protection(self):
        script = r'''
require 'stringio'
module RGSSXP
  APP_FILES_DIR = '/'
  module Timing
    def self.scale; $scale; end
  end
end
load ARGV[0]
RGSSXP::Saves.send(:remove_const, :ROOT)
RGSSXP::Saves.const_set(:ROOT, File.expand_path(File.join(ARGV[1], 'Saves', 'RGSSXP')))
$scale = 1
path = RGSSXP::Saves.filename(0)
RGSSXP::Saves.write(path) { |f| Marshal.dump(7, f) }
legacy = RGSSXP::Saves.open_read(path)
raise 'legacy rate' unless legacy.timing_rate == 40 && Marshal.load(legacy) == 7
legacy.close
$scale = 3
RGSSXP::Saves.write(path) { |f| Marshal.dump(8, f) }
high = RGSSXP::Saves.open_read(path)
raise 'high rate' unless high.timing_rate == 120 && Marshal.load(high) == 8
high.close
before = File.binread(path)
$scale = 1
begin
  RGSSXP::Saves.open_read(path)
  raise 'read should fail'
rescue IOError
end
begin
  RGSSXP::Saves.write(path) { |f| Marshal.dump(9, f) }
  raise 'write should fail'
rescue IOError
end
raise 'save changed' unless File.binread(path) == before
raise 'legacy backup missing' unless File.binread(path + '.bak1').start_with?(RGSSXP::Saves::MAGIC)
class Game_System; end
class Interpreter; end
class Game_Map; end
class Game_Character; end
class Game_Screen; end
class Game_Picture; end
module Graphics
  class << self; attr_accessor :frame_count; end
end
load ARGV[2]
$game_system = Game_System.new
$game_system.instance_variable_set(:@timer, 10)
$game_screen = Game_Screen.new
$game_map = Game_Map.new
$game_player = Game_Character.new
Graphics.frame_count = 40
old = RGSSXP::Saves::ReadStream.new('', Time.now)
RGSSXP::Timing.import_save(old)
raise '40Hz import' unless $game_system.instance_variable_get(:@timer) == 30 && Graphics.frame_count == 120 && old.timing_rate == 120
RGSSXP::Timing.import_save(old)
raise 'double conversion' unless $game_system.instance_variable_get(:@timer) == 30 && Graphics.frame_count == 120
'''
        (ROOT / 'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temp:
            result = subprocess.run([str(RUBY), '--disable-gems', '-e', script, str(ROOT / 'android/core/saves.rb'), temp, str(ROOT / 'android/runtime/lunetile/stock_timing.rb')], capture_output=True, text=True, encoding='utf-8', errors='replace')
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
