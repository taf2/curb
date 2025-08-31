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

  ITERS = 4
  MIN_S = 0.25
  # Each request sleeps 0.25s; two concurrent requests should be ~0.25–0.5s.
  # Allow some jitter in CI environments.
  THRESHOLD = ((MIN_S*(ITERS/2.0)) + (MIN_S/2.0)) # add more jitter for slower CI environments
  SERIAL_TIME_WOULD_BE_ABOUT = MIN_S * ITERS

  def setup
    @port = 9993

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
    end

    assert_equal 200, result
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
    end

    assert_equal [200, 200], results
  end

  private
  def skip_no_async
    if WINDOWS
      warn 'Skipping fiber scheduler tests on Windows'
      return true
    end
    unless HAS_ASYNC
      warn 'Skipping fiber scheduler test (Async gem not available)'
      return true
    end
    false
  end

  def async_run(&block)
    # Prefer newer Async.run to avoid deprecated scheduler.async path.
    if defined?(Async) && Async.respond_to?(:run)
      Async.run(&block)
    else
      Async(&block)
    end
  end
end
if Gem::Version.new(RUBY_VERSION) < Gem::Version.new('3.1')
  warn 'Skipping fiber scheduler tests on Ruby < 3.1'
  return
end
