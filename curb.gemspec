Gem::Specification.new do |s|
  s.name    = "curb"
  s.authors = ["Ross Bamford", "Todd A. Fisher"]
  s.version = '0.8.6'
  s.date    = '2014-07-22'
  s.description = %q{Curb (probably CUrl-RuBy or something) provides Ruby-language bindings for the libcurl(3), a fully-featured client-side URL transfer library. cURL and libcurl live at http://curl.haxx.se/}
  s.email   = 'todd.fisher@gmail.com'
  s.license = 'Ruby'
  s.extra_rdoc_files = ['LICENSE', 'README.markdown']
  s.add_development_dependency "test-unit"

  s.files = `git ls-files -z`.split("\x0")
  #### Load-time details
  s.require_paths = ['lib','ext']
  s.summary = %q{Ruby libcurl bindings}
  s.test_files = s.files.grep(%r{^(test)/})
  s.extensions << 'ext/extconf.rb'

  #### Documentation and testing.
  s.has_rdoc = true
  s.homepage = 'https://github.com/taf2/curb'
  s.rdoc_options = ['--main', 'README.markdown']

  s.platform = Gem::Platform::RUBY
end
