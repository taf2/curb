# DO NOT REMOVE THIS COMMENT - PART OF TESTMODEL.
# Copyright (c)2006 Ross Bamford. See LICENSE.
$CURB_TESTING = true
require 'uri'
require 'stringio'
require 'digest/md5'
require 'rbconfig'
require File.join(RbConfig::CONFIG['rubylibdir'], 'timeout')

$TOPDIR = File.expand_path(File.join(File.dirname(__FILE__), '..'))
$EXTDIR = File.join($TOPDIR, 'ext')
$LIBDIR = File.join($TOPDIR, 'lib')
$:.unshift($LIBDIR)
$:.unshift($EXTDIR)

# Setup SimpleCov for Ruby code coverage if COVERAGE env var is set
if ENV['COVERAGE']
  begin
    require 'simplecov'
    require 'simplecov-lcov'
    
    SimpleCov::Formatter::LcovFormatter.config.report_with_single_file = true
    SimpleCov.formatter = SimpleCov::Formatter::MultiFormatter.new([
      SimpleCov::Formatter::HTMLFormatter,
      SimpleCov::Formatter::LcovFormatter
    ])
    
    SimpleCov.start do
      add_filter '/tests/'
      add_filter '/spec/'
      add_filter '/vendor/'
      add_filter '/.bundle/'
      add_group 'Library', 'lib'
      add_group 'Extensions', 'ext'
      
      # Track branch coverage if available
      enable_coverage :branch if respond_to?(:enable_coverage)
    end
  rescue LoadError
    puts "SimpleCov not available. Install it with: gem install simplecov simplecov-lcov"
  end
end

require 'curb'
begin
  require 'test/unit'
rescue LoadError
  gem 'test/unit'
  require 'test/unit'
end
require 'fileutils'
require 'rbconfig'

# Platform helpers
WINDOWS = /mswin|msys|mingw|cygwin|bccwin|wince|emc|windows/i.match?(RbConfig::CONFIG['host_os'])
NO_FORK = !Process.respond_to?(:fork)

$TEST_URL = "file://#{'/' if RUBY_DESCRIPTION =~ /mswin|msys|mingw|cygwin|bccwin|wince|emc/}#{File.expand_path(__FILE__).tr('\\','/')}"

require 'thread'
require 'webrick'
require 'socket'

# set this to true to avoid testing with multiple threads
# or to test with multiple threads set it to false
# this is important since, some code paths will change depending
# on the presence of multiple threads
TEST_SINGLE_THREADED=false

WEBRICK_TEST_LOG = WEBrick::Log.new(File.open(File::NULL, 'w'), WEBrick::BasicLog::ERROR)

module CurbTestResourceCleanup
  def teardown
    super
  ensure
    begin
      if Curl::Easy.respond_to?(:flush_deferred_multi_closes)
        Curl::Easy.flush_deferred_multi_closes(all_threads: true)
      end
    rescue StandardError
      nil
    end

    begin
      ObjectSpace.each_object(Curl::Multi) do |multi|
        begin
          next if multi.instance_variable_defined?(:@deferred_close) && multi.instance_variable_get(:@deferred_close)
          multi.instance_variable_set(:@requests, {})
          multi._close
        rescue StandardError
          nil
        end
      end
    rescue StandardError
      nil
    end

  end
end

Test::Unit::TestCase.prepend(CurbTestResourceCleanup)

#
# Simple test server to record number of times a request is sent/recieved of a specific
# request type, e.g. GET,POST,PUT,DELETE
#
class TestServlet < WEBrick::HTTPServlet::AbstractServlet

  def self.port=(p)
    @port = p
  end

  def self.port
    @port ||= 9129
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
    elsif req.path.match(/get_cookies$/)
      res['Content-Type'] = "text/plain"
      res.body = req['Cookie']
      return
    end
    respond_with("GET#{req.query_string}",req,res)
  end

  def do_HEAD(req,res)
    res['Location'] = "/nonexistent"
    respond_with("HEAD#{req.query_string}",req,res)
  end

  def do_POST(req,res)
    if req.path.match(/set_cookies$/)
      JSON.parse(req.body || '[]', symbolize_names: true).each do |hash|
        cookie = WEBrick::Cookie.new(hash.fetch(:name), hash.fetch(:value))
        cookie.domain = hash[:domain] if hash.key?(:domain)
        cookie.expires = hash[:expires] if hash.key?(:expires)
        cookie.path = hash[:path] if hash.key?(:path)
        cookie.secure = hash[:secure] if hash.key?(:secure)
        cookie.max_age = hash[:max_age] if hash.key?(:max_age)
        res.cookies.push(cookie)
      end
      respond_with('OK', req, res)
    elsif req.query['filename'].nil?
      if req.body
        params = {}
        req.body.split('&').map{|s| k,v=s.split('='); params[k] = v }
      end
      if params and params['s'] == '500'
        res.status = 500
      elsif params and params['c']
        cookie = URI.decode_www_form_component(params['c']).split('=')
        res.cookies << WEBrick::Cookie.new(*cookie)
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
    respond_with("PATCH\n#{req.body}",req,res)
  end

  def do_OPTIONS(req,res)
    respond_with("OPTIONS#{req.query_string}",req,res)
  end

end

module BugTestServerSetupTeardown
  def setup
    @port ||= 9992
    @server = WEBrick::HTTPServer.new(:Port => @port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    @server.mount_proc("/test") do|req,res|
      if @response_proc
        @response_proc.call(res)
      else
        res.body = "hi"
        res['Content-Type'] = "text/html"
      end
    end

    @thread = Thread.new(@server) do|srv|
      srv.start
    end
  end

  def teardown
    while @server.status != :Shutdown
      @server.shutdown
    end
    @thread.join
  end
end

module TestServerMethods
  def server_responding?(port)
    socket = TCPSocket.new('127.0.0.1', port)
    socket.close
    true
  rescue Errno::ECONNREFUSED, Errno::ECONNRESET, Errno::ECONNABORTED, Errno::EHOSTUNREACH, Errno::ETIMEDOUT, IOError
    false
  end

  def server_startup_timeout
    ENV['RUBY_MEMCHECK_RUNNING'] ? 30 : 5
  end

  def wait_for_server_ready(port, thread: nil)
    deadline = Process.clock_gettime(Process::CLOCK_MONOTONIC) + server_startup_timeout

    loop do
      if thread && !thread.alive?
        return false
      end

      return true if server_responding?(port)

      raise "Failed to startup test server on port #{port}" if Process.clock_gettime(Process::CLOCK_MONOTONIC) >= deadline

      sleep 0.01
    end
  end

  def server_shutdown_timeout
    [server_startup_timeout, 0.25].max
  end

  def wait_for_server_stopped(port, thread: nil)
    deadline = Process.clock_gettime(Process::CLOCK_MONOTONIC) + server_shutdown_timeout

    loop do
      return true unless server_responding?(port)

      if thread && !thread.alive?
        return !server_responding?(port)
      end

      return false if Process.clock_gettime(Process::CLOCK_MONOTONIC) >= deadline

      sleep 0.01
    end
  end

  def locked_file
    File.join(File.dirname(__FILE__),"server_lock-#{@__port}")
  end

  def read_server_lock_pid
    Integer(File.read(locked_file).strip, 10)
  rescue Errno::ENOENT, ArgumentError, TypeError
    nil
  end

  def write_server_lock(pid = Process.pid)
    File.open(locked_file, 'w') { |f| f << "#{pid}\n" }
  end

  def process_alive?(pid)
    return false unless pid && pid.positive?

    Process.kill(0, pid)
    true
  rescue Errno::ESRCH
    false
  rescue Errno::EPERM
    true
  end

  def server_lock_fresh?
    (Time.now - File.mtime(locked_file)) < server_startup_timeout
  rescue Errno::ENOENT
    false
  end

  def stale_server_lock?(port)
    return false unless File.exist?(locked_file)

    pid = read_server_lock_pid
    return false if pid && process_alive?(pid) && server_lock_fresh?
    return false if server_responding?(port)

    true
  end

  def clear_stale_server_lock(port)
    return unless stale_server_lock?(port)

    File.unlink(locked_file)
  rescue Errno::ENOENT
    nil
  end

  def stop_test_server
    server = instance_variable_defined?(:@server) ? @server : nil
    pid = instance_variable_defined?(:@__pid) ? @__pid : nil
    return unless server || pid

    if TEST_SINGLE_THREADED
      @__pid = nil

      if pid
        begin
          Process.kill('INT', pid)
        rescue Errno::ESRCH
          nil
        end

        begin
          Process.wait(pid)
        rescue Errno::ECHILD, Errno::ESRCH
          nil
        end
      end
    else
      thread = instance_variable_defined?(:@test_thread) ? @test_thread : nil
      port = instance_variable_defined?(:@__port) ? @__port : nil
      @server = nil
      @test_thread = nil

      server.shutdown if server
      wait_for_server_stopped(port, thread: thread) if port
      thread.join(server_shutdown_timeout) if thread

      if thread&.alive?
        thread.kill
        thread.join(server_shutdown_timeout)
        wait_for_server_stopped(port) if port
      end
    end

    File.unlink(locked_file) if File.exist?(locked_file)
  rescue Errno::ENOENT
    nil
  end

  def teardown
    super
  ensure
    stop_test_server
  end

  def server_setup(port=9129,servlet=TestServlet)
    @__port = port
    @server = nil unless instance_variable_defined?(:@server)
    @__pid = nil unless instance_variable_defined?(:@__pid)
    @test_thread = nil unless instance_variable_defined?(:@test_thread)
    clear_stale_server_lock(port)

    if @server.nil? && File.exist?(locked_file)
      begin
        wait_for_server_ready(port)
        return
      rescue RuntimeError
        clear_stale_server_lock(port)
      end
    end

    if @server.nil? and !File.exist?(locked_file)
      write_server_lock
      if TEST_SINGLE_THREADED
        rd, wr = IO.pipe
        @__pid = fork do
          rd.close
          rd = nil

          # start up a webrick server for testing delete
          server = WEBrick::HTTPServer.new(:Port => port, :DocumentRoot => File.expand_path(File.dirname(__FILE__)), :Logger => WEBRICK_TEST_LOG, :AccessLog => [])

          server.mount(servlet.path, servlet)
          server.mount("/ext", WEBrick::HTTPServlet::FileHandler, File.join(File.dirname(__FILE__),'..','ext'))

          trap("INT") { server.shutdown }
          GC.start
          server_thread = Thread.new { server.start }

          begin
            if wait_for_server_ready(port, thread: server_thread)
              wr.write('1')
            else
              wr.write('0')
            end
          rescue StandardError
            wr.write('0')
          ensure
            wr.flush
            wr.close
          end

          server_thread.join
        end
        wr.close
        ready = rd.read
        rd.close
        if ready != '1'
          STDERR.puts "Failed to startup test server!"
          exit(1)
        end
      else
        # start up a webrick server for testing delete
        server = WEBrick::HTTPServer.new(:Port => port, :DocumentRoot => File.expand_path(File.dirname(__FILE__)), :Logger => WEBRICK_TEST_LOG, :AccessLog => [])

        server.mount(servlet.path, servlet)
        server.mount("/ext", WEBrick::HTTPServlet::FileHandler, File.join(File.dirname(__FILE__),'..','ext'))

        @server = server
        # Keep a stable reference inside the thread so helper shutdown can clear
        # @server without racing the server startup path.
        @test_thread = Thread.new(server) { |srv| srv.start }

        if !wait_for_server_ready(port, thread: @test_thread)
          STDERR.puts "Failed to startup test server!"
          exit(1)
        end

      end

      exit_code = lambda do
        begin
          stop_test_server
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



# Backport for Ruby 1.8
module Backports
  module Ruby18
    module URIFormEncoding
      TBLENCWWWCOMP_ = {}
      TBLDECWWWCOMP_ = {}

      def encode_www_form_component(str)
        if TBLENCWWWCOMP_.empty?
          256.times do |i|
            TBLENCWWWCOMP_[i.chr] = '%%%02X' % i
          end
          TBLENCWWWCOMP_[' '] = '+'
          TBLENCWWWCOMP_.freeze
        end
        str.to_s.gsub( /([^*\-.0-9A-Z_a-z])/ ) {|*| TBLENCWWWCOMP_[$1] }
      end

      def decode_www_form_component(str)
        if TBLDECWWWCOMP_.empty?
          256.times do |i|
            h, l = i>>4, i&15
            TBLDECWWWCOMP_['%%%X%X' % [h, l]] = i.chr
            TBLDECWWWCOMP_['%%%x%X' % [h, l]] = i.chr
            TBLDECWWWCOMP_['%%%X%x' % [h, l]] = i.chr
            TBLDECWWWCOMP_['%%%x%x' % [h, l]] = i.chr
          end
          TBLDECWWWCOMP_['+'] = ' '
          TBLDECWWWCOMP_.freeze
        end

        raise ArgumentError, "invalid %-encoding (#{str.dump})" unless /\A(?:%[[:xdigit:]]{2}|[^%]+)*\z/ =~ str
        str.gsub( /(\+|%[[:xdigit:]]{2})/ ) {|*| TBLDECWWWCOMP_[$1] }
      end

      def encode_www_form( enum )
        enum.map do |k,v|
          if v.nil?
            encode_www_form_component(k)
          elsif v.respond_to?(:to_ary)
            v.to_ary.map do |w|
              str = encode_www_form_component(k)
              unless w.nil?
                str << '='
                str << encode_www_form_component(w)
              end
            end.join('&')
          else
            str = encode_www_form_component(k)
            str << '='
            str << encode_www_form_component(v)
          end
        end.join('&')
      end

      WFKV_ = '(?:%\h\h|[^%#=;&])'
      def decode_www_form(str, _)
        return [] if str.to_s == ''

        unless /\A#{WFKV_}=#{WFKV_}(?:[;&]#{WFKV_}=#{WFKV_})*\z/ =~ str
          raise ArgumentError, "invalid data of application/x-www-form-urlencoded (#{str})"
        end
        ary = []
        $&.scan(/([^=;&]+)=([^;&]*)/) do
          ary << [decode_www_form_component($1, enc), decode_www_form_component($2, enc)]
        end
        ary
      end
    end
  end
end

unless URI.methods.include?(:encode_www_form)
  URI.extend(Backports::Ruby18::URIFormEncoding)
end
