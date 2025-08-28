# frozen_string_literal: true

require 'thread'

module Curl
  # Minimal per-thread scheduler driver that multiplexes many Easy handles
  # onto a single persistent Multi when a Fiber scheduler is active.
  # No background fiber is used to avoid preventing the scheduler from exiting.
  class SchedulerDriver
    # Heuristic to determine if we are running under a cooperative scheduler
    # (Ruby Fiber scheduler or Async task).
    def self.active?
      # Ruby Fiber scheduler explicitly set:
      begin
        return true if defined?(Fiber) && Fiber.respond_to?(:scheduler) && Fiber.scheduler
      rescue StandardError
      end

      # Async::Task context:
      begin
        if defined?(::Async) && defined?(::Async::Task)
          # Prefer current? if available:
          if ::Async::Task.respond_to?(:current?)
            return true if ::Async::Task.current?
          end
          # Fallback to current which returns the current task or nil:
          if ::Async::Task.respond_to?(:current)
            return true if ::Async::Task.current
          end
        end
      rescue StandardError
      end

      # Non-main fiber (some schedulers may not expose Fiber.scheduler):
      begin
        return true if defined?(Fiber) && Fiber.respond_to?(:current) && defined?(Fiber.main) && Fiber.current != Fiber.main
      rescue StandardError
      end

      false
    end
    def self.current
      # Prefer per-thread driver stored in true thread-local storage.
      driver = Thread.current.thread_variable_get(:curb_scheduler_driver)
      return driver if driver

      driver = Curl.instance_variable_get(:@_curb_scheduler_driver)
      unless driver
        driver = new
        Curl.instance_variable_set(:@_curb_scheduler_driver, driver)
      end
      Thread.current.thread_variable_set(:curb_scheduler_driver, driver)
      driver
    end

    def initialize
      @multi = Curl::Multi.new
      @mutex = Mutex.new
      @queue = []
      @active = 0
      @driving = false
      @batch_deadline = nil
      # Initialized once per process (then attached to thread on demand).
    end

    def perform_easy(easy)
      promise = { done: false }

      # Chain any existing on_complete handler and signal completion.
      prev = easy.on_complete do |curl|
        begin
          prev.call(curl) if prev
        rescue StandardError => e
          puts "error?-> #{e.message}\n#{e.backtrace.join("\n")}"
          # Ignore exceptions from chained handlers to not break the driver.
        end
        promise[:done] = true
        @mutex.synchronize { @active -= 1 }
      end

      batch_deadline = nil
      now = Process.clock_gettime(Process::CLOCK_MONOTONIC)
      @mutex.synchronize do
        @active += 1
        @queue << [easy, promise]
        # Establish a very short batching window so multiple tasks enqueued
        # in the same tick can be picked up before any fiber becomes the driver.
        @batch_deadline ||= now + 0.002 # ~2ms
        batch_deadline = @batch_deadline
      end

      # If within the batching window, yield until the deadline so more tasks
      # can enqueue before we attempt to drive the multi loop.
      while (remaining = batch_deadline - Process.clock_gettime(Process::CLOCK_MONOTONIC)) > 0
        sleep(remaining < 0.001 ? 0 : remaining)
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
            @mutex.synchronize do
              @driving = false
              # Reset batching window after a full pass if no queued/active work.
              @batch_deadline = nil if @active == 0 && @queue.empty?
            end
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
