require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbCurlEasyResolve < Test::Unit::TestCase
  def setup
    @easy = Curl::Easy.new
  end

  def test_resolve
    @easy.resolve = [ "example.com:80:127.0.0.1" ]
    assert_equal @easy.resolve, [ "example.com:80:127.0.0.1" ]
  end

  def test_empty_resolve
    assert_equal @easy.resolve, nil
  end
end
