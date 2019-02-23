require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))
class FooNoToS 
  undef to_s
end

class TestCurbCurlEasy < Test::Unit::TestCase
  def test_global_reset
    Curl.get($TEST_URL)
    # in a Timeout block you should reset the thread current handle 
    Curl.reset
  end

  def test_curlopt_stderr_with_file
    # does not work with Tempfile directly
    path = Tempfile.new('curb_test_curlopt_stderr').path
    File.open(path, 'w') do |file|
      easy = Curl::Easy.new(TestServlet.url)
      easy.verbose = true
      easy.setopt(Curl::CURLOPT_STDERR, file)
      easy.perform
    end
    output = File.read(path)

    assert_match('HTTP/1.1 200 OK ', output)
    assert_match('Host: 127.0.0.1:9129', output)
  end

  def test_curlopt_stderr_with_io
    path = Tempfile.new('curb_test_curlopt_stderr').path
    fd = IO.sysopen(path, 'w')
    io = IO.for_fd(fd)

    easy = Curl::Easy.new(TestServlet.url)
    easy.verbose = true
    easy.setopt(Curl::CURLOPT_STDERR, io)
    easy.perform


    output = File.read(path)

    assert_match(output, 'HTTP/1.1 200 OK')
    assert_match(output, 'Host: 127.0.0.1:9129')
  end

  def test_curlopt_stderr_fails_with_tempdir
    Tempfile.open('curb_test_curlopt_stderr') do |tempfile|
      easy = Curl::Easy.new(TestServlet.url)

      assert_raise(TypeError) do
        easy.setopt(Curl::CURLOPT_STDERR, tempfile)
      end
    end
  end

  def test_curlopt_stderr_fails_with_stringio
    stringio = StringIO.new
    easy = Curl::Easy.new(TestServlet.url)

    assert_raise(TypeError) do
      easy.setopt(Curl::CURLOPT_STDERR, stringio)
    end
  end

  def test_curlopt_stderr_fails_with_string
    string = String.new
    easy = Curl::Easy.new(TestServlet.url)

    assert_raise(TypeError) do
      easy.setopt(Curl::CURLOPT_STDERR, string)
    end
  end

  def test_exception
    begin
      Curl.get('NOT_FOUND_URL')
    rescue
      assert true
    rescue Exception
      assert false, "We should raise StandardError"
    end
  end

  def test_threads
    t = []
    5.times do
      t << Thread.new do
        5.times do
          c = Curl.get($TEST_URL)
          assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
          assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body)
        end
      end
    end

    t.each {|x| x.join }
  end

  def test_class_perform_01   
    assert_instance_of Curl::Easy, c = Curl::Easy.perform($TEST_URL)
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body)
  end    

  def test_class_perform_02
    data = ""
    assert_instance_of Curl::Easy, c = Curl::Easy.perform($TEST_URL) { |curl| curl.on_body { |d| data << d; d.length } }    

    assert_nil c.body_str
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, data)
  end    

  def test_class_perform_03
    assert_raise(Curl::Err::CouldntReadError) { Curl::Easy.perform($TEST_URL + "nonexistent") }
  end    
  
  def test_new_01
    c = Curl::Easy.new
    assert_equal Curl::Easy, c.class
    assert_nil c.url
    assert_nil c.body_str
    assert_nil c.header_str
  end

  def test_new_02
    c = Curl::Easy.new($TEST_URL)
    assert_equal $TEST_URL, c.url
  end
  
  def test_new_03
    blk = lambda { |i| i.length }
    
    c = Curl::Easy.new do |curl|
      curl.on_body(&blk)
    end
    
    assert_nil c.url    
    assert_equal blk, c.on_body   # sets handler nil, returns old handler
    assert_equal nil, c.on_body
  end
    
  def test_new_04
    blk = lambda { |i| i.length }
    
    c = Curl::Easy.new($TEST_URL) do |curl|
      curl.on_body(&blk)
    end
    
    assert_equal $TEST_URL, c.url
    assert_equal blk, c.on_body   # sets handler nil, returns old handler
    assert_equal nil, c.on_body
  end

  class Foo < Curl::Easy ; end
  def test_new_05
    # can use Curl::Easy as a base class
    c = Foo.new
    assert_equal Foo, c.class
    c.url = $TEST_URL
    c.perform
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
  end

  # test invalid use of new
  def test_new_06
    Curl::Easy.new(TestServlet.url) do|curl|
      curl.http_post
      assert_equal "POST\n", curl.body_str
    end
  end

  def test_escape
    c = Curl::Easy.new
    
    assert_equal "one%20two", c.escape('one two')
    assert_equal "one%00two%20three", c.escape("one\000two three")    
  end
  
  def test_unescape
    c = Curl::Easy.new
    
    assert_equal "one two", c.unescape('one%20two')
    
    # prior to 7.15.4 embedded nulls cannot be unescaped
    if Curl::VERNUM >= 0x070f04
      assert_equal "one\000two three", c.unescape("one%00two%20three")
    end
  end
  
  def test_headers
    c = Curl::Easy.new($TEST_URL)
    
    assert_equal({}, c.headers)
    c.headers = "Expect:"
    assert_equal "Expect:", c.headers
    c.headers = ["Expect:", "User-Agent: myapp-0.0.0"]
    assert_equal ["Expect:", "User-Agent: myapp-0.0.0"], c.headers
  end    

  def test_get_01   
    c = Curl::Easy.new($TEST_URL)    
    assert_equal true, c.http_get
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
  end    

  def test_get_02
    data = ""
    c = Curl::Easy.new($TEST_URL) do |curl|
      curl.on_body { |d| data << d; d.length }
    end
    
    assert_equal true, c.http_get    
    
    assert_nil c.body_str
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, data)
  end    

  def test_get_03
    c = Curl::Easy.new($TEST_URL + "nonexistent")        
    assert_raise(Curl::Err::CouldntReadError) { c.http_get }
    assert_equal "", c.body_str
    assert_equal "", c.header_str
  end    


  def test_last_effective_url_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_equal $TEST_URL, c.url
    assert_nil c.last_effective_url
    
    assert c.http_get
    
    assert_equal c.url, c.last_effective_url
    c.url = "file://some/new.url"
    
    assert_not_equal c.last_effective_url, c.url
  end

  def test_http_get_block
    curl = Curl::Easy.http_get(TestServlet.url) do|c|
      c.follow_location = true
      c.max_redirects = 3
    end
    assert_equal curl.url, curl.last_effective_url
    assert_equal 'GET', curl.body_str
  end
  
  def test_local_port_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.local_port    
    assert_nil c.local_port_range
    assert_nil c.proxy_port
    
    c.local_port = 88

    assert_equal 88, c.local_port  
    assert_nil c.local_port_range
    assert_nil c.proxy_port
    
    c.local_port = nil

    assert_nil c.local_port  
    assert_nil c.local_port_range
    assert_nil c.proxy_port
  end
  
  def test_local_port_02
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.local_port    
    assert_raise(ArgumentError) { c.local_port = 0 }
    assert_raise(ArgumentError) { c.local_port = 65536 }
    assert_raise(ArgumentError) { c.local_port = -1 }
  end
  
  def test_local_port_range_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.local_port_range
    assert_nil c.local_port
    assert_nil c.proxy_port

    c.local_port_range = 88
    assert_equal 88, c.local_port_range
    assert_nil c.local_port
    assert_nil c.proxy_port
    
    c.local_port_range = nil
    
    assert_nil c.local_port_range
    assert_nil c.local_port
    assert_nil c.proxy_port
  end
  
  def test_local_port_range_02
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.local_port_range    
    assert_raise(ArgumentError) { c.local_port_range = 0 }
    assert_raise(ArgumentError) { c.local_port_range = 65536 }
    assert_raise(ArgumentError) { c.local_port_range = -1 }
  end
  
  def test_proxy_url_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_equal $TEST_URL, c.url
    assert_nil c.proxy_url
    
    c.proxy_url = "http://some.proxy"    

    assert_equal $TEST_URL, c.url
    assert_equal "http://some.proxy", c.proxy_url
 
    c.proxy_url = nil
    assert_equal $TEST_URL, c.url
    assert_nil c.proxy_url
  end
  
  def test_proxy_port_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.local_port
    assert_nil c.local_port_range    
    assert_nil c.proxy_port    
    
    c.proxy_port = 88

    assert_equal 88, c.proxy_port  
    assert_nil c.local_port
    assert_nil c.local_port_range
    
    c.proxy_port = nil
    assert_nil c.proxy_port  
    assert_nil c.local_port
    assert_nil c.local_port_range
  end
  
  def test_proxy_port_02
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.proxy_port    
    assert_raise(ArgumentError) { c.proxy_port = 0 }
    assert_raise(ArgumentError) { c.proxy_port = 65536 }
    assert_raise(ArgumentError) { c.proxy_port = -1 }
  end
  
  def test_proxy_type_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.proxy_type
    
    c.proxy_type = 3
    assert_equal 3, c.proxy_type
    
    c.proxy_type = nil
    assert_nil c.proxy_type
  end
  
  def test_http_auth_types_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.http_auth_types
    
    c.http_auth_types = 3
    assert_equal 3, c.http_auth_types
    
    c.http_auth_types = nil
    assert_nil c.http_auth_types
  end
  
  def test_proxy_auth_types_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.proxy_auth_types
    
    c.proxy_auth_types = 3
    assert_equal 3, c.proxy_auth_types
    
    c.proxy_auth_types = nil
    assert_nil c.proxy_auth_types
  end
  
  def test_max_redirects_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.max_redirects
    
    c.max_redirects = 3
    assert_equal 3, c.max_redirects
    
    c.max_redirects = nil
    assert_nil c.max_redirects
  end

  def test_timeout_with_floats
    c = Curl::Easy.new($TEST_URL)

    c.timeout = 1.5
    assert_equal 1500, c.timeout_ms
    assert_equal 1.5, c.timeout
  end

  def test_timeout_with_negative
    c = Curl::Easy.new($TEST_URL)

    c.timeout = -1.5
    assert_equal 0, c.timeout
    assert_equal 0, c.timeout_ms

    c.timeout = -4.8
    assert_equal 0, c.timeout
    assert_equal 0, c.timeout_ms
  end

  def test_timeout_01
    c = Curl::Easy.new($TEST_URL)

    assert_equal 0, c.timeout

    c.timeout = 3
    assert_equal 3, c.timeout

    c.timeout = 0
    assert_equal 0, c.timeout
  end

  def test_timeout_ms_01
    c = Curl::Easy.new($TEST_URL)

    assert_equal 0, c.timeout_ms

    c.timeout_ms = 100
    assert_equal 100, c.timeout_ms

    c.timeout_ms = nil
    assert_equal 0, c.timeout_ms
  end

  def test_timeout_ms_with_floats
    c = Curl::Easy.new($TEST_URL)

    c.timeout_ms = 55.5
    assert_equal 55, c.timeout_ms
    assert_equal 0.055, c.timeout
  end

  def test_timeout_ms_with_negative
    c = Curl::Easy.new($TEST_URL)

    c.timeout_ms = -1.5
    assert_equal 0, c.timeout
    assert_equal 0, c.timeout_ms

    c.timeout_ms = -4.8
    assert_equal 0, c.timeout
    assert_equal 0, c.timeout_ms
  end

  def test_connect_timeout_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.connect_timeout
    
    c.connect_timeout = 3
    assert_equal 3, c.connect_timeout
    
    c.connect_timeout = nil
    assert_nil c.connect_timeout
  end

  def test_connect_timeout_ms_01
    c = Curl::Easy.new($TEST_URL)

    assert_nil c.connect_timeout_ms

    c.connect_timeout_ms = 100
    assert_equal 100, c.connect_timeout_ms

    c.connect_timeout_ms = nil
    assert_nil c.connect_timeout_ms
  end

  def test_ftp_response_timeout_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.ftp_response_timeout
    
    c.ftp_response_timeout = 3
    assert_equal 3, c.ftp_response_timeout
    
    c.ftp_response_timeout = nil
    assert_nil c.ftp_response_timeout
  end
  
  def test_dns_cache_timeout_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_equal 60, c.dns_cache_timeout
    
    c.dns_cache_timeout = nil
    assert_nil c.dns_cache_timeout
    
    c.dns_cache_timeout = 30
    assert_equal 30, c.dns_cache_timeout
  end
  
  def test_low_speed_limit_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.low_speed_limit
    
    c.low_speed_limit = 3
    assert_equal 3, c.low_speed_limit
    
    c.low_speed_limit = nil
    assert_nil c.low_speed_limit
  end
  
  def test_low_speed_time_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.low_speed_time
    
    c.low_speed_time = 3
    assert_equal 3, c.low_speed_time
    
    c.low_speed_time = nil
    assert_nil c.low_speed_time
  end
  
  def test_on_body
    blk = lambda { |i| i.length }
    
    c = Curl::Easy.new    
    c.on_body(&blk)
    
    assert_equal blk, c.on_body   # sets handler nil, returns old handler
    assert_equal nil, c.on_body
  end

  def test_inspect_with_no_url
    c = Curl::Easy.new
    assert_equal '#<Curl::Easy>', c.inspect
  end
  
  def test_inspect_with_short_url
    c = Curl::Easy.new('http://www.google.com/')
    assert_equal "#<Curl::Easy http://www.google.com/>", c.inspect
  end
  
  def test_inspect_truncates_to_64_chars
    base_url      = 'http://www.google.com/'
    truncated_url = base_url + 'x' * (64 - '#<Curl::Easy >'.size - base_url.size)
    long_url      = truncated_url + 'yyyy'
    c = Curl::Easy.new(long_url)
    assert_equal 64, c.inspect.size
    assert_equal "#<Curl::Easy #{truncated_url}>", c.inspect
  end
  
  def test_on_header
    blk = lambda { |i| i.length }
    
    c = Curl::Easy.new    
    c.on_header(&blk)
    
    assert_equal blk, c.on_header   # sets handler nil, returns old handler
    assert_equal nil, c.on_header
  end
  
  def test_on_progress
    blk = lambda { |*args| true }
    
    c = Curl::Easy.new    
    c.on_progress(&blk)
    
    assert_equal blk, c.on_progress   # sets handler nil, returns old handler
    assert_equal nil, c.on_progress
  end
  
  def test_on_debug
    blk = lambda { |*args| true }
    
    c = Curl::Easy.new    
    c.on_debug(&blk)
    
    assert_equal blk, c.on_debug   # sets handler nil, returns old handler
    assert_equal nil, c.on_debug
  end
  
  def test_proxy_tunnel
    c = Curl::Easy.new    
    assert !c.proxy_tunnel?
    assert c.proxy_tunnel = true
    assert c.proxy_tunnel?
  end
  
  def test_fetch_file_time
    c = Curl::Easy.new    
    assert !c.fetch_file_time?
    assert c.fetch_file_time = true
    assert c.fetch_file_time?
  end
  
  def test_ssl_verify_peer
    c = Curl::Easy.new    
    assert c.ssl_verify_peer?
    assert !c.ssl_verify_peer = false
    assert !c.ssl_verify_peer?
  end
  
  def test_ssl_verify_host
    c = Curl::Easy.new    
    assert c.ssl_verify_host?
    c.ssl_verify_host = 0
    c.ssl_verify_host = false
    assert !c.ssl_verify_host?
  end
  
  def test_header_in_body
    c = Curl::Easy.new    
    assert !c.header_in_body?
    assert c.header_in_body = true
    assert c.header_in_body?
  end
  
  def test_use_netrc
    c = Curl::Easy.new    
    assert !c.use_netrc?
    assert c.use_netrc = true
    assert c.use_netrc?
  end
  
  def test_follow_location
    c = Curl::Easy.new    
    assert !c.follow_location?
    assert c.follow_location = true
    assert c.follow_location?
  end
  
  def test_unrestricted_auth
    c = Curl::Easy.new    
    assert !c.unrestricted_auth?
    assert c.unrestricted_auth = true
    assert c.unrestricted_auth?
  end  
  
  def test_multipart_form_post
    c = Curl::Easy.new
    assert !c.multipart_form_post?
    assert c.multipart_form_post = true
    assert c.multipart_form_post?
  end

  def test_ignore_content_length
    c = Curl::Easy.new
    assert !c.ignore_content_length?
    assert c.ignore_content_length = true
    assert c.ignore_content_length?
  end

  def test_resolve_mode
    c = Curl::Easy.new
    assert_equal :auto, c.resolve_mode
    c.resolve_mode = :ipv4
    assert_equal :ipv4, c.resolve_mode 
    c.resolve_mode = :ipv6
    assert_equal :ipv6, c.resolve_mode 

    assert_raises(ArgumentError) { c.resolve_mode = :bad }
  end

  def test_enable_cookies
    c = Curl::Easy.new
    assert !c.enable_cookies?
    assert c.enable_cookies = true
    assert c.enable_cookies?
  end

  def test_cookies_option
    c = Curl::Easy.new
    assert_nil c.cookies
    assert_equal "name1=content1; name2=content2;", c.cookies = "name1=content1; name2=content2;"
    assert_equal "name1=content1; name2=content2;", c.cookies
  end

  def test_cookiefile
    c = Curl::Easy.new
    assert_nil c.cookiefile
    assert_equal "some.file", c.cookiefile = "some.file"
    assert_equal "some.file", c.cookiefile       
  end

  def test_cookiejar
    c = Curl::Easy.new
    assert_nil c.cookiejar
    assert_equal "some.file", c.cookiejar = "some.file"
    assert_equal "some.file", c.cookiejar        
  end

  def test_cookielist
    c = Curl::Easy.new TestServlet.url
    c.enable_cookies = true
    c.post_body = URI.encode_www_form('c' => 'somename=somevalue')
    assert_nil c.cookielist
    c.perform
    assert_match(/somevalue/, c.cookielist.join(''))
  end

  def test_on_success
    curl = Curl::Easy.new($TEST_URL)    
    on_success_called = false
    curl.on_success {|c|
      on_success_called = true
      assert_not_nil c.body_str
      assert_equal "", c.header_str
      assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
    }
    curl.perform
    assert on_success_called, "Success handler not called" 
  end

  def test_on_success_with_on_failure
    curl = Curl::Easy.new(TestServlet.url + '/error')
    on_failure_called = false
    curl.on_success {|c| } # make sure we get the failure call even though this handler is defined
    curl.on_failure {|c,code| on_failure_called = true }
    curl.perform
    assert_equal 500, curl.response_code
    assert on_failure_called, "Failure handler not called" 
  end

  def test_on_success_with_on_missing
    curl = Curl::Easy.new(TestServlet.url + '/not_here')
    on_missing_called = false
    curl.on_success {|c| } # make sure we get the missing call even though this handler is defined
    curl.on_missing {|c,code| on_missing_called = true }
    curl.perform
    assert_equal 404, curl.response_code
    assert on_missing_called, "Missing handler not called" 
  end

  def test_on_success_with_on_redirect
    curl = Curl::Easy.new(TestServlet.url + '/redirect')
    on_redirect_called = false
    curl.on_success {|c| } # make sure we get the redirect call even though this handler is defined
    curl.on_redirect {|c,code| on_redirect_called = true }
    curl.perform
    assert_equal 302, curl.response_code
    assert on_redirect_called, "Redirect handler not called" 
  end
  
  def test_get_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_get
    assert_equal 'GET', curl.body_str
  end
  
  def test_post_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_post([Curl::PostField.content('document_id', 5)])
    assert_equal "POST\ndocument_id=5", curl.unescape(curl.body_str)
  end

  def test_post_remote_is_easy_handle
    # see: http://pastie.org/560852 and
    # http://groups.google.com/group/curb---ruby-libcurl-bindings/browse_thread/thread/216bb2d9b037f347?hl=en
    [:post, :get, :head, :delete].each do |method|
      retries = 0
      begin
        count = 0
        Curl::Easy.send("http_#{method}", TestServlet.url) do|c|
          count += 1
          assert_equal Curl::Easy, c.class
        end
        assert_equal 1, count, "For request method: #{method.to_s.upcase}"
      rescue Curl::Err::HostResolutionError => e # travis-ci.org fails to resolve... try again?
        retries+=1
        retry if retries < 3
        raise e
      end
    end
  end

  # see: https://github.com/rvanlieshout/curb/commit/8bcdefddc0162484681ebd1a92d52a642666a445
  def test_post_multipart_array_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.multipart_form_post = true
    fields = [
      Curl::PostField.file('foo', File.expand_path(File.join(File.dirname(__FILE__),'..','README.markdown'))),
      Curl::PostField.file('bar', File.expand_path(File.join(File.dirname(__FILE__),'..','README.markdown')))
    ]
    curl.http_post(fields)
    assert_match(/HTTP POST file upload/, curl.body_str)
    assert_match(/Content-Disposition: form-data/, curl.body_str)
  end
  
  def test_post_with_body_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.post_body = 'foo=bar&encoded%20string=val'
    
    curl.perform
    
    assert_equal "POST\nfoo=bar&encoded%20string=val", curl.body_str
    assert_equal 'foo=bar&encoded%20string=val', curl.post_body
  end
  
  def test_form_post_body_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_post('foo=bar', 'encoded%20string=val')
    
    assert_equal "POST\nfoo=bar&encoded%20string=val", curl.body_str
    assert_equal 'foo=bar&encoded%20string=val', curl.post_body
  end

  def test_post_multipart_file_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.multipart_form_post = true
    pf = Curl::PostField.file('readme', File.expand_path(File.join(File.dirname(__FILE__),'..','README.markdown')))
    curl.http_post(pf)
    assert_match(/HTTP POST file upload/, curl.body_str)
    assert_match(/Content-Disposition: form-data/, curl.body_str)
  end

  def test_delete_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_delete
    assert_equal 'DELETE', curl.body_str
  end

  def test_arbitrary_http_verb
    curl = Curl::Easy.new(TestServlet.url)
    curl.http('PURGE')
    assert_equal 'PURGE', curl.body_str
  end

  def test_head_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_head

    redirect = curl.header_str.match(/Location: (.*)/)

    assert_equal '', curl.body_str
    assert_match('/nonexistent', redirect[1])
  end

  def test_head_accessor
    curl = Curl::Easy.new(TestServlet.url)
    curl.head = true
    curl.perform

    redirect = curl.header_str.match(/Location: (.*)/)

    assert_equal '', curl.body_str
    assert_match('/nonexistent', redirect[1])
    curl.head = false
    curl.perform
    assert_equal 'GET', curl.body_str
  end

  def test_put_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.headers['Content-Type'] = 'application/json'
    assert curl.http_put("message")
    assert_match(/^PUT/, curl.body_str)
    assert_match(/message$/, curl.body_str)
    assert_match(/message$/, curl.body)
    assert_match(/application\/json/, curl.header_str)
    assert_match(/application\/json/, curl.head)
  end 
  
  def test_put_data
    curl = Curl::Easy.new(TestServlet.url)
    curl.put_data = 'message'
    
    curl.perform
    
    assert_match(/^PUT/, curl.body_str)
    assert_match(/message$/, curl.body_str)
  end

  # https://github.com/taf2/curb/issues/101
  def test_put_data_null_bytes
    curl = Curl::Easy.new(TestServlet.url)
    curl.put_data = "a\0b"
 
    curl.perform
    
    assert_match(/^PUT/, curl.body_str)
    assert_match("a\0b", curl.body_str)
  end

  def test_put_nil_data_no_crash
    curl = Curl::Easy.new(TestServlet.url)
    curl.put_data = nil
    
    curl.perform
  end

  def test_put_remote_file
    curl = Curl::Easy.new(TestServlet.url)
    File.open(__FILE__,'rb') do|f|
      assert curl.http_put(f)
    end
    assert_equal "PUT\n#{File.read(__FILE__)}", curl.body_str.tr("\r", '')
  end
  
  def test_put_class_method
    count = 0
    curl = Curl::Easy.http_put(TestServlet.url,File.open(__FILE__,'rb')) do|c|
      count += 1
      assert_equal Curl::Easy, c.class
    end
    assert_equal 1, count
    assert_equal "PUT\n#{File.read(__FILE__)}", curl.body_str.tr("\r", '')
  end

  # Generate a self-signed cert with
  # openssl req -new -newkey rsa:1024 -days 365 -nodes -x509 \
  #   -keyout tests/cert.pem  -out tests/cert.pem
  def test_cert
    curl = Curl::Easy.new(TestServlet.url)
    curl.cert= File.join(File.dirname(__FILE__),"cert.pem")
    assert_match(/cert.pem$/,curl.cert)
  end

  def test_cert_with_password
    curl = Curl::Easy.new(TestServlet.url)
    path = File.join(File.dirname(__FILE__),"cert.pem")
    curl.certpassword = 'password'
    curl.cert = path
    assert_match(/cert.pem$/,curl.cert)
  end

  def test_cert_type
    curl = Curl::Easy.new(TestServlet.url)
    curl.certtype= "DER"
    assert_equal "DER", curl.certtype
  end

  def test_default_certtype
    curl = Curl::Easy.new(TestServlet.url)
    assert_nil curl.certtype
    curl.certtype = "PEM"
    assert_equal "PEM", curl.certtype
  end

  # Generate a CA cert with instructions at
  # http://technocage.com/~caskey/openssl/
  def test_ca_cert
    curl = Curl::Easy.new(TestServlet.url)
    curl.cacert= File.join(File.dirname(__FILE__),"cacert.pem")
    assert_match(/cacert.pem$/, curl.cacert)
  end

  def test_user_agent
    curl = Curl::Easy.new(TestServlet.url)
    curl.useragent= "Curb-Easy/Ruby"
    assert_equal "Curb-Easy/Ruby",curl.useragent
  end

  def test_username_password
    curl = Curl::Easy.new(TestServlet.url)
    curl.username = "foo"
    curl.password = "bar"
    if !curl.username.nil?
      assert_equal "foo", curl.username
      assert_equal "bar", curl.password
    else
      curl.userpwd = "foo:bar"
    end
    curl.http_auth_types = :basic
    #curl.verbose = true
    curl.perform
    assert_equal 'Basic Zm9vOmJhcg==', $auth_header
    $auth_header = nil
    # curl checks the auth type supported by the server, so we have to create a 
    # new easy handle if we're going to change the auth type...

    curl = Curl::Easy.new(TestServlet.url)
    curl.username = "foo"
    curl.password = "bar"
    if curl.username.nil?
      curl.userpwd = "foo:bar"
    end
    curl.http_auth_types = :ntlm
    curl.perform
    assert_equal 'NTLM TlRMTVNTUAABAAAABoIIAAAAAAAAAAAAAAAAAAAAAAA=', $auth_header
  end

  def test_primary_ip
    curl = Curl::Easy.new(TestServlet.url)
    if curl.respond_to?(:primary_ip)
      curl.perform
      assert_equal '127.0.0.1', curl.primary_ip
    end
  end

  def test_post_streaming
    readme = File.expand_path(File.join(File.dirname(__FILE__),'..','README.markdown'))
    
    pf = Curl::PostField.file("filename", readme)

    easy = Curl::Easy.new

    easy.url = TestServlet.url
    easy.multipart_form_post = true
    easy.http_post(pf)

    assert_not_equal(0,easy.body_str.size)
    assert_equal(easy.body_str.tr("\r", ''), File.read(readme))
  end


  def test_easy_close
    easy = Curl::Easy.new
    easy.close
    easy.url = TestServlet.url
    easy.http_get
  end

  def test_easy_reset
    easy = Curl::Easy.new
    easy.url = TestServlet.url + "?query=foo"
    easy.http_get
    settings = easy.reset
    assert settings.key?(:url)
    assert settings.key?(:body_data)
    assert settings.key?(:header_data)
    easy.url = TestServlet.url
    easy.http_get
  end

  def test_easy_use_http_versions
    easy = Curl::Easy.new
    easy.url = TestServlet.url + "?query=foo"
    #puts "http none: #{Curl::HTTP_NONE.inspect}"
    #puts "http1.0: #{Curl::HTTP_1_0.inspect}"
    #puts "http1.1: #{Curl::HTTP_1_1.inspect}"
    easy.version = Curl::HTTP_1_1
    #easy.verbose = true
    easy.http_get
  end

  def test_easy_http_verbs
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_delete
    assert_equal 'DELETE', curl.body_str
    curl.http_get
    assert_equal 'GET', curl.body_str
    curl.http_post
    assert_equal "POST\n", curl.body_str
    curl.http('PURGE')
    assert_equal 'PURGE', curl.body_str
    curl.http_put('hello')
    assert_equal "PUT\nhello", curl.body_str
    curl.http('COPY')
    assert_equal 'COPY', curl.body_str
  end

  def test_easy_http_verbs_must_respond_to_str
    # issue http://github.com/taf2/curb/issues#issue/45
    assert_nothing_raised do
      c = Curl::Easy.new ; c.url = 'http://example.com' ; c.http(:get)
    end

    assert_raise RuntimeError do
      c = Curl::Easy.new ; c.url = 'http://example.com' ; c.http(FooNoToS.new)
    end

  end

  # http://github.com/taf2/curb/issues/#issue/33
  def test_easy_http_verbs_with_errors
    curl = Curl::Easy.new("http://127.0.0.1:9012/") # test will fail if http server on port 9012
    assert_raise Curl::Err::ConnectionFailedError do
      curl.http_delete
    end
    curl.url = TestServlet.url
    curl.http_get
    assert_equal 'GET', curl.body_str
  end

  def test_easy_can_put_with_content_length
    curl = Curl::Easy.new(TestServlet.url)
    rd, wr = IO.pipe
    buf = (("hello")* (1000 / 5))

    producer = Thread.new do
      5.times do
        wr << buf
        sleep 0.1 # act as a slow producer
      end
    end

    consumer = Thread.new do

      #curl.verbose = true
      curl.headers['Content-Length'] = buf.size * 5
      curl.headers['User-Agent']     = "Something else"
      curl.headers['Content-Type']   = "text/javascript"
      curl.headers['Date']           = Time.now.httpdate
      curl.headers['Host']           = 's3.amazonaws.com'
      curl.headers['Accept']         = '*/*'
      curl.headers['Authorization']  = 'Foo Bar Biz Baz'
      curl.http_put(rd)
      assert_match(/^PUT/, curl.body_str)
      assert_match(/hello$/, curl.body_str)
      curl.header_str
      curl.body_str
    end

    producer.join
    wr.close
    consumer.join

  end

  def test_get_set_multi_on_easy
    easy = Curl::Easy.new
    assert_nil easy.multi
    multi = Curl::Multi.new
    easy.multi = multi
    assert_not_nil easy.multi
    assert_equal multi, easy.multi
  end

  def test_raise_on_progress
    c = Curl::Easy.new($TEST_URL)
    c.on_progress {|w,x,y,z| raise "error" }
    c.perform
  rescue => e
    assert_equal 'Curl::Err::AbortedByCallbackError', e.class.to_s
    c.close
  end

  def test_raise_on_success
    c = Curl::Easy.new($TEST_URL)
    c.on_success {|x| raise "error" }
    c.perform
  rescue => e
    assert_equal 'Curl::Err::AbortedByCallbackError', e.class.to_s
    c.close
  end

  def test_raise_on_debug
    c = Curl::Easy.new($TEST_URL)
    c.on_debug { raise "error" }
    c.perform
    assert true, "raise in on debug has no effect"
  end

  def test_status_codes
    curl = Curl::Easy.new(TestServlet.url)
    curl.perform
    assert_equal '200 OK', curl.status
  end

  def test_close_in_on_callbacks
    curl = Curl::Easy.new(TestServlet.url)
    curl.on_body {|d| curl.close; d.size }
    assert_raises RuntimeError do
      curl.perform
    end
  end

  def test_set_unsupported_options
    curl = Curl::Easy.new
    assert_raises TypeError do
      curl.set(99999, 1)
    end
  end

  include TestServerMethods 

  def setup
    server_setup
  end

end
