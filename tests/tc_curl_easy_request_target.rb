require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbCurlEasyRequestTarget < Test::Unit::TestCase
  include TestServerMethods

  def setup
    server_setup
  end

  def test_request_target_absolute_form
    unless Curl.const_defined?(:CURLOPT_REQUEST_TARGET)
      omit('libcurl lacks CURLOPT_REQUEST_TARGET support')
    end

    tmp = Tempfile.new('curb_test_request_target')
    path = tmp.path
    fd = IO.sysopen(path, 'w')
    io = IO.new(fd, 'w')
    io.sync = true

    easy = Curl::Easy.new(TestServlet.url)
    easy.verbose = true
    easy.setopt(Curl::CURLOPT_STDERR, io)

    # Force absolute-form request target, different from the URL host
    easy.request_target = "http://localhost:#{TestServlet.port}#{TestServlet.path}"
    easy.headers = { 'Host' => "example.com" }

    easy.perform

    io.flush
    io.close
    output = File.read(path)

    assert_match(/GET\s+http:\/\/localhost:#{TestServlet.port}#{Regexp.escape(TestServlet.path)}\s+HTTP\/1\.1/, output)
    assert_match(/Host:\s+example\.com/, output)
  ensure
    tmp.close! if defined?(tmp) && tmp
  end
end

