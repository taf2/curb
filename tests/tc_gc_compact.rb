# frozen_string_literal: true

require File.expand_path('helper', __dir__)

class TestGcCompact < Test::Unit::TestCase
  ITERATIONS = (ENV['CURB_GC_COMPACT_ITERATIONS'] || 5).to_i
  EASY_PER_MULTI = 3

  def setup
    omit('GC.compact unavailable on this Ruby') unless defined?(GC.compact)
  end

  def test_multi_perform_with_gc_compact
    ITERATIONS.times do
      multi = Curl::Multi.new
      add_easy_handles(multi)

      compact
      assert_nothing_raised { multi.perform }
      compact
    end
  end

  def test_gc_compact_during_multi_cleanup
    ITERATIONS.times do
      multi = Curl::Multi.new
      add_easy_handles(multi)

      compact
      multi = nil
      compact
    end
  end

  def test_gc_compact_after_detach
    multi = Curl::Multi.new
    handles = add_easy_handles(multi)

    compact
    assert_nothing_raised { multi.perform }

    handles.each { |easy| multi.remove(easy) }
    compact
  end

  private

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
end
