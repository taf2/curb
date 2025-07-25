require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))
require 'json'

class BugIssuePostRedirect < Test::Unit::TestCase
  include BugTestServerSetupTeardown

  def setup
    @port = 9998
    super
    # Mount a POST endpoint that returns a redirect
    @server.mount_proc("/post_redirect") do |req, res|
      if req.request_method == "POST"
        res.status = 302
        res['Location'] = "http://127.0.0.1:#{@port}/redirected"
        res.body = "Redirecting..."
      else
        res.status = 405
        res.body = "Method not allowed"
      end
    end

    # Mount the redirect target
    @server.mount_proc("/redirected") do |req, res|
      res.status = 200
      res['Content-Type'] = "text/plain"
      res.body = "You have been redirected"
    end
  end

  def test_post_with_max_redirects_zero_should_not_follow_redirect
    # Test case replicating the issue: POST with max_redirects=0 and follow_location=false
    # should NOT trigger on_redirect callback or follow the redirect
    
    redirect_called = false
    
    handle = Curl::Easy.new("http://127.0.0.1:#{@port}/post_redirect") do |curl|
      curl.max_redirects = 0
      curl.follow_location = false
      curl.on_redirect do |easy|
        redirect_called = true
      end
      curl.headers['Content-Type'] = 'application/json'
      curl.post_body = {test: "data"}.to_json
    end
    
    handle.http(:POST)
    
    # The response should be the redirect response (302)
    assert_equal 302, handle.response_code
    assert_match(/Redirecting/, handle.body_str)
    
    # on_redirect should NOT be called when follow_location is false
    assert !redirect_called, "on_redirect callback should not be called when follow_location is false"
  end

  def test_post_with_follow_location_true_triggers_redirect
    # Test that on_redirect IS called when follow_location is true
    redirect_called = false
    
    handle = Curl::Easy.new("http://127.0.0.1:#{@port}/post_redirect") do |curl|
      curl.follow_location = true
      curl.on_redirect do |easy|
        redirect_called = true
      end
      curl.headers['Content-Type'] = 'application/json'
      curl.post_body = {test: "data"}.to_json
    end
    
    handle.http(:POST)
    
    # Should follow the redirect and get the final response
    assert_equal 200, handle.response_code
    assert_match(/You have been redirected/, handle.body_str)
    
    # on_redirect SHOULD be called when follow_location is true
    assert redirect_called, "on_redirect callback should be called when follow_location is true"
  end

  def test_curl_post_class_method_respects_redirect_settings
    # Test that Curl.post (class method) respects redirect settings
    # According to the issue, this works correctly
    
    response = Curl.post("http://127.0.0.1:#{@port}/post_redirect", {test: "data"}.to_json) do |curl|
      curl.max_redirects = 0
      curl.follow_location = false
      curl.headers['Content-Type'] = 'application/json'
    end
    
    # Should get the redirect response, not follow it
    assert_equal 302, response.response_code
    assert_match(/Redirecting/, response.body_str)
  end
end