require File.expand_path('helper', __dir__)

class TestCurbRactor < Test::Unit::TestCase
  include TestServerMethods

  RACTOR_WORKERS = 4
  REQUESTS_PER_WORKER = 3

  def setup
    super
    server_setup
  end

  def test_exported_string_constants_are_shareable
    omit('Ractor is unavailable on this Ruby') unless defined?(Ractor)

    %i[CURB_VERSION VERSION CURL_VERSION LONG_VERSION CURL_LONG_VERSION].each do |name|
      value = Curl.const_get(name)
      assert_predicate value, :frozen?, "Curl::#{name} should be frozen"
      assert Ractor.shareable?(value), "Curl::#{name} should be Ractor-shareable"
    end
  end

  def test_independent_ractors_can_perform_requests_concurrently
    original_timeout = Curl::Multi.default_timeout
    original_autoclose = Curl::Multi.autoclose
    omit_unless_curb_ractor_safe

    url = TestServlet.url

    workers = RACTOR_WORKERS.times.map do |worker_id|
      Ractor.new(url, worker_id, REQUESTS_PER_WORKER) do |target, id, request_count|
        Curl.safe! { |config| config.protocols = [:http] }
        Curl::Multi.default_timeout = 25 + id
        Curl::Multi.autoclose = true

        responses = request_count.times.map do
          chunks = 0
          easy = Curl::Easy.new(target)
          easy.on_body do |data|
            chunks += 1
            data.bytesize
          end
          easy.perform
          [easy.response_code, chunks]
        end

        {
          responses: responses,
          timeout: Curl::Multi.default_timeout,
          autoclose: Curl::Multi.autoclose,
          deferred_closes: Curl::Easy.deferred_multi_closes.length,
          version: Curl::VERSION,
        }
      end
    end

    results = workers.map { |worker| ractor_value(worker) }

    results.each_with_index do |result, worker_id|
      assert_equal REQUESTS_PER_WORKER, result[:responses].length
      result[:responses].each do |status, chunks|
        assert_equal 200, status
        assert_operator chunks, :>=, 1
      end
      assert_equal 25 + worker_id, result[:timeout]
      assert_equal true, result[:autoclose]
      assert_equal 0, result[:deferred_closes]
      assert_equal Curl::VERSION, result[:version]
    end

    assert_equal original_timeout, Curl::Multi.default_timeout
    assert_equal original_autoclose, Curl::Multi.autoclose
  ensure
    Curl::Multi.default_timeout = original_timeout if defined?(original_timeout)
    Curl::Multi.autoclose = original_autoclose if defined?(original_autoclose)
  end

  private

  def omit_unless_curb_ractor_safe
    omit('Ractor is unavailable on this Ruby') unless defined?(Ractor)
    omit('This build does not advertise Ractor-safe native support') unless
      Curl.const_defined?(:RACTOR_SAFE) && Curl::RACTOR_SAFE
  end

  def ractor_value(ractor)
    if ractor.respond_to?(:value)
      ractor.value
    else
      ractor.take
    end
  end
end
