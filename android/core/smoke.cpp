#include <ruby.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "rgss_data.h"

int main(int argc, char** argv) {
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    if (ruby_setup() != 0) return 1;
    int state = 0;
    char program[] = "lunetile";
    char no_gems[] = "--disable-gems";
    char expression[] = "-e";
    char empty[] = "";
    char* options[] = {program, no_gems, expression, empty, nullptr};
    void* node = ruby_options(4, options);
    if (!ruby_executable_node(node, &state) || ruby_exec_node(node) != 0) return ruby_cleanup(1);
    rb_protect(lunetile_android_init_data, rb_utf8_str_new_cstr(argc > 3 ? argv[3] : "UTF-8"), &state);
    std::string root = argc > 2 ? argv[2] : argc > 1 ? argv[1] : "";
    if (!root.empty()) {
        size_t slash = root.rfind('/');
        if (slash == std::string::npos) return ruby_cleanup(1);
        std::string relative = root.substr(slash + 1);
        root.resize(slash);
        if (!state) rb_protect(lunetile_android_init_project, rb_utf8_str_new_cstr(root.c_str()), &state);
        if (!state && argc > 2) rb_gv_set("$script_fixture_dir", rb_utf8_str_new_cstr(relative.c_str()));
        if (!state && argc > 1 && std::strcmp(argv[1], "-") != 0) {
            std::string data = argv[1];
            if (data.rfind(root + '/', 0) != 0) return ruby_cleanup(1);
            rb_gv_set("$fixture_dir", rb_utf8_str_new_cstr(data.substr(root.size() + 1).c_str()));
        }
    }
    if (!state) rb_eval_string_protect(R"RUBY(
      t = Table.new(2, 2, 2)
      t[1, 1, 1] = -123
      raise 'Table Marshal' unless Marshal.load(Marshal.dump(t))[1, 1, 1] == -123
      color = Color.new(10, 20, 30, 40)
      raise 'Color Marshal' unless Marshal.load(Marshal.dump(color)) == color
      raise 'Rect Marshal' unless Marshal.load(Marshal.dump(Rect.new(1, 2, 3, 4))) == Rect.new(1, 2, 3, 4)
      ids = RGSSXP::IntegerMap.new
      ids[9] = 'nine'; ids[1] = 'one'; ids[20] = 'twenty'
      restored = Marshal.load(Marshal.dump(ids))
      raise 'IntegerMap Marshal' unless restored.keys == ids.keys && restored[9] == 'nine'
      class Game_Battler; end
      class Game_Map; def initialize; @events = {1 => 'event'}; @common_events = {}; end; end
      $game_map = Game_Map.new
      $game_actors = $game_party = $game_troop = nil
      RGSSXP.restore_game_compatibility
      raise 'integer map restore' unless $game_map.instance_variable_get(:@events).is_a?(RGSSXP::IntegerMap)
      raise 'CP949 conversion' unless "\uAC00".encode('CP949').bytes == [0xB0, 0xA1] && [0xB0, 0xA1].pack('C*').force_encoding('CP949').encode('UTF-8') == "\uAC00"
      raise 'CP932 conversion' unless "\u3042".encode('CP932').bytes == [0x82, 0xA0] && [0x82, 0xA0].pack('C*').force_encoding('CP932').encode('UTF-8') == "\u3042"
      begin
        Table._load('bad')
        raise 'invalid Table accepted'
      rescue ArgumentError
      end
      if $fixture_dir
        actors = load_data(File.join($fixture_dir, 'Actors.rxdata'))
        map = load_data(File.join($fixture_dir, 'Map001.rxdata'))
        system = load_data(File.join($fixture_dir, 'System.rxdata'))
        raise 'Actors' unless actors.is_a?(Array) && actors[1].is_a?(RPG::Actor)
        raise 'Map' unless map.is_a?(RPG::Map) && map.data.is_a?(Table)
        raise 'System' unless system.is_a?(RPG::System)
      end
      if $script_fixture_dir
        script_path = File.join($script_fixture_dir, 'good.rxdata')
        expected_entries = Marshal.load(RGSSXP.read_file(script_path))
        RGSSXP.run_scripts(script_path)
        raise 'script order' unless $script_order == [1, 2] && $script_ids == [41, 3]
        raise 'script encoding' unless $script_text == ($expected_script_text = RGSSXP::DATA_ENCODING == 'CP932' ? "\u3042" : "\uAC00")
        raise 'original script entries changed' unless $RGSS_SCRIPTS == expected_entries
        raise 'extension lookup' unless RGSSXP.read_file(File.join($script_fixture_dir, 'probe'), ['.png']) == 'image bytes'
        ['../outside', '/system/build.prop', 'C:/test'].each do |unsafe|
          begin
            RGSSXP.read_file(unsafe)
            raise 'unsafe path accepted'
          rescue ArgumentError
          end
        end
        begin
          RGSSXP.read_file(File.join($script_fixture_dir, 'missing'))
          raise 'missing file accepted'
        rescue Errno::ENOENT
        end
        if RGSSXP::DATA_ENCODING == 'UTF-8'
          begin
            RGSSXP.read_file(File.join($script_fixture_dir, 'link'))
            raise 'external symlink accepted'
          rescue ArgumentError
          end
        end
        %w[bad_zlib trailing_zlib invalid_text bad_entry].each do |name|
          $script_preflight_marker = false
          failed = false
          begin
            RGSSXP.run_scripts(File.join($script_fixture_dir, name + '.rxdata'))
          rescue StandardError
            failed = true
          end
          raise "#{name} accepted" unless failed
          raise "#{name} executed before validation" if $script_preflight_marker
        end
      end
      true
    )RUBY", &state);
    if (state) {
        VALUE message = rb_obj_as_string(rb_errinfo());
        std::fprintf(stderr, "RGSS data smoke failed: %s\n", StringValueCStr(message));
    } else {
        std::puts(argc > 2 ? "PASS: RGSS values and script loader" : argc > 1 ? "PASS: RGSS values and XP data loaded" : "PASS: RGSS values");
    }
    return ruby_cleanup(state ? 1 : 0);
}
