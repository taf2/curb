require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class BugFollowRedirect288 < Test::Unit::TestCase
  include BugTestServerSetupTeardown

  def setup
    @port = 9999
    super
    @server.mount_proc("/redirect_to_test") do|req,res|
      res.set_redirect(WEBrick::HTTPStatus::TemporaryRedirect, "/test")
    end
  end

  def test_follow_redirect_with_no_redirect

    c = Curl::Easy.new('http://127.0.0.1:9999/test')
    did_call_redirect = false
    c.on_redirect do|x|
      did_call_redirect = true
    end
    c.perform

    assert !did_call_redirect, "should reach this point redirect should not have been called"

    c = Curl::Easy.new('http://127.0.0.1:9999/test')
    did_call_redirect = false
    c.on_redirect do|x|
      did_call_redirect = true
    end
    c.follow_location = true
    c.perform

    assert_equal 0, c.redirect_count
    assert !did_call_redirect, "should reach this point redirect should not have been called"

    c = Curl::Easy.new('http://127.0.0.1:9999/redirect_to_test')
    did_call_redirect = false
    c.on_redirect do|x|
      did_call_redirect = true
    end
    c.perform
    assert_equal 307, c.response_code

    assert did_call_redirect, "we should have called on_redirect"

    c = Curl::Easy.new('http://127.0.0.1:9999/redirect_to_test')
    did_call_redirect = false
    c.follow_location = true
    # NOTE: while this API is not supported by libcurl e.g. there is no redirect function callback in libcurl we could 
    # add support in ruby for this by executing this callback if redirect_count is greater than 0 at the end of a request in curb_multi.c
    c.on_redirect do|x|
      did_call_redirect = true
    end
    c.perform
    assert_equal 1, c.redirect_count
    assert_equal 200, c.response_code

    assert did_call_redirect, "we should have called on_redirect"

    c.url = 'http://127.0.0.1:9999/test'
    c.perform
    assert_equal 0, c.redirect_count
    assert_equal 200, c.response_code

    puts "checking for raise support"
    did_raise = false
    begin
      c = Curl::Easy.new('http://127.0.0.1:9999/redirect_to_test')
      did_call_redirect = false
      c.on_redirect do|x|
        raise "raise"
        did_call_redirect = true
      end
      c.perform
    rescue => e
      did_raise = true
    end
    assert_equal 307, c.response_code
    assert did_raise

  end

end
