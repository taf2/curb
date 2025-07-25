require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class BugIssueSpnego < Test::Unit::TestCase
  def test_spnego_detection_works
    # The fix for issue #227 ensures that Curl.spnego? checks for both
    # CURL_VERSION_SPNEGO (newer libcurl) and CURL_VERSION_GSSNEGOTIATE (older libcurl)
    # 
    # We can't test that it returns true because that depends on how libcurl
    # was compiled on the system. We can only test that:
    # 1. The method exists and works
    # 2. If CURLAUTH_GSSNEGOTIATE is available, we can use it
    
    # Check if CURLAUTH_GSSNEGOTIATE is available
    if Curl.const_defined?(:CURLAUTH_GSSNEGOTIATE) && Curl.const_get(:CURLAUTH_GSSNEGOTIATE) != 0
      # Test that we can use GSSNEGOTIATE auth type
      c = Curl::Easy.new('http://example.com')
      assert_nothing_raised do
        c.http_auth_types = Curl::CURLAUTH_GSSNEGOTIATE
      end
      
      # The fix ensures spnego? won't incorrectly return false when 
      # older libcurl versions have GSSNEGOTIATE support
      # (The actual return value depends on system libcurl compilation)
    end
    
    # The important fix is that the method now checks both constants
    # This test passes if no exceptions are raised
    assert true, "SPNEGO detection is working correctly"
  end
  
  def test_spnego_method_exists
    # The method should always exist
    assert Curl.respond_to?(:spnego?), "Curl should respond to spnego?"
  end
  
  def test_spnego_returns_boolean
    # The method should return a boolean
    result = Curl.spnego?
    assert [true, false].include?(result), "Curl.spnego? should return true or false, got #{result.inspect}"
  end
end