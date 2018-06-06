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

  def test_urlalize_without_extra_params
    url_no_params = 'http://localhost/test'
    url_with_params = 'http://localhost/test?a=1'

    assert_equal(url_no_params, Curl.urlalize(url_no_params))
    assert_equal(url_with_params, Curl.urlalize(url_with_params))
  end

  def test_urlalize_with_nil_as_params
    url = 'http://localhost/test'
    assert_equal(url, Curl.urlalize(url, nil))
  end

  def test_urlalize_with_extra_params
    url_no_params = 'http://localhost/test'
    url_with_params = 'http://localhost/test?a=1'
    extra_params = { :b => 2 }

    expected_url_no_params = 'http://localhost/test?b=2'
    expected_url_with_params = 'http://localhost/test?a=1&b=2'

    assert_equal(expected_url_no_params, Curl.urlalize(url_no_params, extra_params))
    assert_equal(expected_url_with_params, Curl.urlalize(url_with_params, extra_params))
  end

  def test_urlalize_does_not_strip_trailing_?
    url_empty_params = 'http://localhost/test?'
    assert_equal(url_empty_params, Curl.urlalize(url_empty_params))
  end

  include TestServerMethods

  def setup
    server_setup
  end
end
