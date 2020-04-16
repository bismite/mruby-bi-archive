MRuby::Gem::Specification.new('mruby-bi-archive') do |spec|
  spec.license = 'Apache License Version 2.0'
  spec.author = 'kbys <work4kbys@gmail.com>'
  spec.version = '0.2.0'
  spec.add_dependency('mruby-bi-ext')
  spec.add_dependency('mruby-simplemsgpack')
end
