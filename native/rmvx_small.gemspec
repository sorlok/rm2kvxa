Gem::Specification.new do |s|
  s.name    = "rmvx_small"
  s.version = "0.0.1"
  s.summary = "Stripped-down RMVX classes."
  s.author  = "Seth N. Hetu"

  s.files = Dir.glob("ext/**/*.{cpp,rb}")

  s.extensions << "ext/rmvx_small/extconf.rb"

  s.add_development_dependency "rake-compiler"
end
