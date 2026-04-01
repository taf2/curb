# frozen_string_literal: true

require File.expand_path('helper', __dir__)

class TestGcCompact < Test::Unit::TestCase
  ITERATIONS = (ENV['CURB_GC_COMPACT_ITERATIONS'] || 50).to_i
  EASY_PER_MULTI = 3
  GC_CRASH_REGRESSION_ITERATIONS = (ENV['CURB_GC_CRASH_REGRESSION_ITERATIONS'] || [ITERATIONS, 10].min).to_i

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

  def test_gc_compact_after_failed_implicit_multi_cleanup
    GC_CRASH_REGRESSION_ITERATIONS.times do
      run_failed_implicit_multi_cleanup_iteration(compact: true, cleanup_with_objectspace: false)
      collect_after_iteration
    end
  end

  def test_gc_cleanup_after_failed_implicit_multi_callback
    omit('GC cleanup isolation requires fork') if NO_FORK || WINDOWS

    assert_child_process_success do
      GC_CRASH_REGRESSION_ITERATIONS.times do
        run_failed_implicit_multi_cleanup_iteration(compact: true, cleanup_with_objectspace: true)
      end
    end
  end

  def test_gc_cleanup_after_invalid_multi_assignment_rejection
    omit('GC cleanup isolation requires fork') if NO_FORK || WINDOWS

    assert_child_process_success do
      GC_CRASH_REGRESSION_ITERATIONS.times do
        easy = Curl::Easy.new($TEST_URL)

        begin
          easy.multi = Object.new
          raise 'expected TypeError from Curl::Easy#multi='
        rescue TypeError => error
          raise "unexpected error message: #{error.message}" unless /Curl::Multi/.match?(error.message)
        end

        easy = nil
        full_gc(compact: true)
      end
    end
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
    multi.perform { compact }
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
    multi.perform { compact }

    handles.each { |easy| multi.remove(easy); compact }
    compact
  ensure
    multi&.close
  end

  def run_failed_implicit_multi_cleanup_iteration(compact:, cleanup_with_objectspace:)
    easy = Curl::Easy.new($TEST_URL)
    easy.on_complete { raise 'complete blew up' }

    begin
      easy.perform
      raise 'expected callback abort'
    rescue Curl::Err::AbortedByCallbackError => error
      raise "unexpected callback message: #{error.message}" unless error.message == 'complete blew up'
    end

    raise 'expected implicit cleanup to clear easy.multi' unless easy.multi.nil?
  ensure
    easy = nil
    if cleanup_with_objectspace
      run_teardown_multi_cleanup
      flush_deferred_multi_closes_all
    else
      flush_deferred_multi_closes_all
    end
    full_gc(compact: compact)
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

  def full_gc(compact: false)
    GC.start(full_mark: true, immediate_sweep: true)
    GC.compact if compact && GC.respond_to?(:compact)
    GC.start(full_mark: true, immediate_sweep: true)
  end

  def flush_deferred_multi_closes_all
    return unless Curl::Easy.respond_to?(:flush_deferred_multi_closes)

    Curl::Easy.flush_deferred_multi_closes(all_threads: true)
  end

  def run_teardown_multi_cleanup
    ObjectSpace.each_object(Curl::Multi) do |multi|
      begin
        next if multi.instance_variable_defined?(:@deferred_close) && multi.instance_variable_get(:@deferred_close)

        multi.instance_variable_set(:@requests, {})
        multi._close
      rescue StandardError
        nil
      end
    end
  end

  def assert_child_process_success
    pid = fork do
      begin
        yield
        exit! 0
      rescue Exception => e
        warn("#{e.class}: #{e.message}")
        warn(e.backtrace.join("\n")) if e.backtrace
        exit! 1
      end
    end

    _child_pid, status = Process.wait2(pid)
    assert_predicate status, :success?
  end

  def collect_after_iteration
    ObjectSpace.garbage_collect
    ObjectSpace.garbage_collect
  end
end
