require File.join(File.dirname(__FILE__), 'helper')

class TestCurbCurlDownload < Test::Unit::TestCase
  include TestServerMethods 

  def setup
    server_setup(9130)
  end
  
  def test_download_url_to_file
    dl_url = "http://127.0.0.1:9130/ext/curb_easy.c"
    dl_path = File.join("/tmp/dl_url_test.file")

    curb = Curl::Easy.download(dl_url, dl_path)
    assert File.exist?(dl_path)
  end

  def test_download_bad_url_gives_404
    dl_url = "http://127.0.0.1:9130/this_file_does_not_exist.html"
    dl_path = File.join("/tmp/dl_url_test.file")

    curb = Curl::Easy.download(dl_url, dl_path)
    assert_equal Curl::Easy, curb.class
    assert_equal 404, curb.response_code
  end

end
