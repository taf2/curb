require 'curb'
require 'uri'

# expose shortcut methods
module Curl
  
  def self.http(verb, url, post_body=nil, put_data=nil, &block)
    handle = Curl::Easy.new(url)
    handle.post_body = post_body if post_body
    handle.put_data = put_data if put_data
    yield handle if block_given?
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
    http :OPTIONS, urlalize(url, params), nil, nil, &block
  end

  def self.options(url, params={}, &block)
    http :OPTIONS, urlalize(url, params), nil, nil, &block
  end

  def self.urlalize(url, params={})
    query_str = params.map {|k,v| "#{URI.escape(k.to_s)}=#{URI.escape(v.to_s)}" }.join('&')
    if url.match(/\?/)
      "#{url}&#{query_str}"
    else
      "#{url}?#{query_str}"
    end
  end

  def self.postalize(params={})
    params.map {|k,v| "#{URI.escape(k.to_s)}=#{URI.escape(v.to_s)}" }.join('&')
  end

end
