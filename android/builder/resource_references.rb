# Executed by the local Lunetile Ruby checker after the data schema is defined.
class AndroidOpaquePayload
  def initialize(bytes); @bytes = bytes; end
end
%w[Table Color Tone Rect].each do |name|
  klass = Class.new
  klass.define_singleton_method(:_load) { |bytes| AndroidOpaquePayload.new(bytes) }
  Object.const_set(name, klass)
end

def asset(group, name, location)
  return unless name.is_a?(String) && !name.empty?
  puts [group, name.encode('UTF-8').unpack1('H*'), location].join("\t")
end

def audio(group, value, location)
  asset("Audio/#{group}", value.name, location) if value.is_a?(RPG::AudioFile)
end

def resources(value, location, seen)
  return if value.nil? || value.is_a?(Numeric) || value == true || value == false || value.is_a?(String) || value.is_a?(AndroidOpaquePayload)
  return if seen[value.object_id]
  seen[value.object_id] = true
  if value.is_a?(Array)
    value.each_with_index { |v, i| resources(v, "#{location}[#{i}]", seen) }
    return
  elsif value.is_a?(Hash)
    value.each { |k, v| resources(v, "#{location}[#{k.inspect}]", seen) }
    return
  end
  graphics = case value
  when RPG::Actor then {character_name: 'Characters', battler_name: 'Battlers'}
  when RPG::Enemy then {battler_name: 'Battlers'}
  when RPG::Animation then {animation_name: 'Animations'}
  when RPG::Event::Page::Graphic then {character_name: 'Characters'}
  when RPG::Weapon, RPG::Armor, RPG::Item, RPG::Skill then {icon_name: 'Icons'}
  when RPG::Tileset then {tileset_name: 'Tilesets', panorama_name: 'Panoramas', fog_name: 'Fogs', battleback_name: 'Battlebacks'}
  when RPG::System then {windowskin_name: 'Windowskins', title_name: 'Titles', gameover_name: 'Gameovers', battleback_name: 'Battlebacks', battler_name: 'Battlers', battle_transition: 'Transitions'}
  else {}
  end
  graphics.each { |field, dir| asset("Graphics/#{dir}", value.public_send(field), "#{location}.#{field}") }
  if value.is_a?(RPG::Tileset)
    value.autotile_names.each_with_index { |name, i| asset('Graphics/Autotiles', name, "#{location}.autotile_names[#{i}]") }
  end
  value.instance_variables.each do |field|
    child = value.instance_variable_get(field)
    key = field.to_s.delete_prefix('@')
    if child.is_a?(RPG::AudioFile)
      group = if key.end_with?('bgm') then 'BGM' elsif key.end_with?('bgs') then 'BGS' elsif key.end_with?('me') then 'ME' elsif key.end_with?('se') then 'SE' end
      audio(group, child, "#{location}.#{key}") if group
    end
    resources(child, "#{location}.#{key}", seen)
  end
  if value.is_a?(RPG::EventCommand)
    p = value.parameters
    case value.code
    when 131 then asset('Graphics/Windowskins', p[0], "#{location}.parameters[0]")
    when 132, 241 then audio('BGM', p[0], "#{location}.parameters[0]")
    when 133, 249 then audio('ME', p[0], "#{location}.parameters[0]")
    when 245 then audio('BGS', p[0], "#{location}.parameters[0]")
    when 250 then audio('SE', p[0], "#{location}.parameters[0]")
    when 222 then asset('Graphics/Transitions', p[0], "#{location}.parameters[0]")
    when 231 then asset('Graphics/Pictures', p[1], "#{location}.parameters[1]")
    when 204
      group = {0 => 'Panoramas', 1 => 'Fogs', 2 => 'Battlebacks'}[p[0]]
      asset("Graphics/#{group}", p[1], "#{location}.parameters[1]") if group
    when 322
      asset('Graphics/Characters', p[1], "#{location}.parameters[1]")
      asset('Graphics/Battlers', p[3], "#{location}.parameters[3]")
    end
  elsif value.is_a?(RPG::MoveCommand)
    p = value.parameters
    asset('Graphics/Characters', p[0], "#{location}.parameters[0]") if value.code == 41
    audio('SE', p[0], "#{location}.parameters[0]") if value.code == 44
  end
end

Dir.glob(File.join(ARGV.fetch(0).tr('\\', '/'), '*.rxdata')).sort.each do |path|
  next if File.basename(path).casecmp('Scripts.rxdata').zero?
  value = Marshal.load(File.binread(path))
  RGSSXP.normalize_data(value, File.basename(path), {}, [])
  resources(value, File.basename(path), {})
end
