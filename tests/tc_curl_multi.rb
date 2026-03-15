require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))
require 'set'
require 'tmpdir'

class TestCurbCurlMulti < Test::Unit::TestCase
  def teardown
    # get a better read on memory loss when running in valgrind
    ObjectSpace.garbage_collect
    super
  end

  # for https://github.com/taf2/curb/issues/277
  # must connect to an external
  def test_connection_keepalive
    omit('Not supported on Windows runners') if WINDOWS
    # this test fails with libcurl 7.22.0. I didn't investigate, but it may be related
    # to CURLOPT_MAXCONNECTS bug fixed in 7.30.0:
    # https://github.com/curl/curl/commit/e87e76e2dc108efb1cae87df496416f49c55fca0
    omit("Skip, libcurl too old (< 7.22.0)") if Curl::CURL_VERSION.to_f < 8 && Curl::CURL_VERSION.split('.')[1].to_i <= 22

    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    server = WEBrick::HTTPServer.new(:Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    server.mount_proc(TestServlet.path) do |req, res|
      res.body = "GET#{req.query_string}"
      res['Content-Type'] = 'text/plain'
    end
    server_thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: server_thread)

    test_url = "http://127.0.0.1:#{port}#{TestServlet.path}"
    Curl::Multi.autoclose = true
    assert Curl::Multi.autoclose

    if `which ss`.strip.size == 0
      # osx need lsof still :(
      open_fds = lambda do
        out = `/usr/sbin/lsof -nP -a -p #{Process.pid} -iTCP:#{port} -sTCP:ESTABLISHED`
        [out.lines.drop(1).size, 0].max
      end
    else
      ss = `which ss`.strip
      open_fds = lambda do
        `#{ss} -tn4 state established dport = :#{port} | wc -l`.strip.to_i
      end
    end
    Curl::Multi.autoclose = false
    before_open = open_fds.call
    #puts "before_open: #{before_open.inspect}"
    assert !Curl::Multi.autoclose
    multi = Curl::Multi.new
    multi.max_connects = 1 # limit to 1 connection within the multi handle

    did_complete = false
    5.times do |n|
      easy = Curl::Easy.new(test_url) do |curl|
        curl.timeout = 5 # ensure we don't hang for ever connecting to an external host
        curl.on_complete {
          did_complete = true
        }
      end
      multi.add(easy)
    end

    multi.perform
    assert did_complete
    after_open = open_fds.call
    #puts "after_open: #{after_open} before_open: #{before_open.inspect}"
    # Some CI/libcurl combinations tear down the loopback connection before
    # ss/lsof observes it, so only assert that the multi cache does not retain
    # more than one extra connection here.
    assert (after_open - before_open) < 3, "with max connections set to 1 the multi handle should not retain more than one extra connection"
    multi.close

    after_open = open_fds.call
    #puts "after_open: #{after_open} before_open: #{before_open.inspect}"
    assert_equal 0, (after_open - before_open), "after closing the multi handle all connections should be closed"

    Curl::Multi.autoclose = true
    multi = Curl::Multi.new
    did_complete = false
    5.times do |n|
      easy = Curl::Easy.new(test_url) do |curl|
        curl.timeout = 5 # ensure we don't hang for ever connecting to an external host
        curl.on_complete {
          did_complete = true
        }
      end
      multi.add(easy)
    end

    multi.perform
    assert did_complete
    after_open = open_fds.call
    #puts "after_open: #{after_open} before_open: #{before_open.inspect}"
    assert_equal 0, (after_open - before_open), "auto close the connections"
  ensure
    server.shutdown if defined?(server) && server
    server_thread.join(server_startup_timeout) if defined?(server_thread) && server_thread
    server_thread.kill if defined?(server_thread) && server_thread&.alive?
    Curl::Multi.autoclose = false # restore default
  end

  def test_connection_autoclose
    assert !Curl::Multi.autoclose
    Curl::Multi.autoclose = true
    assert Curl::Multi.autoclose
  ensure
    Curl::Multi.autoclose = false # restore default
  end

  def test_perform_can_reuse_multi_when_autoclose_is_enabled
    Curl::Multi.autoclose = true
  
    multi = Curl::Multi.new
    results = []
  
    first = Curl::Easy.new(TestServlet.url)
    first.on_complete { results << first.code }
    multi.add(first)
    multi.perform
  
    second = Curl::Easy.new(TestServlet.url)
    second.on_complete { results << second.code }
    multi.add(second)
    multi.perform
  
    assert_equal [200, 200], results
  ensure
    multi.close if defined?(multi) && multi
    Curl::Multi.autoclose = false
  end

  def test_close_makes_multi_unusable
    multi = Curl::Multi.new
    multi.close
  
    error = assert_raise(Curl::Err::MultiBadHandle) do
      multi.add(Curl::Easy.new($TEST_URL))
    end
  
    assert_match(/Invalid multi handle/i, error.message)
    assert_equal 0, multi.requests.length
  ensure
    multi.close if multi
  end

  def test_new_multi_01
    d1 = String.new
    c1 = Curl::Easy.new($TEST_URL) do |curl|
      curl.headers["User-Agent"] = "myapp-0.0"
      curl.on_body {|d| d1 << d; d.length }
    end

    d2 = String.new
    c2 = Curl::Easy.new($TEST_URL) do |curl|
      curl.headers["User-Agent"] = "myapp-0.0"
      curl.on_body {|d| d2 << d; d.length }
    end

    m = Curl::Multi.new

    m.add( c1 )
    m.add( c2 )

    m.perform

    assert_match(/^# DO NOT REMOVE THIS COMMENT/, d1)
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, d2)
  ensure
    m.close if m
  end

  def test_perform_block
    c1 = Curl::Easy.new($TEST_URL)
    c2 = Curl::Easy.new($TEST_URL)

    m = Curl::Multi.new

    m.add( c1 )
    m.add( c2 )

    m.perform do
      # idle
      #puts "idling..."
    end

    assert_match(/^# DO NOT REMOVE THIS COMMENT/, c1.body_str)
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, c2.body_str)
  ensure
    m.close if m
  end

  def test_multi_easy_get
    n = 1
    urls = []
    n.times { urls << $TEST_URL }
    Curl::Multi.get(urls, {timeout: 5}) {|easy|
      assert_match(/file:/, easy.last_effective_url)
    }
  end

  def test_multi_easy_get_with_error
    omit('Path/line parsing differs on Windows') if WINDOWS
    begin
      did_raise = false
      n = 3
      urls = []
      n.times { urls << $TEST_URL }
      error_line_number_should_be = nil
      Curl::Multi.get(urls, {timeout: 5}) {|easy|
        # if we got this right the error will be reported to be on the line below our error_line_number_should_be
        error_line_number_should_be = __LINE__
        raise 
      }

    rescue Curl::Err::AbortedByCallbackError => e
      did_raise = true
      in_file = e.backtrace.detect {|err| err.match?(File.basename(__FILE__)) }
      #in_file_stack = e.backtrace.select {|err| err.match?(File.basename(__FILE__)) }
      assert_match(__FILE__, in_file)
      in_file.gsub!(__FILE__)
      parts = in_file.split(':')
      parts.shift
      line_no = parts.shift.to_i
      assert_equal error_line_number_should_be+1, line_no.to_i
    end

    assert did_raise, "we should have raised an exception"

  end

  def test_multi_perform_reraises_on_body_exception
    multi = Curl::Multi.new
    easy = Curl::Easy.new(TestServlet.url)
    easy.on_body { raise "body blew up" }

    error = assert_raise(RuntimeError) do
      multi.add(easy)
      multi.perform
    end

    assert_equal "body blew up", error.message
    assert !easy.instance_variable_defined?(:@__curb_callback_error)
  ensure
    multi.close if multi
  end

  def test_multi_perform_reraises_on_body_exception_for_frozen_easy
    multi = Curl::Multi.new
    easy = Curl::Easy.new($TEST_URL)
    callback_error = Class.new(RuntimeError)
    easy.on_body { raise callback_error, "body blew up" }
    easy.freeze

    error = assert_raise(callback_error) do
      multi.add(easy)
      multi.perform
    end

    assert_equal "body blew up", error.message
  ensure
    begin
      multi.close if multi
    rescue StandardError
      multi.instance_variable_set(:@requests, {})
      multi._close
    end
  end

  def test_multi_perform_reraises_on_header_exception
    multi = Curl::Multi.new
    easy = Curl::Easy.new(TestServlet.url)
    easy.on_header { raise "header blew up" }

    error = assert_raise(RuntimeError) do
      multi.add(easy)
      multi.perform
    end

    assert_equal "header blew up", error.message
    assert !easy.instance_variable_defined?(:@__curb_callback_error)
  ensure
    multi.close if multi
  end

  def test_multi_perform_drains_completed_siblings_before_reraising_on_body_exception
    multi = Curl::Multi.new
    failed = Curl::Easy.new($TEST_URL)
    completed = Curl::Easy.new($TEST_URL)
    completions = []

    failed.on_body { raise "body blew up" }
    completed.on_complete { completions << :completed }

    error = assert_raise(RuntimeError) do
      multi.add(failed)
      multi.add(completed)
      multi.perform
    end

    assert_equal "body blew up", error.message
    assert_equal [:completed], completions
    assert multi.idle?, 'The multi handle should be idle after draining the completed batch'
    assert_equal 0, multi.requests.length
    assert_nil failed.multi
    assert_nil completed.multi
    assert !failed.instance_variable_defined?(:@__curb_callback_error)
  ensure
    multi.close if multi
  end

  def test_multi_perform_waits_until_idle_before_reraising_on_complete_exception
    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    server = WEBrick::HTTPServer.new(:Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    server.mount_proc('/fast') do |_req, res|
      res['Content-Type'] = 'text/plain'
      res.body = 'fast'
    end
    server.mount_proc('/slow') do |_req, res|
      res['Content-Type'] = 'text/plain'
      sleep 0.2
      res.body = 'slow'
    end
    server_thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: server_thread)

    multi = Curl::Multi.new
    fast = Curl::Easy.new("http://127.0.0.1:#{port}/fast")
    slow = Curl::Easy.new("http://127.0.0.1:#{port}/slow")
    completions = []

    fast.on_complete do
      completions << :fast
      raise "complete blew up"
    end
    slow.on_complete { completions << :slow }

    error = assert_raise(Curl::Err::AbortedByCallbackError) do
      multi.add(fast)
      multi.add(slow)
      multi.perform
    end

    assert_equal "complete blew up", error.message
    assert_equal [:fast, :slow], completions
    assert multi.idle?, 'The multi handle should be idle before the deferred exception is raised'
    assert_equal 0, multi.requests.length
    assert_nil fast.multi
    assert_nil slow.multi
  ensure
    multi.close if multi
    server.shutdown if defined?(server) && server
    server_thread.join(server_startup_timeout) if defined?(server_thread) && server_thread
    server_thread.kill if defined?(server_thread) && server_thread&.alive?
  end

  def test_multi_perform_keeps_yielding_block_while_draining_deferred_on_complete_exception
    with_queue_refill_test_server do |port, hits|
      multi = Curl::Multi.new
      failed = Curl::Easy.new("http://127.0.0.1:#{port}/fail")
      slow = Curl::Easy.new("http://127.0.0.1:#{port}/slow")
      failure_seen = false
      slow_completed = false
      yielded_during_drain = false

      failed.on_complete do
        failure_seen = true
        raise "complete blew up"
      end
      slow.on_complete { slow_completed = true }

      error = assert_raise(Curl::Err::AbortedByCallbackError) do
        multi.add(failed)
        multi.add(slow)
        multi.perform do
          yielded_during_drain ||= failure_seen && !slow_completed
        end
      end

      assert_equal "complete blew up", error.message
      assert yielded_during_drain,
             "perform block should continue yielding while draining a slow sibling after a deferred callback exception"
      assert_equal 1, hits[:fail]
      assert_equal 1, hits[:slow]
    ensure
      multi.close if multi
    end
  end

  def test_multi_perform_does_not_start_queued_work_after_on_complete_exception
    with_queue_refill_test_server do |port, hits|
      multi = Curl::Multi.new
      failed = Curl::Easy.new("http://127.0.0.1:#{port}/fail")
      slow = Curl::Easy.new("http://127.0.0.1:#{port}/slow")
      queued = Curl::Easy.new("http://127.0.0.1:#{port}/queued")
      failure_seen = false

      failed.on_complete do
        failure_seen = true
        raise "complete blew up"
      end

      error = assert_raise(Curl::Err::AbortedByCallbackError) do
        multi.add(failed)
        multi.add(slow)
        multi.perform do
          next unless failure_seen

          multi.add(queued)
          failure_seen = false
        end
      end

      assert_equal "complete blew up", error.message
      assert_equal 1, hits[:fail]
      assert_equal 1, hits[:slow]
      assert_equal 0, hits[:queued], "queued request should not start once a deferred callback exception is pending"
    ensure
      multi.close if multi
    end
  end

  def test_multi_perform_does_not_start_work_added_within_failing_on_complete_callback
    with_queue_refill_test_server do |port, hits|
      multi = Curl::Multi.new
      failed = Curl::Easy.new("http://127.0.0.1:#{port}/fail")
      queued = Curl::Easy.new("http://127.0.0.1:#{port}/queued")

      failed.on_complete do
        multi.add(queued)
        raise "complete blew up"
      end

      error = assert_raise(Curl::Err::AbortedByCallbackError) do
        multi.add(failed)
        multi.perform
      end

      assert_equal "complete blew up", error.message
      assert_equal 1, hits[:fail]
      assert_equal 0, hits[:queued],
                   "request added from a failing on_complete callback should not start once the multi begins aborting"
    ensure
      multi.close if multi
    end
  end

  def test_multi_perform_does_not_restart_sibling_removed_and_readded_from_failing_on_complete_callback
    with_queue_refill_test_server(wait_fail_until_slow: true) do |port, hits|
      multi = Curl::Multi.new
      failed = Curl::Easy.new("http://127.0.0.1:#{port}/fail")
      slow = Curl::Easy.new("http://127.0.0.1:#{port}/slow")

      failed.on_complete do
        multi.remove(slow)
        multi.add(slow)
        raise "complete blew up"
      end

      error = assert_raise(Curl::Err::AbortedByCallbackError) do
        multi.add(failed)
        multi.add(slow)
        multi.perform
      end

      assert_equal "complete blew up", error.message
      assert_equal 1, hits[:fail]
      assert_equal 1, hits[:slow],
                   "sibling removed and re-added from a failing on_complete callback should be treated as replacement work and not restarted"
    ensure
      multi.close if multi
    end
  end

  def test_multi_perform_does_not_start_work_added_within_on_complete_after_on_body_exception
    with_queue_refill_test_server do |port, hits|
      multi = Curl::Multi.new
      failed = Curl::Easy.new("http://127.0.0.1:#{port}/fail")
      queued = Curl::Easy.new("http://127.0.0.1:#{port}/queued")

      failed.on_body { raise "body blew up" }
      failed.on_complete { multi.add(queued) }

      error = assert_raise(RuntimeError) do
        multi.add(failed)
        multi.perform
      end

      assert_equal "body blew up", error.message
      assert_equal 1, hits[:fail]
      assert_equal 0, hits[:queued],
                   "request added from on_complete after a body callback exception should not start once the multi begins aborting"
    ensure
      multi.close if multi
    end
  end

  def test_multi_http_does_not_fetch_queued_url_after_on_body_exception
    with_queue_refill_test_server do |port, hits|
      urls = [
        { :url => "http://127.0.0.1:#{port}/queued", :method => :get },
        { :url => "http://127.0.0.1:#{port}/slow", :method => :get },
        { :url => "http://127.0.0.1:#{port}/fail", :method => :get, :on_body => proc { raise "body blew up" } }
      ]

      error = assert_raise(RuntimeError) do
        Curl::Multi.http(urls, {:max_connects => 2}) { |_easy, _code, _method| }
      end

      assert_equal "body blew up", error.message
      assert_equal 1, hits[:fail]
      assert_equal 1, hits[:slow]
      assert_equal 0, hits[:queued], "Curl::Multi.http should not refill queued urls after a callback abort"
    end
  end

  def test_multi_perform_runs_status_callbacks_after_on_body_exception
    multi = Curl::Multi.new
    easy = Curl::Easy.new(TestServlet.url)
    callbacks = []

    easy.on_body { raise "body blew up" }
    easy.on_complete { callbacks << :complete }
    easy.on_failure do |_completed_easy, error_info|
      callbacks << :failure
      assert_equal Curl::Err::WriteError, error_info.first
    end

    error = assert_raise(RuntimeError) do
      multi.add(easy)
      multi.perform
    end

    assert_equal "body blew up", error.message
    assert_equal [:complete, :failure], callbacks
    assert multi.idle?
    assert_equal 0, multi.requests.length
  ensure
    multi.close if multi
  end

  def test_multi_perform_preserves_on_body_exception_when_status_callbacks_raise
    multi = Curl::Multi.new
    easy = Curl::Easy.new(TestServlet.url)
    callbacks = []

    easy.on_body { raise "body blew up" }
    easy.on_complete do
      callbacks << :complete
      raise "complete blew up"
    end
    easy.on_failure do |_completed_easy, error_info|
      callbacks << :failure
      assert_equal Curl::Err::WriteError, error_info.first
      raise "failure blew up"
    end

    error = assert_raise(RuntimeError) do
      multi.add(easy)
      multi.perform
    end

    assert_equal "body blew up", error.message
    assert_equal [:complete, :failure], callbacks
    assert multi.idle?
    assert_equal 0, multi.requests.length
  ensure
    multi.close if multi
  end

  # NOTE: if this test runs slowly on Mac OSX, it is probably due to the use of a port install curl+ssl+ares install
  # on my MacBook, this causes curl_easy_init to take nearly 0.01 seconds / * 100 below is 1 second too many!
  def test_n_requests
    n = 100
    m = Curl::Multi.new
    responses = []
    n.times do|i|
      responses[i] = String.new
      c = Curl::Easy.new($TEST_URL) do|curl|
        curl.on_body{|data| responses[i] << data; data.size }
      end
      m.add c
    end

    m.perform

    assert_equal n, responses.size
    n.times do|i|
      assert_match(/^# DO NOT REMOVE THIS COMMENT/, responses[i], "response #{i}")
    end
  ensure
    m.close if m
  end

  def test_n_requests_with_break
    # process n requests then load the handle again and run it again
    n = 2
    m = Curl::Multi.new
    5.times do|it|
      responses = []
      n.times do|i|
        responses[i] = String.new
        c = Curl::Easy.new($TEST_URL) do|curl|
          curl.on_body{|data| responses[i] << data; data.size }
        end
        m.add c
      end
      m.perform

      assert_equal n, responses.size
      n.times do|i|
        assert_match(/^# DO NOT REMOVE THIS COMMENT/, responses[i], "response #{i}")
      end
    end
  ensure
    m.close if m
  end

  def test_idle_check
    m = Curl::Multi.new
    e = Curl::Easy.new($TEST_URL)

    assert(m.idle?, 'A new Curl::Multi handle should be idle')

    assert_nil e.multi

    m.add(e)

    assert_not_nil e.multi

    assert((not m.idle?), 'A Curl::Multi handle with a request should not be idle')

    m.perform

    assert(m.idle?, 'A Curl::Multi handle should be idle after performing its requests')
  ensure
    m.close if m
  end

  def test_requests
    m = Curl::Multi.new

    assert_equal(0, m.requests.length, 'A new Curl::Multi handle should have no requests')

    10.times do
      m.add(Curl::Easy.new($TEST_URL))
    end

    assert_equal(10, m.requests.length, 'multi.requests should contain all the active requests')

    m.perform

    assert_equal(0, m.requests.length, 'A new Curl::Multi handle should have no requests after a perform')
  ensure
    m.close if m
  end

  def test_easy_multi_is_cleared_after_perform
    m = Curl::Multi.new
    c = Curl::Easy.new($TEST_URL)

    m.add(c)
    assert_equal m, c.multi

    m.perform

    assert_nil c.multi
  ensure
    m.close if m
  end

  def test_easy_multi_is_available_during_completion_callbacks
    multi = Curl::Multi.new
    easy = Curl::Easy.new($TEST_URL)
    seen_multi = {}

    easy.on_complete do |completed_easy|
      seen_multi[:complete] = completed_easy.multi
    end

    easy.on_success do |completed_easy|
      seen_multi[:success] = completed_easy.multi
    end

    multi.add(easy)
    multi.perform

    assert_same multi, seen_multi[:complete]
    assert_same multi, seen_multi[:success]
    assert_nil easy.multi
  ensure
    multi.close if multi
  end

  def test_cancel
    m = Curl::Multi.new
    m.cancel! # shouldn't raise anything

    10.times do
      m.add(Curl::Easy.new($TEST_URL))
    end

    m.cancel!

    assert_equal(0, m.requests.size, 'A new Curl::Multi handle should have no requests after being canceled')
  ensure
    m.close if m
  end

  def test_with_success
    c1 = Curl::Easy.new($TEST_URL)
    c2 = Curl::Easy.new($TEST_URL)
    success_called1 = false
    success_called2 = false

    c1.on_success do|c|
      success_called1 = true
      assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
    end

    c2.on_success do|c|
      success_called2 = true
      assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
    end

    m = Curl::Multi.new

    m.add( c1 )
    m.add( c2 )

    m.perform do
      # idle
      #puts "idling..."
    end

    assert success_called2
    assert success_called1
  ensure
    m.close if m
  end

  def test_with_success_cb_with_404
    c1 = Curl::Easy.new("#{$TEST_URL.gsub(/file:\/\//,'')}/not_here")
    c2 = Curl::Easy.new($TEST_URL)
    success_called1 = false
    success_called2 = false

    c1.on_success do|c|
      success_called1 = true
      #puts "success 1 called: #{c.body_str.inspect}"
      assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
    end

    c1.on_failure do|c,rc|
      # rc => [Curl::Err::MalformedURLError, "URL using bad/illegal format or missing URL"]
      assert_equal Curl::Easy, c.class
      assert_equal Curl::Err::MalformedURLError, rc.first
      assert_equal "URL using bad/illegal format or missing URL", rc.last
    end

    c2.on_success do|c|
    #  puts "success 2 called: #{c.body_str.inspect}"
      success_called2 = true
      assert_match(/^# DO NOT REMOVE THIS COMMENT/, c.body_str)
    end

    m = Curl::Multi.new

    #puts "c1: #{c1.url}"
    m.add( c1 )
    #puts "c2: #{c2.url}"
    m.add( c2 )

    #puts "calling"
    m.perform do
      # idle
    end

    assert success_called2
    assert !success_called1
  ensure
    m.close if m
  end

  # This tests whether, ruby's GC will trash an out of scope easy handle
  class TestForScope
    attr_reader :buf

    def t_method
      @buf = String.new
      @m = Curl::Multi.new
      10.times do|i|
        c = Curl::Easy.new($TEST_URL)
        c.on_success{|b| @buf << b.body_str }
        ObjectSpace.garbage_collect
        @m.add(c)
        ObjectSpace.garbage_collect
      end
      ObjectSpace.garbage_collect
    end

    def t_call
      @m.perform do
        ObjectSpace.garbage_collect
      end
    ensure
      @m.close if @m
    end

    def self.test
      ObjectSpace.garbage_collect
      tfs = TestForScope.new
      ObjectSpace.garbage_collect
      tfs.t_method
      ObjectSpace.garbage_collect
      tfs.t_call
      ObjectSpace.garbage_collect

      tfs.buf
    end

  end

  def test_with_garbage_collect
    ObjectSpace.garbage_collect
    buf = TestForScope.test
    ObjectSpace.garbage_collect
    assert_match(/^# DO NOT REMOVE THIS COMMENT/, buf)
  end

=begin
  def test_remote_requests
    responses = {}
    requests = ["http://google.co.uk/", "http://ruby-lang.org/"]
    m = Curl::Multi.new
    # add a few easy handles
    requests.each do |url|
      responses[url] = String.new
      responses["#{url}-header"] = String.new
      c = Curl::Easy.new(url) do|curl|
        curl.follow_location = true
        curl.on_header{|data| responses["#{url}-header"] << data; data.size }
        curl.on_body{|data| responses[url] << data; data.size }
        curl.on_success {
          puts curl.last_effective_url
        }
      end
      m.add(c)
    end

    m.perform

    requests.each do|url|
      puts responses["#{url}-header"].split("\r\n").inspect
      #puts responses[url].size
    end
  end
=end

  def test_multi_easy_get_01
    urls = []
    root_uri = 'http://127.0.0.1:9129/ext/'
    # send a request to fetch all c files in the ext dir
    Dir[File.dirname(__FILE__) + "/../ext/*.c"].each do|path|
      urls << root_uri + File.basename(path)
    end
    urls = urls[0..(urls.size/2)] # keep it fast, webrick...
    Curl::Multi.get(urls, {:follow_location => true}, {:pipeline => true}) do|curl|
      assert_equal 200, curl.response_code
    end
  end

  def test_multi_easy_download_01
    # test collecting response buffers to file e.g. on_body
    root_uri = 'http://127.0.0.1:9129/ext/'
    urls = []
    downloads = []
    file_info = {}

    Dir.mktmpdir('curb-download-') do |download_dir|
      # for each file store the size by file name
      Dir[File.dirname(__FILE__) + "/../ext/*.c"].each do|path|
        urls << (root_uri + File.basename(path))
        downloads << File.join(download_dir, File.basename(path))
        file_info[File.basename(path)] = {:size => File.size(path), :path => path}
      end

      # start downloads
      Curl::Multi.download(urls,{},{},downloads) do|curl,download_path|
        assert_equal 200, curl.response_code
        assert File.exist?(download_path)
        assert_equal file_info[File.basename(download_path)][:size], File.size(download_path), "incomplete download: #{download_path}"
      end
    end
  end

  def test_multi_easy_post_01
    urls = [
      { :url => TestServlet.url + '?q=1', :post_fields => {'field1' => 'value1', 'k' => 'j'}},
      { :url => TestServlet.url + '?q=2', :post_fields => {'field2' => 'value2', 'foo' => 'bar', 'i' => 'j' }},
      { :url => TestServlet.url + '?q=3', :post_fields => {'field3' => 'value3', 'field4' => 'value4'}}
    ]
    Curl::Multi.post(urls, {:follow_location => true, :multipart_form_post => true}, {:pipeline => true}) do|easy|
      str = easy.body_str
      assert_match(/POST/, str)
      fields = {}
      str.gsub(/POST\n/,'').split('&').map{|sv| k, v = sv.split('='); fields[k] = v }
      expected = urls.find{|s| s[:url] == easy.last_effective_url }
      assert_equal expected[:post_fields], fields
      #puts "#{easy.last_effective_url} #{fields.inspect}"
    end
  end

  def test_multi_easy_put_01
    urls = [{ :url => TestServlet.url, :method => :put, :put_data => "message",
             :headers => {'Content-Type' => 'application/json' } },
           { :url => TestServlet.url, :method => :put, :put_data => "message",
             :headers => {'Content-Type' => 'application/json' } }]
    Curl::Multi.put(urls, {}, {:pipeline => true}) do|easy|
      assert_match(/PUT/, easy.body_str)
      assert_match(/message/, easy.body_str)
    end
  end

  def test_multi_easy_http_01
    urls = [
      { :url => TestServlet.url + '?q=1', :method => :post, :post_fields => {'field1' => 'value1', 'k' => 'j'}},
      { :url => TestServlet.url + '?q=2', :method => :post, :post_fields => {'field2' => 'value2', 'foo' => 'bar', 'i' => 'j' }},
      { :url => TestServlet.url + '?q=3', :method => :post, :post_fields => {'field3' => 'value3', 'field4' => 'value4'}},
      { :url => TestServlet.url, :method => :put, :put_data => "message",
        :headers => {'Content-Type' => 'application/json' } },
      { :url => TestServlet.url, :method => :get }
    ]
    Curl::Multi.http(urls, {:pipeline => true}) do|easy, code, method|
      assert_equal 200, code
      case method
      when :post
        assert_match(/POST/, easy.body_str)
      when :get
        assert_match(/GET/, easy.body_str)
      when :put
        assert_match(/PUT/, easy.body_str)
      end
      #puts "#{easy.body_str.inspect}, #{method.inspect}, #{code.inspect}"
    end
  end

  def test_multi_easy_http_with_max_connects
    urls = [
      { :url => TestServlet.url + '?q=1', :method => :get },
      { :url => TestServlet.url + '?q=2', :method => :get },
      { :url => TestServlet.url + '?q=3', :method => :get }
    ]
    Curl::Multi.http(urls, {:pipeline => true, :max_connects => 1}) do|easy, code, method|
      assert_equal 200, code
      case method
      when :post
        assert_match(/POST/, easy.body)
      when :get
        assert_match(/GET/, easy.body)
      when :put
        assert_match(/PUT/, easy.body)
      end
    end
  end

  def test_multi_easy_http_with_max_host_connections
    urls = [
      { :url => TestServlet.url + '?q=1', :method => :get },
      { :url => TestServlet.url + '?q=2', :method => :get },
      { :url => TestServlet.url + '?q=3', :method => :get }
    ]
    Curl::Multi.http(urls, {:pipeline => true, :max_host_connections => 1}) do|easy, code, method|
      assert_equal 200, code
      case method
      when :post
        assert_match(/POST/, easy.body)
      when :get
        assert_match(/GET/, easy.body)
      when :put
        assert_match(/PUT/, easy.body)
      end
    end
  end

  # Regression test for issue #267 (2015): ensure that when reusing
  # easy handles with constrained concurrency, the callback receives
  # the correct URL for each completed request rather than repeating
  # the first URL.
  def test_multi_easy_http_urls_unique_across_max_connects
    urls = [
      { :url => TestServlet.url + '?q=1', :method => :get },
      { :url => TestServlet.url + '?q=2', :method => :get },
      { :url => TestServlet.url + '?q=3', :method => :get }
    ]

    [1, 2, 3].each do |max|
      results = []
      Curl::Multi.http(urls.dup, {:pipeline => true, :max_connects => max}) do |easy, code, method|
        assert_equal 200, code
        assert_equal :get, method
        results << easy.last_effective_url
      end

      # Ensure we saw one completion per input URL
      assert_equal urls.size, results.size, "expected #{urls.size} results with max_connects=#{max}"

      # And that each URL completed exactly once (no accidental reuse/mis-reporting)
      expected_urls = urls.map { |u| u[:url] }
      assert_equal expected_urls.to_set, results.to_set, "unexpected URLs for max_connects=#{max}: #{results.inspect}"
      assert_equal expected_urls.size, results.uniq.size, "duplicate URLs seen for max_connects=#{max}: #{results.inspect}"
    end
  end

  def test_multi_recieves_500
    m = Curl::Multi.new
    e = Curl::Easy.new("http://127.0.0.1:9129/methods")
    failure = false
    e.post_body = "hello=world&s=500"
    e.on_failure{|c,r| failure = true }
    e.on_success{|c| failure = false }
    m.add(e)
    m.perform
    assert failure
    e2 = Curl::Easy.new(TestServlet.url)
    e2.post_body = "hello=world"
    e2.on_failure{|c,r| failure = true }
    m.add(e2)
    m.perform
    failure = false
    assert !failure
    assert_equal "POST\nhello=world", e2.body_str
  ensure
    m.close if m
  end

  def test_remove_exception_is_descriptive
    m = Curl::Multi.new
    c = Curl::Easy.new("http://127.9.9.9:999110")
    m.remove(c)
  rescue => e
    assert_equal 'CURLError: Invalid easy handle', e.message
    assert_equal 0, m.requests.size
  ensure
    m.close if m
  end

  def test_retry_easy_handle
    m = Curl::Multi.new

    tries = 10

    c1 = Curl::Easy.new('http://127.1.1.1:99911') do |curl|
      curl.on_failure {|c,e|
        assert_equal [Curl::Err::MalformedURLError, "URL using bad/illegal format or missing URL"], e
        if tries > 0
          tries -= 1
          m.add(c)
        end
      }
    end

    tries -= 1
    m.add(c1)

    m.perform
    assert_equal 0, tries
    assert_equal 0, m.requests.size
  ensure
    m.close if m
  end

  def test_close_in_on_missing_callback_is_blocked
    m = Curl::Multi.new
    c = Curl::Easy.new(TestServlet.url + '/not_here')
    did_raise = false

    c.on_missing do |easy, _error|
      begin
        easy.close
      rescue RuntimeError
        did_raise = true
      end
    end

    m.add(c)
    m.perform

    assert did_raise
  ensure
    m.close if m
  end

  def test_close_in_on_redirect_callback_is_blocked
    m = Curl::Multi.new
    c = Curl::Easy.new(TestServlet.url + '/redirect')
    did_raise = false

    c.on_redirect do |easy, _error|
      begin
        easy.close
      rescue RuntimeError
        did_raise = true
      end
    end

    m.add(c)
    m.perform

    assert did_raise
  ensure
    m.close if m
  end

  def test_reusing_handle
    m = Curl::Multi.new

    c = Curl::Easy.new('http://127.0.0.1') do|easy|
      easy.on_complete{|e,r| puts e.inspect }
    end

    m.add(c)
    m.add(c)
  rescue => e
    assert Curl::Err::MultiBadEasyHandle == e.class || Curl::Err::MultiAddedAlready == e.class
  ensure
    m.close if m
  end

  def test_multi_default_timeout
    assert_equal 100, Curl::Multi.default_timeout
    Curl::Multi.default_timeout = 12
    assert_equal 12, Curl::Multi.default_timeout
    assert_equal 100, (Curl::Multi.default_timeout = 100)
  end

  def with_queue_refill_test_server(wait_fail_until_slow: false)
    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    hits = Hash.new(0)
    slow_started = false
    slow_started_lock = Mutex.new
    slow_started_condition = ConditionVariable.new
    server = WEBrick::HTTPServer.new(:Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    server.mount_proc('/fail') do |_req, res|
      if wait_fail_until_slow
        slow_started_lock.synchronize do
          slow_started_condition.wait(slow_started_lock, 1) unless slow_started
        end
      end

      hits[:fail] += 1
      res['Content-Type'] = 'text/plain'
      res.body = 'fail'
    end
    server.mount_proc('/slow') do |_req, res|
      slow_started_lock.synchronize do
        hits[:slow] += 1
        slow_started = true
        slow_started_condition.broadcast
      end

      res['Content-Type'] = 'text/plain'
      sleep 0.3
      res.body = 'slow'
    end
    server.mount_proc('/queued') do |_req, res|
      hits[:queued] += 1
      res['Content-Type'] = 'text/plain'
      res.body = 'queued'
    end

    server_thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: server_thread)

    yield port, hits
  ensure
    server.shutdown if defined?(server) && server
    server_thread.join(server_startup_timeout) if defined?(server_thread) && server_thread
    server_thread.kill if defined?(server_thread) && server_thread&.alive?
  end

  include TestServerMethods

  def setup
    server_setup
  end
end
