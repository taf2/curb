require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class BugRaiseOnCallback < Test::Unit::TestCase
  include BugTestServerSetupTeardown

  def setup
    @port = unused_local_port
    super
  end

  def test_on_complte
    url = "http://127.0.0.1:#{@port}/test"
    c = Curl::Easy.new(url)
    did_raise = false
    begin
      c.on_complete do|x|
        assert_equal url, x.url
        raise "error complete" # this will get swallowed
      end
      c.perform
    rescue => e
      did_raise = true
    end
    assert did_raise, "we want to raise an exception if the ruby callbacks raise"

  end

end

#test_on_debug
