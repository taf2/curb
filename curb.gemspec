Gem::Specification.new do |s|
  s.name    = "curb"
  s.authors = ["Ross Bamford", "Todd A. Fisher"]
  s.version = '1.1.0'
  s.date    = '2025-07-24'
  s.description = %q{Curb (probably CUrl-RuBy or something) provides Ruby-language bindings for the libcurl(3), a fully-featured client-side URL transfer library. cURL and libcurl live at http://curl.haxx.se/}
  s.email   = 'todd.fisher@gmail.com'
  s.extra_rdoc_files = ['LICENSE', 'README.md']
  
  s.files = ["LICENSE", "README.md", "Rakefile", "doc.rb", "ext/extconf.rb", "lib/curb.rb", "lib/curl/easy.rb", "lib/curl/multi.rb", "lib/curl.rb", "ext/curb.c", "ext/curb_easy.c", "ext/curb_errors.c", "ext/curb_multi.c", "ext/curb_postfield.c", "ext/curb_upload.c", "ext/banned.h", "ext/curb.h", "ext/curb_easy.h", "ext/curb_errors.h", "ext/curb_macros.h", "ext/curb_multi.h", "ext/curb_postfield.h", "ext/curb_upload.h"]

  #### Load-time details
  s.require_paths = ['lib','ext']
  s.summary = %q{Ruby libcurl bindings}
  s.test_files = ["tests/alltests.rb", "tests/bug_crash_on_debug.rb", "tests/bug_crash_on_progress.rb", "tests/bug_curb_easy_blocks_ruby_threads.rb", "tests/bug_curb_easy_post_with_string_no_content_length_header.rb", "tests/bug_follow_redirect_288.rb", "tests/bug_instance_post_differs_from_class_post.rb", "tests/bug_issue102.rb", "tests/bug_issue277.rb", "tests/bug_multi_segfault.rb", "tests/bug_postfields_crash.rb", "tests/bug_postfields_crash2.rb", "tests/bug_raise_on_callback.rb", "tests/bug_require_last_or_segfault.rb", "tests/bug_resolve.rb", "tests/bugtests.rb", "tests/helper.rb", "tests/mem_check.rb", "tests/require_last_or_segfault_script.rb", "tests/signals.rb", "tests/tc_curl.rb", "tests/tc_curl_download.rb", "tests/tc_curl_easy.rb", "tests/tc_curl_easy_cookielist.rb", "tests/tc_curl_easy_resolve.rb", "tests/tc_curl_easy_setopt.rb", "tests/tc_curl_maxfilesize.rb", "tests/tc_curl_multi.rb", "tests/tc_curl_postfield.rb", "tests/tc_curl_protocols.rb", "tests/timeout.rb", "tests/timeout_server.rb", "tests/unittests.rb"]
  
  s.extensions << 'ext/extconf.rb'
  

  #### Documentation and testing.
  s.homepage = 'https://github.com/taf2/curb'
  s.rdoc_options = ['--main', 'README.md']

  s.metadata = {
    'changelog_uri' => 'https://github.com/taf2/curb/blob/master/ChangeLog.md'
  }

  
  s.platform = Gem::Platform::RUBY
  
  s.licenses = ['Ruby']
end
