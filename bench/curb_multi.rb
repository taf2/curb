require 'rubygems'
$:.unshift File.expand_path(File.dirname(__FILE__))
require '_usage'

$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','ext'))
$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','lib'))

N = (ARGV.shift || 50).to_i
BURL = 'http://127.0.0.1/zeros-2k'

Memory.usage("Curl::Multi(#{N})") do
  require 'curb'

  count = 0
  multi = Curl::Multi.new

  multi.pipeline = true
  multi.max_connects = 10

  # maintain a free list of easy handles, better to reuse an open connection than create a new one...
  free = []

  pending = N
  bytes_received = 0

  # initialize first 10
  10.times do
    easy = Curl::Easy.new(BURL)
    easy.on_body {|d| bytes_received += d.size; d.size } # don't buffer
    easy.on_complete do|c|
      free << c
    end
    multi.add easy
    pending-=1
    break if pending <= 0
  end

  until pending == 0
    multi.perform do
      # idle
      if pending > 0 && free.size > 0
        easy = free.pop
        easy.url = BURL
        multi.add easy
        pending -= 1
      end
    end
  end

end
