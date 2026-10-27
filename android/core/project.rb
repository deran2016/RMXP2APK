module RGSSXP
  def self.resolve_file(name, extensions = [])
    raise IOError, 'Project root is not initialized' unless const_defined?(:PROJECT_ROOT, false)
    base = File.realpath(PROJECT_ROOT)
    name = text(name).tr('\\', '/')
    raise ArgumentError, 'Invalid project path' if name.empty? || name.start_with?('/') || name.match?(/\A[A-Za-z]:/) || name.include?("\0") || name.split('/').include?('..')
    raise TypeError, 'Extensions must be an Array' unless extensions.is_a?(Array)
    suffixes = [''] + extensions.map do |extension|
      extension = text(extension)
      raise ArgumentError, 'Invalid extension' if extension.include?('/') || extension.include?('\\') || extension.include?("\0")
      extension
    end
    suffixes.each do |suffix|
      candidate = File.expand_path(name + suffix, base)
      begin
        resolved = File.realpath(candidate)
      rescue Errno::ENOENT, Errno::ENOTDIR
        next
      end
      raise ArgumentError, 'Project path escapes root' unless resolved.start_with?(base + '/')
      return resolved if File.file?(resolved)
    end
    raise Errno::ENOENT, name
  end

  def self.read_file(name, extensions = [])
    File.binread(resolve_file(name, extensions))
  end
end
