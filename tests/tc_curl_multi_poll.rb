require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

# Coverage for the blocking (non-scheduler) Curl::Multi#perform wait loop:
# descriptors above FD_SETSIZE, long waits, and interrupting those waits.
class TestCurbCurlMultiPoll < Test::Unit::TestCase
  FD_SETSIZE = 1024
  HIGH_FD_TARGET = FD_SETSIZE + 76

  # Queue#pop(timeout:) needs Ruby 3.2. Older Rubies take the keyword hash as
  # the positional non_block flag and raise ThreadError immediately.
  def self.pop_within(queue, seconds)
    deadline = Process.clock_gettime(Process::CLOCK_MONOTONIC) + seconds
    loop do
      begin
        return queue.pop(true)
      rescue ThreadError
        return nil if Process.clock_gettime(Process::CLOCK_MONOTONIC) >= deadline
        sleep 0.01
      end
    end
  end

  # Minimal HTTP/1.1 server so each test controls exactly when a response is
  # written. Every request gets its own connection (Connection: close).
  class TinyServer
    attr_reader :port, :requests

    def initialize
      @listener = TCPServer.new('127.0.0.1', 0)
      @port = @listener.addr[1]
      @requests = Queue.new
      @release = Queue.new
      @clients = []
      @thread = Thread.new { accept_loop }
    end

    def url(path)
      "http://127.0.0.1:#{@port}#{path}"
    end

    # Let every pending /hang request finish.
    def release_hung(count = 64)
      count.times { @release << true }
    end

    def close
      release_hung
      @listener.close rescue nil
      @thread.kill
      @thread.join(2)
      @clients.each { |t| t.kill; t.join(1) }
    end

    private

    def accept_loop
      loop do
        sock = @listener.accept
        @clients << Thread.new(sock) { |s| handle(s) }
      end
    rescue IOError, Errno::EBADF
      nil
    end

    def handle(sock)
      request_line = sock.gets.to_s
      while (line = sock.gets) && line != "\r\n"; end
      path = request_line.split(' ')[1].to_s
      @requests << path

      case path
      when %r{\A/delay/([\d.]+)}
        sleep Float($1)
        respond(sock, "delayed")
      when '/hang'
        TestCurbCurlMultiPoll.pop_within(@release, 10)
        respond(sock, "released")
      else
        respond(sock, "fast")
      end
    rescue IOError, SystemCallError
      nil
    ensure
      sock.close rescue nil
    end

    def respond(sock, body)
      sock.write("HTTP/1.1 200 OK\r\nContent-Length: #{body.bytesize}\r\nConnection: close\r\n\r\n#{body}")
    end
  end

  class PollInterrupt < StandardError; end

  def setup
    super
    @server = TinyServer.new
    @original_default_timeout = Curl::Multi.default_timeout
  end

  def teardown
    Curl::Multi.default_timeout = @original_default_timeout
    @server.close if @server
    super
  end

  def monotonic
    Process.clock_gettime(Process::CLOCK_MONOTONIC)
  end

  # Occupy low descriptor numbers so every socket libcurl opens afterwards is
  # numbered above FD_SETSIZE and cannot be represented in an fd_set.
  def with_fds_above_fd_setsize
    omit('fd numbering differs on Windows') if WINDOWS

    soft, hard = Process.getrlimit(:NOFILE)
    needed = HIGH_FD_TARGET + 256
    if soft < needed
      omit("RLIMIT_NOFILE hard limit #{hard} is too low") if hard != Process::RLIM_INFINITY && hard < needed
      Process.setrlimit(:NOFILE, needed, hard)
    end

    hog = []
    hog << File.open(File::NULL) while hog.empty? || hog.last.fileno < HIGH_FD_TARGET

    probe = Socket.new(:INET, :STREAM)
    assert_operator probe.fileno, :>, FD_SETSIZE, "new sockets should be numbered above FD_SETSIZE"
    probe.close

    yield
  ensure
    hog.each(&:close) if hog
    Process.setrlimit(:NOFILE, soft, hard) if soft && Process.getrlimit(:NOFILE)[0] != soft
  end

  # A request whose socket is above FD_SETSIZE used to be invisible to the
  # curl_multi_fdset/select loop, which then fell back to fixed 100ms sleeps,
  # so each request took at least 100ms. Ten sequential local requests would
  # need a full second.
  def test_easy_perform_with_socket_fds_above_fd_setsize
    with_fds_above_fd_setsize do
      started = monotonic
      10.times do
        assert_equal "fast", Curl.get(@server.url('/fast')).body_str
      end
      elapsed = monotonic - started

      assert_operator elapsed, :<, 0.6, "10 local requests took #{elapsed.round(3)}s with high-numbered sockets"
    end
  end

  # Batches of concurrent requests share each 100ms sleep of the old loop, so
  # run several batches to make that fixed floor (>= 1s total) visible.
  def test_multi_concurrent_requests_with_socket_fds_above_fd_setsize
    with_fds_above_fd_setsize do
      started = monotonic
      5.times do |batch|
        multi = Curl::Multi.new
        easies = 20.times.map do |i|
          easy = Curl::Easy.new(@server.url("/fast?#{batch}-#{i}"))
          multi.add(easy)
          easy
        end

        multi.perform

        easies.each do |easy|
          assert_equal 200, easy.response_code
          assert_equal "fast", easy.body_str
        end
      ensure
        multi.close if multi
      end
      elapsed = monotonic - started

      assert_operator elapsed, :<, 0.8, "5 batches of 20 concurrent local requests took #{elapsed.round(3)}s with high-numbered sockets"
    end
  end

  # With a large default_timeout, readiness on the transfer's socket must still
  # wake the wait immediately rather than waiting out the timeout.
  def test_long_default_timeout_still_wakes_on_socket_activity
    Curl::Multi.default_timeout = 5_000

    multi = Curl::Multi.new
    fast = Curl::Easy.new(@server.url('/fast'))
    delayed = Curl::Easy.new(@server.url('/delay/0.3'))
    multi.add(fast)
    multi.add(delayed)

    started = monotonic
    multi.perform
    elapsed = monotonic - started

    assert_equal "fast", fast.body_str
    assert_equal "delayed", delayed.body_str
    assert_operator elapsed, :<, 2.0, "perform waited #{elapsed.round(3)}s; the 5s default_timeout should not delay completion"
  ensure
    multi.close if multi
  end

  # The block passed to perform is yielded each time the wait times out, so a
  # small default_timeout keeps it running while a transfer is idle.
  def test_perform_block_yields_while_waiting
    Curl::Multi.default_timeout = 20

    multi = Curl::Multi.new
    easy = Curl::Easy.new(@server.url('/delay/0.4'))
    multi.add(easy)

    yields = 0
    multi.perform { yields += 1 }

    assert_equal "delayed", easy.body_str
    assert_operator yields, :>=, 5, "perform block ran #{yields} times during a 400ms idle transfer"
  ensure
    multi.close if multi
  end

  def run_interruptible_hang
    Curl::Multi.default_timeout = 30_000
    errors = Queue.new

    worker = Thread.new do
      Thread.current.report_on_exception = false
      begin
        Curl.get(@server.url('/hang'))
        errors << nil
      rescue Exception => e
        errors << e
        raise
      end
    end

    assert_equal '/hang', self.class.pop_within(@server.requests, 5), "server never received the request"
    # Give the worker a moment to settle into the blocking wait.
    sleep 0.1

    started = monotonic
    yield worker
    joined = worker.join(3) rescue worker
    elapsed = monotonic - started

    assert_not_nil joined, "worker was still blocked 3s after being interrupted"
    assert_operator elapsed, :<, 2.0, "interrupt took #{elapsed.round(3)}s to stop the wait"
    errors.empty? ? nil : errors.pop
  ensure
    if worker&.alive?
      worker.kill
      worker.join(1)
    end
  end

  def test_thread_raise_interrupts_long_wait
    error = run_interruptible_hang { |worker| worker.raise(PollInterrupt, "stop waiting") }
    assert_kind_of PollInterrupt, error
  end

  def test_thread_kill_interrupts_long_wait
    run_interruptible_hang { |worker| worker.kill }
  end

  # A trapped signal must run its handler promptly during a long wait, and the
  # wait must then resume so the transfer still completes. The signal can
  # interrupt the poll with EINTR directly, so this guards against treating
  # that as a failed wait rather than exercising the unblocking function
  # (the Thread#raise/#kill tests cover that).
  def test_signal_trap_runs_during_long_wait_and_transfer_completes
    omit('POSIX signals are not available on Windows') if WINDOWS
    Curl::Multi.default_timeout = 30_000

    handled_at = nil
    previous = trap('USR2') { handled_at = monotonic }
    trapped = true
    sent_at = nil

    signaller = Thread.new do
      self.class.pop_within(@server.requests, 5)
      sleep 0.1
      sent_at = monotonic
      Process.kill('USR2', Process.pid)
      sleep 0.3
      @server.release_hung(1)
    end

    easy = Curl.get(@server.url('/hang'))
    signaller.join(5)

    assert_equal "released", easy.body_str
    assert_not_nil handled_at, "USR2 handler never ran"
    assert_operator handled_at - sent_at, :<, 0.25, "signal handler was delayed #{(handled_at - sent_at).round(3)}s by the wait"
  ensure
    trap('USR2', previous || 'DEFAULT') if trapped
    signaller&.kill
  end
end
