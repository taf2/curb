# Reproduces the FIBER_TODO.md [Critical] finding: a Fiber scheduler that
# implements io_wait/kernel_sleep/block/unblock but NOT the optional io_select
# hook. rb_fiber_scheduler_io_select returns Qundef (without waiting) for such
# schedulers; if the multi-fd wait path of Curl::Multi#_socket_perform treats
# that as a completed wait, the drive loop spins at 100% CPU with no interrupt
# checkpoint and this script never exits — it cannot even service SIGTERM.
#
# Run by tc_fiber_scheduler.rb in a subprocess guarded by a SIGKILL fuse.
# Usage: ruby io_select_less_scheduler_probe.rb <port>
#   <port> must serve HTTP GET /test (see BugTestServerSetupTeardown).

require 'fiber'
require 'curb'

port = Integer(ARGV[0])
url = "http://127.0.0.1:#{port}/test"

class NoIoSelectScheduler
  def fiber(&block)
    Fiber.new(blocking: false, &block)
  end

  def io_wait(io, events, timeout = nil)
    readers = (events & IO::READABLE) != 0 ? [io] : nil
    writers = (events & IO::WRITABLE) != 0 ? [io] : nil
    deadline = monotonic_now + timeout if timeout

    loop do
      readable, writable = IO.select(readers, writers, nil, 0)
      ready = 0
      ready |= IO::READABLE if readable && !readable.empty?
      ready |= IO::WRITABLE if writable && !writable.empty?
      return ready unless ready.zero?
      return false if deadline && monotonic_now >= deadline

      Fiber.yield
    end
  end

  def kernel_sleep(duration = nil)
    deadline = monotonic_now + (duration || 0)
    Fiber.yield while monotonic_now < deadline
    duration
  end

  def block(_blocker, timeout = nil)
    kernel_sleep(timeout)
    false
  end

  def unblock(*); end

  def close; end

  def fiber_interrupt(*); end

  private

  def monotonic_now
    Process.clock_gettime(Process::CLOCK_MONOTONIC)
  end
end

results = []
ticks = 0
done = false

Fiber.set_scheduler(NoIoSelectScheduler.new)
fiber = Fiber.schedule do
  multi = Curl::Multi.new
  begin
    # Two easies so the drive loop tracks more than one socket and takes the
    # multi-fd (io_select) wait branch.
    2.times do
      easy = Curl::Easy.new(url)
      easy.on_complete { results << easy.response_code }
      multi.add(easy)
    end
    multi.send(:_socket_perform)
  ensure
    multi.close
    done = true
  end
end
ticker = Fiber.schedule do
  until done
    sleep 0.005
    ticks += 1 unless done
  end
end

while fiber.alive? || ticker.alive?
  fiber.resume if fiber.alive?
  ticker.resume if ticker.alive?
  sleep 0.001
end
Fiber.set_scheduler(nil)

if results == [200, 200]
  puts "OK ticks=#{ticks}"
  exit 0
else
  puts "FAILED results=#{results.inspect}"
  exit 1
end
