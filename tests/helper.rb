# DO NOT REMOVE THIS COMMENT - PART OF TESTMODEL.
# Copyright (c)2006 Ross Bamford. See LICENSE.
$CURB_TESTING = true
require 'uri'

$TOPDIR = File.expand_path(File.join(File.dirname(__FILE__), '..'))
$EXTDIR = File.join($TOPDIR, 'ext')
$LIBDIR = File.join($TOPDIR, 'lib')
$:.unshift($LIBDIR)
$:.unshift($EXTDIR)

require 'curb'
require 'test/unit'

$TEST_URL = "file://#{URI.escape(File.expand_path(__FILE__).tr('\\','/').tr(':','|'))}"

require 'thread'
require 'webrick'

# keep webrick quiet
class ::WEBrick::HTTPServer
  def access_log(config, req, res)
    # nop
  end
end
class ::WEBrick::BasicLog
  def log(level, data)
    # nop
  end
end

#
# Simple test server to record number of times a request is sent/recieved of a specific 
# request type, e.g. GET,POST,PUT,DELETE
#
class TestServlet < WEBrick::HTTPServlet::AbstractServlet

  def self.port=(p)
    @port = p
  end

  def self.port
    (@port or 9129)
  end

  def self.path
    '/methods'
  end

  def self.url
    "http://127.0.0.1:#{port}#{path}"
  end

  def respond_with(method,req,res)
    res.body = method.to_s
    res['Content-Type'] = "text/plain"
  end

  def do_GET(req,res)
    respond_with(:GET,req,res)
  end

  def do_HEAD(req,res)
    res['Location'] = "/nonexistent"
    respond_with(:HEAD, req, res)
  end

  def do_POST(req,res)
    respond_with(:POST,req,res)
  end

  def do_PUT(req,res)
    respond_with("PUT\n#{req.body}",req,res)
  end

  def do_DELETE(req,res)
    respond_with(:DELETE,req,res)
  end

end

module TestServerMethods
  def locked_file
    File.join(File.dirname(__FILE__),"server_lock-#{@__port}")
  end

  def server_setup(port=9129,servlet=TestServlet)
    @__port = port
    if @server.nil? and !File.exist?(locked_file)
      File.open(locked_file,'w') {|f| f << 'locked' }

      # start up a webrick server for testing delete 
      @server = WEBrick::HTTPServer.new :Port => port, :DocumentRoot => File.expand_path(File.dirname(__FILE__))

      @server.mount(servlet.path, servlet)
      queue = Queue.new # synchronize the thread startup to the main thread

      @test_thread = Thread.new { queue << 1; @server.start }

      # wait for the queue
      value = queue.pop
      if !value
        STDERR.puts "Failed to startup test server!"
        exit(1)
      end

      exit_code = lambda do
        begin
          #puts "stopping"
          File.unlink locked_file if File.exist?(locked_file)
          @server.shutdown unless @server.nil?
        rescue Object => e
          puts "Error #{__FILE__}:#{__LINE__}\n#{e.message}"
        end
      end

      trap("INT"){exit_code.call}
      at_exit{exit_code.call}

    end
  end
end
