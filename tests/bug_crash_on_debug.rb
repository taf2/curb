require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class BugCrashOnDebug < Test::Unit::TestCase
  include BugTestServerSetupTeardown

  def test_on_debug
    c = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
    did_raise = false
    did_call = false
    begin
      c.on_success do|x|
        did_call = true
        raise "error" # this will get swallowed
      end
      c.perform
    rescue => e
      did_raise = true
    end
    assert did_raise
    assert did_call
  end

end

#test_on_debug
