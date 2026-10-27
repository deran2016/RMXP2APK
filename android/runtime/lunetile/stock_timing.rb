# Installed above Main only after the complete stock source collection is verified.
module RGSSXP::Timing
  SAVE_FIELDS = {
    'Game_System'=>%i[timer],
    'Interpreter'=>%i[wait_count],
    'Game_Map'=>%i[fog_tone_duration fog_opacity_duration],
    'Game_Character'=>%i[anime_count stop_count jump_count jump_peak wait_count],
    'Game_Screen'=>%i[tone_duration flash_duration shake_duration weather_duration],
    'Game_Picture'=>%i[duration tone_duration]
  }.freeze unless const_defined?(:SAVE_FIELDS, false)
  def self.import_save(file)
    return if file.timing_rate == 120
    seen = {}.compare_by_identity
    visit = lambda do |object|
      return if object.nil? || seen.key?(object)
      seen[object] = true
      SAVE_FIELDS.each do |name,fields|
        next unless object.is_a?(Object.const_get(name))
        fields.each do |field|
          key = :"@#{field}"
          value = object.instance_variable_get(key)
          object.instance_variable_set(key,value * 3) if value.is_a?(Numeric)
        end
      end
      case object
      when Array then object.each { |value| visit.call(value) }
      when Hash then object.each_value { |value| visit.call(value) }
      else
        if object.class.name&.start_with?('Game_', 'Interpreter')
          object.instance_variables.each { |field| visit.call(object.instance_variable_get(field)) }
        end
      end
    end
    [$game_system,$game_screen,$game_map,$game_player].each { |root| visit.call(root) }
    Graphics.frame_count *= 3
    file.timing_rate = 120
  end
end
