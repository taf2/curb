require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbCurlEasySetOpt < Test::Unit::TestCase
  def setup
    @easy = Curl::Easy.new
  end

  def test_opt_verbose
    @easy.set :verbose, true
    assert @easy.verbose?
  end

  def test_opt_header
    @easy.set :header, true
  end

  def test_opt_noprogress
    @easy.set :noprogress, true
  end

  def test_opt_nosignal
    @easy.set :nosignal, true
  end

  def test_opt_url
    url = "http://google.com/"
    @easy.set :url, url
    assert_equal url, @easy.url
  end

end
