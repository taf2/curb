require 'digest'
require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class BugTestPoisonResponse < Test::Unit::TestCase
  include BugTestServerSetupTeardown

  def setup
    @port = unused_local_port
    @response_proc = lambda do|res|
      sleep 0.5
      res.body = "hi"
      res['Content-Type'] = "text/html"
    end
    super
  end

  def test_bug
    res_a = Curl.get("https://google.com/")

    first_a_body = Digest::MD5.hexdigest(res_a.body)

    res_b = Curl.get("https://unclanked.com/")

    b_body = Digest::MD5.hexdigest(res_b.body)
    a_body = Digest::MD5.hexdigest(res_a.body)
    assert_not_equal a_body, b_body, "we expected #{a_body} to be #{first_a_body}"
  end

end
