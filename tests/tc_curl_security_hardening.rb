require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

# Regression coverage for request-hardening fixes: non-IP sockets and DoH
# endpoints under network_policy = :public, file:// under :public, cookies=
# on cross-host redirects, redirect/timeout defaults, and CR/LF injection.
class TestCurbCurlSecurityHardening < Test::Unit::TestCase
  include TestServerMethods

  def setup
    server_setup
    @servers = []
  end

  def teardown
    @servers.each(&:close)
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
    super
  end

  # Starts a server on 127.0.0.1 that answers each request with the result of
  # the block (given the request path) and records [path, request headers].
  def scripted_server(&respond)
    server = TCPServer.new('127.0.0.1', 0)
    @servers << server
    log = Queue.new
    Thread.new do
      loop do
        client = begin
          server.accept
        rescue IOError, Errno::EBADF
          break
        end
        begin
          request = +''
          request << client.readpartial(4096) until request.include?("\r\n\r\n")
          path = request[/\A\S+ (\S+)/, 1]
          log << [path, request.split("\r\n\r\n", 2).first.lines.drop(1).map(&:strip)]
          client.write(respond.call(path))
        rescue IOError, SystemCallError
          nil
        ensure
          client.close
        end
      end
    end
    [server.addr[1], log]
  end

  def ok_response(body = 'ok')
    "HTTP/1.1 200 OK\r\nContent-Length: #{body.bytesize}\r\nConnection: close\r\n\r\n#{body}"
  end

  def redirect_response(location)
    "HTTP/1.1 302 Found\r\nLocation: #{location}\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
  end

  def drain(log)
    entries = []
    entries << log.pop until log.empty?
    entries
  end

  def require_public_network_policy!
    Curl::Easy.new.network_policy = :public
  rescue NotImplementedError
    omit('network_policy=:public requires CURLOPT_OPENSOCKETFUNCTION support')
  end

  # --- non-IP sockets under :public ----------------------------------------

  def test_unix_socket_set_after_multi_add_is_blocked_by_public_policy
    omit('CURLOPT_UNIX_SOCKET_PATH not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_UNIX_SOCKET_PATH)
    omit('Unix domain sockets are not available on this platform') if WINDOWS || !defined?(UNIXServer)
    require_public_network_policy!

    dir = Dir.mktmpdir('curb-unix')
    path = File.join(dir, 's.sock')
    server = UNIXServer.new(path)
    accepted = Queue.new
    thread = Thread.new do
      socket = server.accept
      accepted << true
      socket.readpartial(4096) rescue nil
      socket.write(ok_response('bypass'))
      socket.close
    end

    easy = Curl::Easy.new('http://198.51.100.1/')
    easy.network_policy = :public
    multi = Curl::Multi.new
    multi.add(easy)
    easy.setopt(Curl::CURLOPT_UNIX_SOCKET_PATH, path)
    multi.perform

    assert_not_equal 'bypass', easy.body_str
    assert_match(/non-IP socket/, easy.unsafe_destination_error)
    assert accepted.empty?, 'the Unix socket server must not be contacted'
  ensure
    multi.close if multi
    thread.kill if thread
    server.close if server
    FileUtils.rm_rf(dir) if dir
  end

  # --- DoH endpoints under :public ------------------------------------------

  def test_public_policy_requires_public_ip_literal_doh_endpoints
    omit('CURLOPT_DOH_URL not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_DOH_URL)
    require_public_network_policy!

    blocked = %w[
      https://localhost/dns-query https://dns.example/dns-query
      https://127.0.0.1/dns-query https://127.1/dns-query https://2130706433/dns-query
      https://[::1]/dns-query https://[::ffff:127.0.0.1]/dns-query https://10.0.0.1/dns-query
    ]
    blocked.each do |doh_url|
      easy = Curl::Easy.new('http://198.51.100.1/')
      easy.network_policy = :public
      easy.doh_url = doh_url
      assert_raise(Curl::Err::UnsafeDestinationError, doh_url) { easy.__send__(:__curb_native_setup!) }
    end

    %w[https://1.1.1.1/dns-query https://[2606:4700:4700::1111]/dns-query].each do |doh_url|
      easy = Curl::Easy.new('http://198.51.100.1/')
      easy.network_policy = :public
      easy.doh_url = doh_url
      assert_nothing_raised(doh_url) { easy.__send__(:__curb_native_setup!) }
    end
  end

  def test_safe_mode_allow_doh_still_rejects_local_doh_endpoint
    omit('CURLOPT_DOH_URL not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_DOH_URL)
    require_public_network_policy!
    port, log = scripted_server { |_| ok_response }

    Curl.safe! { |config| config.allow_doh = true }
    easy = Curl::Easy.new('http://example.com/')
    easy.doh_url = "https://localhost:#{port}/dns-query"

    error = assert_raise(Curl::Err::UnsafeDestinationError) { easy.perform }
    assert_match(/DoH URL must use a public IP address/, error.message)
    assert_empty drain(log)
  end

  def test_public_policy_doh_check_does_not_apply_without_policy
    omit('CURLOPT_DOH_URL not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_DOH_URL)
    easy = Curl::Easy.new('http://198.51.100.1/')
    easy.doh_url = 'https://localhost/dns-query'
    assert_nothing_raised { easy.__send__(:__curb_native_setup!) }
  end

  # --- file:// under :public ------------------------------------------------

  def test_public_policy_rejects_file_urls_without_protocol_allowlist
    require_public_network_policy!
    ['file:///etc/hosts', 'FILE:///etc/hosts', 'file:/etc/hosts', "file://#{File.expand_path(__FILE__)}"].each do |url|
      easy = Curl::Easy.new(url)
      easy.network_policy = :public
      error = assert_raise(Curl::Err::UnsafeDestinationError, url) { easy.perform }
      assert_match(/file:\/\/ URLs are not allowed/, error.message)
    end
  end

  def test_public_policy_respects_explicit_file_protocol_allowlist
    require_public_network_policy!
    easy = Curl::Easy.new("file://#{File.expand_path(__FILE__)}")
    easy.network_policy = :public
    easy.allowed_protocols = [:file]
    easy.perform
    assert_match(/Regression coverage for request-hardening/, easy.body_str)
  end

  def test_file_urls_still_work_without_public_policy
    easy = Curl::Easy.new($TEST_URL)
    easy.perform
    assert_match(/DO NOT REMOVE THIS COMMENT/, easy.body_str)
  end

  # --- cookies= on redirects ------------------------------------------------

  def test_cookies_are_not_forwarded_to_a_different_host_on_redirect
    other_port, other_log = scripted_server { |_| ok_response }
    port, log = scripted_server { |_| redirect_response("http://127.0.0.1:#{other_port}/landing") }

    easy = Curl::Easy.new("http://127.0.0.1:#{port}/start")
    easy.follow_location = true
    easy.cookies = 'session=secret'
    easy.perform

    assert_equal 200, easy.response_code
    first = drain(log)
    second = drain(other_log)
    assert_include first.first.last, 'Cookie: session=secret'
    assert_equal 1, second.length
    assert_empty second.first.last.grep(/\Acookie:/i), 'cookies= must not reach a different host'
  end

  def test_cookies_are_kept_on_same_host_redirect
    port, log = scripted_server { |path| path == '/start' ? redirect_response('/landing') : ok_response }

    easy = Curl::Easy.new("http://127.0.0.1:#{port}/start")
    easy.follow_location = true
    easy.cookies = 'session=secret'
    easy.perform

    requests = drain(log)
    assert_equal ['/start', '/landing'], requests.map(&:first)
    requests.each { |(_, headers)| assert_equal ['Cookie: session=secret'], headers.grep(/\Acookie:/i) }
  end

  def test_explicit_cookie_header_still_wins_over_cookies
    port, log = scripted_server { |_| ok_response }

    easy = Curl::Easy.new("http://127.0.0.1:#{port}/")
    easy.headers['Cookie'] = 'explicit=1'
    easy.cookies = 'session=secret'
    easy.perform

    assert_equal ['Cookie: explicit=1'], drain(log).first.last.grep(/\Acookie:/i)
  end

  def test_cookies_cleared_after_being_set_are_not_sent
    port, log = scripted_server { |_| ok_response }

    easy = Curl::Easy.new("http://127.0.0.1:#{port}/")
    easy.cookies = 'session=secret'
    easy.perform
    easy.cookies = nil
    easy.perform

    first, second = drain(log)
    assert_equal ['Cookie: session=secret'], first.last.grep(/\Acookie:/i)
    assert_empty second.last.grep(/\Acookie:/i)
  end

  def test_cookies_merge_with_cookie_engine_on_one_line
    port, log = scripted_server do |path|
      if path == '/start'
        "HTTP/1.1 302 Found\r\nLocation: /landing\r\nSet-Cookie: fromserver=1\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
      else
        ok_response
      end
    end

    easy = Curl::Easy.new("http://127.0.0.1:#{port}/start")
    easy.follow_location = true
    easy.enable_cookies = true
    easy.cookies = 'session=secret'
    easy.perform

    landing = drain(log).last.last.grep(/\Acookie:/i)
    assert_equal 1, landing.length, 'the cookie engine and cookies= must share one Cookie line'
    assert_match(/fromserver=1/, landing.first)
    assert_match(/session=secret/, landing.first)
  end

  # --- redirect and timeout defaults ----------------------------------------

  def test_unset_max_redirects_caps_redirect_loops_at_thirty
    port, log = scripted_server { |path| redirect_response("/r#{path[/\d+/].to_i + 1}") }

    easy = Curl::Easy.new("http://127.0.0.1:#{port}/r0")
    easy.follow_location = true
    assert_nil easy.max_redirects
    assert_raise(Curl::Err::TooManyRedirectsError) { easy.perform }
    assert_equal 31, drain(log).length
  end

  def test_explicit_unlimited_and_explicit_max_redirects_are_honored
    port, log = scripted_server { |path| (n = path[/\d+/].to_i) < 40 ? redirect_response("/r#{n + 1}") : ok_response }

    easy = Curl::Easy.new("http://127.0.0.1:#{port}/r0")
    easy.follow_location = true
    easy.max_redirects = -1
    easy.perform
    assert_equal 200, easy.response_code
    assert_equal 41, drain(log).length

    easy.max_redirects = 2
    assert_raise(Curl::Err::TooManyRedirectsError) { easy.perform }
    assert_equal 3, drain(log).length
  end

  def test_safe_mode_applies_default_timeout_and_redirect_cap
    config = Curl::SafetyConfig.new
    assert_equal 60, config.timeout
    assert_equal 20, config.max_redirects

    Curl.safe!
    easy = Curl::Easy.new('http://198.51.100.1/')
    Curl.__send__(:apply_safety!, easy)
    assert_equal 60.0, easy.timeout
    assert_equal 20, easy.max_redirects

    unlimited = Curl::Easy.new('http://198.51.100.1/')
    unlimited.max_redirects = -1
    Curl.__send__(:apply_safety!, unlimited)
    assert_equal 20, unlimited.max_redirects
  end

  def test_safe_mode_keeps_handle_limits_and_accepts_overrides
    Curl.safe!
    own = Curl::Easy.new('http://198.51.100.1/')
    own.timeout = 5
    own.max_redirects = 3
    Curl.__send__(:apply_safety!, own)
    assert_equal 5.0, own.timeout
    assert_equal 3, own.max_redirects
    Curl.__send__(:clear_safe!)

    helper = Curl::Easy.new('http://198.51.100.1/')
    helper.safe_http!(:timeout => 7, :max_redirects => 2)
    assert_equal 7.0, helper.timeout
    assert_equal 2, helper.max_redirects

    Curl.safe! { |c| c.timeout = nil; c.max_redirects = nil }
    disabled = Curl::Easy.new('http://198.51.100.1/')
    Curl.__send__(:apply_safety!, disabled)
    assert_equal 0.0, disabled.timeout
    assert_nil disabled.max_redirects
  end

  def test_safety_config_validates_limits
    config = Curl::SafetyConfig.new
    assert_raise(ArgumentError) { config.timeout = 0 }
    assert_raise(ArgumentError) { config.timeout = -1 }
    assert_raise(ArgumentError) { config.max_redirects = -1 }
    config.timeout = 1.5
    config.max_redirects = 0
    assert_equal 1.5, config.timeout
    assert_equal 0, config.max_redirects
  end

  # --- CR/LF/NUL injection --------------------------------------------------

  INJECTIONS = {
    'header value' => ->(easy) { easy.headers['X-User'] = "alice\r\nX-Injected: yes" },
    'header name' => ->(easy) { easy.headers["X-A\r\nX-Injected"] = 'yes' },
    'header array entry' => ->(easy) { easy.headers = ["X-User: alice\nX-Injected: yes"] },
    'header string' => ->(easy) { easy.headers = "X-User: alice\r\nX-Injected: yes" },
    'header NUL' => ->(easy) { easy.headers['X-User'] = "alice\0X-Injected: yes" },
    'useragent' => ->(easy) { easy.useragent = "ua\r\nX-Injected: yes" },
    'cookies' => ->(easy) { easy.cookies = "a=1\r\nX-Injected: yes" },
  }.freeze

  def test_crlf_in_request_headers_is_rejected
    port, log = scripted_server { |_| ok_response }
    INJECTIONS.each do |name, inject|
      easy = Curl::Easy.new("http://127.0.0.1:#{port}/")
      inject.call(easy)
      error = assert_raise(ArgumentError, name) { easy.perform }
      assert_match(/must not contain CR, LF, or NUL/, error.message, name)
    end
    assert_empty drain(log), 'no injected request may reach the server'
  end

  def test_crlf_in_proxy_headers_is_rejected
    omit('proxy_headers not supported') unless Curl::Easy.method_defined?(:proxy_headers=)
    easy = Curl::Easy.new('http://198.51.100.1/')
    easy.proxy_headers['X-Proxy'] = "a\r\nX-Injected: yes"
    assert_raise(ArgumentError) { easy.__send__(:__curb_native_setup!) }
  end

  def test_crlf_in_request_method_and_target_is_rejected
    easy = Curl::Easy.new(TestServlet.url)
    assert_raise(ArgumentError) { easy.setopt(Curl::CURLOPT_CUSTOMREQUEST, "GET / HTTP/1.1\r\nX-Injected: yes\r\n\r\nGET") }
    assert_raise(ArgumentError) { easy.http("GET\r\nX-Injected: yes") }
    if Curl.const_defined?(:CURLOPT_REQUEST_TARGET)
      assert_raise(ArgumentError) { easy.setopt(Curl::CURLOPT_REQUEST_TARGET, "/\r\nX-Injected: yes") }
    end
  end

  def test_ordinary_headers_and_header_removal_still_work
    port, log = scripted_server { |_| ok_response }
    easy = Curl::Easy.new("http://127.0.0.1:#{port}/")
    easy.headers['X-User'] = 'alice'
    easy.headers['Accept'] = ''
    easy.useragent = 'curb-test/1.0'
    easy.perform

    headers = drain(log).first.last
    assert_include headers, 'X-User: alice'
    assert_include headers, 'User-Agent: curb-test/1.0'
    assert_empty headers.grep(/\AAccept:/i)
  end
end
