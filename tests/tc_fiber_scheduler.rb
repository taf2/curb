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

  ITERS = 12
  MIN_S = 0.25
  # Each request sleeps 0.25s; two concurrent requests should be ~0.25–0.5s.
  # Allow some jitter in CI environments.
  THRESHOLD = (MIN_S + (MIN_S/2.0))

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
    unless HAS_ASYNC
      warn 'Skipping fiber scheduler test (Async gem not available)'
      return
    end

    url = "http://127.0.0.1:#{@port}/test"

    started = Time.now
    results = []


    Async do
      m = Curl::Multi.new
      ITERS.times.each do
        c = Curl::Easy.new(url)
        c.on_complete { results << c.code }
        m.add(c)
      end
      m.perform
    end

    duration = Time.now - started

    assert duration < THRESHOLD, "Requests did not run concurrently under fiber scheduler (#{duration}s)"
    assert_equal ITERS, results.size
    assert_equal ITERS.times.map {200}, results
  end

  def test_easy_perform_is_scheduler_friendly
    unless HAS_ASYNC
      warn 'Skipping fiber scheduler test (Async gem not available)'
      return
    end

    url = "http://127.0.0.1:#{@port}/test"

    started = Time.now
    results = []

    Async do |top|
      tasks = ITERS.times.map do
        top.async do
          c = Curl.get(url)
          results << c.code
        end
      end
      tasks.each(&:wait)
    end

    duration = Time.now - started

    assert duration < THRESHOLD, "Requests did not run concurrently under fiber scheduler (#{duration}s)"
    assert_equal ITERS, results.size
    assert_equal ITERS.times.map {200}, results
  end
end
if Gem::Version.new(RUBY_VERSION) < Gem::Version.new('3.1')
  warn 'Skipping fiber scheduler tests on Ruby < 3.1'
  return
end
