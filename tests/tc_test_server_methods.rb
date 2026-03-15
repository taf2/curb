require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))
require 'tmpdir'

class TestServerMethodsWaitForServerReady < Test::Unit::TestCase
  include TestServerMethods

  def server_startup_timeout
    0.05
  end

  def unused_port
    server = TCPServer.new('127.0.0.1', 0)
    port = server.addr[1]
    server.close
    port
  end

  def test_wait_for_server_ready_returns_false_when_thread_dies
    thread = Thread.new {}
    thread.join

    started = Process.clock_gettime(Process::CLOCK_MONOTONIC)
    assert_equal false, wait_for_server_ready(unused_port, thread: thread)
    elapsed = Process.clock_gettime(Process::CLOCK_MONOTONIC) - started

    assert_operator elapsed, :<, 0.5
  end

  def test_wait_for_server_ready_times_out_when_server_never_starts
    error = assert_raise(RuntimeError) do
      wait_for_server_ready(unused_port)
    end

    assert_match(/Failed to startup test server/, error.message)
  end
end

class TestServerMethodsLockHandling < Test::Unit::TestCase
  include TestServerMethods

  def setup
    @lock_dir = Dir.mktmpdir('curb-server-lock')
    @__port = unused_port
  end

  def teardown
    super
    FileUtils.remove_entry(@lock_dir) if @lock_dir && File.exist?(@lock_dir)
  end

  def server_startup_timeout
    0.05
  end

  def locked_file
    File.join(@lock_dir, "server_lock-#{@__port}")
  end

  def unused_port
    server = TCPServer.new('127.0.0.1', 0)
    port = server.addr[1]
    server.close
    port
  end

  def test_clear_stale_server_lock_removes_legacy_lock_when_server_is_down
    File.write(locked_file, 'locked')

    clear_stale_server_lock(@__port)

    assert_equal false, File.exist?(locked_file)
  end

  def test_clear_stale_server_lock_removes_dead_pid_lock_when_server_is_down
    File.write(locked_file, "999999\n")

    clear_stale_server_lock(@__port)

    assert_equal false, File.exist?(locked_file)
  end

  def test_clear_stale_server_lock_keeps_live_pid_lock_during_startup_window
    write_server_lock(Process.pid)

    clear_stale_server_lock(@__port)

    assert_equal true, File.exist?(locked_file)
  end

  def test_clear_stale_server_lock_removes_non_responding_live_pid_lock_after_timeout
    write_server_lock(Process.pid)
    stale_time = Time.now - 1
    File.utime(stale_time, stale_time, locked_file)

    clear_stale_server_lock(@__port)

    assert_equal false, File.exist?(locked_file)
  end

  def test_stop_test_server_removes_owned_lock_and_shuts_listener_down
    server_setup(@__port)
    assert_equal true, File.exist?(locked_file)
    assert_equal true, server_responding?(@__port)

    stop_test_server

    assert_equal false, File.exist?(locked_file)
    assert_equal false, server_responding?(@__port)
  end
end
