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
require 'addressable'
begin
  require 'test/unit'
rescue LoadError
  gem 'test/unit'
  require 'test/unit'
end
require 'fileutils'

$TEST_URL = "file://#{Addressable::URI::escape(File.expand_path(__FILE__).tr('\\','/').tr(':','|'))}"

require 'thread'
require 'webrick'

# set this to true to avoid testing with multiple threads
# or to test with multiple threads set it to false
# this is important since, some code paths will change depending
# on the presence of multiple threads
TEST_SINGLE_THREADED=false

# keep webrick quiet
if Gem::Dependency.new('', '>= 2.1.0').match?('', RUBY_VERSION)
  module SilenceAccessLog; def access_log(config, req, res); end; end
  ::WEBrick::HTTPServer.prepend SilenceAccessLog

  module SilenceLog; def log(level, data); end; end
  ::WEBrick::BasicLog.prepend SilenceLog
else
  module SilenceAccessLog
    def self.included(base)
      base.class_eval do
        alias_method_chain :access_log, :silent
       end
    end

    def access_log_with_silent(config, req, res)
    end
  end

  ::WEBrick::HTTPServer.send(:include, SilenceAccessLog)

  module SilenceLog
    def self.included(base)
      base.class_eval do
        alias_method_chain :log, :silent
       end
    end

    def log_with_silent(level, data)
    end
  end

  ::WEBrick::BasicLog.send(:include, SilenceLog)
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
    defined?(@port) ? @port : 9129
  end

  def self.path
    '/methods'
  end

  def self.url
    "http://127.0.0.1:#{port}#{path}"
  end

  def respond_with(method,req,res)
    res.body = method.to_s
    $auth_header = req['Authorization']
    res['Content-Type'] = "text/plain"
  end

  def do_GET(req,res)
    if req.path.match(/redirect$/)
      res.status = 302
      res['Location'] = '/foo'
    elsif req.path.match(/not_here$/)
      res.status = 404
    elsif req.path.match(/error$/)
      res.status = 500
    end
    respond_with("GET#{req.query_string}",req,res)
  end

  def do_HEAD(req,res)
    res['Location'] = "/nonexistent"
    respond_with("HEAD#{req.query_string}",req,res)
  end

  def do_POST(req,res)
    if req.query['filename'].nil?
      if req.body
        params = {}
        req.body.split('&').map{|s| k,v=s.split('='); params[k] = v }
      end
      if params and params['s'] == '500'
        res.status = 500
      else
        respond_with("POST\n#{req.body}",req,res)
      end
    else
      respond_with(req.query['filename'],req,res)
    end
  end

  def do_PUT(req,res)
    res['X-Requested-Content-Type'] = req.content_type
    respond_with("PUT\n#{req.body}",req,res)
  end

  def do_DELETE(req,res)
    respond_with("DELETE#{req.query_string}",req,res)
  end

  def do_PURGE(req,res)
    respond_with("PURGE#{req.query_string}",req,res)
  end

  def do_COPY(req,res)
    respond_with("COPY#{req.query_string}",req,res)
  end

  def do_PATCH(req,res)
    res['X-Requested-Content-Type'] = req.content_type
    respond_with("PATCH\n#{req.body}",req,res)
  end

  def do_OPTIONS(req,res)
    respond_with("OPTIONS#{req.query_string}",req,res)
  end

end

module TestServerMethods
  def locked_file
    File.join(File.dirname(__FILE__),"server_lock-#{@__port}")
  end

  def server_setup(port=9129,servlet=TestServlet)
    @__port = port
    if not(defined?(@server)) and !File.exist?(locked_file)

      File.open(locked_file,'w') {|f| f << 'locked' }
      if TEST_SINGLE_THREADED
        rd, wr = IO.pipe
        @__pid = fork do
          rd.close
          rd = nil

          # start up a webrick server for testing delete
          server = WEBrick::HTTPServer.new :Port => port, :DocumentRoot => File.expand_path(File.dirname(__FILE__))

          server.mount(servlet.path, servlet)
          server.mount("/ext", WEBrick::HTTPServlet::FileHandler, File.join(File.dirname(__FILE__),'..','ext'))

          trap("INT") { server.shutdown }
          GC.start
          wr.flush
          wr.close
          server.start
        end
        wr.close
        rd.read
        rd.close
      else
        # start up a webrick server for testing delete
        @server = WEBrick::HTTPServer.new :Port => port, :DocumentRoot => File.expand_path(File.dirname(__FILE__))

        @server.mount(servlet.path, servlet)
        @server.mount("/ext", WEBrick::HTTPServlet::FileHandler, File.join(File.dirname(__FILE__),'..','ext'))
        queue = Queue.new # synchronize the thread startup to the main thread

        @test_thread = Thread.new { queue << 1; @server.start }

        # wait for the queue
        value = queue.pop
        if !value
          STDERR.puts "Failed to startup test server!"
          exit(1)
        end

      end

      exit_code = lambda do
        begin
          if File.exist?(locked_file)
            File.unlink locked_file
            if TEST_SINGLE_THREADED
              Process.kill 'INT', @__pid
            else
              @server.shutdown unless @server.nil?
            end
          end
          #@server.shutdown unless @server.nil?
        rescue Object => e
          puts "Error #{__FILE__}:#{__LINE__}\n#{e.message}"
        end
      end

      trap("INT"){exit_code.call}
      at_exit{exit_code.call}

    end
  rescue Errno::EADDRINUSE
  end
end
