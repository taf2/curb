require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class BugCrashOnDebug < Test::Unit::TestCase
  include BugTestServerSetupTeardown

  def test_on_progress_raise
    c = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
    c.on_progress do|x|
      raise "error"
    end
    c.perform

    assert false, "should not reach this point"

  rescue => e
    assert_equal 'Curl::Err::AbortedByCallbackError', e.class.to_s
    c.close
  end

  def test_on_progress_abort
    # see: https://github.com/taf2/curb/issues/192,
    # to pass:
    # 
    # c = Curl::Easy.new('http://127.0.0.1:9999/test')
    # c.on_progress do|x|
    #   puts "we're in the progress callback"
    #   false
    # end
    # c.perform
    # 
    #  notice no return keyword
    #
    c = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
    did_progress = false
    c.on_progress do|x|
      did_progress = true
      return false
    end
    c.perform
    assert did_progress

    assert false, "should not reach this point"

  rescue => e
    assert_equal 'Curl::Err::AbortedByCallbackError', e.class.to_s
    c.close
  end

end
