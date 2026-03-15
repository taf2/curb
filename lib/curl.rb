# frozen_string_literal: true
require 'curb_core'
require 'curl/easy'
require 'curl/multi'
require 'uri'

# expose shortcut methods
module Curl
  def self.scheduler_active?
    Fiber.respond_to?(:scheduler) && !Fiber.scheduler.nil?
  end

  def self.deferred_exception_source_id(state)
    return unless state[:multi].instance_variable_defined?(:@__curb_deferred_exception_source_id)

    state[:multi].instance_variable_get(:@__curb_deferred_exception_source_id)
  end

  def self.scheduler_waiter_blocking_supported?
    scheduler = Fiber.scheduler
    scheduler && scheduler.respond_to?(:block) && scheduler.respond_to?(:unblock)
  end

  def self.wake_scheduler_waiter(waiter)
    fiber = waiter[:fiber]
    scheduler = waiter[:scheduler]
    return unless fiber&.alive? && scheduler&.respond_to?(:unblock)

    scheduler.unblock(waiter, fiber)
  end

  def self.complete_scheduler_waiter(waiter)
    return if waiter[:done]

    waiter[:done] = true
    wake_scheduler_waiter(waiter)
  end

  def self.fail_scheduler_waiter(waiter, error)
    return if waiter[:error]

    waiter[:error] = error
    wake_scheduler_waiter(waiter)
  end

  def self.release_scheduler_error(state, error)
    source_waiter = state[:waiters][deferred_exception_source_id(state)]

    if source_waiter
      fail_scheduler_waiter(source_waiter, error)
    else
      state[:error] = error
      state[:waiters].each_value { |waiter| wake_scheduler_waiter(waiter) }
    end
  end

  def self.block_scheduler_waiter(waiter)
    unless scheduler_waiter_blocking_supported?
      sleep 0
      return
    end

    waiter[:fiber] = Fiber.current
    waiter[:scheduler] ||= Fiber.scheduler
    return if waiter[:done] || waiter[:error]

    waiter[:scheduler].block(waiter, nil)
  ensure
    waiter[:fiber] = nil if waiter[:fiber].equal?(Fiber.current)
  end

  def self.scheduler_yield
    scheduler = Fiber.scheduler

    if scheduler&.respond_to?(:kernel_sleep)
      scheduler.kernel_sleep(0)
    else
      sleep 0
    end
  end

  def self.release_scheduler_waiters(state)
    source_id = deferred_exception_source_id(state)

    state[:waiters].each do |easy_id, waiter|
      next if source_id == easy_id

      complete_scheduler_waiter(waiter) if waiter[:completed]
    end
  end

  def self.perform_with_scheduler(easy)
    state = scheduler_state
    waiter = {completed: false, done: false, error: nil, fiber: nil, scheduler: Fiber.scheduler}
    state[:waiters][easy.object_id] = waiter
    previous_complete = easy.on_complete do |completed_easy|
      previous_complete.call(completed_easy) if previous_complete
      waiter[:completed] = true
    end

    state[:pending] << easy
    ensure_scheduler_driver(state)

    until waiter[:done]
      raise waiter[:error] if waiter[:error]
      raise state[:error] if state[:error]
      block_scheduler_waiter(waiter)
    end

    while state[:driver_running] && state[:pending].empty? &&
          state[:waiters].length == 1 && state[:waiters].key?(easy.object_id)
      scheduler_yield
    end

    true
  ensure
    state[:waiters].delete(easy.object_id) if defined?(state) && state[:waiters]
    if defined?(previous_complete)
      if previous_complete
        easy.on_complete(&previous_complete)
      else
        easy.on_complete
      end
    end
  end

  def self.scheduler_state
    Thread.current.thread_variable_get(:curb_scheduler_state) || begin
      state = {
        multi: Curl::Multi.new,
        pending: [],
        driver_running: false,
        error: nil,
        waiters: {},
      }
      Thread.current.thread_variable_set(:curb_scheduler_state, state)
      state
    end
  end

  def self.ensure_scheduler_driver(state)
    return if state[:driver_running]

    state[:driver_running] = true
    state[:error] = nil

    runner = proc do
      begin
        # Give sibling fibers a chance to enqueue work so the shared multi can
        # batch scheduler-driven Easy#perform calls together.
        pending_count = -1
        until pending_count == state[:pending].size
          pending_count = state[:pending].size
          scheduler_yield
        end

        loop do
          drain_scheduler_pending(state)
          break if state[:multi].idle?

          begin
            state[:multi].perform do
              drain_scheduler_pending(state)
              release_scheduler_waiters(state)
              scheduler_yield
            end
          ensure
            # Release any siblings that completed just before a deferred
            # callback exception is re-raised.
            release_scheduler_waiters(state)
          end
        end
      rescue => e
        release_scheduler_waiters(state)
        release_scheduler_error(state, e)
      ensure
        state[:driver_running] = false
        ensure_scheduler_driver(state) if state[:error].nil? && !state[:pending].empty?
      end
    end

    if Fiber.respond_to?(:schedule)
      Fiber.schedule(&runner)
    else
      Fiber.new(blocking: false, &runner).resume
    end
  end

  def self.drain_scheduler_pending(state)
    pending = state[:pending]
    until pending.empty?
      easy = pending.first

      break if state[:multi].instance_variable_defined?(:@__curb_deferred_exception)

      state[:multi].add(easy)
      break unless state[:multi].requests.key?(easy.object_id)

      pending.shift
    end
  end

  def self.http(verb, url, post_body=nil, put_data=nil, &block)
    if Thread.current[:curb_curl_yielding]
      handle = Curl::Easy.new # we can't reuse this
    else
      handle = Thread.current[:curb_curl] ||= Curl::Easy.new
      handle.reset
    end
    handle.url = url
    handle.post_body = post_body if post_body
    handle.put_data = put_data if put_data
    if block_given?
      Thread.current[:curb_curl_yielding] = true
      yield handle
      Thread.current[:curb_curl_yielding] = false
    end
    handle.http(verb)
    handle
  end

  def self.get(url, params={}, &block)
    http :GET, urlalize(url, params), nil, nil, &block
  end

  def self.post(url, params={}, &block)
    http :POST, url, postalize(params), nil, &block
  end

  def self.put(url, params={}, &block)
    http :PUT, url, nil, postalize(params), &block
  end

  def self.delete(url, params={}, &block)
    http :DELETE, url, postalize(params), nil, &block
  end

  def self.patch(url, params={}, &block)
    http :PATCH, url, postalize(params), nil, &block
  end

  def self.head(url, params={}, &block)
    http :HEAD, urlalize(url, params), nil, nil, &block
  end

  def self.options(url, params={}, &block)
    http :OPTIONS, urlalize(url, params), nil, nil, &block
  end

  def self.urlalize(url, params={})
    uri = URI(url)
    # early return if we didn't specify any extra params
    return uri.to_s if (params || {}).empty?

    params_query = URI.encode_www_form(params || {})
    uri.query = [uri.query.to_s, params_query].reject(&:empty?).join('&')
    uri.to_s
  end

  def self.postalize(params={})
    params.respond_to?(:map) ? URI.encode_www_form(params) : (params.respond_to?(:to_s) ? params.to_s : params)
  end

  def self.reset
    Thread.current[:curb_curl] = Curl::Easy.new
  end

end
