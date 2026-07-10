require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

begin
  require 'async'
rescue LoadError
  HAS_ASYNC = false
else
  HAS_ASYNC = true
end

# This test verifies that Curl::Easy#perform cooperates with Ruby's Fiber scheduler
# by running multiple requests concurrently in a single thread using the Async gem.
class TestCurbFiberScheduler < Test::Unit::TestCase
  include BugTestServerSetupTeardown
  include TestServerMethods

  class RecordingScheduler
    attr_reader :io_wait_events, :io_select_calls

    def initialize
      @io_wait_events = []
      @io_select_calls = 0
    end

    def fiber(&block)
      Fiber.new(blocking: false, &block)
    end

    def io_wait(io, events, timeout = nil)
      @io_wait_events << events
      raise TypeError, "expected Integer events, got #{events.class}" unless events.is_a?(Integer)

      readers = (events & IO::READABLE) != 0 ? [io] : nil
      writers = (events & IO::WRITABLE) != 0 ? [io] : nil
      readable, writable = blocking_io { IO.select(readers, writers, nil, timeout) }

      ready = 0
      ready |= IO::READABLE if readable && !readable.empty?
      ready |= IO::WRITABLE if writable && !writable.empty?
      ready.zero? ? false : ready
    end

    def io_select(readers, writers, excepts, timeout = nil)
      @io_select_calls += 1
      blocking_io { IO.select(readers, writers, excepts, timeout) }
    end

    def kernel_sleep(duration = nil)
      # A bare sleep here would re-enter this hook through the scheduler.
      blocking_io { sleep(duration || 0) }
    end

    def block(_blocker, timeout = nil)
      blocking_io { sleep(timeout || 0) }
      false
    end

    def unblock(*)
    end

    def close
    end

    def fiber_interrupt(*)
    end

    private

    def blocking_io(&block)
      if Fiber.respond_to?(:blocking)
        Fiber.blocking(&block)
      else
        Fiber.new(blocking: true, &block).resume
      end
    end
  end

  ITERS = 4
  MIN_S = 0.25
  # Each request sleeps 0.25s; two concurrent requests should be ~0.25–0.5s.
  # Allow some jitter in CI environments.
  THRESHOLD = ((MIN_S*(ITERS/2.0)) + (MIN_S/2.0)) # add more jitter for slower CI environments
  SERIAL_TIME_WOULD_BE_ABOUT = MIN_S * ITERS

  def setup
    @port = unused_local_port

    @response_proc = lambda do |res|
      res['Content-Type'] = 'text/plain'
      sleep MIN_S
      res.body = '200'
    end
    super
  end

  def test_multi_is_scheduler_friendly
    if skip_no_async
      return
    end

    url = "http://127.0.0.1:#{@port}/test"

    started = Time.now
    results = []


    async_run do
      m = Curl::Multi.new
      ITERS.times.each do
        c = Curl::Easy.new(url)
        c.on_complete { results << c.code }
        m.add(c)
      end
      m.perform
    ensure
      m.close if m
    end

    duration = Time.now - started

    assert duration < THRESHOLD, "Requests did not run concurrently under fiber scheduler (#{duration}s) which exceeds the expected threshold of: #{THRESHOLD} serial time would be about: #{SERIAL_TIME_WOULD_BE_ABOUT}"
    assert_equal ITERS, results.size
    assert_equal ITERS.times.map {200}, results
  end

  def test_easy_perform_is_scheduler_friendly
    if skip_no_async
      return
    end

    url = "http://127.0.0.1:#{@port}/test"

    started = Time.now
    results = []

    async_run do |top|
      tasks = ITERS.times.map do
        top.async do
          #t = Time.now.to_i
          #puts "starting fiber [#{results.size}] -> #{t}"
          c = Curl.get(url)
          #puts "received result: #{results.size} -> #{Time.now.to_f - t.to_f}"
          results << c.code

        end
      end
      tasks.each(&:wait)
    end

    duration = Time.now - started

    assert duration < THRESHOLD, "Requests did not run concurrently under fiber scheduler (#{duration}s) which exceeds the expected threshold of: #{THRESHOLD} serial time would be about: #{SERIAL_TIME_WOULD_BE_ABOUT}"
    assert_equal ITERS, results.size
    assert_equal ITERS.times.map {200}, results
  end

  def test_multi_perform_yields_block_under_scheduler
    if skip_no_async
      return
    end

    url = "http://127.0.0.1:#{@port}/test"
    yielded = 0
    results = []

    async_run do
      m = Curl::Multi.new
      ITERS.times do
        c = Curl::Easy.new(url)
        c.on_complete { results << c.code }
        m.add(c)
      end
      m.perform do
        yielded += 1
      end
    ensure
      m.close if m
    end

    assert_operator yielded, :>=, 1, 'perform did not yield block while waiting under scheduler'
    assert_equal ITERS, results.size
    assert_equal ITERS.times.map {200}, results
  end

  def test_multi_single_request_scheduler_path
    if skip_no_async
      return
    end

    url = "http://127.0.0.1:#{@port}/test"
    result = nil

    async_run do
      m = Curl::Multi.new
      c = Curl::Easy.new(url)
      c.on_complete { result = c.code }
      m.add(c)
      m.perform
    ensure
      m.close if m
    end

    assert_equal 200, result
  end

  def test_multi_single_request_socket_perform_passes_integer_events_to_io_wait
    omit('Skipping custom scheduler test on Windows') if WINDOWS
    omit('Fiber scheduler API unavailable on this Ruby') unless fiber_scheduler_supported?

    url = "http://127.0.0.1:#{@port}/test"
    scheduler = RecordingScheduler.new
    result = nil

    with_scheduler(scheduler) do
      m = Curl::Multi.new
      c = Curl::Easy.new(url)
      c.on_complete { result = c.code }
      m.add(c)
      unless socket_perform_supported?
        omit('socket-action perform path is not available in this build')
      end
      m.send(:_socket_perform)
    ensure
      m.close if m
    end

    assert_equal 200, result
    assert_operator scheduler.io_wait_events.length + scheduler.io_select_calls, :>=, 1
    unless scheduler.io_wait_events.empty?
      assert scheduler.io_wait_events.all? { |events| events.is_a?(Integer) }
      assert scheduler.io_wait_events.any? { |events| (events & (IO::READABLE | IO::WRITABLE)) != 0 }
    end
  end

  # Multi#perform must not fall back to the thread-blocking legacy fdset loop
  # when the calling fiber runs under a scheduler: the waits have to go
  # through the scheduler's io_wait/io_select hooks so sibling fibers run.
  def test_multi_perform_waits_via_scheduler_hooks
    omit('Skipping custom scheduler test on Windows') if WINDOWS
    omit('Fiber scheduler API unavailable on this Ruby') unless fiber_scheduler_supported?

    url = "http://127.0.0.1:#{@port}/test"
    scheduler = RecordingScheduler.new
    result = nil

    with_scheduler(scheduler) do
      m = Curl::Multi.new
      unless socket_perform_supported?
        omit('socket-action perform path is not available in this build')
      end
      c = Curl::Easy.new(url)
      c.on_complete { result = c.code }
      m.add(c)
      m.perform
    ensure
      m.close if m
    end

    assert_equal 200, result
    assert_operator scheduler.io_wait_events.length + scheduler.io_select_calls, :>=, 1,
                    'Multi#perform under a fiber scheduler should wait through the scheduler hooks'
  end

  def test_multi_perform_propagates_io_wrapper_errors
    omit('Skipping custom scheduler test on Windows') if WINDOWS
    omit('Fiber scheduler API unavailable on this Ruby') unless fiber_scheduler_supported?
    omit('socket-action perform path is not available in this build') unless socket_perform_supported?

    url = "http://127.0.0.1:#{@port}/test"
    scheduler = RecordingScheduler.new
    wrapper_error = Class.new(StandardError)
    original_for_fd = IO.method(:for_fd)
    redefine_io_for_fd do |*_args|
      raise wrapper_error, 'IO wrapper failed'
    end

    error = assert_raise(wrapper_error) do
      with_scheduler(scheduler) do
        m = Curl::Multi.new
        m.add(Curl::Easy.new(url))
        m.perform
      ensure
        m.close if m
      end
    end

    assert_equal 'IO wrapper failed', error.message
  ensure
    redefine_io_for_fd(original_for_fd) if original_for_fd
  end

  # Probe-style regression tests: all previous concurrency assertions routed N
  # requests through one multi handle, so libcurl provided the parallelism and
  # they passed even when the wait loop blocked the whole thread. Here a
  # sibling fiber ticks every 10ms while a single request is in flight; a
  # blocking wait loop caps the ticks at roughly one per 100ms select chunk,
  # while a scheduler-friendly loop lets most of the ~MIN_S/0.01 ticks happen.
  def test_sibling_fibers_progress_during_multi_perform
    return if skip_no_async
    return if skip_no_socket_perform

    url = "http://127.0.0.1:#{@port}/test"
    result = nil

    ticks = probe_sibling_ticks do
      m = Curl::Multi.new
      begin
        c = Curl::Easy.new(url)
        c.on_complete { result = c.code }
        m.add(c)
        m.perform
      ensure
        m.close
      end
    end

    assert_equal 200, result
    assert_operator ticks, :>=, SIBLING_TICKS_MIN,
                    "sibling fiber starved during Multi#perform: #{ticks} ticks in a #{MIN_S}s transfer (~#{SIBLING_TICKS_POSSIBLE} possible)"
  end

  def test_sibling_fibers_progress_during_easy_perform
    return if skip_no_async
    return if skip_no_socket_perform

    url = "http://127.0.0.1:#{@port}/test"
    result = nil

    ticks = probe_sibling_ticks do
      c = Curl::Easy.new(url)
      c.perform
      result = c.code
    end

    assert_equal 200, result
    assert_operator ticks, :>=, SIBLING_TICKS_MIN,
                    "sibling fiber starved during Easy#perform: #{ticks} ticks in a #{MIN_S}s transfer (~#{SIBLING_TICKS_POSSIBLE} possible)"
  end

  def test_sibling_fibers_progress_during_multi_perform_with_multiple_sockets
    return if skip_no_async
    return if skip_no_socket_perform

    url = "http://127.0.0.1:#{@port}/test"
    results = []

    ticks = probe_sibling_ticks do
      m = Curl::Multi.new
      begin
        2.times do
          c = Curl::Easy.new(url)
          c.on_complete { results << c.code }
          m.add(c)
        end
        m.perform
      ensure
        m.close
      end
    end

    assert_equal [200, 200], results.sort
    assert_operator ticks, :>=, SIBLING_TICKS_MIN,
                    "sibling fiber starved during multi-fd Multi#perform: #{ticks} ticks in a #{MIN_S}s transfer (~#{SIBLING_TICKS_POSSIBLE} possible)"
  end

  def test_scheduler_timeout_interrupts_single_socket_multi_perform
    return if skip_no_async
    return if skip_no_socket_perform

    assert_scheduler_timeout_interrupts_multi_perform(1)
  end

  def test_scheduler_timeout_interrupts_multi_socket_multi_perform
    return if skip_no_async
    return if skip_no_socket_perform

    assert_scheduler_timeout_interrupts_multi_perform(2)
  end

  def test_multi_perform_runs_work_added_from_final_idle_yield_under_scheduler
    return if skip_no_async
    return if skip_no_socket_perform

    url = "http://127.0.0.1:#{@port}/test"
    completions = []
    empty_yields = 0
    queued_added = false
    previous_autoclose = Curl::Multi.autoclose
    Curl::Multi.autoclose = true

    begin
      async_run do
        m = Curl::Multi.new
        begin
          first = Curl::Easy.new(url)
          first.on_complete { completions << :first }
          m.add(first)

          m.perform do |performing_multi|
            empty_yields += 1 if performing_multi.requests.empty?
            if !queued_added && empty_yields >= 2
              queued_added = true
              queued = Curl::Easy.new(url)
              queued.on_complete { completions << :queued }
              performing_multi.add(queued)
            end
          end
        ensure
          m.close
        end
      end

      assert queued_added, 'test should add queued work from the final idle yield'
      assert_equal [:first, :queued], completions
    ensure
      Curl::Multi.autoclose = previous_autoclose
    end
  end

  # A scheduler that implements io_wait but not the optional io_select hook
  # must neither busy-loop nor fall back to a thread-blocking wait. The broken
  # loop spins without an interrupt checkpoint, so the probe runs in a
  # subprocess guarded by a parent-side SIGKILL fuse.
  def test_socket_perform_progresses_when_scheduler_lacks_io_select
    omit('Skipping custom scheduler test on Windows') if WINDOWS
    omit('Fiber scheduler API unavailable on this Ruby') unless fiber_scheduler_supported?
    unless socket_perform_supported?
      omit('socket-action perform path is not available in this build')
    end

    script = File.join(File.dirname(__FILE__), 'io_select_less_scheduler_probe.rb')
    out_r, out_w = IO.pipe
    pid = Process.spawn(RbConfig.ruby, '-I', $LIBDIR, '-I', $EXTDIR, script, @port.to_s,
                        :out => out_w, :err => out_w)
    out_w.close

    fuse_seconds = 30
    deadline = Process.clock_gettime(Process::CLOCK_MONOTONIC) + fuse_seconds
    status = nil
    loop do
      _, status = Process.waitpid2(pid, Process::WNOHANG)
      break if status
      if Process.clock_gettime(Process::CLOCK_MONOTONIC) > deadline
        begin
          Process.kill('KILL', pid)
        rescue Errno::ESRCH
        end
        begin
          Process.waitpid(pid)
        rescue Errno::ECHILD
        end
        flunk "scheduler without io_select busy-looped _socket_perform (no exit within #{fuse_seconds}s; killed)"
      end
      sleep 0.05
    end

    output = out_r.read
    assert status.success?, "io_select-less scheduler subprocess failed: #{output}"
    match = /^OK ticks=(\d+)$/.match(output)
    assert_not_nil match, "unexpected io_select-less scheduler output: #{output}"
    assert_operator match[1].to_i, :>=, 2,
                    "scheduler without io_select did not make sibling-fiber progress: #{output}"
  ensure
    if pid && !status
      begin
        Process.kill('KILL', pid)
      rescue Errno::ESRCH
      end
      begin
        Process.waitpid(pid)
      rescue Errno::ECHILD
      end
    end
    out_r.close if out_r && !out_r.closed?
    out_w.close if out_w && !out_w.closed?
  end

  def test_multi_reuse_after_scheduler_perform
    unless HAS_ASYNC
      warn 'Skipping fiber scheduler test (Async gem not available)'
      return
    end
  
    url = "http://127.0.0.1:#{@port}/test"
    results = []
  
    async_run do
      m = Curl::Multi.new
      # First round
      c1 = Curl::Easy.new(url)
      c1.on_complete { results << c1.code }
      m.add(c1)
      m.perform
  
      # Second round on same multi
      c2 = Curl::Easy.new(url)
      c2.on_complete { results << c2.code }
      m.add(c2)
      m.perform
    ensure
      m.close if m
    end
  
    assert_equal [200, 200], results
  end

  def test_easy_perform_reuses_scheduler_multi_when_autoclose_is_enabled
    if skip_no_async
      return
    end
  
    url = "http://127.0.0.1:#{@port}/test"
    results = []
    previous_autoclose = Curl::Multi.autoclose
    Curl::Multi.autoclose = true
  
    async_run do
      2.times do
        easy = Curl::Easy.new(url)
        easy.perform
        results << easy.code
      end
    end
  
    assert_equal [200, 200], results
  ensure
    Curl::Multi.autoclose = previous_autoclose if defined?(previous_autoclose)
    cleanup_scheduler_state
  end

  def test_easy_perform_reraises_on_body_exception_under_scheduler
    if skip_no_async
      return
    end
  
    url = "http://127.0.0.1:#{@port}/test"
  
    async_run do |top|
      task = top.async do
        c = Curl::Easy.new(url)
        c.on_body { raise "body blew up" }
        c.on_complete { sleep 0 }
        c.perform
      end
  
      error = assert_raise(RuntimeError) do
        task.wait
      end
  
      assert_equal "body blew up", error.message
    end
  end

  def test_easy_perform_reraises_on_header_exception_under_scheduler
    if skip_no_async
      return
    end

    url = "http://127.0.0.1:#{@port}/test"

    async_run do |top|
      task = top.async do
        c = Curl::Easy.new(url)
        c.on_header { raise "header blew up" }
        c.on_complete { sleep 0 }
        c.perform
      end

      error = assert_raise(RuntimeError) do
        task.wait
      end

      assert_equal "header blew up", error.message
    end
  end

  def test_easy_perform_does_not_reraise_sibling_on_complete_exception_under_scheduler
    if skip_no_async
      return
    end

    with_ephemeral_http_server do |port, hits|
      results = {}

      async_run do |top|
        successful = top.async do
          easy = Curl::Easy.new("http://127.0.0.1:#{port}/fast")
          easy.perform
          results[:successful] = easy.response_code
        rescue => e
          results[:successful] = e
        end

        failing = top.async do
          easy = Curl::Easy.new("http://127.0.0.1:#{port}/slow")
          easy.on_complete { raise "boom" }
          easy.perform
          results[:failing] = :returned
        rescue => e
          results[:failing] = e
        end

        successful.wait
        failing.wait
      end

      assert_equal 200, results[:successful], "successful scheduler peer should return normally"
      assert_kind_of Curl::Err::AbortedByCallbackError, results[:failing]
      assert_equal "boom", results[:failing].message
      assert_equal 1, hits[:fast]
      assert_equal 1, hits[:slow]
    end
  end

  def test_easy_perform_does_not_reraise_fast_on_complete_exception_into_slow_successful_peer_under_scheduler
    if skip_no_async
      return
    end

    with_ephemeral_http_server do |port, hits|
      results = {}

      async_run do |top|
        failing = top.async do
          easy = Curl::Easy.new("http://127.0.0.1:#{port}/fast")
          easy.on_complete { raise "boom" }
          easy.perform
          results[:failing] = :returned
        rescue => e
          results[:failing] = e
        end

        successful = top.async do
          easy = Curl::Easy.new("http://127.0.0.1:#{port}/slow")
          easy.perform
          results[:successful] = easy.response_code
        rescue => e
          results[:successful] = e
        end

        failing.wait
        successful.wait
      end

      assert_equal 200, results[:successful], "slow successful scheduler peer should not inherit a fast sibling callback error"
      assert_kind_of Curl::Err::AbortedByCallbackError, results[:failing]
      assert_equal "boom", results[:failing].message
      assert_equal 1, hits[:fast]
      assert_equal 1, hits[:slow]
    end
  end

  def test_easy_perform_keeps_scheduler_timers_running_while_draining_deferred_on_complete_exception
    if skip_no_async
      return
    end

    with_ephemeral_http_server do |port, hits|
      marks = {}
      started = Process.clock_gettime(Process::CLOCK_MONOTONIC)

      async_run do |top|
        timer = top.async do
          sleep 0.05
          marks[:timer] = Process.clock_gettime(Process::CLOCK_MONOTONIC) - started
        end

        failing = top.async do
          easy = Curl::Easy.new("http://127.0.0.1:#{port}/fast")
          easy.on_complete { raise "boom" }
          easy.perform
          marks[:failing] = :returned
        rescue => e
          marks[:failing] = e
        end

        successful = top.async do
          easy = Curl::Easy.new("http://127.0.0.1:#{port}/slow")
          easy.perform
          marks[:slow_done] = Process.clock_gettime(Process::CLOCK_MONOTONIC) - started
        end

        timer.wait
        failing.wait
        successful.wait
      end

      assert_kind_of Curl::Err::AbortedByCallbackError, marks[:failing]
      assert_operator marks[:timer], :<, marks[:slow_done] - 0.02,
                      "scheduler timer should fire before the slow sibling finishes draining"
      assert_equal 1, hits[:fast]
      assert_equal 1, hits[:slow]
    end
  end

  def test_easy_perform_isolates_public_network_policy_block_under_scheduler
    if skip_no_async
      return
    end

    with_ephemeral_http_server do |port, hits|
      results = {}
      details = {}

      async_run do |top|
        blocked = top.async do
          easy = Curl::Easy.new("http://127.0.0.1:#{port}/fast")
          easy.network_policy = :public
          easy.perform
          results[:blocked] = :returned
        rescue => e
          results[:blocked] = e
          details[:blocked_error] = easy.unsafe_destination_error if defined?(easy) && easy
        end

        successful = top.async do
          easy = Curl::Easy.new("http://127.0.0.1:#{port}/slow")
          easy.perform
          results[:successful] = easy.response_code
        rescue => e
          results[:successful] = e
        end

        blocked.wait
        successful.wait
      end

      assert_equal 200, results[:successful], "scheduler peer without public policy should return normally"
      assert_kind_of Curl::Err::UnsafeDestinationError, results[:blocked]
      assert_match(/127\.0\.0\.1/, results[:blocked].message)
      assert_match(/127\.0\.0\.1/, details[:blocked_error])
      assert_equal 0, hits[:fast], "blocked public-policy peer should not reach the server"
      assert_equal 1, hits[:slow]
    end
  end

  def test_drain_scheduler_pending_does_not_drop_work_rejected_during_deferred_abort
    state = Curl.scheduler_state
    easy = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
    waiter = {completed: false, done: false, error: nil}

    state[:waiters][easy.object_id] = waiter
    state[:pending] << easy
    state[:multi].instance_variable_set(:@__curb_deferred_exception, RuntimeError.new("boom"))

    Curl.drain_scheduler_pending(state)

    assert(state[:pending].include?(easy) || waiter[:error],
           "scheduler enqueue rejected during deferred abort should remain pending or fail its waiter instead of disappearing")
    assert_equal 0, state[:multi].requests.length
  ensure
    cleanup_scheduler_state
  end

  # Sibling-fiber probe bounds: ~MIN_S/0.01 ticks are possible while the
  # request is in flight. A thread-blocking wait loop yields only one tick per
  # 100ms select chunk (2-3 for MIN_S=0.25); require a comfortably higher
  # count while leaving slack for slow CI hosts.
  SIBLING_TICKS_POSSIBLE = (MIN_S / 0.01).to_i
  SIBLING_TICKS_MIN = 8

  private
  def redefine_io_for_fd(callable = nil, &block)
    previous_verbose = $VERBOSE
    $VERBOSE = nil
    if callable
      IO.define_singleton_method(:for_fd, callable)
    else
      IO.define_singleton_method(:for_fd, &block)
    end
  ensure
    $VERBOSE = previous_verbose
  end

  def assert_scheduler_timeout_interrupts_multi_perform(easy_count)
    url = "http://127.0.0.1:#{@port}/test"
    timeout_error = nil
    started = Process.clock_gettime(Process::CLOCK_MONOTONIC)

    async_run do |task|
      begin
        task.with_timeout(0.05) do
          m = Curl::Multi.new
          begin
            easy_count.times { m.add(Curl::Easy.new(url)) }
            m.perform
          ensure
            m.close
          end
        end
      rescue Async::TimeoutError => error
        timeout_error = error
      end
    end

    elapsed = Process.clock_gettime(Process::CLOCK_MONOTONIC) - started
    assert_kind_of Async::TimeoutError, timeout_error,
                   "scheduler timeout was swallowed for #{easy_count} easy handle(s)"
    assert_operator elapsed, :<, MIN_S * 0.8,
                    "scheduler cancellation took #{elapsed}s for #{easy_count} easy handle(s)"
  end

  # Runs the block in one fiber while a sibling fiber ticks every 10ms;
  # returns how many ticks the sibling managed before the block finished.
  def probe_sibling_ticks
    ticks = 0
    done = false

    async_run do |top|
      ticker = top.async do
        until done
          sleep 0.01
          ticks += 1 unless done
        end
      end

      top.async do
        yield
      ensure
        done = true
      end.wait
      ticker.wait
    end

    ticks
  end

  def skip_no_async
    if WINDOWS
      warn 'Skipping fiber scheduler tests on Windows'
      return true
    end
    unless fiber_scheduler_supported?
      warn 'Skipping fiber scheduler tests (Fiber scheduler API unavailable)'
      return true
    end
    unless HAS_ASYNC
      warn 'Skipping fiber scheduler test (Async gem not available)'
      return true
    end
    false
  end

  def skip_no_socket_perform
    unless socket_perform_supported?
      warn 'Skipping fiber scheduler test (socket-action perform path unavailable)'
      return true
    end
    false
  end

  def socket_perform_supported?
    Curl::Multi.private_method_defined?(:_socket_perform)
  end

  def async_run(&block)
    # Prefer newer Async.run to avoid deprecated scheduler.async path.
    if defined?(Async) && Async.respond_to?(:run)
      Async.run(&block)
    else
      Async(&block)
    end
  end

  def fiber_scheduler_supported?
    Fiber.respond_to?(:set_scheduler) && Fiber.respond_to?(:schedule)
  end

  def with_scheduler(scheduler)
    previous_scheduler = Fiber.scheduler if Fiber.respond_to?(:scheduler)
    Fiber.set_scheduler(scheduler)
    fiber = Fiber.schedule { yield }
    fiber.resume while fiber.alive?
  ensure
    Fiber.set_scheduler(previous_scheduler) if Fiber.respond_to?(:set_scheduler)
  end

  def cleanup_scheduler_state
    state = Thread.current.thread_variable_get(:curb_scheduler_state)
    state[:multi].close if state && state[:multi]
    Thread.current.thread_variable_set(:curb_scheduler_state, nil)
  end

  def with_ephemeral_http_server
    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    hits = Hash.new(0)
    server = WEBrick::HTTPServer.new(:Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    server.mount_proc('/fast') do |_req, res|
      hits[:fast] += 1
      res['Content-Type'] = 'text/plain'
      res.body = 'fast'
    end
    server.mount_proc('/slow') do |_req, res|
      hits[:slow] += 1
      res['Content-Type'] = 'text/plain'
      sleep 0.2
      res.body = 'slow'
    end

    server_thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: server_thread)

    yield port, hits
  ensure
    server.shutdown if defined?(server) && server
    server_thread.join(server_startup_timeout) if defined?(server_thread) && server_thread
    server_thread.kill if defined?(server_thread) && server_thread&.alive?
  end
end
