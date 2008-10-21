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

  def self.port
    9129
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

  def do_POST(req,res)
    respond_with(:POST,req,res)
  end

  def do_PUT(req,res)
    respond_with(:PUT,req,res)
  end

  def do_DELETE(req,res)
    respond_with(:DELETE,req,res)
  end

end
