module RGSSXP::Saves
  MAGIC = "RGSSXP\0SAVE\0".b + [1].pack('V')
  HIGH_MAGIC = "RGSSXP\0SAVE\0".b + [2].pack('V')
  LIMIT = 64 * 1024 * 1024
  raise ArgumentError, 'App files directory must be absolute and exist' unless
    RGSSXP::APP_FILES_DIR.start_with?('/') && File.directory?(RGSSXP::APP_FILES_DIR)
  ROOT = File.join(File.expand_path(RGSSXP::APP_FILES_DIR), 'Saves', 'RGSSXP').freeze

  class ReadStream < StringIO
    attr_reader :timing_rate
    def initialize(bytes, modified)
      super(bytes, 'rb')
      @modified = modified
      @timing_rate = 40
    end
    def mtime
      raise IOError, 'closed stream' if closed?
      @modified.dup
    end
  end

  def self.slot(index)
    raise ArgumentError, 'Save slot must be 0..3' unless index.is_a?(Integer) && index.between?(0, 3)
    "Save#{index + 1}.rxdata"
  end

  def self.filename(index, mode = :read)
    raise ArgumentError, 'Invalid save access mode' unless [:read, :write].include?(mode)
    File.join(ROOT, slot(index))
  end

  def self.exist?(index)
    File.file?(filename(index))
  end

  def self.validate_path(path)
    expanded = File.expand_path(path)
    raise ArgumentError, 'Save path outside app slots' unless (0..3).any? { |index| expanded == filename(index) }
    expanded
  end

  def self.open_read(path)
    path = validate_path(path)
    raise IOError, 'Save exceeds 64 MiB' if File.size(path) > LIMIT
    bytes = File.binread(path)
    raise IOError, '120Hz Lunetile saves are not supported by the Android data core' if bytes.start_with?(HIGH_MAGIC)
    raise IOError, 'Unsupported RGSSXP save header or version' unless bytes.start_with?(MAGIC)
    stream = ReadStream.new(bytes, File.mtime(path))
    stream.pos = MAGIC.bytesize
    stream
  end

  def self.valid_save!(bytes)
    raise IOError, 'Save exceeds 64 MiB' if bytes.bytesize > LIMIT
    raise IOError, 'Unsupported RGSSXP save header or version' unless bytes.start_with?(MAGIC)
    stream = StringIO.new(bytes, 'rb')
    stream.pos = MAGIC.bytesize
    count = 0
    until stream.eof?
      Marshal.load(stream)
      count += 1
    end
    raise ArgumentError, 'Empty save payload' if count.zero?
    true
  ensure
    stream&.close
  end

  def self.replace(path, bytes)
    temporary = path + ".tmp-#{Process.pid}-#{Process.clock_gettime(Process::CLOCK_MONOTONIC, :nanosecond)}"
    created = false
    begin
      File.open(temporary, 'wbx') do |file|
        created = true
        file.write(bytes)
        file.flush
        file.fsync
      end
      File.rename(temporary, path)
    ensure
      File.delete(temporary) if created && File.file?(temporary)
    end
  end

  def self.write(path)
    path = validate_path(path)
    stream = StringIO.new(''.b)
    begin
      stream.write(MAGIC)
      yield stream
      bytes = stream.string
      valid_save!(bytes)
      parent = File.dirname(ROOT)
      Dir.mkdir(parent) unless Dir.exist?(parent)
      Dir.mkdir(ROOT) unless Dir.exist?(ROOT)
      versions = [path, *1.upto(3).map { |n| path + ".bak#{n}" }].filter_map do |candidate|
        next unless File.file?(candidate)
        raise IOError, 'Save exceeds 64 MiB' if File.size(candidate) > LIMIT
        old = File.binread(candidate)
        begin
          valid_save!(old)
          old
        rescue ArgumentError, TypeError, EOFError, IOError, NameError, EncodingError
          nil
        end
      end.first(3)
      versions.each_with_index.to_a.reverse_each { |old, i| replace(path + ".bak#{i + 1}", old) }
      replace(path, bytes)
    ensure
      stream.close
    end
    nil
  end
end
