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

  def setup
    @port = 9993
    # Make each response take ~0.25s so sequential total would be ~0.5s.
    @response_proc = lambda do |res|
      sleep 0.25
      res.body = "ok"
      res['Content-Type'] = 'text/plain'
    end
    super
  end

  def test_easy_perform_is_scheduler_friendly
    unless HAS_ASYNC
      warn 'Skipping fiber scheduler test (Async gem not available)'
      return
    end

    url = "http://127.0.0.1:#{@port}/test"

    started = Time.now
    results = []

    Async do
      tasks = 2.times.map do
        Async do
          c = Curl::Easy.new(url)
          c.perform
          results << c.response_code
        end
      end
      tasks.each(&:wait)
    end

    duration = Time.now - started

    # Each request sleeps 0.25s; two concurrent requests should be ~0.25–0.5s.
    # Allow some jitter in CI environments.
    assert duration < 0.55, "Requests did not run concurrently under fiber scheduler (#{duration}s)"
    assert_equal [200, 200], results.sort
  end
end
if Gem::Version.new(RUBY_VERSION) < Gem::Version.new('3.1')
  warn 'Skipping fiber scheduler tests on Ruby < 3.1'
  return
end
