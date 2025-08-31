#!/usr/bin/env ruby

# Use the locally built curb (extension + libs) instead of any installed gem.
$LOAD_PATH.unshift File.expand_path('../lib', __dir__)
$LOAD_PATH.unshift File.expand_path('../ext', __dir__)

require 'curl'

# Demo parameters
GROUPS = (ENV['GROUPS'] || 3).to_i
PER_GROUP = (ENV['PER_GROUP'] || 5).to_i
CONNECT_TIMEOUT = (ENV['CONNECT_TIMEOUT'] || 5).to_i
TOTAL_TIMEOUT   = (ENV['TOTAL_TIMEOUT'] || 15).to_i
DELAY_S   = (ENV['DELAY_S'] || 0.25).to_f

require 'webrick'
require 'async'

# Start a small local HTTP server on loopback and wait until it’s listening.
ready = Queue.new
server = WEBrick::HTTPServer.new(
  BindAddress: '127.0.0.1',
  Port: (ENV['PORT'] || 0).to_i,    # 0 chooses a free port
  Logger: WEBrick::Log.new($stderr, WEBrick::Log::FATAL),
  AccessLog: [],
  StartCallback: -> { ready << true }
)
server.mount_proc('/test') do |_req, res|
  sleep DELAY_S
  res.status = 200
  res['Content-Type'] = 'text/plain'
  res.body = 'OK'
end

server_thread = Thread.new { server.start }
ready.pop # wait until the server is bound
bound_port = server.listeners.first.addr[1]
URL = "http://127.0.0.1:#{bound_port}/test"

puts "Async demo: groups=#{GROUPS} per_group=#{PER_GROUP} url=#{URL} delay=#{DELAY_S}s"

def crawl(url)
  c = Curl::Easy.new
  c.url = url
  c.connect_timeout = CONNECT_TIMEOUT
  c.timeout = TOTAL_TIMEOUT
  start = Time.now
  code = nil
  error = nil
  begin
    c.perform
    code = c.response_code
  rescue => e
    code = -1
    error = "#{e.class}: #{e.message}"
  end
  { code: code, error: error, duration: (Time.now - start) }
end

results = []
started = Time.now
if Async.respond_to?(:run)
  Async.run do |top|
    GROUPS.times do
      PER_GROUP.times do
        top.async do
          results << crawl(URL)
        end
      end
    end
  end
else
  Async do |top|
    GROUPS.times do
      PER_GROUP.times do
        top.async do
          results << crawl(URL)
        end
      end
    end
  end
end
duration = Time.now - started

ok = results.count { |r| r[:code] == 200 }
fail = results.count { |r| r[:code] != 200 }
puts "Completed #{results.size} requests in #{duration.round(3)}s (ok=#{ok}, fail=#{fail})"

server.shutdown
server_thread.join
