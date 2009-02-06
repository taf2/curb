require File.join(File.dirname(__FILE__), 'helper')

class TestCurbCurlEasy < Test::Unit::TestCase
  def test_class_perform_01   
    assert_instance_of Curl::Easy, c = Curl::Easy.perform($TEST_URL)    
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
    assert_equal "", c.header_str
  end    

  def test_class_perform_02
    data = ""
    assert_instance_of Curl::Easy, c = Curl::Easy.perform($TEST_URL) { |curl| curl.on_body { |d| data << d; d.length } }    

    assert_nil c.body_str
    assert_equal "", c.header_str
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, data)
  end    

  def test_class_perform_03
    assert_raise(Curl::Err::CouldntReadError) { c = Curl::Easy.perform($TEST_URL + "nonexistent") }
  end    
  
  def test_new_01
    c = Curl::Easy.new
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
    assert_equal "", c.header_str
  end    

  def test_get_02
    data = ""
    c = Curl::Easy.new($TEST_URL) do |curl|
      curl.on_body { |d| data << d; d.length }
    end
    
    assert_equal true, c.http_get    
    
    assert_nil c.body_str
    assert_equal "", c.header_str
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
  
  def test_timeout_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.timeout
    
    c.timeout = 3
    assert_equal 3, c.timeout
    
    c.timeout = nil
    assert_nil c.timeout
  end
  
  def test_connect_timeout_01
    c = Curl::Easy.new($TEST_URL)
    
    assert_nil c.connect_timeout
    
    c.connect_timeout = 3
    assert_equal 3, c.connect_timeout
    
    c.connect_timeout = nil
    assert_nil c.connect_timeout
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
  
  def test_on_body
    blk = lambda { |i| i.length }
    
    c = Curl::Easy.new    
    c.on_body(&blk)
    
    assert_equal blk, c.on_body   # sets handler nil, returns old handler
    assert_equal nil, c.on_body
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
    assert !c.ssl_verify_host = false
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

  def test_enable_cookies
    c = Curl::Easy.new
    assert !c.enable_cookies?
    assert c.enable_cookies = true
    assert c.enable_cookies?
  end
  
  def test_cookiejar
    c = Curl::Easy.new
    assert_nil c.cookiejar
    assert_equal "some.file", c.cookiejar = "some.file"
    assert_equal "some.file", c.cookiejar        
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
    curl = Curl::Easy.new("#{$TEST_URL.gsub(/file:\/\//,'')}/not_here")
    on_failure_called = false
    curl.on_success {|c| } # make sure we get the failure call even though this handler is defined
    curl.on_failure {|c| on_failure_called = true }
    curl.perform
    assert on_failure_called, "Failure handler not called" 
  end
  
  def test_get_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_get
    assert_equal 'GET', curl.body_str
  end
  
  def test_post_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_post
    assert_equal 'POST', curl.body_str
  end

  def test_delete_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_delete
    assert_equal 'DELETE', curl.body_str
  end

  def test_head_remote
    curl = Curl::Easy.new(TestServlet.url)
    curl.http_head

    redirect = curl.header_str.match(/Location: (.*)/)

    assert_equal '', curl.body_str
    assert_match '/nonexistent', redirect[1]
  end

  def test_put_remote
    curl = Curl::Easy.new(TestServlet.url)
    assert curl.http_put("message")
    assert_match /^PUT/, curl.body_str
    assert_match /message$/, curl.body_str
  end

  include TestServerMethods 

  def setup
    server_setup
  end

end
