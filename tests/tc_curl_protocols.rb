require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbCurlProtocols < Test::Unit::TestCase
  include TestServerMethods

  def setup
    @easy = Curl::Easy.new
    @easy.set :protocols, Curl::CURLPROTO_HTTP | Curl::CURLPROTO_HTTPS
    @easy.follow_location = true
    server_setup
  end

  def teardown
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
    super
  end

  def test_protocol_allowed
    @easy.set :url, "http://127.0.0.1:9129/this_file_does_not_exist.html"
    @easy.perform
    assert_equal 404, @easy.response_code
  end

  def test_protocol_denied
    @easy.set :url, "gopher://google.com/"
    assert_raises Curl::Err::UnsupportedProtocolError do
      @easy.perform
    end
  end

  def test_redir_protocol_allowed
    @easy.set :url, TestServlet.url + "/redirect"
    @easy.set :redir_protocols, Curl::CURLPROTO_HTTP
    @easy.perform
  end

  def test_redir_protocol_denied
    @easy.set :url, TestServlet.url + "/redirect"
    @easy.set :redir_protocols, Curl::CURLPROTO_HTTPS
    assert_raises Curl::Err::UnsupportedProtocolError do
      @easy.perform
    end
  end

  def test_protocols_str_setopt_supported
    omit('CURLOPT_PROTOCOLS_STR not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_PROTOCOLS_STR)

    @easy.set :protocols_str, 'http,https'
    @easy.set :url, "http://127.0.0.1:9129/this_file_does_not_exist.html"
    @easy.perform
    assert_equal 404, @easy.response_code
  end

  def test_redir_protocols_str_setopt_supported
    omit('CURLOPT_REDIR_PROTOCOLS_STR not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_REDIR_PROTOCOLS_STR)

    @easy.set :url, TestServlet.url + "/redirect"
    @easy.set :redir_protocols_str, 'https'
    assert_raises Curl::Err::UnsupportedProtocolError do
      @easy.perform
    end
  end

  def test_allowed_protocols_denies_file_protocol
    easy = Curl::Easy.new($TEST_URL)
    easy.allowed_protocols = [:http, :https]

    assert_raises Curl::Err::UnsupportedProtocolError do
      easy.perform
    end
  end

  def test_safe_http_denies_file_protocol
    easy = Curl::Easy.new($TEST_URL)
    easy.safe_http!

    assert_raises Curl::Err::UnsupportedProtocolError do
      easy.perform
    end
  end

  def test_safe_http_override_is_cleared_by_close
    easy = Curl::Easy.new($TEST_URL)
    easy.safe_http!
    easy.close
    easy.url = $TEST_URL

    easy.perform

    assert_match(/TESTMODEL/, easy.body_str)
  end

  def test_safe_get_denies_file_protocol
    assert_raises Curl::Err::UnsupportedProtocolError do
      Curl.safe_get($TEST_URL)
    end
  end

  def test_safe_get_reapplies_protocols_after_user_block
    assert_raises Curl::Err::UnsupportedProtocolError do
      Curl.safe_get($TEST_URL) do |easy|
        easy.allowed_protocols = [:file]
      end
    end
  end

  def test_safe_get_allows_http_protocol
    easy = Curl.safe_get(TestServlet.url)

    assert_equal 200, easy.response_code
    assert_match(/GET/, easy.body_str)
  end

  def test_safe_bang_denies_file_protocol_for_easy_perform
    Curl.safe!
    easy = Curl::Easy.new($TEST_URL)

    assert_raises Curl::Err::UnsupportedProtocolError do
      easy.perform
    end
  end

  def test_safe_bang_denies_file_protocol_for_shortcut_helpers
    Curl.safe!

    assert_raises Curl::Err::UnsupportedProtocolError do
      Curl.get($TEST_URL)
    end
  end

  def test_safe_bang_reapplies_protocols_after_shortcut_block
    Curl.safe!

    assert_raises Curl::Err::UnsupportedProtocolError do
      Curl.get($TEST_URL) do |easy|
        easy.allowed_protocols = [:file]
      end
    end
  end

  def test_safe_bang_allows_configured_protocols
    Curl.safe! do |config|
      config.protocols = [:http, :file]
    end

    easy = Curl::Easy.new($TEST_URL)
    easy.perform

    assert_match(/DO NOT REMOVE THIS COMMENT/, easy.body_str)
  end

  def test_safe_get_remains_http_safe_with_broader_global_policy
    Curl.safe! do |config|
      config.protocols = [:http, :file]
    end

    assert_raises Curl::Err::UnsupportedProtocolError do
      Curl.safe_get($TEST_URL)
    end
  end

  def test_safe_bang_allows_http_protocol
    Curl.safe!
    easy = Curl.get(TestServlet.url)

    assert_equal 200, easy.response_code
    assert_match(/GET/, easy.body_str)
  end

  def test_safe_bang_applies_to_multi_perform
    Curl.safe!
    easy = Curl::Easy.new($TEST_URL)
    multi = Curl::Multi.new

    multi.add(easy)
    multi.perform

    assert_equal Curl::Err::UnsupportedProtocolError, Curl::Easy.error(easy.last_result).first
  ensure
    multi.close if defined?(multi) && multi
  end

  def test_safe_http_denies_file_redirect
    with_file_redirect_server do |url|
      easy = Curl::Easy.new(url)
      easy.follow_location = true
      easy.safe_http!

      assert_raises Curl::Err::UnsupportedProtocolError do
        easy.perform
      end
    end
  end

  def test_safe_bang_denies_file_redirect
    with_file_redirect_server do |url|
      Curl.safe!

      easy = Curl::Easy.new(url)
      easy.follow_location = true

      assert_raises Curl::Err::UnsupportedProtocolError do
        easy.perform
      end
    end
  end

  def test_safe_http_allows_http_to_https_redirect
    with_http_https_redirect_servers(:http, :https) do |url|
      easy = Curl::Easy.new(url)
      easy.follow_location = true
      easy.ssl_verify_peer = false
      easy.ssl_verify_host = false
      easy.safe_http!

      easy.perform

      assert_equal 200, easy.response_code
      assert_equal 'https target', easy.body_str
    end
  end

  def test_safe_http_allows_https_to_http_redirect
    with_http_https_redirect_servers(:https, :http) do |url|
      easy = Curl::Easy.new(url)
      easy.follow_location = true
      easy.ssl_verify_peer = false
      easy.ssl_verify_host = false
      easy.safe_http!

      easy.perform

      assert_equal 200, easy.response_code
      assert_equal 'http target', easy.body_str
    end
  end

  def test_safe_http_honors_redirect_limit
    with_redirect_loop_server do |url, hits|
      easy = Curl::Easy.new(url)
      easy.follow_location = true
      easy.max_redirects = 2
      easy.safe_http!

      assert_raises Curl::Err::TooManyRedirectsError do
        easy.perform
      end

      assert_operator hits[:loop], :>=, 2
    end
  end

  private

  def with_file_redirect_server
    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    server = WEBrick::HTTPServer.new(:Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    server.mount_proc('/redirect-file') do |_req, res|
      res.status = 302
      res['Location'] = $TEST_URL
      res.body = 'redirect'
    end

    thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: thread)

    yield "http://127.0.0.1:#{port}/redirect-file"
  ensure
    server.shutdown if defined?(server) && server
    thread.join(server_shutdown_timeout) if defined?(thread) && thread
    if defined?(thread) && thread&.alive?
      thread.kill
      thread.join(server_shutdown_timeout)
    end
  end

  def with_redirect_loop_server
    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    hits = Hash.new(0)
    server = WEBrick::HTTPServer.new(:Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    server.mount_proc('/loop') do |_req, res|
      hits[:loop] += 1
      res.status = 302
      res['Location'] = '/loop'
      res.body = 'redirect'
    end

    thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: thread)

    yield "http://127.0.0.1:#{port}/loop", hits
  ensure
    server.shutdown if defined?(server) && server
    thread.join(server_shutdown_timeout) if defined?(thread) && thread
    if defined?(thread) && thread&.alive?
      thread.kill
      thread.join(server_shutdown_timeout)
    end
  end

  def with_http_https_redirect_servers(initial_scheme, target_scheme)
    require 'webrick/https'
    require 'openssl'

    omit('HTTPS redirect fixtures require OpenSSL') unless defined?(OpenSSL::PKey::RSA)

    initial_server = nil
    target_server = nil
    initial_thread = nil
    target_thread = nil

    initial_port = unused_redirect_port
    target_port = unused_redirect_port
    target_server = redirect_matrix_server(target_scheme, target_port)
    target_server.mount_proc('/target') do |_req, res|
      res['Content-Type'] = 'text/plain'
      res.body = "#{target_scheme} target"
    end

    target_thread = Thread.new(target_server) { |srv| srv.start }
    wait_for_server_ready(target_port, thread: target_thread)

    initial_server = redirect_matrix_server(initial_scheme, initial_port)
    initial_server.mount_proc('/redirect') do |_req, res|
      res.status = 302
      res['Location'] = "#{target_scheme}://127.0.0.1:#{target_port}/target"
      res.body = 'redirect'
    end

    initial_thread = Thread.new(initial_server) { |srv| srv.start }
    wait_for_server_ready(initial_port, thread: initial_thread)

    yield "#{initial_scheme}://127.0.0.1:#{initial_port}/redirect"
  ensure
    initial_server.shutdown if initial_server
    target_server.shutdown if target_server
    initial_thread.join(server_shutdown_timeout) if initial_thread
    target_thread.join(server_shutdown_timeout) if target_thread
    [initial_thread, target_thread].compact.each do |thread|
      next unless thread.alive?

      thread.kill
      thread.join(server_shutdown_timeout)
    end
  end

  def redirect_matrix_server(scheme, port)
    options = {:BindAddress => '127.0.0.1', :Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => []}
    return WEBrick::HTTPServer.new(options) if scheme == :http

    cert, key = redirect_matrix_certificate
    WEBrick::HTTPServer.new(options.merge(:SSLEnable => true, :SSLCertificate => cert, :SSLPrivateKey => key))
  end

  def redirect_matrix_certificate
    key = OpenSSL::PKey::RSA.new(2048)
    cert = OpenSSL::X509::Certificate.new
    cert.version = 2
    cert.serial = 1
    cert.subject = OpenSSL::X509::Name.parse('/CN=127.0.0.1')
    cert.issuer = cert.subject
    cert.public_key = key.public_key
    cert.not_before = Time.now - 60
    cert.not_after = Time.now + 3600

    extensions = OpenSSL::X509::ExtensionFactory.new
    extensions.subject_certificate = cert
    extensions.issuer_certificate = cert
    cert.add_extension(extensions.create_extension('basicConstraints', 'CA:FALSE', true))
    cert.add_extension(extensions.create_extension('subjectAltName', 'IP:127.0.0.1', false))
    cert.sign(key, OpenSSL::Digest::SHA256.new)

    [cert, key]
  end

  def unused_redirect_port
    socket = TCPServer.new('127.0.0.1', 0)
    socket.addr[1]
  ensure
    socket.close if socket
  end
end
