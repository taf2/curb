require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

require 'webrick'
class ::WEBrick::HTTPServer ; def access_log(config, req, res) ; end ; end
class ::WEBrick::BasicLog ; def log(level, data) ; end ; end

require 'curl'

class BugCrashOnDebug < Test::Unit::TestCase

  def test_on_debug
    server = WEBrick::HTTPServer.new( :Port => 9999 )
    server.mount_proc("/test") do|req,res|
      res.body = "hi"
      res['Content-Type'] = "text/html"
    end
    puts 'a'
    thread = Thread.new(server) do|srv|
      srv.start
    end
    puts 'b'
    c = Curl::Easy.new('http://127.0.0.1:9999/test')
    c.on_debug do|x|
      puts x.inspect
      raise "error" # this will get swallowed
    end
    c.perform
    puts 'c'
  ensure
    puts 'd'
    server.shutdown
    puts 'e'
    puts thread.exit
    puts 'f'
  end

end

#test_on_debug
