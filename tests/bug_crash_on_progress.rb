require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))
require 'webrick'
class ::WEBrick::HTTPServer ; def access_log(config, req, res) ; end ; end
class ::WEBrick::BasicLog ; def log(level, data) ; end ; end

class BugCrashOnDebug < Test::Unit::TestCase
  
  def test_on_debug
    server = WEBrick::HTTPServer.new( :Port => 9999 )
    server.mount_proc("/test") do|req,res|
      res.body = "hi"
      res['Content-Type'] = "text/html"
    end

    thread = Thread.new(server) do|srv|
      srv.start
    end

    c = Curl::Easy.new('http://127.0.0.1:9999/test')
    c.on_progress do|x|
      raise "error"
    end
    c.perform

    assert false, "should not reach this point"

  rescue => e
    assert_equal 'Curl::Err::AbortedByCallbackError', e.class.to_s
    c.close
  ensure
    server.shutdown
  end
end
