Gem::Specification.new do |s|
  s.name    = "curb"
  s.authors = ["Ross Bamford", "Todd A. Fisher"]
  s.version = '0.9.6'
  s.date    = '2018-05-28'
  s.description = %q{Curb (probably CUrl-RuBy or something) provides Ruby-language bindings for the libcurl(3), a fully-featured client-side URL transfer library. cURL and libcurl live at http://curl.haxx.se/}
  s.email   = 'todd.fisher@gmail.com'
  s.extra_rdoc_files = ['LICENSE', 'README.markdown']

  s.files = ["LICENSE", "README.markdown", "Rakefile", "doc.rb", "ext/extconf.rb", "lib/curl/easy.rb", "lib/curl/multi.rb", "lib/curb.rb", "lib/curl.rb", "ext/curb_easy.c", "ext/curb.c", "ext/curb_multi.c", "ext/curb_upload.c", "ext/curb_postfield.c", "ext/curb_errors.c", "ext/curb_postfield.h", "ext/curb_errors.h", "ext/curb_easy.h", "ext/curb_macros.h", "ext/curb.h", "ext/curb_upload.h", "ext/curb_multi.h"]
  #### Load-time details
  s.require_paths = ['lib','ext']
  s.rubyforge_project = 'curb'
  s.summary = %q{Ruby libcurl bindings}

  s.extensions << 'ext/extconf.rb'

  #### Documentation and testing.
  s.homepage = 'http://curb.rubyforge.org/'
  s.rdoc_options = ['--main', 'README.markdown']
  s.platform = Gem::Platform::RUBY
  s.licenses = ['MIT']
end
