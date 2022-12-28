require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

require 'webrick'
class ::WEBrick::HTTPServer ; def access_log(config, req, res) ; end ; end
class ::WEBrick::BasicLog ; def log(level, data) ; end ; end

require 'curl'

class BugRaiseOnCallback < Test::Unit::TestCase

  def test_on_complte
    server = WEBrick::HTTPServer.new( :Port => 9999 )
    server.mount_proc("/test") do|req,res|
      res.body = "hi"
      res['Content-Type'] = "text/html"
    end
    thread = Thread.new(server) do|srv|
      srv.start
    end
    c = Curl::Easy.new('http://127.0.0.1:9999/test')
    did_raise = false
    begin
      c.on_complete do|x|
        assert_equal 'http://127.0.0.1:9999/test', x.url
        raise "error complete" # this will get swallowed
      end
      c.perform
    rescue => e
      did_raise = true
    end
    assert did_raise, "we want to raise an exception if the ruby callbacks raise"

  ensure
    server.shutdown
    thread.exit
  end

end

#test_on_debug
