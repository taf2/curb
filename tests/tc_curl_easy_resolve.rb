require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbCurlEasyResolve < Test::Unit::TestCase
  include TestServerMethods

  def setup
    server_setup
    @easy = Curl::Easy.new
  end

  def test_resolve
    @easy.resolve = [ "example.com:80:127.0.0.1" ]
    assert_equal @easy.resolve, [ "example.com:80:127.0.0.1" ]
  end

  def test_empty_resolve
    assert_equal @easy.resolve, nil
  end

  def test_setopt_resolve_persists_across_performs
    host = "curb-setopt-resolve.invalid"
    mapping = "#{host}:#{TestServlet.port}:127.0.0.1"

    @easy.url = "http://#{host}:#{TestServlet.port}#{TestServlet.path}"
    @easy.proxy_url = ""
    @easy.dns_cache_timeout = 0
    @easy.set(:resolve, [mapping])

    @easy.perform
    assert_match(/GET/, @easy.body_str)

    @easy.perform
    assert_match(/GET/, @easy.body_str)
  end

  def test_resolve_entries_can_be_objects_that_convert_to_string
    host = "curb-resolve-object.invalid"
    mapping = "#{host}:#{TestServlet.port}:127.0.0.1"
    entry = Object.new
    entry.define_singleton_method(:to_s) { mapping }

    @easy.url = "http://#{host}:#{TestServlet.port}#{TestServlet.path}"
    @easy.proxy_url = ""
    @easy.dns_cache_timeout = 0
    @easy.resolve = [entry]

    @easy.perform
    assert_match(/GET/, @easy.body_str)
  end
end
