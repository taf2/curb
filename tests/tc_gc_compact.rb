# frozen_string_literal: true

require File.expand_path('helper', __dir__)

class TestGcCompact < Test::Unit::TestCase
  ITERATIONS = (ENV['CURB_GC_COMPACT_ITERATIONS'] || 50).to_i
  EASY_PER_MULTI = 3

  def setup
    omit('GC.compact unavailable on this Ruby') unless defined?(GC.compact)
  end

  def test_multi_perform_with_gc_compact
    ITERATIONS.times do
      run_multi_perform_compact_iteration
      collect_after_iteration
    end
  end

  def test_gc_compact_during_multi_cleanup
    omit('GC cleanup isolation requires fork') if NO_FORK || WINDOWS

    pid = fork do
      begin
        ITERATIONS.times do
          run_multi_cleanup_compact_iteration
          collect_after_iteration
        end
        exit! 0
      rescue StandardError => e
        warn("#{e.class}: #{e.message}")
        warn(e.backtrace.join("\n")) if e.backtrace
        exit! 1
      end
    end

    _child_pid, status = Process.wait2(pid)
    assert_predicate status, :success?
  end

  def test_gc_compact_after_detach
    run_multi_detach_compact_iteration
    collect_after_iteration
  end

  def test_gc_compact_easy
    iteration = 0
    responses = []
    while iteration < ITERATIONS
      res = Curl.get($TEST_URL) do |easy|
        easy.timeout = 5
        easy.on_complete { |_e| }
        easy.on_failure { |_e, _code| }
      end
      iteration += 1
      responses << res.body
      compact
    end
  end

  private

  def run_multi_perform_compact_iteration
    multi = Curl::Multi.new
    handles = add_easy_handles(multi)

    compact
    multi.perform
    compact
  ensure
    handles&.each do |easy|
      begin
        easy.close
      rescue StandardError
        nil
      end
    end
    multi&.close
  end

  def run_multi_cleanup_compact_iteration
    multi = Curl::Multi.new
    add_easy_handles(multi)

    compact
    multi = nil
    compact
  end

  def run_multi_detach_compact_iteration
    multi = Curl::Multi.new
    handles = add_easy_handles(multi)

    compact
    multi.perform

    handles.each { |easy| multi.remove(easy) }
    compact
  ensure
    multi&.close
  end

  def add_easy_handles(multi)
    Array.new(EASY_PER_MULTI) do
      Curl::Easy.new($TEST_URL) do |easy|
        easy.timeout = 5
        easy.on_complete { |_e| }
        easy.on_failure { |_e, _code| }
      end.tap { |easy| multi.add(easy) }
    end
  end

  def compact
    GC.compact
  end

  def collect_after_iteration
    ObjectSpace.garbage_collect
    ObjectSpace.garbage_collect
  end
end
