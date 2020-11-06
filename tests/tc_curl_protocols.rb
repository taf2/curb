class TestCurbCurlProtocols < Test::Unit::TestCase
  include TestServerMethods

  def setup
    @easy = Curl::Easy.new
    @easy.set :protocols, Curl::CURLPROTO_HTTP | Curl::CURLPROTO_HTTPS
    @easy.follow_location = true
    server_setup
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
end
