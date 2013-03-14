require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

# This test suite requires the timeout server to be running
# See tests/timeout.rb for more info about the timeout server
class TestCurbSignals < Test::Unit::TestCase

  # Testcase for https://github.com/taf2/curb/issues/117
  def test_continue_after_signal
    trap("SIGUSR1") { }

    curl = Curl::Easy.new(wait_url(2))
    pid = $$
    Thread.new do
      sleep 1
      Process.kill("SIGUSR1", pid)
    end
    assert_equal true, curl.http_get
  end

  private
  
  def wait_url(time)
    "#{server_base}/wait/#{time}"
  end
  
  def serve_url(chunk_size, time, count)
    "#{server_base}/serve/#{chunk_size}/every/#{time}/for/#{count}"
  end
  
  def server_base
    'http://127.0.0.1:9128'
  end
end
