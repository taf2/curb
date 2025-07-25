require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class BugIssueNoproxy < Test::Unit::TestCase
  def test_noproxy_option_support
    # Test that CURLOPT_NOPROXY constant is defined
    assert Curl.const_defined?(:CURLOPT_NOPROXY), "CURLOPT_NOPROXY constant should be defined"
    
    # Test basic noproxy setting using setopt
    c = Curl::Easy.new('https://google.com')
    
    # This should not raise an error
    assert_nothing_raised do
      c.setopt(Curl::CURLOPT_NOPROXY, "localhost,127.0.0.1")
    end
    
    # Test using the convenience method if it exists
    if c.respond_to?(:noproxy=)
      assert_nothing_raised do
        c.noproxy = "localhost,127.0.0.1"
      end
      
      # Test getter if it exists
      if c.respond_to?(:noproxy)
        assert_equal "localhost,127.0.0.1", c.noproxy
      end
    end
  end
  
  def test_noproxy_with_set_method
    c = Curl::Easy.new('https://google.com')
    
    # The issue specifically mentions using the set method
    # This currently raises TypeError as reported in the issue
    assert_nothing_raised(TypeError) do
      c.set(:noproxy, "localhost,127.0.0.1")
    end
  end
  
  def test_noproxy_empty_string
    c = Curl::Easy.new('https://google.com')
    
    # Test setting empty string to override environment variables
    assert_nothing_raised do
      c.setopt(Curl::CURLOPT_NOPROXY, "")
    end
  end
  
  def test_noproxy_nil_value
    c = Curl::Easy.new('https://google.com')
    
    # Test setting nil to reset
    assert_nothing_raised do
      c.setopt(Curl::CURLOPT_NOPROXY, nil)
    end
  end
end