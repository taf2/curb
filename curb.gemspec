Gem::Specification.new do |s|
  s.name    = "curb"
  s.version = "0.2.3"
  s.authors = ["Ross Bamford", "Todd A. Fisher"]
  s.date    = "2008-11-03"
  s.description = %q{Curb (probably CUrl-RuBy or something) provides Ruby-language bindings for the libcurl(3), a fully-featured client-side URL transfer library. cURL and libcurl live at http://curl.haxx.se/}
  s.email   = "todd.fisher@gmail.com"
  s.extra_rdoc_files = ["LICENSE", "README"]
  s.files = ["LICENSE", "README", "Rakefile",
             "doc.rb", "ext/curb.h", "ext/curb_easy.h",
             "ext/curb_errors.h", "ext/curb_macros.h",
             "ext/curb_multi.h", "ext/curb_postfield.h",
             "ext/curb.c", "ext/curb_easy.c", "ext/curb_errors.c",
             "ext/curb_multi.c", "ext/curb_postfield.c",
             "ext/curb.rb", "ext/curl.rb", "ext/extconf.rb"]
  s.has_rdoc = true
  s.homepage = "http://curb.rubyforge.org/"
  s.rdoc_options = ["--main", "README"]
  s.require_paths = ["ext"]
  s.rubyforge_project = %q{curb}
  s.summary = %q{Ruby libcurl bindings}
  s.test_files = ["tests/tc_curl_easy.rb",
                  "tests/tc_curl_multi.rb",
                  "tests/tc_curl_postfield.rb",
                  "tests/unittests.rb",
                  "tests/helper.rb"
                  ]
end
