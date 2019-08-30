
class Bi::Archive
  attr_accessor :on_success,:on_progress,:on_failed

  def self.fetch(name,&callback)
    archive = Bi::Archive.new
    archive.on_success = callback
    archive.fetch name
  end

  # fetch
  def fetch(name)
    if self.respond_to? :_fetch
      self._fetch name, :_on_success, :_on_progress, :_on_failed
    else
      self._load name
      @on_success.call(self)
    end
    return nil
  end

  #
  # fetch callback
  #
  def _on_success(archive)
    puts "Bi::Archive _on_success"
    @on_success.call(archive)
  end
  def _on_progress(archive,transferred,length)
    @on_progress.call(archive,transferred,length) if @on_progress
  end
  def _on_failed(archive,status)
    @on_failed.call(archive,status) if @on_failed
  end

  #
  def filenames
    unless @filenames
      @filenames = @_table.map{|f| f.first }
    end
    @filenames
  end

  #
  def table
    unless @table
      @table = {}
      @_table.each{|f| @table[f.first] = f[1,2] }
    end
    @table
  end

  #
  def read(name,secret)
    start,length = table[name]
    self._read_decrypt(secret,start,length)
  end

  # load texture image
  def texture_image(name,antialias,secret)
    start,length = self.table[name]
    self._texture_decrypt secret, start,length, antialias
  end

  # load music
  def music(name,decrypt_secret)
    Bi::Music.new self.read(name,decrypt_secret)
  end

  # load sound
  def sound(name,decrypt_secret)
    Bi::Sound.new self.read(name,decrypt_secret)
  end

end
