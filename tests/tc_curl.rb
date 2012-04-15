require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurl < Test::Unit::TestCase
  def test_get
    curl = Curl.get(TestServlet.url, {:foo => "bar"})
    assert_equal "GETfoo=bar", curl.body_str

    curl = Curl.options(TestServlet.url, {:foo => "bar"}) do|http|
      http.headers['Cookie'] = 'foo=1;bar=2'
    end
    assert_equal "OPTIONSfoo=bar", curl.body_str
  end

  def test_post
    curl = Curl.post(TestServlet.url, {:foo => "bar"})
    assert_equal "POST\nfoo=bar",  curl.body_str
  end

  def test_put
    curl = Curl.put(TestServlet.url, {:foo => "bar"})
    assert_equal "PUT\nfoo=bar",  curl.body_str
  end

  def test_patch
    curl = Curl.patch(TestServlet.url, {:foo => "bar"})
    assert_equal "PATCH\nfoo=bar", curl.body_str
  end

  def test_options
    curl = Curl.options(TestServlet.url, {:foo => "bar"})
    assert_equal "OPTIONSfoo=bar", curl.body_str
  end

  include TestServerMethods 

  def setup
    server_setup
  end
end
