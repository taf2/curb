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
        Curl.safe! do |config|
          config.protocols = [:http]
          config.network_policy = :none # This concurrency fixture uses localhost.
        end
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

  def test_multipart_snapshots_are_private_to_parallel_ractors
    omit_unless_curb_ractor_safe
    worker_count = Integer(ENV.fetch('CURB_MULTIPART_RACTORS', '4'))
    request_count = Integer(ENV.fetch('CURB_MULTIPART_REQUESTS', '5'))
    workers = worker_count.times.map do |worker_id|
      Ractor.new(TestServlet.url, worker_id, request_count) do |url, id, count|
        easy = Curl::Easy.new(url)
        easy.multipart_form_post = true
        count.times do |iteration|
          name = +"field_#{id}_#{iteration}"
          payload = "payload_#{id}_#{iteration}\0" + ('x' * 1024)
          expected_name = name.dup
          expected_payload = payload.dup
          direct = Curl::PostField.content(name, payload, 'application/octet-stream')
          file = Curl::PostField.file('upload', 'payload.bin') { payload }
          later = Curl::PostField.content('later') do
            name.replace('changed' * 1000)
            payload.replace('changed' * 1000)
            # Ruby #21537: explicit GC in a Windows Ractor can deadlock
            # the VM barrier. Keep parallel mutation coverage on Windows;
            # single-Ractor compaction is covered by the PostField tests.
            if iteration % 10 == 0 && !WINDOWS
              GC.start
              # Ruby 3.4 can crash in method/constant lookup when another
              # Ractor compacts the heap, even with plain TCPSocket requests
              # and no curb loaded. Keep ordinary GC and parallel mutation
              # here; tc_curl_postfield covers compaction without Ractors.
              GC.compact if GC.respond_to?(:compact) && !RUBY_VERSION.start_with?('3.4.')
            end
            'done'
          end
          easy.http_post(direct, file, later)
          body = easy.body_str
          unless easy.response_code == 200 &&
                 body.include?("name=\"#{expected_name}\"") &&
                 body.include?('filename="payload.bin"') &&
                 body.scan(expected_payload).length == 2 &&
                 !body.include?('changed')
            raise "Multipart snapshot changed in Ractor #{id}, request #{iteration}"
          end
        end
        easy.close
        count
      end
    end
    assert_equal Array.new(worker_count, request_count), workers.map { |worker| ractor_value(worker) }
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
