
class Bi::Archive
  attr_accessor :path, :secret
  attr_accessor :callback,:on_progress

  #
  def load(&callback)
    if self.respond_to? :_download
      self._download callback
    else
      _open if File.exis
      callback.call(self)
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
    return nil unless self.table.include? name
    start,length = table[name]
    self._read_decrypt start,length
  end

  # load texture image
  def texture_image(name,antialias)
    return nil unless self.table.include? name
    start,length = self.table[name]
    self._texture_decrypt start,length,antialias
  end

  # load music
  def music(name,decrypt_secret)
    return nil unless self.table.include? name
    Bi::Music.new self.read(name,decrypt_secret)
  end

  # load sound
  def sound(name,decrypt_secret)
    return nil unless self.table.include? name
    Bi::Sound.new self.read(name,decrypt_secret)
  end

end
