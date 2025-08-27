# frozen_string_literal: true
require 'curb_core'
require 'curl/easy'
require 'curl/multi'
require 'curl/scheduler'
require 'uri'

# expose shortcut methods
module Curl

  def self.http(verb, url, post_body=nil, put_data=nil, &block)
    # In a fiber-scheduler environment, prefer a fresh Easy handle per call to
    # avoid sharing a single per-thread handle across concurrent fibers.
    scheduler_active = defined?(Fiber) && Fiber.respond_to?(:scheduler) && Fiber.scheduler
    use_fresh_easy = Thread.current[:curb_curl_yielding] || scheduler_active

    if use_fresh_easy
      handle = Curl::Easy.new
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
