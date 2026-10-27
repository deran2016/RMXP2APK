module RGSSXP
  def self.run_scripts(path)
    bytes = read_file(path)
    raise IOError, 'Scripts archive exceeds 128 MiB' if bytes.bytesize > 128 * 1024 * 1024
    entries = Marshal.load(bytes)
    raise TypeError, 'Scripts must be an Array' unless entries.is_a?(Array)
    raise ArgumentError, 'Too many script entries' if entries.size > 10_000
    total = 0
    prepared = entries.each_with_index.map do |entry, index|
      unless entry.is_a?(Array) && entry.size == 3 && entry[0].is_a?(Integer) && entry[1].is_a?(String) && entry[2].is_a?(String)
        raise TypeError, "Invalid script entry #{index}"
      end
      begin
        name = entry[1].dup.force_encoding(SOURCE_ENCODING).encode(Encoding::UTF_8)
        source = inflate(entry[2]).force_encoding(SOURCE_ENCODING).encode(Encoding::UTF_8)
        raise EncodingError, 'Invalid text' unless name.valid_encoding? && source.valid_encoding?
      rescue EncodingError => error
        raise EncodingError, "Script #{index}: #{error.message}"
      end
      # The original bytes remain in $RGSS_SCRIPTS; eval receives UTF-8 source.
      source.sub!(/\A(\#![^\n]*\n)?([ \t]*\#[^\n]*?(?:coding|encoding)[ \t]*[:=][ \t]*)([-\w.]+)/i) { "#{$1}#{$2}UTF-8" }
      total += source.bytesize
      raise IOError, 'Inflated scripts exceed 128 MiB' if total > 128 * 1024 * 1024
      [name, source]
    end
    $RGSS_SCRIPTS = entries
    prepared.each_with_index { |(name, source), index| eval(source, TOPLEVEL_BINDING, "#{index}:#{name}", 1) }
    nil
  end
end
