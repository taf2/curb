require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbCurlMaxFileSize < Test::Unit::TestCase
  def setup
    @easy = Curl::Easy.new
  end

  def test_maxfilesize
    @easy.set(Curl::CURLOPT_MAXFILESIZE, 5000000)
  end

end
