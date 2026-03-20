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

  def self.perform_with_scheduler(easy)
    state = scheduler_state
    waiter = {done: false}
    previous_complete = easy.on_complete do |completed_easy|
      waiter[:done] = true
      previous_complete.call(completed_easy) if previous_complete
    end

    state[:pending] << easy
    ensure_scheduler_driver(state)

    until waiter[:done]
      raise state[:error] if state[:error]
      sleep 0
    end

    true
  ensure
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
          sleep 0
        end

        loop do
          drain_scheduler_pending(state)
          break if state[:multi].idle?

          state[:multi].perform do
            drain_scheduler_pending(state)
          end
        end
      rescue => e
        state[:error] = e
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
      state[:multi].add(pending.shift)
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
