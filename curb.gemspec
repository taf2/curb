Gem::Specification.new do |s|
  s.name = %q{curb}
  s.version = "0.2.3"
 
  s.authors = ["Ross Bamford", "Todd A. Fisher"]
  s.date = %q{2008-11-03}
  s.description = %q{Curb (probably CUrl-RuBy or something) provides Ruby-language bindings for the libcurl(3), a fully-featured client-side URL transfer library. cURL and libcurl live at http://curl.haxx.se/}
  s.email = %q{todd.fisher@gmail.com}
  s.executables = []
  s.extra_rdoc_files = ["LICENSE", "README"]
  s.files = ["LICENSE", "README", "Rakefile",
             "doc.rb", "curb.h", "curb_easy.h",
             "curb_errors.h", "curb_macros.h",
             "curb_multi.h", "curb_postfield.h",
             "curb.c", "curb_easy.c", "curb_errors.c",
             "curb_multi.c", "curb_postfield.c",
             "curb.rb", "curl.rb", "extconf.rb"]
  s.has_rdoc = true
  s.homepage = %q{http://curb.rubyforge.org/}
  s.rdoc_options = ["--main", "README"]
  s.require_paths = ["ext"]
  s.rubyforge_project = %q{curb}
  s.rubygems_version = %q{0.2.3}
  s.summary = %q{Ruby libcurl bindings}
  s.test_files = ["test/tc_curl_easy.rb",
                  "test/tc_curl_multi.rb",
                  "test/tc_curl_postfield.rb",
                  "test/unittests.rb",
                  "test/helper.rb",
                  "test/bug_curb_easy_blocks_ruby_threads.rb",
                  "test/bug_instance_post_differs_from_class_post.rb",
                  "test/bug_require_last_or_segfault.rb",
                  "test/alltests.rb"]
end
