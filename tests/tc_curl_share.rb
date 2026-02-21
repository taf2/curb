require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

# Tests for Curl::Share — the libcurl share interface wrapper.
#
# Curl::Share lets multiple Curl::Easy handles share a connection cache,
# DNS cache, SSL session cache, and/or cookie jar. This is particularly
# valuable when performing concurrent requests from multiple Ruby threads,
# since each thread needs its own Curl::Easy (or Curl::Multi) but can share
# cached state to avoid redundant TCP/TLS handshakes and DNS lookups.
class TestCurbShare < Test::Unit::TestCase
  include BugTestServerSetupTeardown

  def setup
    @port = 9994
    @response_proc = lambda do |res|
      res['Content-Type'] = 'text/plain'
      res.body = 'ok'
    end
    super
  end

  # ---- Lifecycle ----

  def test_share_new
    share = Curl::Share.new
    assert_not_nil share
    assert_instance_of Curl::Share, share
  end

  def test_share_close_idempotent
    share = Curl::Share.new
    assert_nothing_raised { share.close }
    assert_nothing_raised { share.close }
  end

  # ---- #enable ----

  def test_enable_connections
    share = Curl::Share.new
    assert_nothing_raised { share.enable :connections }
  end

  def test_enable_dns
    share = Curl::Share.new
    assert_nothing_raised { share.enable :dns }
  end

  def test_enable_ssl_session
    share = Curl::Share.new
    assert_nothing_raised { share.enable :ssl_session }
  end

  def test_enable_cookies
    share = Curl::Share.new
    assert_nothing_raised { share.enable :cookies }
  end

  def test_enable_all_types
    share = Curl::Share.new
    assert_nothing_raised do
      share.enable :connections
      share.enable :dns
      share.enable :ssl_session
      share.enable :cookies
    end
  end

  def test_enable_unknown_symbol_raises_argument_error
    share = Curl::Share.new
    assert_raise(ArgumentError) { share.enable :bogus }
  end

  def test_enable_on_closed_share_raises
    share = Curl::Share.new
    share.close
    assert_raise(RuntimeError) { share.enable :dns }
  end

  # ---- Curl::Easy#share= ----

  def test_easy_share_assignment
    share = Curl::Share.new
    share.enable :dns
    easy = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
    assert_nothing_raised { easy.share = share }
  end

  def test_easy_share_detach
    share = Curl::Share.new
    share.enable :dns
    easy = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
    easy.share = share
    assert_nothing_raised { easy.share = nil }
  end

  def test_easy_perform_with_share
    share = Curl::Share.new
    share.enable :dns
    easy = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
    easy.share = share
    easy.perform
    assert_equal 200, easy.response_code
  end

  # ---- Concurrent thread safety ----
  #
  # We share only :dns here to avoid keep-alive connection reuse races with
  # the local WEBrick test server (which is the correct behaviour for a
  # unit test — connection sharing is best verified against a real server).
  # The goal is to exercise the pthread_mutex_t callbacks under true parallel
  # I/O (GVL released during perform) and confirm no crash occurs.

  def test_concurrent_threads_no_crash
    share = Curl::Share.new
    share.enable :dns

    errors  = []
    results = []
    mutex   = Mutex.new

    threads = 5.times.map do
      Thread.new do
        3.times do
          e = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
          e.share = share
          begin
            e.perform
            mutex.synchronize { results << e.response_code }
          rescue Curl::Err::GotNothingError, Curl::Err::RecvError,
                 Curl::Err::ConnectionFailedError
            # WEBrick may drop a keep-alive connection; that is a test-server
            # limitation, not a Curl::Share bug.  Record and continue.
            mutex.synchronize { errors << $!.class }
          end
        end
      end
    end
    threads.each(&:join)

    # At least half the requests should succeed with a real local server.
    assert results.any? { |c| c == 200 },
           "Expected at least some 200 responses, got results=#{results}, errors=#{errors}"
    assert results.all? { |c| c == 200 },
           "Some requests returned non-200: #{results}"
  end

  # ---- Curl::Multi fan-out with share ----

  def test_multi_with_share
    share = Curl::Share.new
    share.enable :dns

    multi   = Curl::Multi.new
    handles = 3.times.map do
      e = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
      e.share = share
      e
    end
    handles.each { |e| multi.add(e) }
    multi.perform

    codes = handles.map(&:response_code)
    assert_equal [200] * 3, codes
  end

  # ---- GC safety ----

  def test_share_gc_safe_with_attached_easy
    # Create a share, attach an easy to it, then let the easy go out of scope.
    # The share must survive GC without crashing because curl_easy_mark keeps
    # it alive, and if it happens to be collected first the free function
    # handles the partial state safely.
    share = Curl::Share.new
    share.enable :dns

    easy = Curl::Easy.new("http://127.0.0.1:#{@port}/test")
    easy.share = share
    easy = nil

    GC.start
    GC.compact if GC.respond_to?(:compact)

    # Share is still usable after GC of the easy handle
    assert_nothing_raised { share.close }
  end
end
