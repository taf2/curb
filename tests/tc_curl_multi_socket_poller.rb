require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

begin
  require 'async'
rescue LoadError
  nil
end

# Coverage for the socket-action loop's kernel event queue (epoll/kqueue):
# under a fiber scheduler every libcurl socket is waited on through a single
# poller descriptor with io_wait, instead of the optional io_select hook.
class TestCurbCurlMultiSocketPoller < Test::Unit::TestCase
  class PollerAbort < StandardError; end

  class RecordingScheduler
    attr_reader :io_wait_calls, :io_select_calls

    def initialize
      @io_wait_calls = []
      @io_select_calls = 0
    end

    def fiber(&block)
      Fiber.new(blocking: false, &block)
    end

    def io_wait(io, events, timeout = nil)
      @io_wait_calls << [io.fileno, events]
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

  # io_select is an optional hook; many schedulers only implement io_wait.
  class IoWaitOnlyScheduler < RecordingScheduler
    undef_method :io_select
  end

  # Counts Async's io_select calls, each of which starts a Thread. Prepended
  # to a single scheduler's singleton class so other tests are unaffected.
  module AsyncIoSelectCounter
    attr_reader :curb_io_select_calls

    def io_select(*args)
      @curb_io_select_calls = (@curb_io_select_calls || 0) + 1
      super
    end
  end

  STREAM_CHUNKS = 32
  STREAM_BYTES = 8192

  def setup
    super
    omit('The socket-action loop is not built on Windows') if WINDOWS
    omit('Fiber scheduler API unavailable on this Ruby') unless Fiber.respond_to?(:set_scheduler) && Fiber.respond_to?(:schedule)
    omit('socket-action perform path is not available in this build') unless Curl::Multi.private_method_defined?(:_socket_perform)

    @server = CurbTinyHTTPServer.new
    @original_default_timeout = Curl::Multi.default_timeout
  end

  def teardown
    Curl::Multi.default_timeout = @original_default_timeout if @original_default_timeout
    # Easy#perform under a scheduler keeps a per-thread shared multi; drop it so
    # later tests do not inherit state from this file's Async runs.
    state = Thread.current.thread_variable_get(:curb_scheduler_state)
    state[:multi].close if state && state[:multi]
    Thread.current.thread_variable_set(:curb_scheduler_state, nil)
    @server.close if @server
    super
  end

  def poller
    Curl::Multi.send(:_socket_poller)
  end

  def monotonic
    Process.clock_gettime(Process::CLOCK_MONOTONIC)
  end

  def with_scheduler(scheduler)
    previous_scheduler = Fiber.scheduler if Fiber.respond_to?(:scheduler)
    Fiber.set_scheduler(scheduler)
    fiber = Fiber.schedule { yield }
    fiber.resume while fiber.alive?
  ensure
    Fiber.set_scheduler(previous_scheduler)
  end

  def stream_path
    "/stream/#{STREAM_CHUNKS}/#{STREAM_BYTES}"
  end

  def expected_stream_body
    CurbTinyHTTPServer.stream_body(STREAM_CHUNKS, STREAM_BYTES)
  end

  def perform_streams(count)
    multi = Curl::Multi.new
    easies = count.times.map do
      easy = Curl::Easy.new(@server.url(stream_path))
      multi.add(easy)
      easy
    end
    multi.perform
    easies
  ensure
    multi.close if multi
  end

  def eventpoll_fd_count
    Dir.children('/proc/self/fd').count do |fd|
      begin
        File.readlink("/proc/self/fd/#{fd}") == 'anon_inode:[eventpoll]'
      rescue SystemCallError
        false
      end
    end
  end

  def test_socket_poller_reports_backend
    assert_include [nil, 'epoll', 'kqueue'], poller
    if RUBY_PLATFORM =~ /linux/
      assert_equal 'epoll', poller
    elsif RUBY_PLATFORM =~ /darwin|bsd/
      assert_equal 'kqueue', poller
    end
  end

  # With several sockets in flight, every wait goes through io_wait on one
  # descriptor (the poller), and the io_select hook is never used.
  def test_concurrent_streams_wait_on_single_poller_descriptor
    omit('no kernel event queue in this build') unless poller
    scheduler = RecordingScheduler.new
    easies = nil

    with_scheduler(scheduler) { easies = perform_streams(8) }

    easies.each do |easy|
      assert_equal 200, easy.response_code
      assert_equal expected_stream_body, easy.body_str
    end
    assert_equal 0, scheduler.io_select_calls, 'io_select should not be used when a poller is available'
    assert_operator scheduler.io_wait_calls.length, :>=, 1
    assert_equal 1, scheduler.io_wait_calls.map(&:first).uniq.length,
                 "expected every wait on one poller descriptor, saw #{scheduler.io_wait_calls.map(&:first).uniq.inspect}"
    assert(scheduler.io_wait_calls.all? { |_fd, events| events == IO::READABLE },
           'the poller descriptor should only be waited on for readability')
  end

  # Without io_select the old loop waited on one arbitrary socket for up to
  # default_timeout, so an idle socket first in line delayed every other
  # transfer. The poller wakes for whichever socket becomes ready.
  def test_scheduler_without_io_select_is_not_delayed_by_idle_socket
    omit('no kernel event queue in this build') unless poller
    Curl::Multi.default_timeout = 2_000
    scheduler = IoWaitOnlyScheduler.new
    fast_elapsed = nil
    hung = Curl::Easy.new(@server.url('/hang'))
    fast = Curl::Easy.new(@server.url('/fast'))

    started = monotonic
    fast.on_complete do
      fast_elapsed = monotonic - started
      @server.release_hung(1)
    end

    with_scheduler(scheduler) do
      multi = Curl::Multi.new
      begin
        multi.add(hung)
        multi.add(fast)
        multi.perform
      ensure
        multi.close
      end
    end

    assert_equal "fast", fast.body_str
    assert_equal "released", hung.body_str
    assert_not_nil fast_elapsed
    assert_operator fast_elapsed, :<, 1.0, "fast transfer waited #{fast_elapsed.round(3)}s behind an idle socket"
  end

  def test_async_concurrent_streams_do_not_use_io_select
    omit('Async gem not available') unless defined?(Async::Scheduler)
    omit('no kernel event queue in this build') unless poller
    bodies = []
    scheduler = nil

    Async do |task|
      scheduler = Fiber.scheduler
      scheduler.singleton_class.prepend(AsyncIoSelectCounter)
      12.times.map do
        task.async { bodies << Curl.get(@server.url(stream_path)).body_str }
      end.each(&:wait)
    end

    assert_equal 12, bodies.length
    assert(bodies.all? { |body| body == expected_stream_body }, 'streamed bodies were not reassembled intact')
    assert_equal 0, scheduler.curb_io_select_calls.to_i, 'Async#io_select starts a thread per call and should not be used'
  end

  # Completions that queue new requests close and open sockets within one
  # perform, so descriptor numbers are reused while the poller stays open.
  def test_socket_churn_within_one_perform
    omit('Async gem not available') unless defined?(Async::Scheduler)
    total = 30
    bodies = []
    started = monotonic

    Async do
      multi = Curl::Multi.new
      remaining = total
      add_next = lambda do
        next if remaining.zero?
        remaining -= 1
        easy = Curl::Easy.new(@server.url('/fast'))
        easy.on_complete do |completed|
          bodies << completed.body_str
          add_next.call
        end
        multi.add(easy)
      end

      begin
        2.times { add_next.call }
        multi.perform
      ensure
        multi.close
      end
    end

    assert_equal Array.new(total, "fast"), bodies
    assert_operator monotonic - started, :<, 10.0
  end

  # A peer that disconnects early or resets the connection must fail only its
  # own transfer: readiness is reported, libcurl sees the error on read, and
  # the loop neither spins nor stalls the healthy transfer.
  def test_truncated_and_reset_transfers_fail_without_stalling
    scheduler = RecordingScheduler.new
    truncated = Curl::Easy.new(@server.url('/truncate'))
    reset = Curl::Easy.new(@server.url('/reset'))
    healthy = Curl::Easy.new(@server.url(stream_path))
    started = monotonic

    with_scheduler(scheduler) do
      multi = Curl::Multi.new
      begin
        [truncated, reset, healthy].each { |easy| multi.add(easy) }
        multi.perform
      ensure
        multi.close
      end
    end

    assert_operator monotonic - started, :<, 5.0
    assert_equal expected_stream_body, healthy.body_str
    assert_not_equal 0, truncated.last_result, 'a truncated body should fail the transfer'
    assert_not_equal 0, reset.last_result, 'a reset connection should fail the transfer'
  end

  def test_poller_descriptor_closed_after_perform
    omit('eventpoll descriptors are only visible on Linux') unless poller == 'epoll' && File.directory?('/proc/self/fd')
    before = eventpoll_fd_count

    with_scheduler(RecordingScheduler.new) { 3.times { perform_streams(2) } }

    assert_equal before, eventpoll_fd_count
  end

  def test_poller_descriptor_closed_when_perform_raises
    omit('eventpoll descriptors are only visible on Linux') unless poller == 'epoll' && File.directory?('/proc/self/fd')
    before = eventpoll_fd_count

    assert_raise(PollerAbort) do
      with_scheduler(RecordingScheduler.new) do
        multi = Curl::Multi.new
        begin
          2.times { multi.add(Curl::Easy.new(@server.url('/delay/0.2'))) }
          yields = 0
          # Raise mid-transfer, after the loop has waited on the poller.
          multi.perform do
            yields += 1
            raise PollerAbort, 'stop the drive loop' if yields >= 3
          end
        ensure
          multi.close
        end
      end
    end

    assert_equal before, eventpoll_fd_count
  end

  # If wrapping the poller descriptor in an IO fails, the error propagates and
  # the descriptor is still closed.
  def test_poller_descriptor_closed_when_io_wrapper_fails
    omit('eventpoll descriptors are only visible on Linux') unless poller == 'epoll' && File.directory?('/proc/self/fd')
    wrapper_error = Class.new(StandardError)
    original_for_fd = IO.method(:for_fd)
    before = eventpoll_fd_count

    begin
      redefine_io_for_fd { |*| raise wrapper_error, 'IO wrapper failed' }
      assert_raise(wrapper_error) do
        with_scheduler(RecordingScheduler.new) do
          multi = Curl::Multi.new
          begin
            2.times { multi.add(Curl::Easy.new(@server.url('/fast'))) }
            multi.perform
          ensure
            multi.close
          end
        end
      end
    ensure
      redefine_io_for_fd(original_for_fd)
    end

    assert_equal before, eventpoll_fd_count
  end

  private

  def redefine_io_for_fd(callable = nil, &block)
    previous_verbose = $VERBOSE
    $VERBOSE = nil
    IO.define_singleton_method(:for_fd, callable || block)
  ensure
    $VERBOSE = previous_verbose
  end
end
