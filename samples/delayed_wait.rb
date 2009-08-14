# DelayedEasy hits several domains and then interacts with the
# responses. Each interaction will trigger a check to see if that
# response has already come back and been cached.  If it has not,
# the interaction will be forced to wait for Curl::Easy#perform to
# come back with the data.  In this way, you could fire off several
# requests (ala in a rails controller), and not need to wait until
# the response data is needed (at the last possible second, like
# a view).  Using Curl::Easy means the requests are all truely
# asynchronous, and time spent waiting for one request to finish
# is not wasted, but could be used pulling down another request
# in parallel.
#

# Use local curb
#

$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), '..', 'ext')))
$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), '..', 'lib')))
require 'curb'

# DelayedEasy class
#
# Fires Curb requests right away, then caches the result. If the
# object has any access before the cache is populated, it waits
# on the request to complete.
#

module Curl
  class DelayedEasy
    attr_accessor :response_thread, :delayed_response
    
    def initialize(*args, &blk)
      curl = nil
      self.response_thread = Thread.new {
        curl = Curl::Easy.new(*args, &blk)
        curl.perform
        @delayed_response = curl
      }
      self
    end

    def delayed_response
      @delayed_response or
      ( puts 'waiting...'; self.response_thread.join; self.delayed_response )
    end

    def method_missing(method_name,*args,&blk)
      self.delayed_response.send(method_name, *args, &blk)
    end

  end
end

# An example for DelayedEasy
#
# Some URLs to smack in our demo.
#

urls = ["http://boingboing.net",
        "http://www.yahoo.com",
        "http://slashdot.org",
        "http://www.google.com",
        "http://www.yelp.com",
        "http://www.givereal.com",
        "http://www.google.co.uk/",
        "http://www.ruby-lang.org/"]

# Fire a new DelayedEasy for each url.
#

requests = {}
urls.each do |url|
  requests[url] = Curl::DelayedEasy.new(url)
end

# Now puts the response data. The output will show
# that some interactions need to wait for the response,
# while others proceed with no 'waiting' puts.
#

urls.each do |url|
  puts "[#{url} #{requests[url].total_time}] #{requests[url].body_str[0,30].gsub("\n", '')}..."
  puts
end

# Sample output: 
#
# mixonic@pandora ~/Projects/curb $ time ruby samples/delayed_wait.rb 
# waiting...
# [http://boingboing.net 1.36938] <!DOCTYPE html PUBLIC "-//W3C/...
#
# [http://www.yahoo.com 0.315822] <html><head><title>Yahoo!</t...
#
# [http://slashdot.org 1.007763] <!DOCTYPE HTML PUBLIC "-//W3C/...
#
# [http://www.google.com 0.796156] <!doctype html><html><head><me...
#
# waiting...
# [http://www.yelp.com 1.365811] <!DOCTYPE HTML PUBLIC "-//W3C/...
#
# [http://www.givereal.com 0.803108] <!DOCTYPE html PUBLIC "-//W3C/...
#
# [http://www.google.co.uk/ 0.601572] <!doctype html><html><head><me...
#
# waiting...
# [http://www.ruby-lang.org/ 0.628023] <html><body>302 Found</body></...
#
#
# real  0m1.658s
# user  0m0.043s
# sys 0m0.030s
#
