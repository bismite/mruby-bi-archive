
class Bi::Archive
  attr_accessor :path, :secret
  attr_accessor :callback,:on_progress

  #
  def load(&callback)
    if self.respond_to? :_download
      self._download callback
    else
      _open if File.exists?(self.path)
      @available = true
      callback.call(self) if callback
    end
    return nil
  end

  def available?
    @available
  end

  #
  def filenames
    self.table.keys
  end

  #
  def table
    unless @table
      @table = {}
      if @_table
        @_table.each{|f| @table[f.first] = f[1,2] }
      end
    end
    @table
  end

  #
  def read(name)
    if self.table.include?(name)
      start,length = table[name]
      self._read_decrypt start,length
    elsif @fallback & File.file?(name)
      File.open(name).read
    end
  end

  # load texture image
  def texture(name,antialias=false)
    if self.table.include?(name)
      start,length = self.table[name]
      self._texture_decrypt start,length,antialias
    elsif @fallback & File.file?(name)
      Bi::Texture.new name, antialias
    end
  end

  # load music
  def music(name)
    if self.table.include?(name)
      Bi::Music.new self.read(name)
    elsif @fallback & File.file?(name)
      Bi::Music.new File.open(name).read()
    end
  end

  # load sound
  def sound(name)
    if self.table.include?(name)
      Bi::Sound.new self.read(name)
    elsif @fallback & File.file?(name)
      Bi::Sound.new File.open(name).read
    end
  end

end
