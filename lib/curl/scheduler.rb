# frozen_string_literal: true

require 'thread'

module Curl
  # Minimal per-thread scheduler driver that multiplexes many Easy handles
  # onto a single persistent Multi when a Fiber scheduler is active.
  # No background fiber is used to avoid preventing the scheduler from exiting.
  class SchedulerDriver
    def self.current
      Thread.current[:curb_scheduler_driver] ||= new
    end

    def initialize
      @multi = Curl::Multi.new
      @mutex = Mutex.new
      @queue = []
      @active = 0
      @driving = false
    end

    def perform_easy(easy)
      promise = { done: false }

      # Chain any existing on_complete handler and signal completion.
      prev = easy.on_complete do |curl|
        begin
          prev.call(curl) if prev
        rescue StandardError
          # Ignore exceptions from chained handlers to not break the driver.
        end
        promise[:done] = true
        @mutex.synchronize { @active -= 1 }
      end

      @mutex.synchronize do
        @active += 1
        @queue << [easy, promise]
      end

      drive_multi_until { promise[:done] }
      true
    end

    private

    def drive_multi_until
      until yield
        became_driver = false
        @mutex.synchronize do
          unless @driving
            @driving = true
            became_driver = true
          end
        end

        if became_driver
          begin
            # Drain queued work before driving and while idle.
            drain_queue!
            @multi.perform do |_m|
              drain_queue!
            end
          ensure
            @mutex.synchronize { @driving = false }
          end
        else
          # Yield to scheduler and let the driver fiber proceed.
          sleep 0
        end
      end
    end

    def drain_queue!
      loop do
        job = nil
        @mutex.synchronize { job = @queue.shift }
        break unless job

        easy, _promise = job
        @multi.add(easy)
      end
    end
  end
end
